#include "OFS_ChapterManager.h"
#include "state/states/ChapterState.h"

#include "OpenFunscripter.h"
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

bool OFS_ChapterManager::ExportClip(const Chapter& chapter, const std::string& outputDirStr) noexcept
{
    auto app = OpenFunscripter::ptr;
    char startTimeChar[16];
    char endTimeChar[16];
    stbsp_snprintf(startTimeChar, sizeof(startTimeChar), "%f", chapter.startTime);
    stbsp_snprintf(endTimeChar, sizeof(endTimeChar), "%f", chapter.endTime);

    
    auto outputDir = Util::PathFromString(outputDirStr);
    auto mediaPath = Util::PathFromString(app->player->VideoPath());

    auto& projectState = app->LoadedProject->State();

    for(auto& script : app->LoadedFunscripts())
    {
        auto scriptOutputPath = (outputDir / (chapter.name + "_" + script->Title()));
        scriptOutputPath.replace_extension(".funscript");
        auto scriptOutputPathStr = scriptOutputPath.u8string();

        auto clippedScript = Funscript();
        auto slice = script->GetSelection(chapter.startTime, chapter.endTime);
        clippedScript.SetActions(slice);
        clippedScript.AddEditAction(FunscriptAction(chapter.startTime, script->GetPositionAtTime(chapter.startTime)), 0.001f);
        clippedScript.AddEditAction(FunscriptAction(chapter.endTime, script->GetPositionAtTime(chapter.endTime)), 0.001f);
        clippedScript.SelectAll();
        clippedScript.MoveSelectionTime(-chapter.startTime, 0.f);

        // FIXME: chapters and bookmarks are not included
        auto funscriptJson = clippedScript.Serialize(projectState.metadata, false);
        auto funscriptText = Util::SerializeJson(funscriptJson);
        Util::WriteFile(scriptOutputPathStr.c_str(), funscriptText.data(), funscriptText.size());
    }

    auto clippedMedia = Util::PathFromString("");
    clippedMedia.replace_filename(chapter.name + "_" + mediaPath.filename().u8string());
    clippedMedia.replace_extension(mediaPath.extension());
    
    auto videoOutputPath = outputDir / clippedMedia;
    auto videoOutputString = videoOutputPath.u8string();
    
    auto ffmpegPath = Util::FfmpegPath().u8string();
    auto mediaPathStr = mediaPath.u8string();

    std::array<const char*, 17> args = {
        ffmpegPath.c_str(),
        "-y",
        "-ss", startTimeChar,
        "-to", endTimeChar,
        "-i", mediaPathStr.c_str(),
        "-vcodec", "copy",
        "-acodec", "copy",
        videoOutputString.c_str(),
        nullptr
    };

    struct subprocess_s proc;
    if (subprocess_create(args.data(), subprocess_option_no_window, &proc) != 0) {
        return false;
    }

    if (proc.stdout_file) {
        fclose(proc.stdout_file);
        proc.stdout_file = nullptr;
    }

    if (proc.stderr_file) {
        fclose(proc.stderr_file);
        proc.stderr_file = nullptr;
    }

    int returnCode;
    subprocess_join(&proc, &returnCode);
    subprocess_destroy(&proc);

    return returnCode == 0;
}