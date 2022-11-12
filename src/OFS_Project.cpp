#include "OFS_Project.h"
#include "OFS_Localization.h"
#include "OFS_ImGui.h"
#include "OFS_DynamicFontAtlas.h"

#include <algorithm>

static std::array<const char*, 6> VideoExtensions{
    ".mp4",
    ".mkv",
    ".webm",
    ".wmv",
    ".avi",
    ".m4v",
};

static std::array<const char*, 4> AudioExtensions{
    ".mp3",
    ".ogg",
    ".flac",
    ".wav",
};

inline bool static HasMediaExtension(const std::string& pathStr) noexcept
{
    auto path = Util::PathFromString(pathStr);
    auto ext = path.extension().u8string();

    bool hasMediaExt = std::any_of(VideoExtensions.begin(), VideoExtensions.end(),
        [&ext](auto validExt) noexcept {
            return strcmp(ext.c_str(), validExt) == 0;
        });

    if (!hasMediaExt) {
        hasMediaExt = std::any_of(AudioExtensions.begin(), AudioExtensions.end(),
            [&ext](auto validExt) noexcept {
                return strcmp(ext.c_str(), validExt) == 0;
            });
    }

    return hasMediaExt;
}

inline bool FindMedia(const std::string& pathStr, std::string* outMedia) noexcept
{
    auto path = Util::PathFromString(pathStr);
    auto pathDir = path.parent_path();

    auto filename = path.stem().u8string();

    std::error_code ec;
    std::filesystem::directory_iterator dirIt(pathDir, ec);
    for (auto& entry : dirIt) {
        auto entryName = entry.path().stem().u8string();
        if (entryName == filename) {
            auto entryPathStr = entry.path().u8string();

            if (HasMediaExtension(entryPathStr)) {
                *outMedia = entryPathStr;
                return true;
            }
        }
    }

    return false;
}

OFS_Project::OFS_Project() noexcept
{
    stateHandle = OFS_ProjectState<ProjectState>::Register(ProjectState::StateName);
    bookmarkStateHandle = OFS_ProjectState<ProjectBookmarkState>::Register(ProjectBookmarkState::StateName);
    Funscripts.emplace_back(std::move(std::make_shared<Funscript>()));
}

OFS_Project::~OFS_Project() noexcept
{
}

bool OFS_Project::Load(const std::string& path) noexcept
{
    FUN_ASSERT(!valid, "Can't import if project is already loaded.");
#if 0
	std::vector<uint8_t> projectBin;
	if(Util::ReadFile(path.c_str(), projectBin) > 0) {
		bool succ;
		auto projectState = Util::ParseCBOR(projectBin, &succ);
		if(succ) {
			valid = OFS_StateManager::Get()->DeserializeProjectAll(projectState);
		}
	}
#else
    std::string projectJson = Util::ReadFileString(path.c_str());
    if (!projectJson.empty()) {
        bool succ;
        auto json = Util::ParseJson(projectJson, &succ);
        if (succ) {
            valid = OFS_StateManager::Get()->DeserializeProjectAll(json);
        }
    }
#endif

    if (valid) {
        auto& projectState = State();
        OFS_Binary::Deserialize(projectState.binaryFunscriptData, *this);

        lastPath = path;
    }

    return valid;
}

bool OFS_Project::ImportFromFunscript(const std::string& file) noexcept
{
    FUN_ASSERT(!valid, "Can't import if project is already loaded.");

    auto& projectState = State();
    auto basePath = Util::PathFromString(file);
    lastPath = basePath.replace_extension(OFS_Project::Extension).u8string();

    if (Util::FileExists(file)) {
        Funscripts.clear();
        if (!AddFunscript(file)) {
            addError("Failed to load funscript.");
            return valid;
        }

        std::string absMediaPath;
        if (FindMedia(file, &absMediaPath)) {
            projectState.relativeMediaPath = MakePathRelative(absMediaPath);
            valid = true;
        }
        else {
            addError("Failed to find media for funscript.");
            return valid;
        }
    }

    return valid;
}

bool OFS_Project::ImportFromMedia(const std::string& file) noexcept
{
    FUN_ASSERT(!valid, "Can't import if project is already loaded.");

    if (!HasMediaExtension(file)) {
        // Unsupported media.
        addError("Unsupported media file extension.");
        return false;
    }

    auto& projectState = State();
    auto basePath = Util::PathFromString(file);
    lastPath = basePath.replace_extension(OFS_Project::Extension).u8string();

    basePath = Util::PathFromString(file);
    if (Util::FileExists(file)) {
        projectState.relativeMediaPath = MakePathRelative(file);
        auto funscriptPath = basePath;
        auto funscriptPathStr = funscriptPath.replace_extension(".funscript").u8string();

        Funscripts.clear();
        AddFunscript(funscriptPathStr);
        valid = true;
    }

    return valid;
}

bool OFS_Project::AddFunscript(const std::string& path) noexcept
{
    bool loadedScript = false;
    auto script = std::make_shared<Funscript>();
    if (script->open(path)) {
        // Add existing script to project
        script = Funscripts.emplace_back(std::move(script));
        loadedScript = true;
    }
    else {
        // Add empty script to project
        script = std::make_shared<Funscript>();
        script->UpdatePath(path);
        script = Funscripts.emplace_back(std::move(script));
    }
    OFS_DynFontAtlas::AddText(script->Title.c_str());
    return loadedScript;
}

void OFS_Project::RemoveFunscript(int32_t idx) noexcept
{
    if (idx >= 0 && idx < Funscripts.size()) {
        Funscripts.erase(Funscripts.begin() + idx);
    }
}

void OFS_Project::Save(const std::string& path, bool clearUnsavedChanges) noexcept
{
    {
        auto& projectState = State();
        projectState.binaryFunscriptData.clear();
        auto size = OFS_Binary::Serialize(projectState.binaryFunscriptData, *this);
        projectState.binaryFunscriptData.resize(size);
    }
    auto projectState = OFS_StateManager::Get()->SerializeProjectAll();

#if 0
	auto projectBin = Util::SerializeCBOR(projectState);
	Util::WriteFile(path.c_str(), projectBin.data(), projectBin.size());
#else
    auto projectJson = Util::SerializeJson(projectState, false);
    Util::WriteFile(path.c_str(), projectJson.data(), projectJson.size());
#endif
    if (clearUnsavedChanges) {
        for (auto& script : Funscripts) {
            script->SetSavedFromOutside();
        }
    }
}

void OFS_Project::Update(float delta, bool idleMode) noexcept
{
    if (!idleMode) {
        auto& projectState = State();
        projectState.activeTimer += delta;
    }
}

bool OFS_Project::HasUnsavedEdits() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    for (auto& script : Funscripts) {
        if (script->HasUnsavedEdits()) {
            return true;
        }
    }
    return false;
}

void OFS_Project::ShowProjectWindow(bool* open) noexcept
{
    if (*open) {
        ImGui::OpenPopup(TR_ID("PROJECT", Tr::PROJECT));
    }

    if (ImGui::BeginPopupModal(TR_ID("PROJECT", Tr::PROJECT), open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
        OFS_PROFILE(__FUNCTION__);
        auto& projectState = State();
        auto& Metadata = projectState.metadata;
        ImGui::PushID(Metadata.title.c_str());

        ImGui::Text("%s: %s", TR(MEDIA), projectState.relativeMediaPath.c_str());

        Util::FormatTime(Util::FormatBuffer, sizeof(Util::FormatBuffer), projectState.activeTimer, true);
        ImGui::Text("%s: %s", TR(TIME_SPENT), Util::FormatBuffer);
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

void OFS_Project::ExportFunscripts() noexcept
{
    auto& state = State();
    for (auto& script : Funscripts) {
        FUN_ASSERT(!script->Path().empty(), "path is empty");
        if (!script->Path().empty()) {
            script->LocalMetadata = state.metadata;
            script->save(script->Path());
        }
    }
}

void OFS_Project::ExportFunscript(const std::string& outputPath, int32_t idx) noexcept
{
    FUN_ASSERT(idx >= 0 && idx < Funscripts.size(), "out of bounds");
    auto& state = State();
    Funscripts[idx]->LocalMetadata = state.metadata;
    Funscripts[idx]->save(outputPath);
}

std::string OFS_Project::MakePathAbsolute(const std::string& relPathStr) const noexcept
{
    auto relPath = Util::PathFromString(relPathStr);
    FUN_ASSERT(relPath.is_relative(), "Path isn't relative");
    if(relPath.is_absolute())
    {
        LOGF_ERROR("Path was already absolute. \"%s\"", relPathStr.c_str());
        return relPathStr;
    }
    else 
    {
        auto projectDir = Util::PathFromString(lastPath);
        projectDir.replace_filename("");
        auto absPath = (projectDir / relPath).u8string();
        LOGF_INFO("Convert relative path \"%s\" to absolute \"%s\"", relPath.u8string().c_str(), absPath.c_str());
        return absPath;
    }
}

std::string OFS_Project::MakePathRelative(const std::string& absPathStr) const noexcept
{
    auto absPath = Util::PathFromString(absPathStr);
    auto projectDir = Util::PathFromString(lastPath).parent_path();
    auto relPath = absPath.lexically_relative(projectDir);
    auto relPathStr = relPath.u8string();
    LOGF_INFO("Convert absolute path \"%s\" to relative \"%s\"", absPathStr.c_str(), relPathStr.c_str());
    return relPathStr;
}

std::string OFS_Project::MediaPath() const noexcept
{
    auto& projectState = State();
    return MakePathAbsolute(projectState.relativeMediaPath);
}