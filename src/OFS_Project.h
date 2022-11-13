#pragma once

#include "state/ProjectState.h"
#include "Funscript.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#define OFS_PROJECT_EXT ".ofsp"

class OFS_Project {
private:
    uint32_t stateHandle = 0xFFFF'FFFF;
    uint32_t bookmarkStateHandle = 0xFFFF'FFFF;

    std::string lastPath;

    std::string notValidError;
    bool valid = false;

    void addError(const std::string& error) noexcept
    {
        valid = false;
        notValidError += "\n";
        notValidError += error;
    }
    void loadNecessaryGlyphs() noexcept;
    void loadMultiAxis(const std::string& rootScript) noexcept;

public:
    static constexpr auto Extension = OFS_PROJECT_EXT;

    OFS_Project() noexcept;
    OFS_Project(const OFS_Project&) = delete;
    OFS_Project(OFS_Project&&) = delete;
    ~OFS_Project() noexcept;

    std::vector<std::shared_ptr<Funscript>> Funscripts;

    bool Load(const std::string& path) noexcept;
    void Save(bool clearUnsavedChanges) noexcept { Save(lastPath, clearUnsavedChanges); }
    void Save(const std::string& path, bool clearUnsavedChanges) noexcept;

    bool ImportFromFunscript(const std::string& path) noexcept;
    bool ImportFromMedia(const std::string& path) noexcept;

    bool AddFunscript(const std::string& path) noexcept;
    void RemoveFunscript(int32_t idx) noexcept;

    void Update(float delta, bool idleMode) noexcept;
    void ShowProjectWindow(bool* open) noexcept;
    bool HasUnsavedEdits() noexcept;


    inline void SetActiveIdx(uint32_t activeIdx) noexcept { State().activeScriptIdx = activeIdx; }
    inline uint32_t ActiveIdx() const noexcept { return State().activeScriptIdx; }
    inline std::shared_ptr<Funscript>& ActiveScript() noexcept { return Funscripts[ActiveIdx()]; }

    inline const std::string& Path() const noexcept { return lastPath; }
    inline bool IsValid() const noexcept { return valid; }
    inline const std::string& NotValidError() const noexcept { return notValidError; }
    inline ProjectState& State() const noexcept { return ProjectState::State(stateHandle); }
    inline ProjectBookmarkState& Bookmarks() noexcept { return ProjectBookmarkState::State(bookmarkStateHandle); }

    void ExportFunscripts() noexcept;
    void ExportFunscripts(const std::string& outputDir) noexcept;
    void ExportFunscript(const std::string& outputPath, int32_t idx) noexcept;
    std::unique_ptr<struct BlockingTaskData> ExportClips(const std::string& outputDirectory, float totalDuration, float frameTime) noexcept;

    std::string MakePathAbsolute(const std::string& relPath) const noexcept;
    std::string MakePathRelative(const std::string& absPath) const noexcept;
    std::string MediaPath() const noexcept;

    template<typename S>
    void serialize(S& s)
    {
        s.ext(*this, bitsery::ext::Growable{},
            [](S& s, OFS_Project& o) {
                s.container(o.Funscripts, 100,
                    [](S& s, std::shared_ptr<Funscript>& script) {
                        s.ext(script, bitsery::ext::StdSmartPtr{});
                    });
            });
    }
};