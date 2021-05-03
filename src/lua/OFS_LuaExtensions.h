#pragma once
#include "OFS_Lua.h"

#include <queue>
#include <string>

struct OFS_LuaTask
{
	std::string Function;
};

class OFS_LuaExtensions
{
private:
	void RunTask(OFS_LuaTask& task) noexcept;
public:
	lua_State* L = nullptr;
	static constexpr const char* ExtensionDir = "extensions";

	// tables/fields
	static constexpr const char* GlobalActionMetaTable = "ActionMetaTable";
	static constexpr const char* ScriptIdxUserdata = "ScriptIdx";
	static constexpr const char* ScriptActionsField = "actions";

	// functions
	static constexpr const char* EntryPoint = "init";
	static constexpr const char* RenderGui = "gui";

	static bool InMainThread;

	std::queue<OFS_LuaTask> Tasks;
	
	OFS_LuaExtensions() noexcept;
	~OFS_LuaExtensions() noexcept;

	bool LoadExtension(const char* extensionPath) noexcept;
	void ShowExtensions(bool* open) noexcept;
};