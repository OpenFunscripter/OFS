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
            OFS_ImGuiAPI::InputNumberWithoutStepSize,
            OFS_ImGuiAPI::InputNumber
        )
    );

    ofs.set_function("InputInt", 
        sol::overload(
            OFS_ImGuiAPI::InputInt,
            OFS_ImGuiAPI::InputIntWithoutStepSize
        )
    );

    ofs.set_function("Drag",
        sol::overload(
            OFS_ImGuiAPI::DragNumberWithoutStepSize,
            OFS_ImGuiAPI::DragNumber
        )
    );
    ofs.set_function("DragInt",
        sol::overload(
            OFS_ImGuiAPI::DragIntWithoutStepSize,
            OFS_ImGuiAPI::DragInt
        )
    );

    ofs["Slider"] = OFS_ImGuiAPI::SliderNumber;
    ofs["SliderInt"] = OFS_ImGuiAPI::SliderInt;

    ofs["Checkbox"] = OFS_ImGuiAPI::Checkbox;
    ofs["Combo"] = OFS_ImGuiAPI::Combo;
    ofs["SameLine"] = OFS_ImGuiAPI::SameLine;
    ofs["Separator"] = OFS_ImGuiAPI::Separator;
    ofs["NewLine"] = OFS_ImGuiAPI::NewLine;
    ofs["Tooltip"] = OFS_ImGuiAPI::Tooltip;
    ofs["CollapsingHeader"] = OFS_ImGuiAPI::CollapsingHeader;
    ofs["BeginDisabled"] = [this](bool disabled) noexcept {return this->BeginDisabled(disabled); };
    ofs["EndDisabled"] = [this]() noexcept {return this->EndDisabled(); };
}

OFS_ImGuiAPI::~OFS_ImGuiAPI() noexcept
{

}

bool OFS_ImGuiAPI::Validate() noexcept
{
    // if this returns false the api was used incorrectly
    bool valid = BeginEndDisableCounter == 0;
    if(!valid) ErrorStr = "Wrong use of BeginDisabled/EndDisabled";
	if(!valid) TrySafe(); // try prevent crash
    return valid;
}

void OFS_ImGuiAPI::TrySafe() noexcept
{
    // try to safe ofs from crashing due to wrong api use
    while(this->BeginEndDisableCounter > 0) {
        ImGui::EndDisabled();
        this->BeginEndDisableCounter -= 1;
    }
}

void OFS_ImGuiAPI::BeginDisabled(bool disable) noexcept
{
    this->BeginEndDisableCounter += 1;
    ImGui::BeginDisabled(disable);
}

void OFS_ImGuiAPI::EndDisabled() noexcept
{
    if(this->BeginEndDisableCounter > 0) {
        this->BeginEndDisableCounter -= 1;
        ImGui::EndDisabled();
    }
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
    auto currentItemStr = lua_tostring(items.lua_state(), currentItemStack.m_index);

    if(currentItemStr) {
        if(ImGui::BeginCombo(txt, currentItemStr)) {
            for(int i=1, size=items.size(); i <= size; i += 1) {
                sol::reference item = items[i];
                auto stackItem = sol::stack::push_pop(item);
                auto itemStr = lua_tostring(items.lua_state(), stackItem.m_index);
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

void OFS_ImGuiAPI::Tooltip(const char* txt) noexcept
{
    if(ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(txt);
        ImGui::EndTooltip();
    }
}

bool OFS_ImGuiAPI::CollapsingHeader(const char* txt) noexcept
{
    return ImGui::CollapsingHeader(txt, ImGuiTreeNodeFlags_None);
}