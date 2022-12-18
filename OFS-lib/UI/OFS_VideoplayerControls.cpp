#include "OFS_VideoplayerControls.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "SDL_timer.h"
#include "OFS_EventSystem.h"
#include "OFS_Videoplayer.h"
#include "OFS_VideoplayerEvents.h"
#include "OFS_Localization.h"
#include "OFS_DynamicFontAtlas.h"

#include "state/states/ChapterState.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "OFS_GL.h"

void OFS_VideoplayerControls::VideoLoaded(const VideoLoadedEvent* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if(ev->playerType != VideoplayerType::Main) return;
    videoPreview->PreviewVideo(ev->videoPath, 0.f);
}

void OFS_VideoplayerControls::Init(OFS_Videoplayer* player, bool hwAccel) noexcept
{
    if(this->player) return;
    this->player = player;
    chapterStateHandle = OFS_ProjectState<ChapterState>::Register(ChapterState::StateName);
    Heatmap = std::make_unique<FunscriptHeatmap>();
    videoPreview = std::make_unique<VideoPreview>(hwAccel);
    videoPreview->Init();

    EV::Queue().appendListener(VideoLoadedEvent::EventType,
        VideoLoadedEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_VideoplayerControls::VideoLoaded)));
}

inline static ImRect GetWidgetBB(float heightMulti) noexcept
{
    auto& style = ImGui::GetStyle();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetFontSize() * heightMulti;
    const auto cursorPos = ImGui::GetCursorScreenPos();
    return ImRect(cursorPos + style.FramePadding, cursorPos + ImVec2(w, h) - style.FramePadding);
}

inline static ImRect GetChapterWidgetBB() noexcept
{
    auto& style = ImGui::GetStyle();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetContentRegionAvail().y;
    const auto cursorPos = ImGui::GetCursorScreenPos();
    return ImRect(cursorPos + style.FramePadding, cursorPos + ImVec2(w, h) - style.FramePadding);
}

bool OFS_VideoplayerControls::DrawTimelineWidget(const char* label, float* position) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    bool change = false;
    
    if(ImGui::GetCurrentWindowRead()->SkipItems)
        return false;

    auto drawList = ImGui::GetWindowDrawList();
    const auto& style = ImGui::GetStyle();
    const auto id = ImGui::GetID(label);

    const auto frameBB = GetWidgetBB(1.5f);;
    const float h = frameBB.GetHeight();

    ImGui::ItemSize(frameBB, style.FramePadding.y);
    if (!ImGui::ItemAdd(frameBB, id, &frameBB))
        return false;


    const float currentPosX = frameBB.Min.x + frameBB.GetWidth() * (*position);
    const float offsetProgressH = h / 5.f;
    const float offsetProgressW = currentPosX - frameBB.Min.x;
    drawList->AddRectFilled(
        frameBB.Min + ImVec2(0.f, offsetProgressH),
        frameBB.Min + ImVec2(offsetProgressW, frameBB.GetHeight()) + ImVec2(0.f, offsetProgressH),
        ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
    drawList->AddRectFilled(frameBB.Min + ImVec2(offsetProgressW, offsetProgressH),
        frameBB.Max + ImVec2(0.f, offsetProgressH),
        IM_COL32(150, 150, 150, 255));

    // position highlighX
    ImVec2 p1(currentPosX, frameBB.Min.y);
    ImVec2 p2(currentPosX, frameBB.Max.y);
    constexpr float timelinePosCursorW = 2.f;
    drawList->AddLine(p1 + ImVec2(0.f, h / 3.f), p2 + ImVec2(0.f, h / 3.f), IM_COL32(255, 0, 0, 255), timelinePosCursorW);

    Heatmap->DrawHeatmap(drawList, frameBB.Min, frameBB.Max);

    const uint32_t timelineCursorBackColor = IM_COL32(255, 255, 255, 255);
    const uint32_t timelineCursorFrontColor = IM_COL32(0, 0, 0, 255);
    auto mouse = ImGui::GetMousePos();
    float relTimelinePos = ((mouse.x - frameBB.Min.x) / frameBB.GetWidth());

    if (ImGui::IsItemHovered()) {
        drawList->AddLine(ImVec2(mouse.x, frameBB.Min.y),
            ImVec2(mouse.x, frameBB.Max.y),
            timelineCursorBackColor, timelinePosCursorW);
        drawList->AddLine(ImVec2(mouse.x, frameBB.Min.y),
            ImVec2(mouse.x, frameBB.Max.y),
            timelineCursorFrontColor, timelinePosCursorW / 2.f);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            *position = relTimelinePos;
            change = true;
            dragging = true;
        }

        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            if (SDL_GetTicks() - lastPreviewUpdate >= PreviewUpdateMs) {
                videoPreview->Play();
                videoPreview->SetPosition(relTimelinePos);
                lastPreviewUpdate = SDL_GetTicks();
            }
            ImGui::BeginTooltipEx(ImGuiWindowFlags_None, ImGuiTooltipFlags_None);
            {
                const ImVec2 ImageDim = ImVec2(ImGui::GetFontSize()*7.f * (16.f / 9.f), ImGui::GetFontSize() * 7.f);
                ImGui::Image((void*)(intptr_t)videoPreview->FrameTex(), ImageDim);
                float timeSeconds = player->Duration() * relTimelinePos;
                float timeDelta = timeSeconds - player->CurrentTime();

                char timeBuf1[16];
                char timeBuf2[16];
                Util::FormatTime(timeBuf1, sizeof(timeBuf1), timeSeconds, false);
                Util::FormatTime(timeBuf2, sizeof(timeBuf2), (timeDelta > 0) ? timeDelta : -timeDelta, false);
                if (timeDelta > 0)
                    ImGui::Text("%s (+%s)", timeBuf1, timeBuf2);
                else
                    ImGui::Text("%s (-%s)", timeBuf1, timeBuf2);
            }
            ImGui::EndTooltip();
        }
    }
    else 
    {
        videoPreview->Pause();
    }

    if (dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        // FIXME: this can be done easier by just using the mouse position
        if (mouseDelta.x != 0.f) {
            if(mouse.x >= frameBB.Min.x && mouse.x <= frameBB.Max.x)
            {
                float startDragRelPos = (((mouse.x - mouseDelta.x) - frameBB.Min.x) / frameBB.GetWidth());
                float dragPosDelta = relTimelinePos - startDragRelPos;
                *position += dragPosDelta;
                change = true;
            }
            else if(mouse.x >= frameBB.Min.x)
            {
                *position = 1.f;
                change = true;
            }
            else if(mouse.x <= frameBB.Max.x)
            {
                *position = 0.f;
                change = true;
            }
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    }
    else {
        dragging = false;
    }

    drawList->AddLine(p1, p2, timelineCursorBackColor, timelinePosCursorW);
    drawList->AddLine(p1, p2, timelineCursorFrontColor, timelinePosCursorW / 2.f);

    constexpr float min_val = 0.f;
    constexpr float max_val = 1.f;
    if (change) { 
        *position = Util::Clamp(*position, min_val, max_val); 
    }

    return change;
}

bool OFS_VideoplayerControls::DrawChapter(ImDrawList* drawList, const ImRect& frameBB, Chapter& chapter, ImDrawFlags drawFlags, float currentTime) noexcept
{
    auto renderColor = ImGui::ColorConvertFloat4ToU32(chapter.color);

    const float totalDuration = player->Duration();

    const float bookmarkSize = ImGui::GetFontSize()/3.f;

    bool contextMenuOpen = false;

    auto chapterRect = ImRect(
        frameBB.Min + ImVec2((chapter.startTime / totalDuration) * frameBB.GetWidth(), 0.f),
        frameBB.Min + ImVec2((chapter.endTime / totalDuration) * frameBB.GetWidth(), frameBB.GetHeight() - bookmarkSize)
    );

    auto mousePos = ImGui::GetMousePos();
    bool chapterHover = false;
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
    {
        auto hoverRect = chapterRect;
        hoverRect.Max.y -= bookmarkSize;

        if(hoverRect.Contains(mousePos))
        {
            renderColor = ImColor(
                Util::Clamp(chapter.color.Value.x > 0.5f ? chapter.color.Value.x - 0.25f : chapter.color.Value.x + 0.25f, 0.f, 1.f),
                Util::Clamp(chapter.color.Value.y > 0.5f ? chapter.color.Value.y - 0.25f : chapter.color.Value.y + 0.25f, 0.f, 1.f),
                Util::Clamp(chapter.color.Value.z > 0.5f ? chapter.color.Value.z - 0.25f : chapter.color.Value.z + 0.25f, 0.f, 1.f)
            );
            chapterHover = true;
        }
    }
    drawList->AddRectFilled(chapterRect.Min, chapterRect.Max, renderColor, 10.f, drawFlags);

    if(currentTime >= chapter.startTime && currentTime <= chapter.endTime)
    {
        drawList->AddRect(chapterRect.Min, chapterRect.Max, IM_COL32(255, 255, 255, 200), 10.f, drawFlags, 3.f);
    }

    if(!chapter.name.empty())
    {
        auto textSize = ImGui::CalcTextSize(chapter.name.c_str());
        if(textSize.x <= chapterRect.GetWidth()) 
        {
            const auto textOffset = ImVec2(
                (chapterRect.GetWidth() - textSize.x) / 2.f,
                (chapterRect.GetHeight() - textSize.y) / 2.f
            );
            drawList->AddText(chapterRect.Min + textOffset, ImGui::GetColorU32(ImGuiCol_Text), chapter.name.c_str());
        }
        else if(chapterHover)
        {
            OFS::Tooltip(chapter.name.c_str());
        }
    }
    
    if(chapterHover)
    {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            // When at the startTime go to endTime
            if(std::abs(currentTime - chapter.startTime) <= 0.01f)
            {
                player->SetPositionExact(chapter.endTime);
            }
            // When at the endTime go to startTime
            else if(std::abs(currentTime - chapter.endTime) <= 0.01f)
            {
                player->SetPositionExact(chapter.startTime);
            }
            // Default to startTime
            else 
            {
                player->SetPositionExact(chapter.startTime);
            }
        }
        else if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("ChapterContextMenu");
        }

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
            {
                memcpy((float*)&chapter.color.Value.x, payload->Data, sizeof(float) * 3);
            }
            else if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
            {
                memcpy((float*)&chapter.color.Value.x, payload->Data, sizeof(float) * 4);
            }
            
            ImGui::EndDragDropTarget();
        }
    }

    if(ImGui::BeginPopup("ChapterContextMenu"))
    {
        contextMenuOpen = true;
        char timeBuf1[16];
        char timeBuf2[16];
        Util::FormatTime(timeBuf1, sizeof(timeBuf1), chapter.startTime, true);
        Util::FormatTime(timeBuf2, sizeof(timeBuf2), chapter.endTime, true);

        ImGui::Text("%s - %s", timeBuf1, timeBuf2);
        ImGui::SetNextItemWidth(ImGui::GetItemRectSize().x);
        bool chapterChange = ImGui::InputTextWithHint("##ChapterName", TR(NAME), &chapter.name);
        
        ImGui::Separator();

        if(ImGui::MenuItem(TR(SET_CHAPTER_SIZE))) 
        {
            auto& chapterState = ChapterState::State(chapterStateHandle);
            if(chapterState.SetChapterSize(chapter, currentTime))
            {
                EV::Enqueue<ChapterStateChanged>();
            }
        }

        if(ImGui::MenuItem(TR(ADD_NEW_BOOKMARK)))
        {
            auto& chapterState = ChapterState::State(chapterStateHandle);
            if(auto bookmark = chapterState.AddBookmark(currentTime)) 
            {
                EV::Enqueue<ChapterStateChanged>();
            }
        }

        if(ImGui::MenuItem(TR(EXPORT_CLIP)))
        {
            EV::Enqueue<ExportClipForChapter>(chapter);
        }

        ImGui::ColorEdit3(TR(COLOR), &chapter.color.Value.x, ImGuiColorEditFlags_NoInputs);

        if(ImGui::MenuItem(TR(REMOVE)))
        {
            Util::YesNoCancelDialog(TR(REMOVE_CHAPTER), 
                std::string(TR(REMOVE_CHAPTER_MSG)) + FMT("\n[%s]", chapter.name.c_str()),
                [chapterPtr = &chapter, stateHandle = chapterStateHandle](auto result)
                {
                    if(result == Util::YesNoCancel::Yes)
                    {
                        auto& state = ChapterState::State(stateHandle);
                        auto it = std::find_if(state.chapters.begin(), state.chapters.end(),
                            [chapterPtr](auto& chapter) { return chapterPtr == &chapter; });
                        if(it != state.chapters.end()) {
                            state.chapters.erase(it);
                            EV::Enqueue<ChapterStateChanged>();
                        }
                    }
                });
        }
        if(chapterChange)
        {
            EV::Enqueue<ChapterStateChanged>();
        }
        ImGui::EndPopup();
    }
    return contextMenuOpen;
}

bool OFS_VideoplayerControls::DrawBookmark(ImDrawList* drawList, const ImRect& frameBB, Bookmark& bookmark) noexcept
{
    const float bookmarkSize = ImGui::GetFontSize()/3.f;
    auto totalDuration = player->Duration();

    auto p1 = frameBB.Min + ImVec2((bookmark.time / totalDuration) * frameBB.GetWidth(), frameBB.GetHeight() - bookmarkSize);
    auto bookmarkColor = IM_COL32(255, 255, 255, 255);
    bool contextMenuOpen = false;

    auto mousePos = ImGui::GetMousePos();
    bool bookmarkHover = false;
    if(ImGui::IsItemHovered())
    {
        auto bookmarkRect = ImRect(p1 - ImVec2(bookmarkSize, bookmarkSize), p1 + ImVec2(bookmarkSize, bookmarkSize));
        if(bookmarkRect.Contains(mousePos))
        {
            bookmarkColor = IM_COL32(0, 255, 255, 255);
            bookmarkHover = true;
        }
    }

    if(bookmarkHover)
    {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            player->SetPositionExact(bookmark.time);
        }
        else if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("BookmarkContextMenu");
        }

        if(!bookmark.name.empty())
        {
            OFS::Tooltip(bookmark.name.c_str());
        }
    }

    drawList->AddCircleFilled(p1, bookmarkSize, bookmarkColor, 4);

    if(ImGui::BeginPopup("BookmarkContextMenu"))
    {
        contextMenuOpen = true;
        char timeBuf[16];
        Util::FormatTime(timeBuf, sizeof(timeBuf), bookmark.time, true);
        ImGui::TextUnformatted(timeBuf);
        ImGui::SetNextItemWidth(ImGui::GetItemRectSize().x);
        bool bookmarkChange = ImGui::InputTextWithHint("##bookmarkName", TR(NAME), &bookmark.name);
        
        if(ImGui::MenuItem(TR(REMOVE)))
        {
            Util::YesNoCancelDialog(TR(REMOVE_BOOKMARK), 
                std::string(TR(REMOVE_BOOKMARK_MSG)) + FMT("\n[%s]", bookmark.name.c_str()),
                [bookmarkPtr = &bookmark, stateHandle = chapterStateHandle](auto result)
                {
                    if(result == Util::YesNoCancel::Yes)
                    {
                        auto& state = ChapterState::State(stateHandle);
                        auto it = std::find_if(state.bookmarks.begin(), state.bookmarks.end(),
                            [bookmarkPtr](auto& bookmark) { return bookmarkPtr == &bookmark; });
                        if(it != state.bookmarks.end()) {
                            state.bookmarks.erase(it);
                            EV::Enqueue<ChapterStateChanged>();
                        }
                    }
                });
        }
        if(bookmarkChange)
        {
            EV::Enqueue<ChapterStateChanged>();
        }
        ImGui::EndPopup();
    }
    return contextMenuOpen;
}

void OFS_VideoplayerControls::DrawChapterWidget(ImDrawList* drawList, float currentTime) noexcept
{
    if(ImGui::GetCurrentWindowRead()->SkipItems)
        return;

    const auto& style = ImGui::GetStyle();
    const auto id = ImGui::GetID("ChapterWidget");

    const auto frameBB = GetChapterWidgetBB();
    if(frameBB.GetHeight() < ImGui::GetFontSize())
        return;
    
    ImGui::ItemSize(frameBB, style.FramePadding.y);
    if (!ImGui::ItemAdd(frameBB, id, &frameBB))
        return;

    auto& state = ChapterState::State(chapterStateHandle);
    drawList->AddRectFilled(frameBB.Min, frameBB.Max, IM_COL32(50, 50, 50, 255), 10.f, ImDrawFlags_RoundCornersAll);

    bool contextMenu = false;

    // Chapters
    if(!state.chapters.empty())
    {
        // First chapter
        auto& chapter = state.chapters.front();
        ImGui::PushID(0);
        contextMenu |= DrawChapter(drawList, frameBB, chapter, ImDrawFlags_RoundCornersLeft | ImDrawFlags_RoundCornersTop, currentTime);
        ImGui::PopID();
    }

    for(int i=1, size=state.chapters.size() - 1; i < size; i += 1)
    {
        auto& chapter = state.chapters[i];
        ImGui::PushID(i);
        contextMenu |= DrawChapter(drawList, frameBB, chapter, ImDrawFlags_RoundCornersTop, currentTime);
        ImGui::PopID();
    }
    
    if(state.chapters.size() >= 2)
    {
        // Last chapter
        auto& chapter = state.chapters.back();
        ImGui::PushID(state.chapters.size());
        contextMenu |= DrawChapter(drawList, frameBB, chapter, ImDrawFlags_RoundCornersRight | ImDrawFlags_RoundCornersTop, currentTime);
        ImGui::PopID();
    }

    // Bookmarks
    int idOffset = state.chapters.size();
    for(int i=0, size=state.bookmarks.size(); i < size; i += 1)
    {
        auto& bookmark = state.bookmarks[i];
        ImGui::PushID(i + idOffset);
        contextMenu |= DrawBookmark(drawList, frameBB, bookmark);
        ImGui::PopID();
    }

    if(!contextMenu && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("ChapterBookmarkContextMenu");
    }

    if(ImGui::BeginPopup("ChapterBookmarkContextMenu"))
    {
        if(ImGui::MenuItem(TR(ADD_NEW_CHAPTER)))
        {
            auto& state = ChapterState::State(chapterStateHandle);
            if(auto chapter = state.AddChapter(currentTime, player->Duration())) {
                EV::Enqueue<ChapterStateChanged>();
            }
        }
        if(ImGui::MenuItem(TR(ADD_NEW_BOOKMARK)))
        {
            auto& state = ChapterState::State(chapterStateHandle);
            if(auto bookmark = state.AddBookmark(currentTime)) {
                EV::Enqueue<ChapterStateChanged>();
            }
        }
        ImGui::EndPopup();
    }
}

void OFS_VideoplayerControls::DrawTimeline() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    FUN_ASSERT(player != nullptr, "nullptr");
    ImGui::Begin(TR_ID(TimeId, Tr::TIME));

    {
        constexpr float speedCalcUpdateFrequency = 1.0f;
        if (!player->IsPaused()) {
            if ((SDL_GetTicks() - measureStartTime) / 1000.0f >= speedCalcUpdateFrequency) {
                float duration = player->Duration();
                float position = player->CurrentPercentPosition();
                float expectedStep = speedCalcUpdateFrequency / duration;
                float actualStep = std::abs(position - lastPlayerPosition);
                actualPlaybackSpeed = actualStep / expectedStep;

                lastPlayerPosition = player->CurrentPercentPosition();
                measureStartTime = SDL_GetTicks();
            }
        }
        else {
            lastPlayerPosition = player->CurrentPercentPosition();
            measureStartTime = SDL_GetTicks();
        }
    }

    ImGui::Columns(5, 0, false);
    {
        char timeBuf1[16];
        char timeBuf2[16];

        float time = player->CurrentTime();
        Util::FormatTime(timeBuf1, sizeof(timeBuf1), time, true);
        Util::FormatTime(timeBuf2, sizeof(timeBuf2), player->Duration(), true);

        ImGui::Text(" %s / %s (x%.03f)", timeBuf1, timeBuf2, actualPlaybackSpeed);
        ImGui::NextColumn();
    }

    auto& style = ImGui::GetStyle();
    ImGui::SetColumnWidth(0, ImGui::GetItemRectSize().x + style.ItemSpacing.x);

    if (ImGui::Button("1x", ImVec2(0, 0))) {
        player->SetSpeed(1.f);
    }
    ImGui::SetColumnWidth(1, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
    ImGui::NextColumn();

    if (ImGui::Button("-10%", ImVec2(0, 0))) {
        player->AddSpeed(-0.10f);
    }
    ImGui::SetColumnWidth(2, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
    ImGui::NextColumn();

    if (ImGui::Button("+10%", ImVec2(0, 0))) {
        player->AddSpeed(0.10f);
    }
    ImGui::SetColumnWidth(3, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-1.f);
    float currentSpeed = player->CurrentSpeed();
    if (ImGui::SliderFloat("##Speed", &currentSpeed, 
        OFS_Videoplayer::MinPlaybackSpeed, OFS_Videoplayer::MaxPlaybackSpeed,
        "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
        player->SetSpeed(currentSpeed);
    }
    OFS::Tooltip(TR(SPEED));

    ImGui::Columns(1, 0, false);

    float position = player->CurrentPercentPosition();
    if (DrawTimelineWidget(TR_ID("TIMELINE", Tr::TIMELINE), &position)) {
        if (!player->IsPaused()) {
            hasSeeked = true;
        }
        player->SetPositionPercent(position, true);
    }    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && hasSeeked) {
        player->SetPaused(false);
        hasSeeked = false;
    }

    // Spacing
    ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize()/2.f));

    DrawChapterWidget(ImGui::GetWindowDrawList(), player->CurrentTime());

    ImGui::End();
}

void OFS_VideoplayerControls::DrawControls() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    FUN_ASSERT(player != nullptr, "nullptr");

    ImGui::Begin(TR_ID(ControlId, Tr::CONTROLS));

    constexpr float seekTime = 3.f;
    // Playback controls
    ImGui::Columns(5, 0, false);
    if (ImGui::Button(ICON_STEP_BACKWARD /*"<"*/, ImVec2(-1, 0))) {
        if (player->IsPaused()) {
            player->PreviousFrame();
        }
    }
    ImGui::NextColumn();
    if (ImGui::Button(ICON_BACKWARD /*"<<"*/, ImVec2(-1, 0))) {
        player->SeekRelative(-seekTime);
    }
    ImGui::NextColumn();

    if (ImGui::Button((player->IsPaused()) ? ICON_PLAY : ICON_PAUSE, ImVec2(-1, 0))) {
        player->TogglePlay();
    }
    ImGui::NextColumn();

    if (ImGui::Button(ICON_FORWARD /*">>"*/, ImVec2(-1, 0))) {
        player->SeekRelative(seekTime);
    }
    ImGui::NextColumn();

    if (ImGui::Button(ICON_STEP_FORWARD /*">"*/, ImVec2(-1, 0))) {
        if (player->IsPaused()) {
            player->NextFrame();
        }
    }
    ImGui::NextColumn();

    ImGui::Columns(2, 0, false);
    if (ImGui::Checkbox(mute ? ICON_VOLUME_OFF : ICON_VOLUME_UP, &mute)) {
        if (mute) {
            player->Mute();
        }
        else {
            player->Unmute();
        }
    }
    ImGui::SetColumnWidth(0, ImGui::GetItemRectSize().x + 10);
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(-1);
    float volume = player->Volume();
    if (ImGui::SliderFloat("##Volume", &volume, 0.0f, 1.0f)) {
        volume = Util::Clamp(volume, 0.0f, 1.f);
        player->SetVolume(volume);
        if (volume > 0.0f) {
            mute = false;
        }
    }
    ImGui::NextColumn();
    ImGui::End();
}

#include "OFS_Shader.h"
#include "imgui_impl/imgui_impl_opengl3.h"

std::vector<uint8_t> OFS_VideoplayerControls::RenderHeatmapToBitmapWithChapters(int16_t width, int16_t height, int16_t chapterHeight) noexcept
{
    width = Util::Clamp<int16_t>(width, 32, FunscriptHeatmap::MaxResolution);
    height = Util::Clamp<int16_t>(height + chapterHeight, 32, FunscriptHeatmap::MaxResolution);

    // Prepare temporary framebuffer
    uint32_t tmpFramebuffer = 0;
    uint32_t tmpColorTex = 0;

    glGenFramebuffers(1, &tmpFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, tmpFramebuffer);

    glGenTextures(1, &tmpColorTex);
    glBindTexture(GL_TEXTURE_2D, tmpColorTex);

    glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, width, height, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmpColorTex, 0);
    GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, DrawBuffers);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: leaking memory
        return {};
    }

    // Backup out main ImGuiContext
    auto prevContext = ImGui::GetCurrentContext();

    // Create a temporary ImGuiContext
    auto tmpContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(tmpContext);
    ImGui_ImplOpenGL3_Init(OFS_SHADER_VERSION);

    // Prepare drawing a single image
    auto& io = ImGui::GetIO();
    io.DisplaySize.x = width;
    io.DisplaySize.y = height;

    // This may be bad
    OFS_DynFontAtlas::ptr->forceRebuild = true;
    OFS_DynFontAtlas::RebuildFont(prevContext->FontSize);

    // windows don't render on the first frame which is why we do multiple runs
    for(int i=0; i < 2; i += 1)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // Draw calls
        {
            auto& style = ImGui::GetStyle();

            ImGui::Begin("##heatmapRenderChapters", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
            ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetWindowSize(io.DisplaySize, ImGuiCond_Always);
            auto drawList = ImGui::GetWindowDrawList();
            auto availWidth = io.DisplaySize.x - style.FramePadding.x - style.ItemSpacing.x;
            Heatmap->DrawHeatmap(drawList, ImVec2(style.FramePadding.x + style.ItemSpacing.x, 0.f), ImVec2(availWidth, height - chapterHeight));

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::Dummy(ImVec2(0, height - chapterHeight));
            DrawChapterWidget(ImGui::GetWindowDrawList(), -1.f);
            ImGui::PopStyleVar();
            ImGui::End();
        }

        // Render image
        ImGui::Render();
        OFS_ImGui::CurrentlyRenderedViewport = ImGui::GetMainViewport();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        OFS_ImGui::CurrentlyRenderedViewport = nullptr;
    }


    // Grab the bitmap
    std::vector<uint8_t> bitmap;
    bitmap.resize((size_t)io.DisplaySize.x * (size_t)(io.DisplaySize.y) * (size_t)4, 0);
    glReadPixels(0, 0, io.DisplaySize.x, io.DisplaySize.y, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.data());

    // Destroy everything
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext(tmpContext);

    glDeleteTextures(1, &tmpColorTex);
    glDeleteFramebuffers(1, &tmpFramebuffer);

    // Reset to default framebuffer and main ImGuiContext    
    ImGui::SetCurrentContext(prevContext);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return bitmap;
}