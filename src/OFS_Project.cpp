#include "OFS_Project.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "SDL_thread.h"

#include "EventSystem.h"
#include "OpenFunscripter.h"

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
	        project->ImportFunscript(file.u8string());
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
			Loaded = true;
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
	*OFS_ScriptSettings::player = VideoplayerWindow::OFS_VideoPlayerSettings();
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
		Funscripts.clear();
		auto state = OFS_Binary::Deserialize(ProjectBuffer, *this);
		if (state == bitsery::ReaderError::NoError && Valid) {
			Loaded = true;
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
	Metadata.duration = app->player->getDuration();
	Settings.lastPlayerPosition = app->player->getCurrentPositionSeconds();

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
		Funscripts.emplace_back(std::move(script));
		Save(true);
	}
	else {
		// add empty script to project
		script = std::make_shared<Funscript>();
		script->UpdatePath(path);
		Funscripts.emplace_back(std::move(script));
		Save(true);
	}
}

void OFS_Project::RemoveFunscript(int idx) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (idx >= 0 && idx < Funscripts.size()) {
		Funscripts.erase(Funscripts.begin() + idx);
		Save(true);
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
		Funscripts.emplace_back(std::move(script));
		Loaded = true;
		FUN_ASSERT(!LastPath.empty(), "path empty");
		Save(true);
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
	LastPath = basePath.replace_extension("").u8string() + OFS_Project::Extension;
	if (Util::FileExists(LastPath)) {
		// there already exists a project file 
		// and we don't want to overwrite it
		return false;
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

		std::string statusText;

		int i = 0;
		while (i < bookmarks.size())
		{
			split = true;
			auto& bookmarkName = bookmarks[i].name;
			auto startTime = bookmarks[i].atS;
			float endTime;
			
			statusText = "Exporting: " + bookmarkName;
			bTaskData->TaskDescription = statusText.c_str();

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
					endTime = app->player->getDuration() * 1000;
				}
				else
				{
					endTime = bookmarks[i + 1].atS - app->player->getFrameTime();
				}
			}

			if (split)
			{
				char startTimeChar[16];
				char endTimeChar[16];

				stbsp_snprintf(startTimeChar, 10, "%f", startTime);
				stbsp_snprintf(endTimeChar, 10, "%f", endTime);

				auto videoOutputPath = outputPath / (app->LoadedFunscripts()[0]->Title + "_" + bookmarkName + ".mp4");
				auto videoOutputString = videoOutputPath.u8string();

				// Slice Funscripts
				auto newScript = Funscript();
				for (auto& script : app->LoadedFunscripts()) {
					std::filesystem::path scriptOutputPath = outputPath / (script->Title + "_" + bookmarkName + ".funscript");
					auto scriptOutputString = scriptOutputPath.u8string();

					auto scriptSlice = script->GetSelection(startTime, endTime);
					newScript.SetActions(FunscriptArray());
					newScript.UpdatePath(scriptOutputString);
					newScript.AddActionRange(scriptSlice, false);
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
				auto [status, ec] = reproc::run(args.data());

				LOGF_DEBUG("OFS_Project::ExportClips: %s", ec.message().c_str());
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
	task->TaskDescription = "Exporting clips";
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
		ImGui::OpenPopup("Project");
	}

	if (ImGui::BeginPopupModal("Project", open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
	{
		OFS_PROFILE(__FUNCTION__);
		auto app = OpenFunscripter::ptr;
		ImGui::PushID(Metadata.title.c_str());
		
		ImGui::Text("Media: %s", MediaPath.c_str()); 
		OFS::Tooltip(MediaPath.c_str());
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextDisabled("Scripts");
		for (auto& script : Funscripts) {
			if (ImGui::Button(script->Title.c_str(), ImVec2(-1.f, 0.f))) {
				Util::SaveFileDialog("Change default location",
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
			OFS::Tooltip("Change location");
		}
		ImGui::PopID();
		ImGui::EndPopup();
	}
}
