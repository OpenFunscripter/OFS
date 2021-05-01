#pragma once
#include "OFS_Lua.h"

#include <string>

class OFS_LuaExtensions
{
public:
	lua_State* L = nullptr;

	static constexpr const char* ExtensionDir = "extensions";
	static constexpr const char* EntryPoint = "init";
	static constexpr const char* RenderGui = "gui";
	
	OFS_LuaExtensions() noexcept;
	~OFS_LuaExtensions() noexcept;

	bool LoadExtension(const char* extensionPath) noexcept;
	void ShowExtensions(bool* open) noexcept;
};