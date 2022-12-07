#pragma once
#include "OFS_StateHandle.h"

#include <vector>
#include <string>

#include "OFS_Event.h"

struct Chapter
{
    float startTime = 0.f;
    float endTime = 0.f;
    std::string name;
    ImColor color = IM_COL32(123, 56, 87, 255);

    std::string StartTimeToString() const noexcept;
    std::string EndTimeToString() const noexcept;
};

struct Bookmark
{
    float time = 0.f;
    std::string name;

    std::string TimeToString() const noexcept;
};

struct ChapterState
{
    static constexpr auto StateName = "ChapterState";
    std::vector<Chapter> chapters;
    std::vector<Bookmark> bookmarks;

    inline static ChapterState& State(uint32_t stateHandle) noexcept
    {
        return OFS_ProjectState<ChapterState>(stateHandle).Get();
    }

    inline static ChapterState& StaticStateSlow() noexcept
    {
        auto handle = OFS_ProjectState<ChapterState>::Register(StateName);
        return State(handle);
    }

    bool SetChapterSize(Chapter& chapter, float toTime) noexcept;
    Chapter* AddChapter(float time, float duration) noexcept;
    Bookmark* AddBookmark(float time) noexcept;
};

REFL_TYPE(Chapter)
    REFL_FIELD(startTime)
    REFL_FIELD(endTime)
    REFL_FIELD(name)
    REFL_FIELD(color)
REFL_END

REFL_TYPE(Bookmark)
    REFL_FIELD(time)
    REFL_FIELD(name)
REFL_END

REFL_TYPE(ChapterState)
    REFL_FIELD(chapters)
    REFL_FIELD(bookmarks)
REFL_END


class ExportClipForChapter : public OFS_Event<ExportClipForChapter>
{
    public:
    Chapter chapter;
    ExportClipForChapter(const Chapter& chapter) noexcept
        : chapter(chapter) {}
};