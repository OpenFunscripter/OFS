#include "OFS_LuaProcessAPI.h"
#include "OFS_LuaExtensionApi.h"

OFS_ProcessAPI::~OFS_ProcessAPI() noexcept
{

}

OFS_ProcessAPI::OFS_ProcessAPI(sol::usertype<OFS_ExtensionAPI>& ofs) noexcept
{
    sol::state_view Lua(ofs.lua_state());
    auto process = Lua.new_usertype<OFS_LuaProcess>("Process", 
        sol::factories<>(OFS_LuaProcess::CreateProcess));
    process["alive"] = &OFS_LuaProcess::IsAlive;
    process["join"] = &OFS_LuaProcess::Join;
    process["detach"] = &OFS_LuaProcess::Detach;
    process["kill"] = &OFS_LuaProcess::Shutdown;
}

std::unique_ptr<OFS_LuaProcess> OFS_LuaProcess::CreateProcess(const char* prog, sol::variadic_args va) noexcept
{
    const char** args = (const char**)alloca(sizeof(const char*) * (va.size() + 2));
    args[0] = prog;
    int index = 1;
    for(auto arg : va) {
        args[index] = lua_tostring(arg.lua_state(), arg.stack_index());
        if(!sol::stack::check<const char*>(arg.lua_state(), arg.stack_index())) {
            luaL_error(arg.lua_state(), "Provided argument can't be turned into a string.");
            return nullptr;
        }
        index += 1;
    }
    args[va.size() + 1] = nullptr;
    struct subprocess_s p{0};
    bool succ = subprocess_create(args, subprocess_option_inherit_environment | subprocess_option_no_window, &p) == 0;
    if(succ) {
        auto process = std::make_unique<OFS_LuaProcess>(p);
        return std::move(process);
    }
    return nullptr;
}