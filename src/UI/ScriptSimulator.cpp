#include "ScriptSimulator.h"

#include "OpenFunscripter.h"
#include "imgui.h"
#include "imgui_internal.h"

void ScriptSimulator::ShowSimulator(bool* open)
{
    if (*open) {
        auto ptr = OpenFunscripter::ptr;

        ImGui::Begin("Simulator", open, ImGuiWindowFlags_None 
            /*| ImGuiWindowFlags_NoBackground */
            /*| ImGuiWindowFlags_NoDecoration*/
            | ImGuiWindowFlags_NoDocking);
        
        //auto draw_list = ImGui::GetWindowDrawList();
        //auto front_draw = ImGui::GetForegroundDrawList();

        //ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        //ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        //auto& style = ImGui::GetStyle();
        //const ImGuiID itemID = ImGui::GetID("##Simulator");
        //ImRect itemBB(canvas_pos, canvas_pos + canvas_size);
        //ImGui::ItemAdd(itemBB, itemID);
        //
        //

        //auto windowPos = ImGui::GetWindowPos();
        //front_draw->AddQuadFilled(
        //        p1 - ImVec2(width / 2.f, 0.f) + windowPos, p1 + ImVec2(width / 2.f, 0.f) + windowPos,
        //        p2 + ImVec2(width / 2.f, 0.f) + windowPos, p2 - ImVec2(width / 2.f, 0.f) + windowPos,
        //        IM_COL32(255, 0, 0, 255)
        //    );

        const FunscriptAction* scriptAction = nullptr;
        int pos = ptr->LoadedFunscript->GetPositionAtTime(ptr->player.getCurrentPositionMs());
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::VSliderInt("", ImGui::GetContentRegionAvail(), &pos, 0, 100);
        ImGui::PopItemFlag();
        ImGui::End();
    }
}
