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
    CenterSimulator();
}

void ScriptSimulator::CenterSimulator()
{
    const float default_len = width * 3.f;
    auto Size = ImGui::GetMainViewport()->Size;
    p1 = (Size / 2.f);
    p1.y -= default_len/2.f;
    p1.x -=  (width / 2.f);
    p2 = p1 + ImVec2(0.f, default_len);
}

void ScriptSimulator::ShowSimulator(bool* open)
{
    if (*open) {
        auto ptr = OpenFunscripter::ptr;

        ImGui::Begin("Simulator", open, ImGuiWindowFlags_None 
            /*| ImGuiWindowFlags_NoBackground */
            /*| ImGuiWindowFlags_NoDecoration*/);
        
        auto draw_list = ImGui::GetWindowDrawList();
        auto front_draw = ImGui::GetForegroundDrawList();
        ImGuiContext* g = ImGui::GetCurrentContext();
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        auto& style = ImGui::GetStyle();
        const ImGuiID itemID = ImGui::GetID("##Simulator");
        
        int currentPos = ptr->LoadedFunscript->GetPositionAtTime(ptr->player.getCurrentPositionMs());

        auto offset = window->ViewportPos; 
        
        ImVec2 direction = p1 - p2;
        direction = Normalize(direction);
        ImVec2 barP1 = offset + p1 - (direction * (borderSize / 2.f));
        ImVec2 barP2 = offset + p2 + (direction * (borderSize / 2.f));
        float distance = Distance(barP1, barP2);
        auto perpendicular = Normalize(p1 -  p2);
        perpendicular = ImVec2(-perpendicular.y, perpendicular.x);

        // FRONT BAR
        float percent = currentPos / 100.f;
        front_draw->AddLine(
            barP2 + ((direction * distance)*percent),
            barP2,
            frontColor,
            width - borderSize
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
                    auto indicator1 = barP2 + (direction * distance * (previousAction->pos / 100.f)) - (perpendicular * (width / 2.f));
                    auto indicator2 = barP2 + (direction * distance * (previousAction->pos / 100.f)) + (perpendicular * (width / 2.f));
                    front_draw->AddLine(
                        indicator1,
                        indicator2,
                        ImGui::ColorConvertFloat4ToU32(indicatorColor),
                        borderSize/2.f
                    );
                }
            }
            if (nextAction != nullptr) {
                if (nextAction->pos > 0 && nextAction->pos < 100) {
                    auto indicator1 = barP2 + (direction * distance * (nextAction->pos / 100.f)) - (perpendicular * (width / 2.f));
                    auto indicator2 = barP2 + (direction * distance * (nextAction->pos / 100.f)) + (perpendicular * (width / 2.f));
                    front_draw->AddLine(
                        indicator1,
                        indicator2,
                        ImGui::ColorConvertFloat4ToU32(indicatorColor),
                        borderSize / 2.f
                    );
                }
            }
        }

        // BORDER
        auto borderOffset = perpendicular * (width / 2.f);
        front_draw->AddQuad(
            offset + p1 - borderOffset, offset + p1 + borderOffset,
            offset + p2 + borderOffset, offset + p2 - borderOffset,
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderSize
        );


        // TEXT
        char tmp[4];
        stbsp_snprintf(tmp, sizeof(tmp), "%d", currentPos);
        ImGui::PushFont(OpenFunscripter::BiggerFont);
        auto textOffset = ImGui::CalcTextSize(tmp);
        textOffset /= 2.f;
        textOffset.y = 0.f;
        front_draw->AddText(
            barP2 + direction * distance * 0.5f - textOffset,
            ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
            tmp
        );
        ImGui::PopFont();

        auto barCenter = barP2 + (direction * (distance / 2.f));

        if (ShowMovementHandle) {
            front_draw->AddCircle(barP1, width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            front_draw->AddCircle(barP2, width/2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
            front_draw->AddCircle(barCenter, width / 2.f, IM_COL32(255, 0, 0, 255), 0, 5.f);
        }

        auto mouse = ImGui::GetMousePos();
        float p1Distance = Distance(mouse, barP1);
        float p2Distance = Distance(mouse, barP2);
        float barCenterDistance = Distance(mouse, barCenter);

        if (p1Distance <= (width / 2.f)) {
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = p1;
                dragging = &p1;
            }
        }
        else if (p2Distance <= (width/2.f)) {
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = p2;
                dragging = &p2;
            }
        }
        else if (barCenterDistance <= (width / 2.f)) {
            g->HoveredWindow = window;
            g->HoveredDockNode = window->DockNode;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                startDragP1 = p1;
                startDragP2 = p2;
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
                p1 = startDragP1 + delta;
                p2 = startDragP2 + delta;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { movingBar = false; }
        }

        if (ImGui::Button("Center simulator", ImVec2(-1.f, 0.f))) { CenterSimulator(); }
        ImGui::ColorEdit4("Border", &borderColor.Value.x);
        ImGui::ColorEdit4("Front", &frontColor.Value.x);
        ImGui::ColorEdit4("Indicator", &indicatorColor.Value.x);
        ImGui::DragFloat("Width", &width);
        ImGui::DragFloat("Border", &borderSize);
        ImGui::Checkbox("Indicators", &EnableIndicators);
        
        borderSize = Util::Clamp<float>(borderSize, 0.f, 1000.f);
        width = Util::Clamp<float>(width, 0.f, 1000.f);
        ImGui::End();
    }
}
