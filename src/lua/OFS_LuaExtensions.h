#pragma once
#include "OFS_Lua.h"

#include <queue>
#include <string>
#include <vector>
#include <filesystem>

#include "OFS_Reflection.h"

struct OFS_LuaTask
{
	lua_State* L = nullptr;
	std::string Function;
};

struct OFS_LuaExtension
{
	static constexpr const char* MainFile = "main.lua";
	uint32_t Hash;

	std::string Name;
	std::string Directory;

	lua_State* L = nullptr;

	bool Active = false;
	double MaxTime = 0.0;

	bool Load(const std::filesystem::path& directory) noexcept;
	void Shutdown() noexcept {
		if (L) {
			lua_close(L); L = 0;
		}
	}
	template<class Archive>
	void reflect(Archive& ar)
	{
		OFS_REFLECT(Name, ar);
		OFS_REFLECT(Directory, ar);
		OFS_REFLECT(Active, ar);
		Hash = Util::Hash(Directory.c_str(), Directory.size());
	}
};

class OFS_LuaExtensions
{
private:
	std::string LastConfigPath;
	void RemoveNonExisting() noexcept;
public:
	std::vector<OFS_LuaExtension> Extensions;
	void UpdateExtensionList() noexcept;

	static constexpr const char* ExtensionDir = "extensions";

	// tables/fields
	static constexpr const char* GlobalActionMetaTable = "ActionMetaTable";
	static constexpr const char* ScriptIdxUserdata = "ScriptIdx";
	static constexpr const char* ScriptActionsField = "actions";

	// functions
	static constexpr const char* InitFunction = "init";
	static constexpr const char* RenderGui = "gui";


	static bool InMainThread;
	std::queue<OFS_LuaTask> Tasks;
	
	OFS_LuaExtensions() noexcept;
	~OFS_LuaExtensions() noexcept;

	void load(const std::string& path) noexcept;
	void save() noexcept;

	void ShowExtensions(bool* open) noexcept;

	template<typename Archive>
	void reflect(Archive& ar)
	{
		OFS_REFLECT(Extensions, ar);
	}
};