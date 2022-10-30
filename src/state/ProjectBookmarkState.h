#pragma once

#include "OFS_StateHandle.h"
#include <string>

enum class BookmarkType : uint8_t {
    Regular,
    StartMarker,
    EndMarker
};

struct Bookmark {
    float atS; // floating point seconds
    std::string name;
    BookmarkType type = BookmarkType::Regular;

    static constexpr char startMarker[] = "_start";
    static constexpr char endMarker[] = "_end";
    
    Bookmark() noexcept {}
    Bookmark(std::string&& name, float atSeconds) noexcept
        : name(std::move(name)), atS(atSeconds) { UpdateType(); }
    Bookmark(const std::string& name, float atSeconds) noexcept
        : name(name), atS(atSeconds) { UpdateType(); }
    
    void UpdateType() noexcept;
};

REFL_TYPE(Bookmark)
    REFL_FIELD(atS)
    REFL_FIELD(name)
    REFL_FIELD(type, serializeEnum())
REFL_END

struct ProjectBookmarkState
{
    static constexpr auto StateName = "ProjectBookmarkState";   
    std::vector<Bookmark> Bookmarks;

    void AddBookmark(Bookmark&& bookmark) noexcept;
    inline static ProjectBookmarkState& State(uint32_t stateHandle) noexcept
    {
        return OFS_ProjectState<ProjectBookmarkState>(stateHandle).Get();
    }
};

REFL_TYPE(ProjectBookmarkState)
    REFL_FIELD(Bookmarks)
REFL_END