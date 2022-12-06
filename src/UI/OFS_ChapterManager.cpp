#include "OFS_ChapterManager.h"
#include "state/states/ChapterState.h"

#include "OFS_EventSystem.h"
#include "OFS_VideoplayerEvents.h"

inline float randf() noexcept
{
    return (float)rand() / (float)RAND_MAX;
}

OFS_ChapterManager::OFS_ChapterManager() noexcept
{
    stateHandle = OFS_ProjectState<ChapterState>::Register(ChapterState::StateName);
    
    EV::Queue().appendListener(DurationChangeEvent::EventType, 
        DurationChangeEvent::HandleEvent(
            [this](const DurationChangeEvent* ev) noexcept
            {
                auto& state = ChapterState::State(stateHandle);
                #if 0

                // Generate random chapters
                float duration = ev->duration;
                state.chapters.clear();
                state.bookmarks.clear();


                srand(SDL_GetTicks());

                float minChapterLen = duration * 0.01f;
                float maxChapterLen = duration * 0.05f;

                float currentTime = 0.f;

                while(currentTime < duration)
                {
                    float newChapterLen = minChapterLen + ((maxChapterLen - minChapterLen) * randf());

                    if(currentTime + newChapterLen > duration)
                    {
                        newChapterLen -=  (currentTime + newChapterLen) - duration;
                    }

                    Chapter newChapter
                    {
                        currentTime,
                        currentTime + newChapterLen,
                        FMT("%.1f", newChapterLen)
                    };
                    state.chapters.emplace_back(std::move(newChapter));
                    currentTime += newChapterLen;
                }
                
                for(int i=0; i < 10; i += 1)
                {
                    float bookmarkTime = duration * randf();
                    Bookmark bookmark
                    {
                        bookmarkTime,
                        FMT("%.1f", bookmarkTime)
                    };
                    state.bookmarks.emplace_back(std::move(bookmark));
                }

                LOGF_DEBUG("Generated %lld random chapters", state.chapters.size());
                #endif
            }
        )
    );
}

OFS_ChapterManager::~OFS_ChapterManager() noexcept
{

}