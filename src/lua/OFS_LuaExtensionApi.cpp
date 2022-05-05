#include "OFS_LuaExtensionApi.h"
#include "OFS_Util.h"
#include "LuaBridge/LuaBridge.h"


OFS_PlayerAPI::OFS_PlayerAPI() noexcept
{
    //luabridge::getGlobalNamespace(L)
    //    .beginNamespace(OFS_ExtensionAPI::DefaultNamespace)
    //    .endNamespace();
    LOG_DEBUG("foo");    
}

OFS_PlayerAPI::~OFS_PlayerAPI() noexcept
{

}


OFS_ExtensionAPI::~OFS_ExtensionAPI() noexcept
{

}

OFS_ExtensionAPI::OFS_ExtensionAPI(lua_State* L) noexcept
{
    static int foo = 420;

    luabridge::getGlobalNamespace(L)
        .beginClass<OFS_PlayerAPI>(PlayerNamespace)
        .addConstructor<void(*)(void)>()
        .addStaticProperty("foo", &foo)
        .addFunction("bar", &OFS_PlayerAPI::bar)
        .endClass();


    luabridge::getGlobalNamespace(L)
        .beginNamespace(OFS_ExtensionAPI::PlayerNamespace)
        .endNamespace();
}
