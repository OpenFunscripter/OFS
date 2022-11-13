#include "OFS_VideoplayerControls.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "SDL_timer.h"

#include "OFS_Videoplayer.h"
#include "OFS_VideoplayerEvents.h"

#include "imgui.h"
#include "imgui_internal.h"

static char tmp_buf[2][32];

void OFS_VideoplayerControls::VideoLoaded(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    FUN_ASSERT(ev.user.data1 != nullptr, "data was null");
    if (ev.user.data1 != nullptr) {
        videoPreview->previewVideo(*(std::string*)ev.user.data1, 0.f);
    }
}

void OFS_VideoplayerControls::Init(OFS_Videoplayer* player) noexcept
{
    this->player = player;
    Heatmap = std::make_unique<FunscriptHeatmap>();
    videoPreview = std::make_unique<VideoPreview>();
    videoPreview->setup(false);
    EventSystem::ev().Subscribe(VideoEvents::VideoLoaded, EVENT_SYSTEM_BIND(this, &OFS_VideoplayerControls::VideoLoaded));
}

bool OFS_VideoplayerControls::DrawTimelineWidget(const char* label, float* position, TimelineCustomDrawFunc&& customDraw) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    bool change = false;
    
    auto drawList = ImGui::GetWindowDrawList();

    if(ImGui::GetCurrentWindowRead()->SkipItems)
        return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiID id = ImGui::GetID(label);
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetFontSize() * 1.5f;

    const ImRect frameBB(ImGui::GetCursorScreenPos() + style.FramePadding, ImGui::GetCursorScreenPos() + ImVec2(w, h) - style.FramePadding);
    const ImRect totalBB(frameBB.Min, frameBB.Max);

    ImGui::ItemSize(totalBB, style.FramePadding.y);
    if (!ImGui::ItemAdd(totalBB, id, &frameBB))
        return false;

    const bool item_hovered = ImGui::IsItemHovered();

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

    customDraw(drawList, frameBB, item_hovered);

    // position highlighX
    ImVec2 p1(currentPosX, frameBB.Min.y);
    ImVec2 p2(currentPosX, frameBB.Max.y);
    constexpr float timeline_pos_cursor_w = 4.f;
    drawList->AddLine(p1 + ImVec2(0.f, h / 3.f), p2 + ImVec2(0.f, h / 3.f), IM_COL32(255, 0, 0, 255), timeline_pos_cursor_w / 2.f);

    // gradient + shadow
    Heatmap->DrawHeatmap(drawList, frameBB.Min, frameBB.Max);

    const ImColor timeline_cursor_back = IM_COL32(255, 255, 255, 255);
    const ImColor timeline_cursor_front = IM_COL32(0, 0, 0, 255);
    auto mouse = ImGui::GetMousePos();
    float relTimelinePos = ((mouse.x - frameBB.Min.x) / frameBB.GetWidth());

    if (item_hovered) {
        drawList->AddLine(ImVec2(mouse.x, frameBB.Min.y),
            ImVec2(mouse.x, frameBB.Max.y),
            timeline_cursor_back, timeline_pos_cursor_w);
        drawList->AddLine(ImVec2(mouse.x, frameBB.Min.y),
            ImVec2(mouse.x, frameBB.Max.y),
            timeline_cursor_front, timeline_pos_cursor_w / 2.f);

        ImGui::BeginTooltipEx(ImGuiWindowFlags_None, ImGuiTooltipFlags_None);
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                *position = relTimelinePos;
                change = true;
                dragging = true;
            }

            videoPreview->update();
            if (SDL_GetTicks() - lastPreviewUpdate >= PreviewUpdateMs) {
                videoPreview->setPosition(relTimelinePos);
                lastPreviewUpdate = SDL_GetTicks();
            }
            const ImVec2 ImageDim = ImVec2(ImGui::GetFontSize()*7.f * (16.f / 9.f), ImGui::GetFontSize() * 7.f);
            ImGui::Image((void*)(intptr_t)videoPreview->renderTexture, ImageDim);
            float timeSeconds = player->Duration() * relTimelinePos;
            float timeDelta = timeSeconds - player->CurrentTime();
            Util::FormatTime(tmp_buf[0], sizeof(tmp_buf[0]), timeSeconds, false);
            Util::FormatTime(tmp_buf[1], sizeof(tmp_buf[1]), (timeDelta > 0) ? timeDelta : -timeDelta, false);
            if (timeDelta > 0)
                ImGui::Text("%s (+%s)", tmp_buf[0], tmp_buf[1]);
            else
                ImGui::Text("%s (-%s)", tmp_buf[0], tmp_buf[1]);
        }
        ImGui::EndTooltip();
    }

    if (dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

        if (mouseDelta.x != 0.f) {
            if(mouse.x >= frameBB.Min.x && mouse.x <= frameBB.Max.x)
            {
                float startDragRelPos = (((mouse.x - mouseDelta.x) - frameBB.Min.x) / frameBB.GetWidth());
                float dragPosDelta = relTimelinePos - startDragRelPos;
                *position += dragPosDelta;
                change = true;
            }
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    }
    else {
        dragging = false;
    }

    drawList->AddLine(p1, p2, timeline_cursor_back, timeline_pos_cursor_w);
    drawList->AddLine(p1, p2, timeline_cursor_front, timeline_pos_cursor_w / 2.f);

    constexpr float min_val = 0.f;
    constexpr float max_val = 1.f;
    if (change) { 
        *position = Util::Clamp(*position, min_val, max_val); 
    }

    return change;
}

void OFS_VideoplayerControls::DrawTimeline(bool* open, TimelineCustomDrawFunc&& customDraw) noexcept
{
    if (open != nullptr && !*open) return;
    OFS_PROFILE(__FUNCTION__);
    FUN_ASSERT(player != nullptr, "nullptr");
    ImGui::Begin(TR_ID(TimeId, Tr::TIME), open);

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
        // format total duration
        // this doesn't need to be done every frame
        Util::FormatTime(tmp_buf[1], sizeof(tmp_buf[1]), player->Duration(), true);

        double time_seconds = player->CurrentTime();
        Util::FormatTime(tmp_buf[0], sizeof(tmp_buf[0]), time_seconds, true);
        ImGui::Text(" %s / %s (x%.03f)", tmp_buf[0], tmp_buf[1], actualPlaybackSpeed);
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
    if (DrawTimelineWidget(TR_ID("TIMELINE", Tr::TIMELINE), &position, std::move(customDraw))) {
        if (!player->IsPaused()) {
            hasSeeked = true;
        }
        player->SetPositionPercent(position, true);
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && hasSeeked) {
        player->SetPaused(false);
        hasSeeked = false;
    }

    ImGui::End();
}

void OFS_VideoplayerControls::DrawControls(bool* open) noexcept
{
    if (open != nullptr && !*open) return;
    OFS_PROFILE(__FUNCTION__);
    FUN_ASSERT(player != nullptr, "nullptr");

    ImGui::Begin(TR_ID(ControlId, Tr::CONTROLS), open);

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
