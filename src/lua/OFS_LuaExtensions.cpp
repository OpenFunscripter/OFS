#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OpenFunscripter.h"

#include "imgui.h"
#include "imgui_stdlib.h"
#include <filesystem>

constexpr const char* ScriptName = "fancy_extension.lua";

#ifdef WIN32
//used to obtain file age on windows
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif


bool OFS_LuaExtensions::InMainThread = false;

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

int LuaDrag(lua_State* L) noexcept;
int LuaShowText(lua_State* L) noexcept;
int LuaButton(lua_State* L) noexcept;
int LuaInput(lua_State* L) noexcept;
static constexpr struct luaL_Reg imguiLib[] = {
	{"Text", LuaShowText},
	{"Button", LuaButton},
	{"Input", LuaInput},
	{"Drag", LuaDrag},
	{NULL, NULL}
};

static int LuaDrag(lua_State* L) noexcept 
{
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
	const char* str = lua_tostring(L, 1);
	if (lua_isinteger(L, 2)) {
		int result = lua_tointeger(L, 2); // trucates to 32 bit
		valueChanged = ImGui::DragInt(str, &result);
		lua_pushinteger(L, result);
	}
	else if (lua_isnumber(L, 2)) {
		float result = lua_tonumber(L, 2); // precision loss
		valueChanged = ImGui::DragFloat(str, &result);
		lua_pushnumber(L, result);
	}
	else {
		return 0;
	}

	lua_pushboolean(L, valueChanged);
	return 2;
}

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
		result = ImGui::Button(str);
	}
	lua_pushboolean(L, result);
	return 1;
}

static int LuaInput(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
	const char* str = lua_tostring(L, 1);
	if (lua_isinteger(L, 2)) {
		int result = lua_tointeger(L, 2); // this will truncate to 32 bit
		valueChanged = ImGui::InputInt(str, &result);
		lua_pushinteger(L, result);
	}
	else if (lua_isnumber(L, 2)) {	
		lua_Number result = result = lua_tonumber(L, 2);
		valueChanged = ImGui::InputDouble(str, &result);
		lua_pushnumber(L, result);
	}
	else if (lua_isstring(L, 2)) {
		char buffer[512];
		const char* result = lua_tostring(L, 2);
		strcpy(buffer, result);
		valueChanged = ImGui::InputText(str, buffer, sizeof(buffer));
		lua_pushstring(L, buffer);
	}
	else {
		return 0;
	}
	lua_pushboolean(L, valueChanged);
	return 2;
}

int LuaGetActiveIdx(lua_State* L) noexcept;
int LuaGetScript(lua_State* L) noexcept;
int LuaAddAction(lua_State* L) noexcept;
int LuaRemoveAction(lua_State* L) noexcept;
int LuaScheduleTask(lua_State* L) noexcept;
int LuaSnapshot(lua_State* L) noexcept;
static constexpr struct luaL_Reg ofsLib[] = {
	{"Script", LuaGetScript},
	{"AddAction", LuaAddAction},
	{"RemoveAction", LuaRemoveAction},
	{"ActiveIdx", LuaGetActiveIdx},
	{"Task", LuaScheduleTask},
	{"Snapshot", LuaSnapshot},
	{NULL, NULL}
};

static int LuaSnapshot(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);

	if (nargs == 0) {
		// snapshot all scripts
		if (OFS_LuaExtensions::InMainThread) {
			app->undoSystem->Snapshot(StateType::CUSTOM_LUA);
		}
		else {
			// for thread safety reasons we do this
			auto handle = EventSystem::ev().WaitableSingleShot([](void*) {
				auto app = OpenFunscripter::ptr;
				app->undoSystem->Snapshot(StateType::CUSTOM_LUA);
			}, 0);
			handle->wait();
		}
	}
	else if (nargs == 1) {
		// snapshot a single script
		assert(lua_istable(L, 1));
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptIdxUserdata);
		assert(lua_isuserdata(L, -1));
		auto index = (intptr_t)lua_touserdata(L, -1);
		if (index >= 0 && index < app->LoadedFunscripts().size()) {
			if (OFS_LuaExtensions::InMainThread) {
				app->undoSystem->Snapshot(StateType::CUSTOM_LUA, app->LoadedFunscripts()[index]);
			}
			else {
				// for thread safety reasons we do this
				auto handle = EventSystem::ev().WaitableSingleShot([](void* index) {
					auto i = (intptr_t)index;
					auto app = OpenFunscripter::ptr;
					app->undoSystem->Snapshot(StateType::CUSTOM_LUA, app->LoadedFunscripts()[i]);
					}, (void*)(intptr_t)index);
				handle->wait();
			}
		}
	}

	return 0;
}

static int LuaGetActiveIdx(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	luaL_argcheck(L, nargs == 0, 1, "Expected no arguments.");
	auto app = OpenFunscripter::ptr;
	lua_pushinteger(L, app->ActiveFunscriptIndex() + 1);
	return 1;
}

static int LuaAddAction(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	if (nargs == 3) {
		assert(lua_istable(L, 1)); // script
		assert(lua_isnumber(L, 2)); // timestamp
		assert(lua_isnumber(L, 3)); // pos

		lua_getfield(L, 1, OFS_LuaExtensions::ScriptIdxUserdata);
		assert(lua_isuserdata(L, -1));
		auto index = (intptr_t)lua_touserdata(L, -1);
		assert(index >= 0 && index < app->LoadedFunscripts().size());

		auto& script = app->LoadedFunscripts()[index];

		double atTime = lua_tonumber(L, 2) / 1000.0;
		assert(atTime >= 0.f);
		int pos = lua_tointeger(L, 3);
		assert(pos >= 0 && pos <= 100);
		FunscriptAction newAction(atTime, pos);
		
		script->AddAction(newAction);

		auto actionCount = script->Actions().size();
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptActionsField); 
		
		const FunscriptAction* begin = &script->Actions()[0];
		for (int i = 0, size = actionCount; i < size; ++i) {
			lua_pushlightuserdata(L, (void*)(begin+i));
			assert(lua_istable(L, -2));
			lua_rawseti(L, -2, i + 1);
		}
	}
	return 0;
}

static int LuaRemoveAction(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;

	int nargs = lua_gettop(L);

	if (nargs == 2) {
		assert(lua_istable(L, 1));
		assert(lua_isuserdata(L, 2));
		
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptIdxUserdata); // 3
		assert(lua_isuserdata(L, -1));
		auto index = (intptr_t)lua_touserdata(L, -1);
		assert(index >= 0 && index < app->LoadedFunscripts().size());

		auto& script = app->LoadedFunscripts()[index];
		FunscriptAction* action = (FunscriptAction*)lua_touserdata(L, 2);
		assert(action);

		script->RemoveAction(*action, true);
		int actionCount = script->Actions().size();
		
		// update all FunscriptAction pointers
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptActionsField); // 4
		assert(lua_istable(L, -1));
		
		// remove element
		lua_pushnil(L);
		lua_seti(L, -2, actionCount+1);

		if (actionCount == 0) return 0;

		const FunscriptAction* begin = &script->Actions()[0];
		for (int i = 0, size = script->Actions().size(); i < size; ++i) {
			lua_pushlightuserdata(L, (void*)(begin+i));
			assert(lua_istable(L, -2));
			lua_rawseti(L, -2, i + 1);
		}
	}
	return 0;
}

static int LuaGetScript(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		assert(lua_isnumber(L, 1));
		lua_Integer index = lua_tointeger(L, 1);

		if (index >= 1 && index <= app->LoadedFunscripts().size()) {
			auto& script = app->LoadedFunscripts()[index - 1];
			lua_createtable(L, 0, 2); // 2
			
			lua_pushlightuserdata(L, (void*)(intptr_t)(index - 1));
			lua_setfield(L, -2, OFS_LuaExtensions::ScriptIdxUserdata); // pops off

			lua_createtable(L, script->Actions().size(), 2); // 3
			for(int i=0, size=script->Actions().size(); i < size; ++i) {
				auto& action = script->Actions()[i];
				lua_pushlightuserdata(L, (void*)&action);
				lua_getglobal(L, OFS_LuaExtensions::GlobalActionMetaTable);
				assert(lua_isuserdata(L, -2));
				lua_setmetatable(L, -2);
				assert(lua_istable(L, -2));
				lua_rawseti(L, -2, i + 1);
			}
			assert(lua_istable(L, -1));
			lua_setfield(L, 2, OFS_LuaExtensions::ScriptActionsField);
			return 1;
		}
	}
	return 0;
}

static int LuaScheduleTask(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	auto& ext = app->extensions;

	int nargs = lua_gettop(L);
	if (nargs >= 1) {
		luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
		const char* functionName = lua_tostring(L, 1);

		OFS_LuaTask task;
		task.Function = functionName;
		ext->Tasks.emplace(std::move(task));
	}

	return 0;
}


static void InitLuaGlobalMetatables(lua_State* L) noexcept
{
	auto setter = [](lua_State* L) -> int {
		int nargs = lua_gettop(L);
		if (nargs == 3) {
			FunscriptAction* action = (FunscriptAction*)lua_touserdata(L, 1);
			assert(action);
			const char* key = lua_tostring(L, 2);
			lua_Number value = lua_tonumber(L, 3);
					
			if (strcmp(key, "pos") == 0) {
				action->pos = Util::Clamp(value, 0.0, 100.0);
			}
			else if (strcmp(key, "at") == 0) {
				action->atS = value / 1000.0;
			}
		}
		return 0;
	};

	auto getter = [](lua_State* L) -> int {
		int nargs = lua_gettop(L);
		if (nargs == 2) {
			FunscriptAction* action = (FunscriptAction*)lua_touserdata(L, 1);
			assert(action);
			const char* key = lua_tostring(L, 2);
			if (strcmp(key, "pos") == 0) {
				lua_pushinteger(L, action->pos);
			}
			else if (strcmp(key, "at") == 0) {
				lua_pushnumber(L, (double)action->atS * 1000.0);
			}
			return 1;
		}
		return 0;
	};
	
	lua_createtable(L, 0, 2);
	lua_pushcfunction(L, getter);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, setter);
	lua_setfield(L, -2, "__newindex");

	lua_setglobal(L, OFS_LuaExtensions::GlobalActionMetaTable);
}

void OFS_LuaExtensions::RunTask(OFS_LuaTask& taks) noexcept
{
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
	
	
	// put all ofs functions into a ofs table
	lua_createtable(L, 0, sizeof(ofsLib)/sizeof(luaL_Reg) + sizeof(imguiLib)/sizeof(luaL_Reg));
	luaL_setfuncs(L, ofsLib, 0);
	luaL_setfuncs(L, imguiLib, 0);
	lua_setglobal(L, "ofs");

	InitLuaGlobalMetatables(L);

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
	
	auto app = OpenFunscripter::ptr;

	static bool AutoReload = true;
	static uint64_t FileAge = GetWriteTime((const wchar_t*)CurrentPath.c_str());

	ImGui::Begin("Extension", open, ImGuiWindowFlags_None);
	if (!app->blockingTask.Running) {
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
		lua_getglobal(L, RenderGui);
		lua_pushnumber(L, ImGui::GetIO().DeltaTime);
		OFS_LuaExtensions::InMainThread = true; // HACK
		int result = lua_pcall(L, 1, 1, 0); // 1 arguments 1 result
		if (result) {
			const char* error = lua_tostring(L, -1);
			FUN_ASSERT(false, error);
		}
		std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - startTime;
		static double time = duration.count();
		static double maxTime = time;
		if (duration.count() > maxTime) maxTime = duration.count();
		time += duration.count(); time /= 2.0;
		ImGui::Text("Lua time: %lf ms", time * 1000.0);
		ImGui::Text("Lua max time: %lf ms", maxTime * 1000.0);

		if (!Tasks.empty()) {
			auto taskData = std::make_unique<BlockingTaskData>();
			taskData->TaskDescription = "Lua extension";
			taskData->TaskThreadFunc = [](void* data)->int
			{
				BlockingTaskData* task = (BlockingTaskData*)data;
				OFS_LuaExtensions* ext = (OFS_LuaExtensions*)task->User;
				task->MaxProgress = ext->Tasks.size();
				while (!ext->Tasks.empty()) {
					OFS_LuaTask& lua = ext->Tasks.front();
					lua_getglobal(ext->L, lua.Function.c_str());
					if (lua_isfunction(ext->L, -1)) {
						OFS_LuaExtensions::InMainThread = false; // HACK
						int result = lua_pcall(ext->L, 0, 1, 0); // 0 arguments 1 result
					}
					ext->Tasks.pop();
					++task->Progress;
				}
				return 0;
			};
			taskData->User = this;
			app->blockingTask.DoTask(std::move(taskData));
		}
	}
	ImGui::End();
}
