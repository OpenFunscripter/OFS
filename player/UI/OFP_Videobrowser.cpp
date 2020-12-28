#include "OFP_Videobrowser.h"

#include "OFS_Util.h"
#include "EventSystem.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "glad/glad.h"
#include "SDL_thread.h"
#include "SDL_atomic.h"

#include <filesystem>
#include <sstream>
#include <unordered_set>

#include "xxhash.h"
#include "reproc++/run.hpp"

#ifdef WIN32
//used to obtain drives on windows
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

struct TextureHandle {
	int32_t ref_count = 0;
	uint32_t texId = 0;
};

static std::unordered_map<uint32_t, TextureHandle> TextureHashtable;


constexpr int MaxThumbailProcesses = 4;
static SDL_sem* ThumbnailThreadSem = nullptr;

void Videobrowser::updateCache(const std::string& path) noexcept
{
	static struct IterateDirData {
		std::string path;
		bool running = false;
		Videobrowser* browser;
	} IterateThread;
	if (IterateThread.running) return;

	auto iterateDirThread = [](void* ctx) -> int {
		IterateDirData* data = (IterateDirData*)ctx;
		Videobrowser& browser = *data->browser;
		
		SDL_AtomicLock(&browser.ItemsLock);
		browser.Items.clear();
		SDL_AtomicUnlock(&browser.ItemsLock);

		auto pathObj = Util::PathFromString(data->path);
		pathObj = std::filesystem::absolute(pathObj);
		std::error_code ec;
		for (auto& p : std::filesystem::directory_iterator(pathObj, ec)) {
			auto extension = p.path().extension().u8string();


			auto it = std::find_if(BrowserExtensions.begin(), BrowserExtensions.end(),
				[&](auto& ext) {
					return std::strcmp(ext.first, extension.c_str()) == 0;
			});
		
			if (it != BrowserExtensions.end()) {
				auto funscript = p.path();
				funscript.replace_extension(".funscript");
				bool matchingScript = Util::FileExists(funscript.u8string());
				// valid extension + matching script
				size_t byte_count = p.file_size();
				SDL_AtomicLock(&browser.ItemsLock);
				browser.Items.emplace_back(p.path().u8string(), byte_count, it->second, matchingScript);
				SDL_AtomicUnlock(&browser.ItemsLock);
			}
			else if (p.is_directory()) {
				SDL_AtomicLock(&browser.ItemsLock);
				browser.Items.emplace_back(p.path().u8string(), 0, false, false);
				SDL_AtomicUnlock(&browser.ItemsLock);
			}
		}
		SDL_AtomicLock(&browser.ItemsLock);
		std::sort(browser.Items.begin(), browser.Items.end(),
			[](auto& item1, auto& item2) {
				return item1.filename < item2.filename;
			}
		);
		std::sort(browser.Items.begin(), browser.Items.end(), [](auto& item1, auto& item2) {
			return item1.IsDirectory() && !item2.IsDirectory();
		});
		browser.Items.insert(browser.Items.begin(), {(pathObj / "..").u8string(), 0, false, false });
		SDL_AtomicUnlock(&browser.ItemsLock);

		data->running = false;
		return 0;
	};

	IterateThread.running = true;
	IterateThread.path = path;
	IterateThread.browser = this;
	auto thread = SDL_CreateThread(iterateDirThread, "IterateDirectory", &IterateThread);
	SDL_DetachThread(thread);
	CacheNeedsUpdate = false;
}

#ifdef WIN32
void Videobrowser::chooseDrive() noexcept
{
	Items.clear();
	auto AvailableDrives = GetLogicalDrives();
	uint32_t Mask = 0b1;
	std::stringstream ss;
	for (int i = 0; i < 32; i++) {
		if (AvailableDrives & (Mask << i)) {
			ss << (char)('A' + i);
			ss << ":\\\\";
			Items.emplace_back(ss.str(), 0, false, false);
			ss.str("");
		}
	}
}
#endif

Videobrowser::Videobrowser()
{
	if (ThumbnailThreadSem == nullptr) {
		ThumbnailThreadSem = SDL_CreateSemaphore(MaxThumbailProcesses);
	}
}

void Videobrowser::ShowBrowser(const char* Id, bool* open) noexcept
{
	if (CacheNeedsUpdate) { updateCache(CurrentPath); }

	SDL_AtomicLock(&ItemsLock);
	ImGui::Begin(Id, NULL, ImGuiWindowFlags_NoDecoration);
	ImGui::SliderInt("Row", &ItemsPerRow, 1, 100);
	auto availSpace = ImGui::GetContentRegionMax();
	auto& style = ImGui::GetStyle();

	const float ItemWidth = (availSpace.x - (2.f * style.ItemInnerSpacing.x) - (ItemsPerRow*style.ItemSpacing.x)) / (float)ItemsPerRow;
	const float ItemHeight = (9.f/16.f)*ItemWidth;
	const auto ItemDim = ImVec2(ItemWidth, ItemHeight);
	
	ImGui::BeginChild("Items");

	int index = 0;
	for (auto& item : Items) {
		if (!item.IsDirectory()) {
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_PlotLinesHovered]);
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_PlotLines]);
			auto fileClickHandler = [](VideobrowserItem& item) {
				LOGF_DEBUG("File: \"%s\"", item.path.c_str());
				LOGF_DEBUG("Extension: \"%s\"", item.extension.c_str());
				LOGF_DEBUG("Name: \"%s\"", item.filename.c_str());
			};

			uint32_t texId = item.GetTexId();
			ImColor FileTint = item.HasMatchingScript ? IM_COL32_WHITE : IM_COL32(75, 75, 75, 255);
			if (!item.Focussed) {
				FileTint.Value.x *= 0.75f;
				FileTint.Value.y *= 0.75f;
				FileTint.Value.z *= 0.75f;
			}
			if (texId != 0) {
				if (ImGui::ImageButton((void*)(intptr_t)texId, ItemDim, ImVec2(0,0), ImVec2(1,1), 0, ImVec4(0,0,0,0), FileTint)) {
					fileClickHandler(item);
				}
				item.Focussed = ImGui::IsItemFocused() || ImGui::IsItemHovered();
			}
			else {
				if(!item.HasMatchingScript) { ImGui::PushStyleColor(ImGuiCol_Button, FileTint.Value); }
				
				if (ImGui::Button(item.filename.c_str(), ItemDim)) {
					fileClickHandler(item);
				}
				if (!item.HasMatchingScript) { ImGui::PopStyleColor(1); }
			}
			ImGui::PopStyleColor(2);
		}
		else {
			if (ImGui::Button(item.filename.c_str(), ItemDim)) {
#ifdef WIN32
				auto pathObj = Util::PathFromString(item.path);
				auto pathObjAbs = std::filesystem::absolute(pathObj);
				if (pathObj != pathObjAbs && pathObj.root_path() == pathObjAbs) {
					chooseDrive();
				}
				else
#endif
				{
					CurrentPath = item.path;
					CacheNeedsUpdate = true;
				}
			}
		}
		Util::Tooltip(item.filename.c_str());
		index++;
		if (index % ItemsPerRow != 0) {
			ImGui::SameLine();
		}
	}
	ImGui::EndChild();
	ImGui::End();
	SDL_AtomicUnlock(&ItemsLock);
}

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

VideobrowserItem::VideobrowserItem(const std::string& path, size_t byte_count, bool genThumb, bool matchingScript) noexcept
{
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
						LOGF_INFO("Loaded texture: \"%s\"", data->thumbOutputFilePath.c_str());
					}
				}

				delete data;
			}, data);
		};
		GenLoadThreadData* data = (GenLoadThreadData*)user;

		if (SDL_SemWait(ThumbnailThreadSem) != 0) {
			LOGF_ERROR("%s", SDL_GetError());
			return 0;
		}
		if (Util::FileExists(data->thumbOutputFilePath.c_str()))
		{
			loadTextureOnMainThread(data);
			SDL_SemPost(ThumbnailThreadSem);
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
				SDL_SemPost(ThumbnailThreadSem);
				return false;
			}

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

		SDL_SemPost(ThumbnailThreadSem);
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

int32_t VideobrowserEvents::FileClicked = 0;

void VideobrowserEvents::RegisterEvents() noexcept
{
	FileClicked = SDL_RegisterEvents(1);
}
