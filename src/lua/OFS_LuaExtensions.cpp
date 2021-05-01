#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OpenFunscripter.h"

#include "imgui.h"
#include <filesystem>

constexpr const char* ScriptName = "fancy_extension.lua";

#ifdef WIN32
//used to obtain file age on windows
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

std::u16string CurrentPath;

static uint64_t GetWriteTime(const wchar_t* path) {
	OFS_PROFILE(__FUNCTION__);
	std::error_code ec;
	uint64_t timestamp = 0;
	HANDLE file = CreateFileW((wchar_t*)path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {	}
	else {
		FILETIME ftCreate;
		GetFileTime(file, NULL, NULL, &ftCreate);
		timestamp = *(uint64_t*)&ftCreate;
		CloseHandle(file);
	}
	return timestamp;
}


int LuaPrint(lua_State* L) noexcept;

static constexpr struct luaL_Reg printlib[] = {
  {"print", LuaPrint},
  {NULL, NULL} /* end of array */
};

static int LuaPrint(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);

	std::stringstream ss;
	for (int i = 1; i <= nargs; ++i) {
		const char* str = lua_tostring(L, i);
		if (str != nullptr) {
			size_t len = strlen(str);
			if (len >= 1024) {
				ss.write(str, 1024);
				ss << "[...] " << len << " characters were truncated";
			}
			else {
				ss << str;
			}
		}
	}
	ss << std::endl;

	LOG_INFO(ss.str().c_str());
	return 0;
}


static bool LuaCall(lua_State* L, const char* function) noexcept
{
	lua_getglobal(L, function);
	if (true /*|| lua_getglobal(L, function)*/) {
		int result = lua_pcall(L, 0, 1, 0); // 0 arguments 1 results
		return result == 0;
	}
	return false;
}

int LuaShowText(lua_State* L) noexcept;
int LuaButton(lua_State* L) noexcept;
int LuaInputDouble(lua_State* L) noexcept;
static constexpr struct luaL_Reg imguiLib[] = {
	{"ShowText", LuaShowText},
	{"Button", LuaButton},
	{"InputDouble", LuaInputDouble},
	{NULL, NULL}
};

static int LuaShowText(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		const char* str = lua_tostring(L, 1);
		if (str) ImGui::TextUnformatted(str);
	}
	return 0;
}

static int LuaButton(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	bool result = false;
	if (nargs == 1) {
		const char* str = lua_tostring(L, 1);
		if (str) {
			result = ImGui::Button(str);
		}
	}
	lua_pushboolean(L, result);
	return 1;
}

static int LuaInputDouble(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	lua_Number result = 0.0;
	if (nargs == 2) {
		const char* str = lua_tostring(L, 1);
		result = lua_tonumber(L, 2);
		if (str) {
			valueChanged = ImGui::InputDouble(str, &result);
		}
	}
	lua_pushnumber(L, result);
	lua_pushboolean(L, valueChanged);
	return 2;
}

int LuaIterateScript(lua_State* L) noexcept;
static constexpr struct luaL_Reg ofsLib[] = {
	{"Iterate", LuaIterateScript},
	{NULL, NULL}
};

static int LuaIterateScript(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;

	int nargs = lua_gettop(L);
	if (nargs == 1) {
		lua_Integer index = lua_tointeger(L, 1);
		if (index >= 1 && index <= app->ActiveFunscript()->Actions().size()) {
			auto& action = app->ActiveFunscript()->Actions()[index-1];
			lua_createtable(L, 0, 3);
			
			lua_pushnumber(L, (double)action.atS * 1000.0);
			lua_setfield(L, -2, "at");

			lua_pushinteger(L, action.pos);
			lua_setfield(L, -2, "pos");

			lua_pushboolean(L, app->ActiveFunscript()->IsSelected(action));
			lua_setfield(L, -2, "selected");
			return 1;
		}
	}

	return 0;
}

OFS_LuaExtensions::OFS_LuaExtensions() noexcept
{
	auto extensionDir = Util::Prefpath(ExtensionDir);
	std::filesystem::path extensionPath = (Util::PathFromString(extensionDir) / ScriptName);
	std::filesystem::create_directories(extensionDir);
	
	CurrentPath = extensionPath.u16string();
	LoadExtension(extensionPath.u8string().c_str());
}

OFS_LuaExtensions::~OFS_LuaExtensions() noexcept
{
	lua_close(L);
}


bool OFS_LuaExtensions::LoadExtension(const char* extensionPath) noexcept
{
	std::vector<uint8_t> extensionText;
	Util::ReadFile(extensionPath, extensionText);
	FUN_ASSERT(extensionText.size() > 0, "no code");
	extensionText.emplace_back('\0');

	if (L) { lua_close(L); L = 0; }
	
	L = luaL_newstate();
	luaL_openlibs(L);
	lua_getglobal(L, "_G");
	luaL_setfuncs(L, printlib, 0);
	luaL_setfuncs(L, imguiLib, 0);
	luaL_setfuncs(L, ofsLib, 0);

	int status = luaL_dostring(L, (const char*)extensionText.data());
	if (status != 0) {
		const char* error = lua_tostring(L, -1);
		FUN_ASSERT(false, error);
		return false;
	}
	LuaCall(L, EntryPoint);

	return true;
}

void OFS_LuaExtensions::ShowExtensions(bool* open) noexcept
{
	if (!*open) return;
	OFS_PROFILE(__FUNCTION__);
	
	static bool AutoReload = false;
	static uint64_t FileAge = GetWriteTime((const wchar_t*)CurrentPath.c_str());

	ImGui::Begin("Extensions", open, ImGuiWindowFlags_None);
	ImGui::Checkbox("Auto reload", &AutoReload);
	ImGui::SameLine(); ImGui::Text("age: %lld", FileAge);
	if (AutoReload) {
		auto age = GetWriteTime((const wchar_t*)CurrentPath.c_str());
		if (age != FileAge) {
			FileAge = age;
			auto extensionDir = Util::Prefpath(ExtensionDir);
			auto extensionPath = (Util::PathFromString(extensionDir) / ScriptName).u8string();
			LoadExtension(extensionPath.c_str());
		}
	}
	
	ImGui::SetWindowSize(ImVec2(300.f, 200.f), ImGuiCond_FirstUseEver);
	auto startTime = std::chrono::high_resolution_clock::now();
	if (!LuaCall(L, RenderGui)) {
		const char* error = lua_tostring(L, -1);
		FUN_ASSERT(false, error);
	}
	std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - startTime;
	static double time = duration.count();
	time += duration.count(); time /= 2.0;
	ImGui::Text("Lua time: %lf ms", time * 1000.0);
	ImGui::End();
}
