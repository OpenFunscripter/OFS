#include "OFS_ImGui.h"

#include "imgui_internal.h"

void OFS::ImageWithId(ImGuiID id, ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col) noexcept
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    if (border_col.w > 0.0f)
        bb.Max += ImVec2(2, 2);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return;

    if (border_col.w > 0.0f)
    {
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(border_col), 0.0f);
        window->DrawList->AddImage(user_texture_id, bb.Min + ImVec2(1, 1), bb.Max - ImVec2(1, 1), uv0, uv1, ImGui::GetColorU32(tint_col));
    }
    else
    {
        window->DrawList->AddImage(user_texture_id, bb.Min, bb.Max, uv0, uv1, ImGui::GetColorU32(tint_col));
    }
}

bool OFS::GamepadContextMenu() noexcept
{
    ImGuiWindow* window = GImGui->CurrentWindow;
    if (window->SkipItems)
        return false;
    ImGuiID id = window->DC.LastItemId; // If user hasn't passed an ID, we can use the LastItemID. Using LastItemID as a Popup ID won't conflict!
    IM_ASSERT(id != 0);                                                  // You cannot pass a NULL str_id if the last item has no identifier (e.g. a Text() item)
    if (ImGui::IsNavInputTest(ImGuiNavInput_Input, ImGuiInputReadMode_Pressed) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        ImGui::OpenPopupEx(id, ImGuiPopupFlags_None);
    return ImGui::BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
}
