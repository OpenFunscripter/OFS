#include "OFS_LuaExtensionAPI.h"
#include "OFS_Util.h"
#include "OFS_LuaExtensions.h"

#include <sstream>

constexpr const char* LuaDefaultFunctions = R"(
binding = {}
function clamp(val, min, max)
	return math.min(max, math.max(val, min))
end
)";

static int LuaPrint(sol::variadic_args va) noexcept
{
	std::stringstream logMsg;

	OFS_LuaExtension* ext = sol::state_view(va.lua_state())[OFS_LuaExtensions::GlobalExtensionPtr];
	logMsg << '[' << ext->Name << "]: ";
	for(auto arg : va) {
		auto str = lua_tostring(va.lua_state(), arg.stack_index());
		if(str) {
			logMsg << str;
			logMsg << ' '; 
		}
		else {
			luaL_error(va.lua_state(), "Type can't be turned into a string.");
			return 0;
		}
	}
	logMsg << '\n';
	auto finalMsg = logMsg.str();
	OFS_LuaExtensions::ExtensionLogBuffer.AddLog(finalMsg.c_str());
	return 0;
}

OFS_ExtensionAPI::OFS_ExtensionAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept
{
	auto L = sol::state_view(ofs.lua_state());
    guiAPI = std::make_unique<OFS_ImGuiAPI>(ofs);
	procAPI = std::make_unique<OFS_ProcessAPI>(ofs);
    scriptAPI = std::make_unique<OFS_ScriptAPI>(ofs);
    playerAPI = std::make_unique<OFS_PlayerAPI>(L);

	L.set_function("print", LuaPrint);

    int status = luaL_dostring(L, LuaDefaultFunctions);
	FUN_ASSERT(status == 0, "defaults failed");
}

OFS_ExtensionAPI::~OFS_ExtensionAPI() noexcept
{

}
