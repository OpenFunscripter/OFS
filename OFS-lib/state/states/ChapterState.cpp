#include "ChapterState.h"

inline static bool checkForOverlapChapters(const std::vector<Chapter>& chapters, float startTime, float endTime, const Chapter* exclude = nullptr) noexcept
{
    for(int i=0, size=chapters.size(); i < size; i += 1)
    {
        auto& chapter = chapters[i];
        if(exclude == &chapter) continue;
        if(startTime <= chapter.endTime && chapter.startTime <= endTime)
        {
            return true;
        }
    }
    return false;
}

bool ChapterState::SetChapterSize(Chapter& chapter, float toTime) noexcept
{
    const float testStartTime = toTime < chapter.startTime ? toTime : chapter.startTime;
    const float testEndTime = toTime > chapter.endTime ? toTime : chapter.endTime;

    if(toTime >= chapter.startTime && toTime <= chapter.endTime)
    {
        // intersects with itself means it should shrink
        if(std::abs(toTime - chapter.startTime) < std::abs(toTime - chapter.endTime))
        {
            chapter.startTime = toTime;
            return true;
        }
        else
        {
            chapter.endTime = toTime;
            return true;
        }
    }
    else if(checkForOverlapChapters(chapters, testStartTime, testEndTime, &chapter))
    {
        // check if truncating toTime is possible
        auto it = std::find_if(chapters.begin(), chapters.end(),
            [&](auto& c) noexcept { return &c == &chapter; }); 
        if(it != chapters.end())
        {
            if(toTime < chapter.startTime)
            {
                // check left neighbour
                if(it == chapters.begin())
                    return false;
                auto n = it - 1;
                toTime = std::nextafter(n->endTime, chapter.startTime);
            }
            else if(toTime > chapter.endTime)
            {
                // check right neighbour
                auto n = it + 1;
                if(n == chapters.end()) 
                    return false;
                toTime = std::nextafter(n->startTime, chapter.startTime);
            }
        }
    }

    if(toTime < chapter.startTime)
    {
        // grow left
        chapter.startTime = toTime;
        return true;
    }
    else if(toTime > chapter.endTime)
    {
        // grow right
        chapter.endTime = toTime;
        return true;
    }


    return false;
}

Chapter* ChapterState::AddChapter(float time, float duration) noexcept
{
    float startTime = time;
    float endTime = startTime + (0.01f * duration);

    if(checkForOverlapChapters(chapters, startTime, endTime))
    {
        return nullptr;
    }

    Chapter newChapter = {0};
    newChapter.startTime = startTime;
    newChapter.endTime = endTime;

    if(chapters.empty())
    {
        auto& c = chapters.emplace_back(std::move(newChapter));
        return &c;
    }
    else 
    {
        for(int i=0, size=chapters.size(); i < size; i += 1)
        {
            auto& chapter = chapters[i];
            if(chapter.startTime >= newChapter.endTime)
            {
                // insert before this chapter
                auto it = chapters.insert(chapters.begin() + i, std::move(newChapter));
                return &(*it);
            }
        }
        auto& c = chapters.emplace_back(std::move(newChapter));
        return &c;
    }

    return nullptr;
}

Bookmark* ChapterState::AddBookmark(float time) noexcept
{
    for(auto& bookmark : bookmarks)
    {
        // Minimum time between bookmarks is 1 second
        if(std::abs(bookmark.time - time) <= 1.f)
        {
            return nullptr;
        }
    }
    auto& newBookmark = bookmarks.emplace_back();
    newBookmark.time = time;
    return &newBookmark;
}

std::string Chapter::StartTimeToString() const noexcept
{
    char tmpBuf[16];
    int size = Util::FormatTime(tmpBuf, sizeof(tmpBuf), startTime, true);
    return std::string(tmpBuf, size);
}

std::string Chapter::EndTimeToString() const noexcept
{
    char tmpBuf[16];
    int size = Util::FormatTime(tmpBuf, sizeof(tmpBuf), endTime, true);
    return std::string(tmpBuf, size);    
}

std::string Bookmark::TimeToString() const noexcept
{
    char tmpBuf[16];
    int size = Util::FormatTime(tmpBuf, sizeof(tmpBuf), time, true);
    return std::string(tmpBuf, size);
}