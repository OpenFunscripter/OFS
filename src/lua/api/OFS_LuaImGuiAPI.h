#pragma once
#include "OFS_Lua.h"

#include <string>
#include <tuple>

class OFS_ImGuiAPI
{
    private:
    uint32_t BeginEndDisableCounter = 0;
    std::string ErrorStr; 

    static void Text(const char* txt) noexcept;
    static bool Button(const char* txt) noexcept;
    static void Tooltip(const char* txt) noexcept;
    static void SameLine() noexcept;
    static void Separator() noexcept;
    static void NewLine() noexcept;
    void BeginDisabled(bool disable) noexcept;
    void EndDisabled() noexcept;

    static std::tuple<lua_Number, bool> DragNumberWithoutStepSize(const char* txt, lua_Number current) noexcept
    { return DragNumber(txt, current, 1.0); }
    static std::tuple<lua_Number, bool> DragNumber(const char* txt, lua_Number current, lua_Number stepSize) noexcept;

    static std::tuple<lua_Integer, bool> DragIntWithoutStepSize(const char* txt, lua_Integer current) noexcept
    { return DragInt(txt, current, 1.0); }
    static std::tuple<lua_Integer, bool> DragInt(const char* txt, lua_Integer current, lua_Integer stepSize) noexcept;

    static std::tuple<std::string, bool> InputText(const char* txt, std::string current) noexcept;

    static std::tuple<lua_Integer, bool> InputIntWithoutStepSize(const char* txt, lua_Integer current) noexcept
    { return InputInt(txt, current, 1); }
    static std::tuple<lua_Integer, bool> InputInt(const char* txt, lua_Integer current, lua_Integer stepSize) noexcept;

    static std::tuple<lua_Number, bool> InputNumberWithoutStepSize(const char* txt, lua_Number current) noexcept 
    { return InputNumber(txt, current, 0.0); }
    static std::tuple<lua_Number, bool> InputNumber(const char* txt, lua_Number current, lua_Number stepSize) noexcept;

    static std::tuple<lua_Number, bool> SliderNumber(const char* txt, lua_Number current, lua_Number min, lua_Number max) noexcept;
    static std::tuple<lua_Integer, bool> SliderInt(const char* txt, lua_Integer current, lua_Integer min, lua_Integer max) noexcept;

    static std::tuple<bool, bool> Checkbox(const char* txt, bool current) noexcept;
    static std::tuple<lua_Integer, bool> Combo(const char* txt, lua_Integer currentSelection, sol::table items) noexcept;
    static bool CollapsingHeader(const char* txt) noexcept;

    public:
    OFS_ImGuiAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept;
    ~OFS_ImGuiAPI() noexcept;

    bool Validate() noexcept;
    const std::string& Error() const noexcept { return ErrorStr; }
    void TrySafe() noexcept;
};