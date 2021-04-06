#include "OFS_Project.h"
#include "OFS_Util.h"

#include "SDL_thread.h"

#include "EventSystem.h"
#include "OpenFunscripter.h"

bool OFS_Project::FindMedia(const std::string& funscriptPath) noexcept
{
	FUN_ASSERT(MediaPath.empty(), "theres already is a video/audio file");

	std::filesystem::path basePath = Util::PathFromString(funscriptPath);
	basePath.replace_extension("");
	for (auto& ext : OpenFunscripter::SupportedVideoExtensions)
	{
		auto path = basePath.u8string() + ext;
		if (Util::FileExists(path))
		{
			MediaPath = path;		
			LastPath = basePath.u8string() + OFS_Project::Extension;
			return true;
		}
	}

	for (auto& ext : OpenFunscripter::SupportedAudioExtensions)
	{
		auto path = basePath.u8string() + ext;
		if (Util::FileExists(path))
		{
			MediaPath = path;
			LastPath = basePath.u8string() + OFS_Project::Extension;
			return true;
		}
	}

	return false;
}

void OFS_Project::LoadScripts(const std::string& funscriptPath) noexcept
{
	auto loadRelatedScripts = [](OFS_Project* project, const std::string& file) noexcept
	{
	    std::vector<std::filesystem::path> relatedFiles;
	    {
	        auto filename = Util::Filename(file) + '.';
	        auto searchDirectory = Util::PathFromString(file);
	        searchDirectory.remove_filename();
	        std::error_code ec;
	        std::filesystem::directory_iterator dirIt(searchDirectory, ec);
	        for (auto&& pIt : dirIt) {
	            auto p = pIt.path();
	            auto extension = p.extension().u8string();
	            auto currentFilename = p.filename().replace_extension("").u8string();
	            if ( extension == ".funscript" 
	                && Util::StringStartsWith(currentFilename, filename)
	                && currentFilename != filename)
	            {
	                LOGF_DEBUG("%s", p.u8string().c_str());
	                relatedFiles.emplace_back(std::move(p));
	            }
	        }
	    }
	    // reorder for 3d simulator
	    std::array<std::string, 3> desiredOrder {
	        // it's in reverse order
	        ".twist.funscript",
	        ".pitch.funscript",
	        ".roll.funscript"
	    };
	    if (relatedFiles.size() > 1) {
	        for (auto& ending : desiredOrder) {
	            for(int i=0; i < relatedFiles.size(); i++) {
	                auto& path = relatedFiles[i];
	                if (Util::StringEndsWith(path.u8string(), ending)) {
	                    auto move = std::move(path);
	                    relatedFiles.erase(relatedFiles.begin() + i);
	                    relatedFiles.emplace_back(std::move(move));
	                    break;
	                }
	            }
	        }
	    }
	    // load the related files
	    for(int i = relatedFiles.size()-1; i >= 0; i--) {
	        auto& file = relatedFiles[i];
	        project->ImportFunscript(file.u8string());
	    }
	};
	
	if (ImportFunscript(funscriptPath))
	{
		loadRelatedScripts(this, funscriptPath);
	}
}

OFS_Project::OFS_Project() noexcept
{
	ProjectMut = SDL_CreateMutex();
}

OFS_Project::~OFS_Project() noexcept
{
	SDL_DestroyMutex(ProjectMut);
}

void OFS_Project::Clear() noexcept
{
	LastPath.clear();
	MediaPath.clear();
	Funscripts.clear();
	Funscripts.emplace_back(std::move(std::make_shared<Funscript>()));
	Settings = OFS_ScriptSettings();
}

void OFS_Project::Load(const std::string& path) noexcept
{
	FUN_ASSERT(!path.empty(), "path empty");
	LastPath = path;
	ProjectBuffer.clear();
	if (Util::ReadFile(path.c_str(), ProjectBuffer) > 0) {
		Funscripts.clear();
		OFS_BENCHMARK(__FUNCTION__);
		auto state = OFS_Binary::Deserialize(ProjectBuffer, *this);
		if (state == bitsery::ReaderError::NoError) {
			Loaded = true;
		}
		else {
			Clear();
		}
	}
	else {
		FUN_ASSERT(false, "foo");
	}
}

struct SaveThreadData
{
	std::string* path;
	ByteBuffer* buffer;
	SDL_mutex* mut;
} ThreadData;


void OFS_Project::Save(const std::string& path) noexcept
{
	FUN_ASSERT(!path.empty(), "path empty");
	{
		OFS_BENCHMARK(__FUNCTION__);
		SDL_LockMutex(ProjectMut);

		ProjectBuffer.clear();
		OFS_Binary::Serialize(ProjectBuffer, *this);
	}
	
	auto saveThread = [](void* data)
	{
		OFS_BENCHMARK(__FUNCTION__ " Thread");
		SaveThreadData* save = (SaveThreadData*)data;
		Util::WriteFile(save->path->c_str(), save->buffer->data(), save->buffer->size());

		EventSystem::SingleShot([](void* mutex) {
			// mutex gets unlocked back on the main thread via event
			SDL_UnlockMutex((SDL_mutex*)mutex);
		}, save->mut);
		return 0;
	};
	{
		OFS_BENCHMARK("CreateSaveThread");
		ThreadData.buffer = &ProjectBuffer;
		ThreadData.path = &LastPath;
		ThreadData.mut = ProjectMut;
		auto thread = SDL_CreateThread(saveThread, __FUNCTION__, &ThreadData);
		SDL_DetachThread(thread);
	}
}

void OFS_Project::AddFunscript(const std::string& path) noexcept
{
	FUN_ASSERT(false, "not implemented");
}

void OFS_Project::RemoveFunscript(int idx) noexcept
{
	if (idx >= 0 && idx < Funscripts.size()) {
		Funscripts.erase(Funscripts.begin() + idx);
		Save();
	}
}

bool OFS_Project::ImportFunscript(const std::string& path) noexcept
{
	if (MediaPath.empty()) {
		bool foundMedia = FindMedia(path);
		if (!foundMedia) {
			Clear();
			Loaded = false;
			return false;
		}
	}

	// video loaded import script
	if (!Loaded) Funscripts.clear(); // clear placeholder
	auto script = std::make_shared<Funscript>();
	if (script->open(path))	{
		Funscripts.emplace_back(std::move(script));
		Loaded = true;
		FUN_ASSERT(!LastPath.empty(), "path empty");
		Save();
		return true;
	}
	else {
		Loaded = false;
	}
	return false;
}

void OFS_Project::Import(const std::string& path) noexcept
{
	Loaded = false;
	std::filesystem::path basePath = Util::PathFromString(path);
	if (basePath.extension().u8string() == ".funscript") {
		LoadScripts(path);
	}
	else {
		// assume media
		MediaPath = path;
		LastPath = basePath.replace_extension("").u8string() + OFS_Project::Extension;
		basePath.replace_extension(".funscript");
		LoadScripts(basePath.u8string());
	}
	//if (!Util::FileExists(MediaPath)) {
	//	Loaded = false;
	//	Clear();
	//	return;
	//}
	//basePath.replace_extension("");
    //auto funscriptPath = basePath.u8string() + ".funscript";
	//if (ImportFunscript(funscriptPath)) {
	//	LastPath = basePath.u8string() + OFS_Project::Extension;
	//	Loaded = true;
	//	Save();
	//}
	//else {
	//	Clear();
	//	Loaded = false;
	//}
}

void OFS_Project::ExportFunscript(const std::string& outputPath, int idx) noexcept
{
	FUN_ASSERT(false, "not implemented");
}

void OFS_Project::ExportFunscripts(const std::string& outputPath) noexcept
{
	FUN_ASSERT(false, "not implemented");
}
