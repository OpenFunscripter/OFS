#include "OFS_LuaExtensionApi.h"
#include "OFS_Util.h"
#include "OFS_LuaExtensions.h"

#include "EASTL/string.h"

constexpr const char* LuaDefaultFunctions = R"(
binding = {}
function clamp(val, min, max)
	return math.min(max, math.max(val, min))
end
)";

static int LuaPrint(sol::variadic_args va) noexcept
{
	eastl::string logMsg;
	logMsg.reserve(256);

	OFS_LuaExtension* ext = sol::state_view(va.lua_state())[OFS_LuaExtensions::GlobalExtensionPtr];
	logMsg.append_sprintf("[%s]: ", ext->Name.c_str());
	for(auto arg : va) {
		auto str = lua_tostring(va.lua_state(), arg.stack_index());
		if(str) {
			logMsg.append(str);
			logMsg.append(1, ' ');
		}
		else {
			luaL_error(va.lua_state(), "Type can't be turned into a string.");
			return 0;
		}
	}
	logMsg.append(1, '\n');
	OFS_LuaExtensions::ExtensionLogBuffer.AddLog(logMsg.c_str());
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
