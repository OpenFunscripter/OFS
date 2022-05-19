#include "OFS_LuaImGuiAPI.h"
#include "imgui.h"
#include "imgui_stdlib.h"

OFS_ImGuiAPI::OFS_ImGuiAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept
{
    ofs["Text"] = OFS_ImGuiAPI::Text;
    ofs["Button"] = OFS_ImGuiAPI::Button;
    ofs.set_function("Input", 
        sol::overload(
            OFS_ImGuiAPI::InputText,
            OFS_ImGuiAPI::InputIntWithoutStepSize,
            OFS_ImGuiAPI::InputInt,
            OFS_ImGuiAPI::InputNumberWithoutStepSize,
            OFS_ImGuiAPI::InputNumber
        )
    );
    ofs.set_function("Drag",
        sol::overload(
            OFS_ImGuiAPI::DragIntWithoutStepSize,
            OFS_ImGuiAPI::DragInt,
            OFS_ImGuiAPI::DragNumberWithoutStepSize,
            OFS_ImGuiAPI::DragNumber
        )
    );
    ofs.set_function("Slider",
        sol::overload(
            OFS_ImGuiAPI::SliderInt,
            OFS_ImGuiAPI::SliderNumber
        )
    );
    ofs["Checkbox"] = OFS_ImGuiAPI::Checkbox;
    ofs["Combo"] = OFS_ImGuiAPI::Combo;
    ofs["SameLine"] = OFS_ImGuiAPI::SameLine;
    ofs["Separator"] = OFS_ImGuiAPI::Separator;
    ofs["NewLine"] = OFS_ImGuiAPI::NewLine;
}

OFS_ImGuiAPI::~OFS_ImGuiAPI() noexcept
{

}

void OFS_ImGuiAPI::Text(const char* txt) noexcept
{
    ImGui::TextUnformatted(txt);
}

bool OFS_ImGuiAPI::Button(const char* txt) noexcept
{
    return ImGui::Button(txt);
}

std::tuple<lua_Number, bool> OFS_ImGuiAPI::DragNumber(const char* txt, lua_Number current, lua_Number stepSize) noexcept
{
    bool valueChanged = ImGui::DragScalar(txt, ImGuiDataType_Double, &current, stepSize);
    return std::make_tuple(current, valueChanged);
}

std::tuple<lua_Integer, bool> OFS_ImGuiAPI::DragInt(const char* txt, lua_Integer current, lua_Integer stepSize) noexcept
{
    bool valueChanged = ImGui::DragScalar(txt, ImGuiDataType_S64, &current, stepSize);
    return std::make_tuple(current, valueChanged);
}

std::tuple<std::string, bool> OFS_ImGuiAPI::InputText(const char* txt, std::string current) noexcept
{
    // FIXME: this function is a causing a std::string heap allocation everytime it's called (when size > 15)
    bool valueChanged = ImGui::InputText(txt, &current);
    return std::make_tuple(current, valueChanged);
}

std::tuple<lua_Integer, bool> OFS_ImGuiAPI::InputInt(const char* txt, lua_Integer current, lua_Integer stepSize) noexcept
{
    bool valueChanged = ImGui::InputScalar(txt, ImGuiDataType_S64, &current, &stepSize);
    return std::make_tuple(current, valueChanged);
}

std::tuple<lua_Number, bool> OFS_ImGuiAPI::InputNumber(const char* txt, lua_Number current, lua_Number stepSize) noexcept
{
    bool valueChanged = ImGui::InputDouble(txt, &current, stepSize);
    return std::make_tuple(current, valueChanged);
}

std::tuple<lua_Number, bool> OFS_ImGuiAPI::SliderNumber(const char* txt, lua_Number current, lua_Number min, lua_Number max) noexcept
{
    bool valueChanged = ImGui::SliderScalar(txt, ImGuiDataType_Double, &current, &min, &max);
    return std::make_tuple(current, valueChanged);
}

std::tuple<lua_Integer, bool> OFS_ImGuiAPI::SliderInt(const char* txt, lua_Integer current, lua_Integer min, lua_Integer max) noexcept
{
    bool valueChanged = ImGui::SliderScalar(txt, ImGuiDataType_S64, &current, &min, &max);
    return std::make_tuple(current, valueChanged);
}

std::tuple<bool, bool> OFS_ImGuiAPI::Checkbox(const char* txt, bool current) noexcept
{
    bool valueChanged = ImGui::Checkbox(txt, &current);
    return std::make_tuple(current, valueChanged);
}

std::tuple<lua_Integer, bool> OFS_ImGuiAPI::Combo(const char* txt, lua_Integer currentSelection, sol::table items) noexcept
{
    bool valueChanged = false;
    sol::reference currentItem = items.raw_get<sol::reference>(currentSelection);
    auto currentItemStack = sol::stack::push_pop(currentItem);   
    auto currentItemStr = lua_tostring(items.lua_state(), currentItemStack.idx);

    if(currentItemStr) {
        if(ImGui::BeginCombo(txt, currentItemStr)) {
            for(int i=1, size=items.size(); i <= size; i += 1) {
                sol::reference item = items[i];
                auto stackItem = sol::stack::push_pop(item);
                auto itemStr = lua_tostring(items.lua_state(), stackItem.idx);
                if(itemStr) {
                    if(ImGui::Selectable(itemStr, i == currentSelection)) {
                        currentSelection = i;
                        valueChanged = true;
                    }
                }
            }
            ImGui::EndCombo();
        }
    }
    else {
        luaL_error(items.lua_state(), "Something went wrong in ofs.Combo");
    }
    return std::make_tuple(currentSelection, valueChanged);
}

void OFS_ImGuiAPI::SameLine() noexcept
{
    ImGui::SameLine();
}

void OFS_ImGuiAPI::NewLine() noexcept
{
    ImGui::NewLine();
}

void OFS_ImGuiAPI::Separator() noexcept
{
    ImGui::Separator();
}