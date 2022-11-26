#include "OFS_ScriptSimulator.h"
#include "OFS_ImGui.h"

#include "OFS_EventSystem.h"
#include "Funscript.h"
#include "OFS_DynamicFontAtlas.h"

#include "imgui.h"
#include "stb_sprintf.h"

#include "state/SimulatorState.h"

inline static float Distance(const ImVec2& p1, const ImVec2& p2) noexcept
{
    ImVec2 diff = p1 - p2;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

inline static ImVec2 Normalize(const ImVec2& p) noexcept
{
    auto mag = Distance(ImVec2(0.f, 0.f), p);
    return ImVec2(p.x / mag, p.y / mag);
}

inline static uint32_t GetColor(const ImColor& col, float opacity) noexcept
{
    auto color = ImGui::ColorConvertFloat4ToU32(col);
    ((uint8_t*)&color)[IM_COL32_A_SHIFT / 8] = ((uint8_t)(255 * col.Value.w * opacity));
    return color;
}

void ScriptSimulator::Init() noexcept
{
    stateHandle = OFS_ProjectState<SimulatorState>::Register(SimulatorState::StateName);
    EV::Queue().appendListener(SDL_MOUSEMOTION,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &ScriptSimulator::MouseMovement)));
}

inline static float CalcBearing(const ImVec2 p1, const ImVec2 p2) noexcept 
{
    float theta = SDL_atan2f(p2.x - p1.x, p1.y - p2.y);
    if (theta < 0.0)
        theta += M_PI*2.f;
    return theta;
}

void ScriptSimulator::MouseMovement(const OFS_SDL_Event* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& state = SimulatorState::State(stateHandle);
    auto& motion = ev->sdl.motion;
    const auto& simP1 = state.P1;
    const auto& simP2 = state.P2;

    if (std::abs(simP1.x - simP2.x) > std::abs(simP1.y - simP2.y)) {
        // horizontal
        auto [top_x, bottom_x] = std::minmax(simP1.x, simP2.x);
        mouseValue = motion.x - top_x;
        mouseValue /= (bottom_x - top_x);
    }
    else {
        // vertical
        auto [top_y, bottom_y] = std::minmax(simP1.y, simP2.y);
        mouseValue = motion.y - bottom_y;
        mouseValue /= top_y - bottom_y;
    }
    auto clamped = Util::Clamp(mouseValue, 0.f, 1.f);

    // Create a axis aligned rectangle with the size of the simulator + padding
    constexpr float areaPadding = 10.f;
    float simLength = Distance(simP1, simP2);
    auto simPos = (simP1 + simP2) / 2.f;
    ImRect areaRect;
    areaRect.Min = simPos - ImVec2(areaPadding, areaPadding);
    areaRect.Max = simPos + ImVec2(state.Width, simLength) + ImVec2(areaPadding, areaPadding);
    areaRect.Min -= ImVec2(state.Width / 2.f, simLength / 2.f);
    areaRect.Max -= ImVec2(state.Width / 2.f, simLength / 2.f);

    // rotate mouse pos into the same direction as the simulator
    float theta = CalcBearing(simP1, simP2);
    ImVec2 mousePosOnSim = ImVec2(motion.x, motion.y) - simPos;
    mousePosOnSim = simPos + ImRotate(mousePosOnSim, -SDL_cosf(theta), SDL_sinf(theta));
    // check if mousePos is on the simulator
    MouseOnSimulator = areaRect.Contains(mousePosOnSim);

    mouseValue = clamped;
    mouseValue = mouseValue * 2.f - 1.f;
}

void ScriptSimulator::CenterSimulator() noexcept
{
    auto& state = SimulatorState::State(stateHandle);
    const float default_len = Util::Clamp(state.Width * 3.f, state.Width, 1000.f);
    auto Size = ImGui::GetMainViewport()->Size;
    state.P1 = (Size / 2.f);
    state.P1.y -= default_len/2.f;
    state.P1.x -=  (state.Width / 2.f);
    state.P2 = state.P1 + ImVec2(0.f, default_len);
}

void ScriptSimulator::ShowSimulator(bool* open, std::shared_ptr<Funscript>& activeScript, float currentTime, bool splineMode) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);
    float currentPos = 0;

    if (positionOverride >= 0.f) {
        currentPos = positionOverride;
        positionOverride = -1.f;
    }
    else {
        currentPos = splineMode 
            ? activeScript->SplineClamped(currentTime) 
            : activeScript->GetPositionAtTime(currentTime);
    }

    if (EnableVanilla) {
        ImGui::Begin(TR_ID(WindowId, Tr::SIMULATOR), open, 
            ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoDocking);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::VSliderFloat("", ImGui::GetContentRegionAvail(), &currentPos, 0, 100);
        ImGui::PopItemFlag();
        ImGui::End();
        if (!*open) {
            EnableVanilla = false;
            *open = true;
        }
        return;
    }

    ImGui::Begin(TR_ID(WindowId, Tr::SIMULATOR), open, ImGuiWindowFlags_None);
    char tmp[4];
    auto frontDraw = ImGui::GetForegroundDrawList();
    ImGuiContext* g = ImGui::GetCurrentContext();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    auto& style = ImGui::GetStyle();
    
    auto& state = SimulatorState::State(stateHandle);

    ImGui::Checkbox(FMT("%s %s", TR(LOCK), state.LockedPosition ? ICON_LINK : ICON_UNLINK), &state.LockedPosition);
    ImGui::Columns(2, 0, false);
    if (ImGui::Button(TR(CENTER), ImVec2(-1.f, 0.f))) { CenterSimulator(); }
    ImGui::NextColumn();
    if (ImGui::Button(TR(INVERT), ImVec2(-1.f, 0.f))) { 
        auto tmp = state.P1;
        state.P1 = state.P2;
        state.P2 = tmp; 
    }
    ImGui::Columns(1);

    ImGui::Columns(2, 0, false);
    if (ImGui::Button(TR(LOAD_CONFIG), ImVec2(-1.f, 0.f))) {
        auto& dState = SimulatorDefaultConfigState::StaticStateSlow();
        state = dState.defaultState;
    }
    ImGui::NextColumn();
    if (ImGui::Button(TR(SAVE_CONFIG), ImVec2(-1.f, 0.f))) { 
        Util::YesNoCancelDialog(TR(SAVE_SIMULATOR_CONFIG),
            TR(SAVE_SIMULATOR_CONFIG_MSG), 
            [this](Util::YesNoCancel result) {
                if(result == Util::YesNoCancel::Yes) {
                    auto& dState = SimulatorDefaultConfigState::StaticStateSlow();
                    auto& state = SimulatorState::State(stateHandle);
                    dState.defaultState = state;
                }
            }
        );
    }
    ImGui::Columns(1);
    if (ImGui::CollapsingHeader(TR(CONFIGURATION), ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::ColorEdit4(TR(TEXT), &state.Text.Value.x);
        ImGui::ColorEdit4(TR(BORDER), &state.Border.Value.x);
        ImGui::ColorEdit4(TR(FRONT), &state.Front.Value.x);
        ImGui::ColorEdit4(TR(BACK), &state.Back.Value.x);
        ImGui::ColorEdit4(TR(INDICATOR), &state.Indicator.Value.x);
        ImGui::ColorEdit4(TR(LINES), &state.ExtraLines.Value.x);

        if (ImGui::DragFloat(TR(WIDTH), &state.Width)) {
            state.Width = Util::Clamp<float>(state.Width, 0.f, 1000.f);
        }
        if (ImGui::DragFloat(TR(BORDER), &state.BorderWidth, 0.5f)) {
            state.BorderWidth = Util::Clamp<float>(state.BorderWidth, 0.f, 1000.f);
        }
        ImGui::DragFloat(TR(LINE), &state.LineWidth, 0.5f);
        if (ImGui::SliderFloat(TR(OPACITY), &state.GlobalOpacity, 0.f, 1.f)) {
            state.GlobalOpacity = Util::Clamp<float>(state.GlobalOpacity, 0.f, 1.f);
        }

        if (ImGui::DragFloat(FMT("%s2", TR(LINE)), &state.ExtraLineWidth, 0.5f)) {
            state.ExtraLineWidth = Util::Clamp<float>(state.ExtraLineWidth, 0.5f, 1000.f);
        }

        ImGui::Checkbox(TR(INDICATOR), &state.EnableIndicators);
        ImGui::SameLine(); 
        ImGui::Checkbox(TR(LINES), &state.EnableHeightLines);
        if (ImGui::InputInt(TR(EXTRA_LINES), &state.ExtraLinesCount, 1, 2)) {
            state.ExtraLinesCount = Util::Clamp(state.ExtraLinesCount, 0, 10);
        }
        ImGui::Checkbox(TR(SHOW_POSITION), &state.EnablePosition);
        ImGui::Checkbox(TR(VANILLA), &EnableVanilla);
        OFS::Tooltip(TR(VANILLA_TOOLTIP));
        if(ImGui::Button(TR(RESET_TO_DEFAULTS), ImVec2(-1.f, 0.f))) { 
            state = SimulatorState();
        }
    }
        
    // Because the simulator is always drawn on top
    // we don't draw if there is a popup modal
    // as that would be irritating
    if (ImGui::GetTopMostPopupModal() != nullptr) {
        ImGui::End();
        return;
    }

    auto offset = window->ViewportPos; 
    
    ImVec2 direction = state.P1 - state.P2;
    direction = Normalize(direction);
    ImVec2 barP1 = offset + state.P1 - (direction * (state.BorderWidth / 2.f));
    ImVec2 barP2 = offset + state.P2 + (direction * (state.BorderWidth / 2.f));
    float distance = Distance(barP1, barP2);
    auto perpendicular = Normalize(state.P1 - state.P2);
    perpendicular = ImVec2(-perpendicular.y, perpendicular.x);

    // BACKGROUND
    frontDraw->AddLine(
        barP1 + direction,
        barP2 - direction,
        GetColor(state.Back, state.GlobalOpacity),
        state.Width - state.BorderWidth + 1.f
    );

    // FRONT BAR
    float percent = currentPos / 100.f;
    frontDraw->AddLine(
        barP2 + ((direction * distance)*percent),
        barP2,
        GetColor(state.Front, state.GlobalOpacity),
        state.Width - state.BorderWidth + 1.f
    );

    // BORDER
    if (state.BorderWidth > 0.f) {
        auto borderOffset = perpendicular * (state.Width / 2.f);
        frontDraw->AddQuad(
            offset + state.P1 - borderOffset, offset + state.P1 + borderOffset,
            offset + state.P2 + borderOffset, offset + state.P2 - borderOffset,
            GetColor(state.Border, state.GlobalOpacity),
            state.BorderWidth
        );
    }

    // HEIGHT LINES
    if (state.EnableHeightLines) {
        for (int i = 1; i < 10; i++) {
            float pos = i * 10.f;
            auto indicator1 =
                barP2
                + (direction * distance * (pos / 100.f))
                - (perpendicular * (state.Width / 2.f))
                + (perpendicular * (state.BorderWidth / 2.f));
            auto indicator2 =
                barP2
                + (direction * distance * (pos / 100.f))
                + (perpendicular * (state.Width / 2.f))
                - (perpendicular * (state.BorderWidth / 2.f));
        
            frontDraw->AddLine(
                indicator1,
                indicator2,
                GetColor(state.ExtraLines, state.GlobalOpacity),
                state.LineWidth
            );
        }

    }
    if (state.ExtraLinesCount > 0) {
        // extra height lines
        for (int i = -state.ExtraLinesCount; i < 1; ++i) {
            float pos = i * 10.f;
            auto indicator1 =
                barP2
                + (direction * distance * (pos / 100.f))
                - (perpendicular * (state.Width / 2.f))
                + (perpendicular * (state.BorderWidth / 2.f));
            auto indicator2 =
                barP2
                + (direction * distance * (pos / 100.f))
                + (perpendicular * (state.Width / 2.f))
                - (perpendicular * (state.BorderWidth / 2.f));

            frontDraw->AddLine(
                indicator1,
                indicator2,
                GetColor(state.ExtraLines, state.GlobalOpacity),
                state.ExtraLineWidth
            );
        }
        for (int i = 10; i < (11+state.ExtraLinesCount); ++i) {
            float pos = i * 10.f;
            auto indicator1 =
                barP2
                + (direction * distance * (pos / 100.f))
                - (perpendicular * (state.Width / 2.f))
                + (perpendicular * (state.BorderWidth / 2.f));
            auto indicator2 =
                barP2
                + (direction * distance * (pos / 100.f))
                + (perpendicular * (state.Width / 2.f))
                - (perpendicular * (state.BorderWidth / 2.f));

            frontDraw->AddLine(
                indicator1,
                indicator2,
                GetColor(state.ExtraLines, state.GlobalOpacity),
                state.ExtraLineWidth
            );
        }
    }

    // INDICATORS
    if (state.EnableIndicators) {
        auto previousAction = activeScript->GetActionAtTime(currentTime, 0.02f);
        if (previousAction == nullptr) {
            previousAction = activeScript->GetPreviousActionBehind(currentTime);
        }
        auto nextAction = activeScript->GetNextActionAhead(currentTime);
        if (previousAction != nullptr && nextAction == previousAction) {
            nextAction = activeScript->GetNextActionAhead(previousAction->atS);
        }

        if (previousAction != nullptr) {
            if (previousAction->pos > 0 && previousAction->pos < 100) {
                auto indicator1 =
                    barP2
                    + (direction * distance * (previousAction->pos / 100.f))
                    - (perpendicular * (state.Width / 2.f))
                    + (perpendicular * (state.BorderWidth / 2.f));
                auto indicator2 =
                    barP2
                    + (direction * distance * (previousAction->pos / 100.f))
                    + (perpendicular * (state.Width / 2.f))
                    - (perpendicular * (state.BorderWidth / 2.f));
                auto indicatorCenter = barP2 + (direction * distance * (previousAction->pos / 100.f));
                frontDraw->AddLine(
                    indicator1,
                    indicator2,
                    GetColor(state.Indicator, state.GlobalOpacity),
                    state.LineWidth
                );
                stbsp_snprintf(tmp, sizeof(tmp), "%d", previousAction->pos);
                auto textOffset = ImGui::CalcTextSize(tmp);
                textOffset /= 2.f;
                frontDraw->AddText(indicatorCenter - textOffset, GetColor(state.Text, state.GlobalOpacity), tmp);
            }
        }
        if (nextAction != nullptr) {
            if (nextAction->pos > 0 && nextAction->pos < 100) {
                auto indicator1 =
                    barP2
                    + (direction * distance * (nextAction->pos / 100.f))
                    - (perpendicular * (state.Width / 2.f))
                    + (perpendicular * (state.BorderWidth / 2.f));
                auto indicator2 =
                    barP2
                    + (direction * distance * (nextAction->pos / 100.f))
                    + (perpendicular * (state.Width / 2.f))
                    - (perpendicular * (state.BorderWidth / 2.f));
                auto indicatorCenter = barP2 + (direction * distance * (nextAction->pos / 100.f));
                frontDraw->AddLine(
                    indicator1,
                    indicator2,
                    GetColor(state.Indicator, state.GlobalOpacity),
                    state.LineWidth
                );
                stbsp_snprintf(tmp, sizeof(tmp), "%d", nextAction->pos);
                auto textOffset = ImGui::CalcTextSize(tmp);
                textOffset /= 2.f;
                frontDraw->AddText(indicatorCenter - textOffset, GetColor(state.Text, state.GlobalOpacity), tmp);
            }
        }
    }

    // TEXT
    if (state.EnablePosition) {
        stbsp_snprintf(tmp, sizeof(tmp), "%.0f", currentPos);
        ImGui::PushFont(OFS_DynFontAtlas::DefaultFont2);
        auto textOffset = ImGui::CalcTextSize(tmp);
        textOffset /= 2.f;
        frontDraw->AddText(
            barP2 + direction * distance * 0.5f - textOffset,
            GetColor(state.Text, state.GlobalOpacity),
            tmp
        );
        ImGui::PopFont();
    }

    if (!state.LockedPosition)
    {
        auto barCenter = barP2 + (direction * (distance / 2.f));

        constexpr bool ShowMovementHandle = false;
        if constexpr (ShowMovementHandle) {
            frontDraw->AddCircle(barP1, state.Width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            frontDraw->AddCircle(barP2, state.Width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            frontDraw->AddCircle(barCenter, state.Width / 2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
        }

        auto mouse = ImGui::GetMousePos();
        float p1Distance = Distance(mouse, barP1);
        float p2Distance = Distance(mouse, barP2);
        float barCenterDistance = Distance(mouse, barCenter);

        if (p1Distance <= (state.Width / 2.f)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            // FIXME
            //g->HoveredWindow = window;
            //g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = state.P1;
                dragging = &state.P1;
            }
        }
        else if (p2Distance <= (state.Width/2.f)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            // FIXME
            //g->HoveredWindow = window;
            //g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = state.P2;
                dragging = &state.P2;
            }
        }
        else if (barCenterDistance <= (state.Width / 2.f)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            // FIXME
            //g->HoveredWindow = window;
            //g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = state.P1;
                startDragP2 = state.P2;
                IsMovingSimulator = true;
            }
        }

        if (dragging != nullptr) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                *dragging = startDragP1 + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            }
            if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { dragging = nullptr; }
        }
        else if (IsMovingSimulator) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                state.P1 = startDragP1 + delta;
                state.P2 = startDragP2 + delta;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { IsMovingSimulator = false; }
        }
    }
    else { dragging = nullptr; IsMovingSimulator = false; }
    ImGui::End();
}
