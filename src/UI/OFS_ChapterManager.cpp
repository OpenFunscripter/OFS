#include "OFS_ChapterManager.h"
#include "state/states/ChapterState.h"

#include "OpenFunscripter.h"
#include "OFS_EventSystem.h"
#include "OFS_VideoplayerEvents.h"
#include "OFS_Localization.h"

#include "imgui.h"
#include "imgui_stdlib.h"

OFS_ChapterManager::OFS_ChapterManager() noexcept
{
    stateHandle = OFS_ProjectState<ChapterState>::Register(ChapterState::StateName);
}

OFS_ChapterManager::~OFS_ChapterManager() noexcept
{

}

ChapterState& OFS_ChapterManager::State() noexcept
{
    return ChapterState::State(stateHandle);
}

void OFS_ChapterManager::ShowWindow(bool* open) noexcept
{
    if(!*open) return;
    auto& chapterState = ChapterState::State(stateHandle);
    ImGui::Begin(TR_ID("ChapterManager", Tr::CHAPTERS), open);

    if(ImGui::BeginTable("##chapterTable", 4, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(TR(CHAPTER), ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn(TR(BEGIN), ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn(TR(END), ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("##controls", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        bool chapterStateChange = false;
        int deleteIdx = -1;
        char timeBuf[16];
        for(int i=0, size=chapterState.chapters.size(); i < size; i += 1)
        {
            auto& chapter = chapterState.chapters[i];
            ImGui::PushID(i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::ColorEdit3("##chapterColorPicker", &chapter.color.Value.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            chapterStateChange |= ImGui::InputText("##chapterName", &chapter.name);
            ImGui::TableNextColumn();

            Util::FormatTime(timeBuf, sizeof(timeBuf), chapter.startTime, true);
            ImGui::TextUnformatted(timeBuf);

            ImGui::TableNextColumn();
            Util::FormatTime(timeBuf, sizeof(timeBuf), chapter.endTime, true);
            ImGui::TextUnformatted(timeBuf);

            ImGui::TableNextColumn();
            if (ImGui::Button(TR(DELETE))) 
            {
                deleteIdx = i;
                chapterStateChange = true;
            }
            ImGui::PopID();
        }

        if(deleteIdx >= 0 && deleteIdx < chapterState.chapters.size())
        {
            auto it = chapterState.chapters.begin() + deleteIdx;
            chapterState.chapters.erase(it);
        }
        
        if(chapterStateChange)
            EV::Enqueue<ChapterStateChanged>();

        ImGui::EndTable();
    }

    ImGui::End();
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