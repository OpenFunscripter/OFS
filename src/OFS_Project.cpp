#include "OFS_Project.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "OFS_DynamicFontAtlas.h"
#include "SDL_thread.h"

#include "EventSystem.h"
#include "OpenFunscripter.h"

#include "subprocess.h"
#include "stb_sprintf.h"

ScriptSimulator::SimulatorSettings* OFS_Project::ProjSettings::Simulator = nullptr;

bool OFS_Project::FindMedia(const std::string& funscriptPath) noexcept
{
	OFS_PROFILE(__FUNCTION__);
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
	OFS_PROFILE(__FUNCTION__);
	auto loadRelatedScripts = [](OFS_Project* project, const std::string& file) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
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
			auto filePathString = file.u8string();
	        project->ImportFunscript(filePathString);
	    }
	};
	
	if (ImportFunscript(funscriptPath))
	{
		loadRelatedScripts(this, funscriptPath);
	}
	else {
		// insert empty script
		if (!MediaPath.empty())
		{
			LoadedSuccessful();
			AddFunscript(funscriptPath);
		}
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
	OFS_PROFILE(__FUNCTION__);
	Loaded = false;
	LastPath.clear();
	MediaPath.clear();
	Funscripts.clear();
	Funscripts.emplace_back(std::move(std::make_shared<Funscript>()));
	Settings = OFS_ScriptSettings();
	ProjectSettings = OFS_Project::ProjSettings();
	Metadata = OpenFunscripter::ptr->settings->data().defaultMetadata;
	FUN_ASSERT(OFS_ScriptSettings::player != nullptr, "player not set");
	*OFS_ScriptSettings::player = OFS_VideoplayerWindow::OFS_VideoPlayerSettings();
}

void OFS_Project::LoadedSuccessful() noexcept
{
	if(Loaded) return;
	Loaded = true;
	Metadata.title = Util::PathFromString(LastPath)
		.replace_extension("")
		.filename()
		.u8string();
	OFS_DynFontAtlas::AddText(Metadata.title.c_str());
}

bool OFS_Project::Load(const std::string& path) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	FUN_ASSERT(!path.empty(), "path empty");
	Clear();
	auto ProjectPath = Util::PathFromString(path);
	if (ProjectPath.extension().u8string() != OFS_Project::Extension) {
		return false;
	}

	Valid = false;
	LastPath = path;
	ProjectBuffer.clear();
	if (Util::ReadFile(ProjectPath.u8string().c_str(), ProjectBuffer) > 0) {
		OFS_DynFontAtlas::AddText(path);
		Funscripts.clear();
		auto state = OFS_Binary::Deserialize(ProjectBuffer, *this);
		if (state == bitsery::ReaderError::NoError && Valid) {
			OFS_DynFontAtlas::AddText(Metadata.type);
			OFS_DynFontAtlas::AddText(Metadata.title);
			OFS_DynFontAtlas::AddText(Metadata.creator);
			OFS_DynFontAtlas::AddText(Metadata.script_url);
			OFS_DynFontAtlas::AddText(Metadata.video_url);
			for(auto& tag : Metadata.tags) OFS_DynFontAtlas::AddText(tag);
			for(auto& performer : Metadata.performers) OFS_DynFontAtlas::AddText(performer);
			OFS_DynFontAtlas::AddText(Metadata.description);
			OFS_DynFontAtlas::AddText(Metadata.license);
			OFS_DynFontAtlas::AddText(Metadata.notes);
			LoadedSuccessful();
			return true;
		}
		else {
			Clear();
		}
	}
	return false;
}

void OFS_Project::Save(const std::string& path, bool clearUnsavedChanges) noexcept
{
	if (!Loaded) return;
	OFS_PROFILE(__FUNCTION__);
	FUN_ASSERT(!path.empty(), "path empty");
	Valid = true;

	auto app = OpenFunscripter::ptr;
	Metadata.title = Util::PathFromString(LastPath)
		.replace_extension("")
		.filename()
		.u8string();
	OFS_DynFontAtlas::AddText(Metadata.title.c_str());
	Metadata.duration = app->player->Duration();
	Settings.lastPlayerPosition = app->player->CurrentTime();

	size_t writtenSize = 0;
	{
		SDL_LockMutex(ProjectMut);
		ProjectBuffer.clear();
		writtenSize = OFS_Binary::Serialize(ProjectBuffer, *this);
	}
	
	OFS_AsyncIO::Write write;
	write.Path = path;
	write.Buffer = ProjectBuffer.data();
	write.Size = writtenSize;
	write.Userdata = (void*)ProjectMut;
	write.Callback = [](auto& w)
	{
		EventSystem::SingleShot([](void* mutex) {
			 //mutex gets unlocked back on the main thread via event
			SDL_UnlockMutex((SDL_mutex*)mutex);
		}, w.Userdata);
	};
	app->IO->PushWrite(std::move(write));

	// this resets HasUnsavedEdits()
	if (clearUnsavedChanges) {
		for (auto& script : Funscripts) script->SetSavedFromOutside();
	}
}

void OFS_Project::AddFunscript(const std::string& path) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	FUN_ASSERT(Loaded, "Project not loaded");
	// this can either be a new one or an existing one
	auto script = std::make_shared<Funscript>();
	if (script->open(path)) {
		// add existing script to project
		script = Funscripts.emplace_back(std::move(script));
	}
	else {
		// add empty script to project
		script = std::make_shared<Funscript>();
		script->UpdatePath(path);
		script = Funscripts.emplace_back(std::move(script));
	}
	OFS_DynFontAtlas::AddText(script->Title.c_str());
}

void OFS_Project::RemoveFunscript(int idx) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (idx >= 0 && idx < Funscripts.size()) {
		Funscripts.erase(Funscripts.begin() + idx);
	}
}

bool OFS_Project::ImportFunscript(const std::string& path) noexcept
{
	OFS_PROFILE(__FUNCTION__);
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
		OFS_DynFontAtlas::AddText(path);
		Funscripts.emplace_back(std::move(script));
		FUN_ASSERT(!LastPath.empty(), "path empty");
		LoadedSuccessful();
		return true;
	}
	else {
		Loaded = false;
	}
	return false;
}

bool OFS_Project::Import(const std::string& path) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	Loaded = false;
	auto basePath = Util::PathFromString(path);
	LastPath =
		basePath.replace_extension("").u8string() + OFS_Project::Extension;
	if (Util::FileExists(LastPath))
	{
		LOGF_INFO(
			"There's already a project file for \"%s\". opening that "
			"instead...",
			path.c_str());
		auto importPath = LastPath;
		return this->Load(importPath);
	}

	basePath = Util::PathFromString(path);
	if (basePath.extension().u8string() == ".funscript") {
		LoadScripts(path);
	}
	else {
		// assume media
		MediaPath = path;
		basePath.replace_extension(".funscript");
		LoadScripts(basePath.u8string());
	}
	if(Loaded && !Funscripts.empty()) {
		if(!Funscripts.front()->LocalMetadata.title.empty()) {
			Metadata = Funscripts.front()->LocalMetadata;
		}
	}
	return Loaded;
}

void OFS_Project::ExportFunscript(const std::string& outputPath, int idx) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	FUN_ASSERT(idx >= 0 && idx < Funscripts.size(), "out of bounds");
	Funscripts[idx]->LocalMetadata = Metadata; // copy metadata
	Funscripts[idx]->save(outputPath);
}

void OFS_Project::ExportFunscripts(const std::string& outputPath) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto outPath = Util::PathFromString(outputPath);
	for (auto& script : Funscripts) {
		auto savePath =  outPath / (script->Title + ".funscript");
		script->LocalMetadata = Metadata; // copy metadata
		script->save(savePath.u8string());
	}
}

void OFS_Project::ExportFunscripts() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	for (auto& script : Funscripts)
	{
		FUN_ASSERT(!script->Path().empty(), "path is empty");
		if (!script->Path().empty()) {
			script->LocalMetadata = Metadata; // copy metadata
			script->save(script->Path());
		}
	}
}

void OFS_Project::ExportClips(const std::string& outputDirectory) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	FUN_ASSERT(!Settings.Bookmarks.empty(), "No bookmarks created");
	if (Settings.Bookmarks.empty()) {
		return;
	}

	struct ExportTaskData
	{
		std::filesystem::path outputPath;
	};

	auto blockingTask = [](void* data) -> int
	{
		auto bTaskData = (BlockingTaskData*)data;
		auto exportData = (ExportTaskData*)bTaskData->User;

		auto app = OpenFunscripter::ptr;

		bool split;
		auto& bookmarks = app->LoadedProject->Settings.Bookmarks;
		auto& outputPath = exportData->outputPath;
		auto ffmpegPath = Util::FfmpegPath().u8string();

		bTaskData->Progress = 0;
		bTaskData->MaxProgress = bookmarks.size();
		char formatBuffer[1024];

		int i = 0;
		while (i < bookmarks.size())
		{
			split = true;
			auto& bookmarkName = bookmarks[i].name;
			auto startTime = bookmarks[i].atS;
			float endTime;
			

			if (bookmarks[i].type == OFS_ScriptSettings::Bookmark::BookmarkType::END_MARKER)
			{
				split = false;
			}
			else if (bookmarks[i].type == OFS_ScriptSettings::Bookmark::BookmarkType::START_MARKER) {
				endTime = bookmarks[i + 1].atS;
				i++;
			}
			else if (bookmarks[i].type == OFS_ScriptSettings::Bookmark::BookmarkType::REGULAR)
			{
				if (i == bookmarks.size() - 1)
				{
					endTime = app->player->Duration();
				}
				else
				{
					endTime = bookmarks[i + 1].atS - app->player->FrameTime();
				}
			}

			bTaskData->Progress = i;

			if (split)
			{
				char startTimeChar[16];
				char endTimeChar[16];

				stbsp_snprintf(startTimeChar, sizeof(startTimeChar), "%f", startTime);
				stbsp_snprintf(endTimeChar, sizeof(endTimeChar), "%f", endTime);

				stbsp_snprintf(formatBuffer, sizeof(formatBuffer), "%s_%s.mp4", bookmarkName.c_str(), app->LoadedFunscripts()[0]->Title.c_str());
				auto videoOutputPath = outputPath / formatBuffer;
				auto videoOutputString = videoOutputPath.u8string();

				// Slice Funscripts
				auto newScript = Funscript();
				newScript.LocalMetadata = app->LoadedProject->Metadata;
				for (auto& script : app->LoadedFunscripts()) {
					stbsp_snprintf(formatBuffer, sizeof(formatBuffer), "%s_%s.funscript", bookmarkName.c_str(), script->Title.c_str());
					std::filesystem::path scriptOutputPath = outputPath / formatBuffer;
					auto scriptOutputString = scriptOutputPath.u8string();

					auto scriptSlice = script->GetSelection(startTime, endTime);
					newScript.SetActions(FunscriptArray());
					newScript.UpdatePath(scriptOutputString);
					newScript.AddMultipleActions(scriptSlice);
					newScript.AddAction(FunscriptAction(startTime, script->GetPositionAtTime(startTime)));
					newScript.AddAction(FunscriptAction(endTime, script->GetPositionAtTime(endTime)));
					newScript.SelectAll();
					newScript.MoveSelectionTime(-startTime, 0);
					newScript.save(scriptOutputString, false);
				}

				// Slice Video
				std::array<const char*, 14> args =
				{
					ffmpegPath.c_str(),
					"-y",
					"-ss", startTimeChar,
					"-to", endTimeChar,
					"-i",  app->LoadedProject->MediaPath.c_str(),
					"-c:v", "libx264",
					"-c:a", "aac",
					videoOutputString.c_str(),
					nullptr
				};
				struct subprocess_s proc;
				if(subprocess_create(args.data(), subprocess_option_no_window, &proc) != 0) {
					delete exportData;
					return 0;
				}

				if(proc.stdout_file) 
				{
					fclose(proc.stdout_file);
					proc.stdout_file = nullptr;
				}

				if(proc.stderr_file)
				{
					fclose(proc.stderr_file);
					proc.stderr_file = nullptr;
				}

				int return_code;
				subprocess_join(&proc, &return_code);
				subprocess_destroy(&proc);
			}
			i++;
		}
		delete exportData;
		return 0;
	};

	auto taskData = new ExportTaskData;
	taskData->outputPath = Util::PathFromString(outputDirectory);

	auto task = std::make_unique<BlockingTaskData>();
	task->TaskThreadFunc = blockingTask;
	task->TaskDescription = TR(TASK_EXPORTING_CLIPS);
	task->User = taskData;
	OpenFunscripter::ptr->blockingTask.DoTask(std::move(task));
}

bool OFS_Project::HasUnsavedEdits() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	bool unsavedChanges = false;
	for (auto&& script : Funscripts) {
		unsavedChanges = unsavedChanges || script->HasUnsavedEdits();
		if (unsavedChanges) break;
	}
	return unsavedChanges;
}

void OFS_Project::ShowProjectWindow(bool* open) noexcept
{
	if (*open) {
		ImGui::OpenPopup(TR_ID("PROJECT", Tr::PROJECT));
	}

	if (ImGui::BeginPopupModal(TR_ID("PROJECT", Tr::PROJECT), open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
	{
		OFS_PROFILE(__FUNCTION__);
		auto app = OpenFunscripter::ptr;
		ImGui::PushID(Metadata.title.c_str());
		
		ImGui::Text("%s: %s", TR(MEDIA), MediaPath.c_str()); 
		OFS::Tooltip(MediaPath.c_str());
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextDisabled(TR(SCRIPTS));
		for (auto& script : Funscripts) {
			if (ImGui::Button(script->Title.c_str(), ImVec2(-1.f, 0.f))) {
				Util::SaveFileDialog(TR(CHANGE_DEFAULT_LOCATION),
					script->Path(),
					[&](auto result) {
						if (!result.files.empty()) {
							auto newPath = Util::PathFromString(result.files[0]);
							if (newPath.extension().u8string() == ".funscript") {
								script->UpdatePath(newPath.u8string());
							}
						}
					});
			}
			OFS::Tooltip(TR(CHANGE_LOCATION));
		}
		ImGui::PopID();
		ImGui::EndPopup();
	}
}
