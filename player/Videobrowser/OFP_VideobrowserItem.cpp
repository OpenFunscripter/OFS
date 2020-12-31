#include "OFP_VideobrowserItem.h"

#include "OFS_Util.h"
#include "EventSystem.h"
#include "OFP_Videobrowser.h"

#include "SDL_thread.h"
#include "SDL_atomic.h"

#include "xxhash.h"
#include "reproc++/run.hpp"
#include "glad/glad.h"

#include <sstream>
#include <filesystem>
#include <unordered_map>

struct TextureHandle {
	int32_t ref_count = 0;
	uint32_t texId = 0;
};

static std::unordered_map<uint32_t, TextureHandle> TextureHashtable;

constexpr int MaxThumbailProcesses = 4;


VideobrowserItem::~VideobrowserItem()
{
	if (HasThumbnail) {
		auto it = TextureHashtable.find(this->Id);
		if (it != TextureHashtable.end()) {
			it->second.ref_count--;
			if (it->second.ref_count == 0 && it->second.texId != 0) {
				glDeleteTextures(1, &it->second.texId);
				it->second.texId = 0;
				LOGF_DEBUG("Freed texture: \"%s\"", this->filename.c_str());
			}
		}
	}
}

uint64_t VideobrowserItem::GetTexId() const
{
	if (!HasThumbnail) return 0;
	auto it = TextureHashtable.find(this->Id);
	if (it != TextureHashtable.end()) {
		return it->second.texId;
	}
	return 0;
}

VideobrowserItem::VideobrowserItem(const std::string& path, size_t byte_count, uint64_t lastEdit, bool genThumb, bool matchingScript) noexcept
{
	this->lastEdit = lastEdit;
	auto pathObj = Util::PathFromString(path);
	pathObj.make_preferred();
	if (!std::filesystem::is_directory(pathObj)) {
		this->extension = pathObj.extension().u8string();
	}

	this->HasMatchingScript = matchingScript;
	this->path = pathObj.u8string();
	this->filename = pathObj.filename().u8string();
	if (this->filename.empty()) {
		this->filename = pathObj.u8string();
	}

	{
		// the byte count gets included in the hash
		// to ensure the thumbnails gets regenerated
		// when the content changes
		std::stringstream ss;
		ss << this->filename;
		ss << byte_count;
		auto hashString = ss.str();
		this->Id = XXH64(hashString.c_str(), hashString.size(), 0);
	}

	if (genThumb) {
		auto it = TextureHashtable.find(this->Id);
		if (it != TextureHashtable.end()) {
			it->second.ref_count++; // increment ref_count
			GenThumbail(false);
		}
		else {
			// insert handle
			TextureHashtable.insert(std::make_pair(this->Id, TextureHandle{1, 0}));
			GenThumbail(true);
		}
	}
}

void VideobrowserItem::GenThumbail(bool startThread) noexcept
{
	HasThumbnail = true;
	auto thumbPath = Util::PrefpathOFP("thumbs");
	Util::CreateDirectories(thumbPath);
	auto thumbPathObj = Util::PathFromString(thumbPath);

	std::string thumbFileName;
	std::string thumbFilePath;
	{
		{
			std::stringstream ss;
			ss << this->Id << ".jpg";
			thumbFileName = ss.str();
		}

		auto thumbFilePathObj = thumbPathObj / thumbFileName;
		thumbFilePath = thumbFilePathObj.u8string();
	}

	// generate/loads thumbnail
	struct GenLoadThreadData {
		std::string videoPath;
		std::string thumbOutputFilePath;
		uint32_t Id;
		bool startFfmpeg = false;
	};

	auto genThread = [](void* user) -> int {
		auto loadTextureOnMainThread = [](GenLoadThreadData* data) {
			EventSystem::SingleShot([](void* ctx) {
				GenLoadThreadData* data = (GenLoadThreadData*)ctx;
				int w, h;
				auto it = TextureHashtable.find(data->Id);
				if (it != TextureHashtable.end() && it->second.ref_count > 0 && it->second.texId == 0) {
					if (Util::LoadTextureFromFile(data->thumbOutputFilePath.c_str(), &it->second.texId, &w, &h)) {
						//LOGF_DEBUG("Loaded texture: \"%s\"", data->thumbOutputFilePath.c_str());
					}
					else {
						LOGF_WARN("Failed loading texture: \"%s\"", data->thumbOutputFilePath.c_str());
					}
				}

				delete data;
				}, data);
		};
		GenLoadThreadData* data = (GenLoadThreadData*)user;

		if (SDL_SemWait(Videobrowser::ThumbnailThreadSem) != 0) {
			LOGF_ERROR("%s", SDL_GetError());
			return 0;
		}
		if (Util::FileExists(data->thumbOutputFilePath.c_str()))
		{
			loadTextureOnMainThread(data);
			SDL_SemPost(Videobrowser::ThumbnailThreadSem);
			return 0;
		}

		auto ffmpeg_path = Util::FfmpegPath();

		char buffer[1024];

		constexpr std::array<const char*, 2> fmts = {
			OFS_SYSTEM_CMD(R"("%s" -y -ss 00:00:26 -i "%s" -vf "thumbnail=120,scale=360:200" -frames:v 1 "%s")"),	// seeks 26 seconds into the video
			OFS_SYSTEM_CMD(R"("%s" -y -i "%s" -vf "thumbnail=120,scale=360:200" -frames:v 1 "%s")")   			// for files shorter than 26 seconds
		};

		bool success = false;
		for (auto& fmt : fmts) {
			int num = stbsp_snprintf(buffer, sizeof(buffer), fmt,
				ffmpeg_path.u8string().c_str(),
				data->videoPath.c_str(),
				data->thumbOutputFilePath.c_str()
			);
			FUN_ASSERT(num < sizeof(buffer), "buffer to small");

			if (num >= sizeof(buffer)) {
				SDL_SemPost(Videobrowser::ThumbnailThreadSem);
				return false;
			}

			LOGF_DEBUG("Running: %s", buffer);
#if WIN32
			auto wide = Util::Utf8ToUtf16(buffer);
			success = _wsystem(wide.c_str()) == 0;
#else
			success = std::system(buffer) == 0;
#endif
			success = success && Util::FileExists(data->thumbOutputFilePath);
			if (success) {
				break;
			}
		}

		SDL_SemPost(Videobrowser::ThumbnailThreadSem);
		if (success) {
			loadTextureOnMainThread(data);
		}
		else {
			delete data;
		}

		return 0;
	};

	GenLoadThreadData* data = new GenLoadThreadData;
	data->Id = this->Id;
	data->thumbOutputFilePath = thumbFilePath;
	data->videoPath = this->path;
	auto thread = SDL_CreateThread(genThread, "GenThumbnail", data);
	SDL_DetachThread(thread);
}

void VideobrowserItem::IncrementRefCount() noexcept
{
	auto it = TextureHashtable.find(this->Id);
	if (it != TextureHashtable.end()) {
		it->second.ref_count++;
	}
}