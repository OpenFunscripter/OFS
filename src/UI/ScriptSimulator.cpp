#include "ScriptSimulator.h"

#include "OpenFunscripter.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "stb_sprintf.h"

inline static float Distance(const ImVec2& p1, const ImVec2& p2) {
    ImVec2 diff = p1 - p2;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

inline static ImVec2 Normalize(const ImVec2& p) {
    auto mag = Distance(ImVec2(0.f, 0.f), p);
    return ImVec2(p.x / mag, p.y / mag);
}

void ScriptSimulator::setup()
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(SDL_MOUSEMOTION, EVENT_SYSTEM_BIND(this, &ScriptSimulator::MouseMovement));
    app->events->Subscribe(SDL_MOUSEBUTTONDOWN, EVENT_SYSTEM_BIND(this, &ScriptSimulator::MouseDown));
}

void ScriptSimulator::MouseMovement(SDL_Event& ev)
{
    SDL_MouseMotionEvent& motion = ev.motion;
    // there's alot of indirection here
    const auto& simP1 = simulator.P1;
    const auto& simP2 = simulator.P2;

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
    MouseBetweenSimulator = clamped == mouseValue;
    mouseValue = clamped;
    mouseValue = ((mouseValue - 0.f) / (1.f - 0.f)) * (1.f - -1.f) + -1.f;
}

void ScriptSimulator::MouseDown(SDL_Event& ev)
{
    auto& button = ev.button;
    auto modstate = SDL_GetModState();
    if (modstate & KMOD_SHIFT && button.button == SDL_BUTTON_LEFT) {
        if (MouseBetweenSimulator) {
            auto app = OpenFunscripter::ptr;
            app->script().undoSystem->Snapshot(StateType::ADD_EDIT_ACTION);
            app->scripting->addEditAction(FunscriptAction(app->player.getCurrentPositionMsInterp(), 50 + (50 * mouseValue)));
        }
    }
}


void ScriptSimulator::CenterSimulator()
{
    const float default_len = Util::Clamp(simulator.Width * 3.f, simulator.Width, 1000.f);
    auto Size = ImGui::GetMainViewport()->Size;
    simulator.P1 = (Size / 2.f);
    simulator.P1.y -= default_len/2.f;
    simulator.P1.x -=  (simulator.Width / 2.f);
    simulator.P2 = simulator.P1 + ImVec2(0.f, default_len);
}

void ScriptSimulator::ShowSimulator(bool* open) 
{
    if (*open) {
        auto app = OpenFunscripter::ptr;
        float currentPos = 0;

        if (positionOverride >= 0.f) {
            currentPos = positionOverride;
            positionOverride = -1.f;
        }
        else {
            currentPos = app->ActiveFunscript()->GetPositionAtTime(app->player.getCurrentPositionMsInterp(), app->scripting->Overlay()->SplineLines);
        }

        if (EnableVanilla) {
            ImGui::Begin(SimulatorId, open, 
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

        ImGui::Begin(SimulatorId, open, ImGuiWindowFlags_None);
        char tmp[4];
        auto draw_list = ImGui::GetWindowDrawList();
        auto front_draw = ImGui::GetForegroundDrawList();
        ImGuiContext* g = ImGui::GetCurrentContext();
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        auto& style = ImGui::GetStyle();
        const ImGuiID itemID = ImGui::GetID("##Simulator");
        

        ImGui::Columns(2, 0, false);
        if (ImGui::Button("Center", ImVec2(-1.f, 0.f))) { CenterSimulator(); }
        ImGui::NextColumn();
        if (ImGui::Button("Invert", ImVec2(-1.f, 0.f))) { 
            auto tmp = simulator.P1;
            simulator.P1 = simulator.P2;
            simulator.P2 = tmp; 
        }
        ImGui::Columns(1);
        if (ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_SpanAvailWidth)) {
            ImGui::ColorEdit4("Text", &simulator.Text.Value.x);
            ImGui::ColorEdit4("Border", &simulator.Border.Value.x);
            ImGui::ColorEdit4("Front", &simulator.Front.Value.x);
            ImGui::ColorEdit4("Back", &simulator.Back.Value.x);
            ImGui::ColorEdit4("Indicator", &simulator.Indicator.Value.x);
            ImGui::ColorEdit4("Lines", &simulator.ExtraLines.Value.x);

            if (ImGui::DragFloat("Width", &simulator.Width)) {
                simulator.Width = Util::Clamp<float>(simulator.Width, 0.f, 1000.f);
            }
            if (ImGui::DragFloat("Border", &simulator.BorderWidth, 0.5f)) {
                simulator.BorderWidth = Util::Clamp<float>(simulator.BorderWidth, 0.f, 1000.f);
            }
            ImGui::DragFloat("Line", &simulator.LineWidth, 0.5f);
            if (ImGui::SliderFloat("Opacity", &simulator.GlobalOpacity, 0.f, 1.f)) {
                simulator.GlobalOpacity = Util::Clamp<float>(simulator.GlobalOpacity, 0.f, 1.f);
            }

            if (ImGui::DragFloat("Lines2", &simulator.ExtraLineWidth, 0.5f)) {
                simulator.ExtraLineWidth = Util::Clamp<float>(simulator.ExtraLineWidth, 0.5f, 1000.f);
            }

            ImGui::Checkbox("Indicators", &simulator.EnableIndicators);
            ImGui::SameLine(); 
            ImGui::Checkbox("Lines", &simulator.EnableHeightLines);
            if (ImGui::InputInt("Extra lines", &simulator.ExtraLinesCount, 1, 2)) {
                simulator.ExtraLinesCount = Util::Clamp(simulator.ExtraLinesCount, 0, 10);
            }
            ImGui::Checkbox("Show position", &simulator.EnablePosition);
        }

        if (ImGui::Button("Vanilla")) {
            EnableVanilla = true;
        }
        Util::Tooltip("Switch to vanilla simulator.");

        // Because the simulator is always drawn on top
        // we don't draw if there is a popup modal
        // as that would be irritating
        if (ImGui::GetTopMostPopupModal() != nullptr) {
            ImGui::End();
            return;
        }

        auto offset = window->ViewportPos; 
        
        ImVec2 direction = simulator.P1 - simulator.P2;
        direction = Normalize(direction);
        ImVec2 barP1 = offset + simulator.P1 - (direction * (simulator.BorderWidth / 2.f));
        ImVec2 barP2 = offset + simulator.P2 + (direction * (simulator.BorderWidth / 2.f));
        float distance = Distance(barP1, barP2);
        auto perpendicular = Normalize(simulator.P1 - simulator.P2);
        perpendicular = ImVec2(-perpendicular.y, perpendicular.x);

        // BACKGROUND
        front_draw->AddLine(
            barP1 + direction,
            barP2 - direction,
            GetColor(simulator.Back),
            simulator.Width - simulator.BorderWidth + 1.f
        );

        // FRONT BAR
        float percent = currentPos / 100.f;
        front_draw->AddLine(
            barP2 + ((direction * distance)*percent),
            barP2,
            GetColor(simulator.Front),
            simulator.Width - simulator.BorderWidth + 1.f
        );

        // BORDER
        if (simulator.BorderWidth > 0.f) {
            auto borderOffset = perpendicular * (simulator.Width / 2.f);
            front_draw->AddQuad(
                offset + simulator.P1 - borderOffset, offset + simulator.P1 + borderOffset,
                offset + simulator.P2 + borderOffset, offset + simulator.P2 - borderOffset,
                GetColor(simulator.Border),
                simulator.BorderWidth
            );
        }

        // HEIGHT LINES
        if (simulator.EnableHeightLines) {
            for (int i = 1; i < 10; i++) {
                float pos = i * 10.f;
                auto indicator1 =
                    barP2
                    + (direction * distance * (pos / 100.f))
                    - (perpendicular * (simulator.Width / 2.f))
                    + (perpendicular * (simulator.BorderWidth / 2.f));
                auto indicator2 =
                    barP2
                    + (direction * distance * (pos / 100.f))
                    + (perpendicular * (simulator.Width / 2.f))
                    - (perpendicular * (simulator.BorderWidth / 2.f));
            
                front_draw->AddLine(
                    indicator1,
                    indicator2,
                    GetColor(simulator.ExtraLines),
                    simulator.LineWidth
                );
            }

        }
        if (simulator.ExtraLinesCount > 0) {
            // extra height lines
            for (int i = -simulator.ExtraLinesCount; i < 1; i++) {
                float pos = i * 10.f;
                auto indicator1 =
                    barP2
                    + (direction * distance * (pos / 100.f))
                    - (perpendicular * (simulator.Width / 2.f))
                    + (perpendicular * (simulator.BorderWidth / 2.f));
                auto indicator2 =
                    barP2
                    + (direction * distance * (pos / 100.f))
                    + (perpendicular * (simulator.Width / 2.f))
                    - (perpendicular * (simulator.BorderWidth / 2.f));

                front_draw->AddLine(
                    indicator1,
                    indicator2,
                    GetColor(simulator.ExtraLines),
                    simulator.ExtraLineWidth
                );
            }
            for (int i = 10; i < (11+simulator.ExtraLinesCount); i++) {
                float pos = i * 10.f;
                auto indicator1 =
                    barP2
                    + (direction * distance * (pos / 100.f))
                    - (perpendicular * (simulator.Width / 2.f))
                    + (perpendicular * (simulator.BorderWidth / 2.f));
                auto indicator2 =
                    barP2
                    + (direction * distance * (pos / 100.f))
                    + (perpendicular * (simulator.Width / 2.f))
                    - (perpendicular * (simulator.BorderWidth / 2.f));

                front_draw->AddLine(
                    indicator1,
                    indicator2,
                    GetColor(simulator.ExtraLines),
                    simulator.ExtraLineWidth
                );
            }
        }

        // INDICATORS
        if (simulator.EnableIndicators) {
            auto previousAction = app->ActiveFunscript()->GetActionAtTime(app->player.getCurrentPositionMs(), app->player.getFrameTimeMs());
            if (previousAction == nullptr) {
                previousAction = app->ActiveFunscript()->GetPreviousActionBehind(app->player.getCurrentPositionMs());
            }
            auto nextAction = app->ActiveFunscript()->GetNextActionAhead(app->player.getCurrentPositionMs());
            if (previousAction != nullptr && nextAction == previousAction)
            {
                nextAction = app->ActiveFunscript()->GetNextActionAhead(previousAction->at);
            }

            if (previousAction != nullptr) {
                if (previousAction->pos > 0 && previousAction->pos < 100) {
                    auto indicator1 =
                        barP2
                        + (direction * distance * (previousAction->pos / 100.f))
                        - (perpendicular * (simulator.Width / 2.f))
                        + (perpendicular * (simulator.BorderWidth / 2.f));
                    auto indicator2 =
                        barP2
                        + (direction * distance * (previousAction->pos / 100.f))
                        + (perpendicular * (simulator.Width / 2.f))
                        - (perpendicular * (simulator.BorderWidth / 2.f));
                    auto indicatorCenter = barP2 + (direction * distance * (previousAction->pos / 100.f));
                    front_draw->AddLine(
                        indicator1,
                        indicator2,
                        GetColor(simulator.Indicator),
                        simulator.LineWidth
                    );
                    stbsp_snprintf(tmp, sizeof(tmp), "%d", previousAction->pos);
                    auto textOffset = ImGui::CalcTextSize(tmp);
                    textOffset /= 2.f;
                    front_draw->AddText(indicatorCenter - textOffset, GetColor(simulator.Text), tmp);
                }
            }
            if (nextAction != nullptr) {
                if (nextAction->pos > 0 && nextAction->pos < 100) {
                    auto indicator1 =
                        barP2
                        + (direction * distance * (nextAction->pos / 100.f))
                        - (perpendicular * (simulator.Width / 2.f))
                        + (perpendicular * (simulator.BorderWidth / 2.f));
                    auto indicator2 =
                        barP2
                        + (direction * distance * (nextAction->pos / 100.f))
                        + (perpendicular * (simulator.Width / 2.f))
                        - (perpendicular * (simulator.BorderWidth / 2.f));
                    auto indicatorCenter = barP2 + (direction * distance * (nextAction->pos / 100.f));
                    front_draw->AddLine(
                        indicator1,
                        indicator2,
                        GetColor(simulator.Indicator),
                        simulator.LineWidth
                    );
                    stbsp_snprintf(tmp, sizeof(tmp), "%d", nextAction->pos);
                    auto textOffset = ImGui::CalcTextSize(tmp);
                    textOffset /= 2.f;
                    front_draw->AddText(indicatorCenter - textOffset, GetColor(simulator.Text), tmp);
                }
            }
        }


        // TEXT
        if (simulator.EnablePosition) {
            stbsp_snprintf(tmp, sizeof(tmp), "%.0f", currentPos);
            ImGui::PushFont(OpenFunscripter::DefaultFont2);
            auto textOffset = ImGui::CalcTextSize(tmp);
            textOffset /= 2.f;
            front_draw->AddText(
                barP2 + direction * distance * 0.5f - textOffset,
                GetColor(simulator.Text),
                tmp
            );
            ImGui::PopFont();
        }

        auto barCenter = barP2 + (direction * (distance / 2.f));

        constexpr bool ShowMovementHandle = false;
        if constexpr (ShowMovementHandle) {
            front_draw->AddCircle(barP1, simulator.Width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            front_draw->AddCircle(barP2, simulator.Width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            front_draw->AddCircle(barCenter, simulator.Width / 2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
        }

        auto mouse = ImGui::GetMousePos();
        float p1Distance = Distance(mouse, barP1);
        float p2Distance = Distance(mouse, barP2);
        float barCenterDistance = Distance(mouse, barCenter);

        if (p1Distance <= (simulator.Width / 2.f)) {
            OpenFunscripter::SetCursorType(ImGuiMouseCursor_Hand);
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = simulator.P1;
                dragging = &simulator.P1;
            }
        }
        else if (p2Distance <= (simulator.Width/2.f)) {
            OpenFunscripter::SetCursorType(ImGuiMouseCursor_Hand);
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = simulator.P2;
                dragging = &simulator.P2;
            }
        }
        else if (barCenterDistance <= (simulator.Width / 2.f)) {
            OpenFunscripter::SetCursorType(ImGuiMouseCursor_ResizeAll);
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = simulator.P1;
                startDragP2 = simulator.P2;
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
                simulator.P1 = startDragP1 + delta;
                simulator.P2 = startDragP2 + delta;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { IsMovingSimulator = false; }
        }


        ImGui::End();
    }
}
