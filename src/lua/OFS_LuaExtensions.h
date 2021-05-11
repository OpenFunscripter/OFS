#pragma once
#include "OFS_Lua.h"

#include <queue>
#include <string>
#include <vector>
#include <filesystem>

#include "OFS_Util.h"
#include "OFS_Reflection.h"

#include "EASTL/vector_set.h"

struct OFS_LuaTask
{
	lua_State* L = nullptr;
	std::string Function;
};

struct OFS_BindableLuaFunction
{
	std::string GlobalName;
	std::string Name;
	std::string Description;
};

struct OFS_BindableLuaFunctionLessOperator
{
	bool operator()(const OFS_BindableLuaFunction& a, const OFS_BindableLuaFunction& b) const noexcept
	{
		return a.GlobalName < b.GlobalName;
	}
};

struct OFS_LuaExtension
{
	static constexpr const char* MainFile = "main.lua";
	uint32_t Hash;

	std::string Name;
	std::string NameId;
	std::string Directory;

	lua_State* L = nullptr;

	bool Active = false;
	double MaxTime = 0.0;

	std::string ExtensionError;

	eastl::vector_set<OFS_BindableLuaFunction, OFS_BindableLuaFunctionLessOperator> Bindables;

	void Fail(const char* error) noexcept
	{
		ExtensionError = error;
		Shutdown();
	}

	bool Load(const std::filesystem::path& directory) noexcept;
	void Shutdown() noexcept {
		MaxTime = 0.0;
		Bindables.clear();
		if (L) {
			lua_close(L); L = 0;
		}
	}
	template<class Archive>
	void reflect(Archive& ar)
	{
		OFS_REFLECT(Name, ar);
		OFS_REFLECT(NameId, ar);
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
	static constexpr const char* DynamicBindingHandler = "OFS_LuaExtensions";
	static bool DevMode;
	std::vector<OFS_LuaExtension> Extensions;
	void UpdateExtensionList() noexcept;

	static constexpr const char* ExtensionDir = "extensions";

	// tables/fields
	static constexpr const char* DefaultNamespace = "ofs";
	static constexpr const char* PlayerNamespace = "player";

	static constexpr const char* GlobalExtensionPtr = "OFS_ExtensionPtr";
	static constexpr const char* GlobalActionMetaTable = "OFS_TmpActionMetaTable";
	static constexpr const char* ScriptIdxUserdata = "OFS_ScriptIdx";
	static constexpr const char* ScriptActionsField = "actions";

	// functions
	static constexpr const char* InitFunction = "init";
	static constexpr const char* UpdateFunction = "update";
	static constexpr const char* RenderGui = "gui";


	static bool InMainThread;
	std::queue<OFS_LuaTask> Tasks;
	
	OFS_LuaExtensions() noexcept;
	~OFS_LuaExtensions() noexcept;

	void load(const std::string& path) noexcept;
	void save() noexcept;

	void Update(float delta) noexcept;
	void ShowExtensions(bool* open) noexcept;
	void HandleBinding(class Binding* binding) noexcept;

	template<typename Archive>
	void reflect(Archive& ar)
	{
		OFS_REFLECT(Extensions, ar);
		OFS_REFLECT(DevMode, ar);
	}
};