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
        auto ptr = OpenFunscripter::ptr;
        if (EnableVanilla) {
            ImGui::Begin("Simulator", open, 
                ImGuiWindowFlags_NoBackground
                | ImGuiWindowFlags_NoDocking);
            int pos = ptr->LoadedFunscript->GetPositionAtTime(ptr->player.getCurrentPositionMs());
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::VSliderInt("", ImGui::GetContentRegionAvail(), &pos, 0, 100);
            ImGui::PopItemFlag();
            ImGui::End();
            if (!*open) {
                EnableVanilla = false;
                *open = true;
            }
            return;
        }

        ImGui::Begin("Simulator", open, ImGuiWindowFlags_None);
        char tmp[4];
        auto draw_list = ImGui::GetWindowDrawList();
        auto front_draw = ImGui::GetForegroundDrawList();
        ImGuiContext* g = ImGui::GetCurrentContext();
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        auto& style = ImGui::GetStyle();
        const ImGuiID itemID = ImGui::GetID("##Simulator");
        

        if (ImGui::Button("Center simulator", ImVec2(-1.f, 0.f))) { CenterSimulator(); }
        if (ImGui::Button("Invert", ImVec2(-1.f, 0.f))) { 
            auto tmp = simulator.P1;
            simulator.P1 = simulator.P2;
            simulator.P2 = tmp; 
        }
        ImGui::ColorEdit4("Text", &simulator.Text.Value.x);
        ImGui::ColorEdit4("Border", &simulator.Border.Value.x);
        ImGui::ColorEdit4("Front", &simulator.Front.Value.x);
        ImGui::ColorEdit4("Indicator", &simulator.Indicator.Value.x);
        ImGui::DragFloat("Width", &simulator.Width);
        ImGui::DragFloat("Border", &simulator.BorderWidth);
        ImGui::Checkbox("Indicators", &EnableIndicators);
        ImGui::SameLine(); ImGui::Checkbox("Vanilla", &EnableVanilla);
        Util::Tooltip("Close window to go back");
        simulator.BorderWidth = Util::Clamp<float>(simulator.BorderWidth, 0.f, 1000.f);
        simulator.Width = Util::Clamp<float>(simulator.Width, 0.f, 1000.f);

        // Because the simulator is always drawn on top
        // we don't draw if there is a popup modal
        // as that would be irritating
        if (ImGui::GetTopMostPopupModal() != nullptr) {
            ImGui::End();
            return;
        }

        int currentPos = ptr->LoadedFunscript->GetPositionAtTime(ptr->player.getCurrentPositionMs());

        auto offset = window->ViewportPos; 
        
        ImVec2 direction = simulator.P1 - simulator.P2;
        direction = Normalize(direction);
        ImVec2 barP1 = offset + simulator.P1 - (direction * (simulator.BorderWidth / 2.f));
        ImVec2 barP2 = offset + simulator.P2 + (direction * (simulator.BorderWidth / 2.f));
        float distance = Distance(barP1, barP2);
        auto perpendicular = Normalize(simulator.P1 - simulator.P2);
        perpendicular = ImVec2(-perpendicular.y, perpendicular.x);

        // FRONT BAR
        float percent = currentPos / 100.f;
        front_draw->AddLine(
            barP2 + ((direction * distance)*percent),
            barP2,
            simulator.Front,
            simulator.Width - simulator.BorderWidth
        );

        // INDICATORS
        if (EnableIndicators) {
            auto app = OpenFunscripter::ptr;
            auto previousAction = app->LoadedFunscript->GetActionAtTime(app->player.getCurrentPositionMs(), app->player.getFrameTimeMs());
            if (previousAction == nullptr) {
                previousAction = app->LoadedFunscript->GetPreviousActionBehind(app->player.getCurrentPositionMs());
            }
            auto nextAction = app->LoadedFunscript->GetNextActionAhead(app->player.getCurrentPositionMs());
            if (previousAction != nullptr && nextAction == previousAction)
            {
                nextAction = app->LoadedFunscript->GetNextActionAhead(previousAction->at);
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
                        ImGui::ColorConvertFloat4ToU32(simulator.Indicator),
                        simulator.BorderWidth/2.f
                    );
                    stbsp_snprintf(tmp, sizeof(tmp), "%d", previousAction->pos);
                    auto textOffset = ImGui::CalcTextSize(tmp);
                    textOffset /= 2.f;
                    front_draw->AddText(indicatorCenter - textOffset, ImGui::ColorConvertFloat4ToU32(simulator.Text), tmp);
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
                        ImGui::ColorConvertFloat4ToU32(simulator.Indicator),
                        simulator.BorderWidth / 2.f
                    );
                    stbsp_snprintf(tmp, sizeof(tmp), "%d", nextAction->pos);
                    auto textOffset = ImGui::CalcTextSize(tmp);
                    textOffset /= 2.f;
                    front_draw->AddText(indicatorCenter - textOffset, ImGui::ColorConvertFloat4ToU32(simulator.Text), tmp);
                }
            }
        }

        // BORDER
        auto borderOffset = perpendicular * (simulator.Width / 2.f);
        front_draw->AddQuad(
            offset + simulator.P1 - borderOffset, offset + simulator.P1 + borderOffset,
            offset + simulator.P2 + borderOffset, offset + simulator.P2 - borderOffset,
            ImGui::ColorConvertFloat4ToU32(simulator.Border),
            simulator.BorderWidth
        );


        // TEXT
        stbsp_snprintf(tmp, sizeof(tmp), "%d", currentPos);
        ImGui::PushFont(OpenFunscripter::DefaultFont2);
        auto textOffset = ImGui::CalcTextSize(tmp);
        textOffset /= 2.f;
        front_draw->AddText(
            barP2 + direction * distance * 0.5f - textOffset,
            ImGui::ColorConvertFloat4ToU32(simulator.Text),
            tmp
        );
        ImGui::PopFont();

        auto barCenter = barP2 + (direction * (distance / 2.f));

        if (ShowMovementHandle) {
            front_draw->AddCircle(barP1, simulator.Width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            front_draw->AddCircle(barP2, simulator.Width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            front_draw->AddCircle(barCenter, simulator.Width / 2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
        }

        auto mouse = ImGui::GetMousePos();
        float p1Distance = Distance(mouse, barP1);
        float p2Distance = Distance(mouse, barP2);
        float barCenterDistance = Distance(mouse, barCenter);

        if (p1Distance <= (simulator.Width / 2.f)) {
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = simulator.P1;
                dragging = &simulator.P1;
            }
        }
        else if (p2Distance <= (simulator.Width/2.f)) {
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = simulator.P2;
                dragging = &simulator.P2;
            }
        }
        else if (barCenterDistance <= (simulator.Width / 2.f)) {
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = simulator.P1;
                startDragP2 = simulator.P2;
                movingBar = true;
            }
        }

        if (dragging != nullptr) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                *dragging = startDragP1 + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            }
            if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { dragging = nullptr; }
        }
        else if (movingBar) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                simulator.P1 = startDragP1 + delta;
                simulator.P2 = startDragP2 + delta;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { movingBar = false; }
        }


        ImGui::End();
    }
}
