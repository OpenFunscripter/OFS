#pragma once

#include "OFS_Lua.h"

#include <memory>

class OFS_PlayerAPI
{
    public:
    OFS_PlayerAPI() noexcept;
    ~OFS_PlayerAPI() noexcept;

    const char* bar() noexcept 
    {
        return "bar";
    }
};

class OFS_ExtensionAPI
{
    public:
    static constexpr const char* DefaultNamespace = "ofs";
    static constexpr const char* PlayerNamespace = "player";

    OFS_ExtensionAPI(lua_State* L) noexcept;
    ~OFS_ExtensionAPI() noexcept;
};