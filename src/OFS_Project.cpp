#include "OFS_Project.h"
#include "OFS_Localization.h"
#include "OFS_ImGui.h"
#include "OFS_DynamicFontAtlas.h"
#include "OFS_BlockingTask.h"
#include "OFS_EventSystem.h"

#include "subprocess.h"

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
    Funscripts.emplace_back(std::move(std::make_shared<Funscript>()));
}

OFS_Project::~OFS_Project() noexcept
{
}

void OFS_Project::loadNecessaryGlyphs() noexcept
{
    // This should be called after loading or importing.
    auto& projectState = State();
    auto& metadata = projectState.metadata;
    OFS_DynFontAtlas::AddText(metadata.type);
    OFS_DynFontAtlas::AddText(metadata.title);
    OFS_DynFontAtlas::AddText(metadata.creator);
    OFS_DynFontAtlas::AddText(metadata.script_url);
    OFS_DynFontAtlas::AddText(metadata.video_url);
    for (auto& tag : metadata.tags) OFS_DynFontAtlas::AddText(tag);
    for (auto& performer : metadata.performers) OFS_DynFontAtlas::AddText(performer);
    OFS_DynFontAtlas::AddText(metadata.description);
    OFS_DynFontAtlas::AddText(metadata.license);
    OFS_DynFontAtlas::AddText(metadata.notes);
    for (auto& script : Funscripts) OFS_DynFontAtlas::AddText(script->Title().c_str());
    OFS_DynFontAtlas::AddText(lastPath);
}

bool OFS_Project::Load(const std::string& path) noexcept
{
    FUN_ASSERT(!valid, "Can't import if project is already loaded.");
#if 1
    std::vector<uint8_t> projectBin;
    if (Util::ReadFile(path.c_str(), projectBin) > 0) {
        bool succ;
        auto projectState = Util::ParseCBOR(projectBin, &succ);
        if (succ) {
            valid = OFS_StateManager::Get()->DeserializeProjectAll(projectState, true);
        }
    }
#else
    std::string projectJson = Util::ReadFileString(path.c_str());
    if (!projectJson.empty()) {
        bool succ;
        auto json = Util::ParseJson(projectJson, &succ);
        if (succ) {
            valid = OFS_StateManager::Get()->DeserializeProjectAll(json, false);
        }
        else {
            valid = false;
            addError("Failed to parse project.\nIt likely is an old project file not supported in " OFS_LATEST_GIT_TAG);
        }
    }
#endif

    if (valid) {
        auto& projectState = State();
        OFS_Binary::Deserialize(projectState.binaryFunscriptData, *this);
        lastPath = path;
        loadNecessaryGlyphs();
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
        loadMultiAxis(file);

        std::string absMediaPath;
        if (FindMedia(file, &absMediaPath)) {
            projectState.relativeMediaPath = MakePathRelative(absMediaPath);
            valid = true;
            loadNecessaryGlyphs();
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
        loadMultiAxis(funscriptPathStr);
        valid = true;
        loadNecessaryGlyphs();
    }

    return valid;
}

bool OFS_Project::AddFunscript(const std::string& path) noexcept
{
    bool loadedScript = false;

    bool succ = false;
    auto jsonText = Util::ReadFileString(path.c_str());
    auto json = Util::ParseJson(jsonText, &succ);

    auto script = std::make_shared<Funscript>();
    auto metadata = Funscript::Metadata();

    bool isFirstFunscript = Funscripts.size() == 0;
    if (succ && script->Deserialize(json, &metadata, isFirstFunscript)) {
        // Add existing script to project
        script = Funscripts.emplace_back(std::move(script));
        script->UpdateRelativePath(MakePathRelative(path));
        if (isFirstFunscript) {
            // Initialize project metadata using the first funscript
            auto& projectState = State();
            projectState.metadata = metadata;
        }
        loadedScript = true;
    }
    else {
        // Add empty script to project
        script = std::make_shared<Funscript>();
        script->UpdateRelativePath(MakePathRelative(path));
        script = Funscripts.emplace_back(std::move(script));
    }
    return loadedScript;
}

void OFS_Project::RemoveFunscript(int32_t idx) noexcept
{
    if (idx >= 0 && idx < Funscripts.size()) {
        EV::Enqueue<FunscriptRemovedEvent>(Funscripts[idx]->Title());
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

#if 1
    auto projectState = OFS_StateManager::Get()->SerializeProjectAll(true);
    auto projectBin = Util::SerializeCBOR(projectState);
    Util::WriteFile(path.c_str(), projectBin.data(), projectBin.size());
#else
    auto projectState = OFS_StateManager::Get()->SerializeProjectAll(false);
    auto projectJson = Util::SerializeJson(projectState, false);
    Util::WriteFile(path.c_str(), projectJson.data(), projectJson.size());
#endif
    if (clearUnsavedChanges) {
        for (auto& script : Funscripts) {
            script->ClearUnsavedEdits();
        }
    }
}

void OFS_Project::Update(float delta, bool idleMode) noexcept
{
    if (!idleMode) {
        auto& projectState = State();
        projectState.activeTimer += delta;
    }
    for (auto& script : Funscripts) script->Update();
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
            if (ImGui::Button(script->Title().c_str(), ImVec2(-1.f, 0.f))) {
                Util::SaveFileDialog(TR(CHANGE_DEFAULT_LOCATION),
                    MakePathAbsolute(script->RelativePath()),
                    [&](auto result) {
                        if (!result.files.empty()) {
                            auto newPath = Util::PathFromString(result.files[0]);
                            if (newPath.extension().u8string() == ".funscript") {
                                script->UpdateRelativePath(MakePathRelative(newPath.u8string()));
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
        FUN_ASSERT(!script->RelativePath().empty(), "path is empty");
        if (!script->RelativePath().empty()) {
            auto json = script->Serialize(state.metadata, true);
            script->ClearUnsavedEdits();
            auto jsonText = Util::SerializeJson(json, false);
            Util::WriteFile(MakePathAbsolute(script->RelativePath()).c_str(), jsonText.data(), jsonText.size());
        }
    }
}

void OFS_Project::ExportFunscripts(const std::string& outputDir) noexcept
{
    auto& state = State();
    for (auto& script : Funscripts) {
        FUN_ASSERT(!script->RelativePath().empty(), "path is empty");
        if (!script->RelativePath().empty()) {
            auto filename = Util::PathFromString(script->RelativePath()).filename();
            auto outputPath = (Util::PathFromString(outputDir) / filename).u8string();
            auto json = script->Serialize(state.metadata, true);
            script->ClearUnsavedEdits();
            auto jsonText = Util::SerializeJson(json, false);
            Util::WriteFile(outputPath.c_str(), jsonText.data(), jsonText.size());
        }
    }
}

void OFS_Project::ExportFunscript(const std::string& outputPath, int32_t idx) noexcept
{
    FUN_ASSERT(idx >= 0 && idx < Funscripts.size(), "out of bounds");
    auto& state = State();
    auto json = Funscripts[idx]->Serialize(state.metadata, true);
    Funscripts[idx]->ClearUnsavedEdits();
    // Using this function changes the default path
    Funscripts[idx]->UpdateRelativePath(MakePathRelative(outputPath));
    auto jsonText = Util::SerializeJson(json, false);
    Util::WriteFile(outputPath.c_str(), jsonText.data(), jsonText.size());
}

void OFS_Project::loadMultiAxis(const std::string& rootScript) noexcept
{
    std::vector<std::filesystem::path> relatedFiles;
    {
        auto filename = Util::Filename(rootScript) + '.';
        auto searchDirectory = Util::PathFromString(rootScript);
        searchDirectory.remove_filename();

        std::error_code ec;
        std::filesystem::directory_iterator dirIt(searchDirectory, ec);
        for (auto&& entry : dirIt) {
            auto extension = entry.path()
                                 .extension()
                                 .u8string();
            auto currentFilename = entry.path()
                                       .filename()
                                       .replace_extension("")
                                       .u8string();

            if (extension == Funscript::Extension
                && Util::StringStartsWith(currentFilename, filename)
                && currentFilename != filename) {
                relatedFiles.emplace_back(entry.path());
            }
        }
    }
    // reorder for 3d simulator
    std::array<std::string, 3> desiredOrder{
        // it's in reverse order
        ".twist.funscript",
        ".pitch.funscript",
        ".roll.funscript"
    };
    if (relatedFiles.size() > 1) {
        for (auto& ending : desiredOrder) {
            for (int i = 0; i < relatedFiles.size(); i++) {
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
    for (int i = relatedFiles.size() - 1; i >= 0; i -= 1) {
        auto& file = relatedFiles[i];
        auto filePathString = file.u8string();
        AddFunscript(filePathString);
    }
}

std::string OFS_Project::MakePathAbsolute(const std::string& relPathStr) const noexcept
{
    auto relPath = Util::PathFromString(relPathStr);
    FUN_ASSERT(relPath.is_relative(), "Path isn't relative");
    if (relPath.is_absolute()) {
        LOGF_ERROR("Path was already absolute. \"%s\"", relPathStr.c_str());
        return relPathStr;
    }
    else {
        auto projectDir = Util::PathFromString(lastPath);
        projectDir.remove_filename();
        std::error_code ec;
        auto absPath = std::filesystem::absolute(projectDir / relPath, ec);
        if (!ec) {
            auto absPathStr = absPath.u8string();
            LOGF_INFO("Convert relative path \"%s\" to absolute \"%s\"", relPath.u8string().c_str(), absPathStr.c_str());
            return absPathStr;
        }
        FUN_ASSERT(false, "This must not happen.");
        LOG_ERROR("Failed to convert path to absolute path.");
        return "";
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
