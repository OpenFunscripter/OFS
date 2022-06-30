#pragma once
#include "OFS_Lua.h"

#include "api/OFS_LuaImGuiAPI.h"
#include "api/OFS_LuaScriptAPI.h"
#include "api/OFS_LuaPlayerAPI.h"
#include "api/OFS_LuaProcessAPI.h"

#include <memory>

class OFS_ExtensionAPI
{
    public: 
    static constexpr const char* DefaultNamespace = "ofs";
    static constexpr const char* PlayerNamespace = "player";
    static constexpr uint32_t VersionAPI = 1;

    std::unique_ptr<OFS_ImGuiAPI> guiAPI;
    std::unique_ptr<OFS_ProcessAPI> procAPI;
    std::unique_ptr<OFS_PlayerAPI> playerAPI;
    std::unique_ptr<OFS_ScriptAPI> scriptAPI;

    OFS_ExtensionAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept;
    ~OFS_ExtensionAPI() noexcept;
};
