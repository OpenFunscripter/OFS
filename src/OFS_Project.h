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
    bool valid = false;

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

    bool Import(const std::string& path) noexcept;

    void Update(float delta, bool idleMode) noexcept;
    void ShowProjectWindow(bool* open) noexcept;

    bool HasUnsavedEdits() noexcept;

    inline const std::string& Path() const noexcept { return lastPath; }
    inline bool IsValid() const noexcept { return valid; }
    inline ProjectState& State() noexcept { return ProjectState::State(stateHandle); }
    inline ProjectBookmarkState& Bookmarks() noexcept { return ProjectBookmarkState::State(bookmarkStateHandle); }


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