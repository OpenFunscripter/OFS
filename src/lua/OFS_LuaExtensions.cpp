#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_Serialization.h"
#include "OFS_ImGui.h"
#include "OpenFunscripter.h"
#include "OFS_LuaCoreExtension.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "luasocket.h"

#include <filesystem>
#include <algorithm>
#include <sstream>
#include "EASTL/string.h"

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#endif

#include "lstate.h"


constexpr const char* LuaDefaultFunctions = R"(
function clamp(val, min, max)
	return math.min(max, math.max(val, min))
end
)";

static OFS::AppLog ExtensionLogBuffer;

SDL_threadID OFS_LuaExtensions::MainThread = SDL_ThreadID();
bool OFS_LuaExtensions::DevMode = false;
bool OFS_LuaExtensions::ShowLogs = false;

#ifndef NDEBUG
// asserts that the Lua stack was cleaned properly
struct LuaStackCheck
{
	static inline size_t LuaStackSize(lua_State* L) noexcept
	{
		return (size_t)L->top - (size_t)L->stack;
	}

	lua_State* L = nullptr;
	size_t InitialStackSize = 0;
	int valuesReturned = 0;
	inline LuaStackCheck(lua_State* L, int stackValuesReturned = 0) noexcept
		: L(L), valuesReturned(stackValuesReturned)
	{
		InitialStackSize = LuaStackSize(L);
	}

	inline ~LuaStackCheck() noexcept
	{
		auto newStackSize = LuaStackSize(L);
		newStackSize -= sizeof(StackValue) * (size_t)valuesReturned;
		FUN_ASSERT(newStackSize == InitialStackSize, "Stack wasn't cleaned.");
	}
};
#define CLEAN_STACK_CHECK(L, ReturnValues) LuaStackCheck XXXcheckStack(L, ReturnValues);
#else
#define CLEAN_STACK_CHECK
#endif


inline int Lua_Pcall(lua_State* L, int a, int b, int c) noexcept
{
#ifndef NDEBUG
	static volatile bool call = false;
	FUN_ASSERT(!call, "this shouldn't happen");
	call = true;
#endif
	auto result = lua_pcall(L, a, b, c);
#ifndef NDEBUG
	call = false;
#endif
	return result;
}

static int LuaPrint(lua_State* L) noexcept;
static constexpr struct luaL_Reg printlib[] = {
  {"print", LuaPrint},
  {NULL, NULL} /* end of array */
};

static int LuaPrint(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	int nargs = lua_gettop(L);

	eastl::string logMsg;
	logMsg.reserve(1024);

	lua_getglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);
	assert(lua_isuserdata(L, -1));
	OFS_LuaExtension* ext = (OFS_LuaExtension*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	logMsg.append_sprintf("[%s]: ", ext->Name.c_str());
	for (int i = 1; i <= nargs; ++i) {
		const char* str = lua_tostring(L, i);
		if (str != nullptr) {
			logMsg.append(str);
		}
	}

	logMsg.append(1, '\n');
	ExtensionLogBuffer.AddLog(logMsg.c_str());
	return 0;
}

static int LuaSlider(lua_State* L) noexcept;
static int LuaDrag(lua_State* L) noexcept;
static int LuaShowText(lua_State* L) noexcept;
static int LuaButton(lua_State* L) noexcept;
static int LuaInput(lua_State* L) noexcept;
static int LuaSameLine(lua_State* L) noexcept;
static int LuaCheckbox(lua_State* L) noexcept;
static int LuaSeparator(lua_State* L) noexcept;
static int LuaSpacing(lua_State* L) noexcept;
static int LuaNewLine(lua_State* L) noexcept;
static constexpr struct luaL_Reg imguiLib[] = {
	{"Text", LuaShowText},
	{"Button", LuaButton},
	{"Input", LuaInput},
	{"Drag", LuaDrag},
	{"Checkbox", LuaCheckbox},
	{"Slider", LuaSlider},

	{"SameLine", LuaSameLine},
	{"Separator", LuaSeparator},
	{"Spacing", LuaSpacing},
	{"NewLine", LuaNewLine},
	{NULL, NULL}
};

static int LuaSlider(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	if (nargs >= 4) {
		CLEAN_STACK_CHECK(L, 2);
		luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string");
		luaL_argcheck(L, lua_isnumber(L, 2), 2, "Expected number");
		luaL_argcheck(L, lua_isnumber(L, 3), 3, "Expected min number");
		luaL_argcheck(L, lua_isnumber(L, 4), 4, "Expected max number");

		const char* str = lua_tostring(L, 1);
		if (lua_isinteger(L, 2)) {
			int value = lua_tointeger(L, 2);
			int min = lua_tointeger(L, 3);
			int max = lua_tointeger(L, 4);
			valueChanged = ImGui::SliderInt(str, &value, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);

			lua_pushinteger(L, value);
			lua_pushboolean(L, valueChanged);
			return 2;
		}
		else {
			float value = lua_tonumber(L, 2);
			float min = lua_tonumber(L, 3);
			float max = lua_tonumber(L, 4);

			valueChanged = ImGui::SliderFloat(str, &value, min, max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			lua_pushnumber(L, value);
			lua_pushboolean(L, valueChanged);
			return 2;
		}
	}
	return 0;
}

static int LuaSpacing(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	ImGui::Spacing();
	return 0;
}

static int LuaNewLine(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	ImGui::NewLine();
	return 0;
}

static int LuaSeparator(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	ImGui::Separator();
	return 0;
}

static int LuaCheckbox(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	if (nargs >= 2) {
		CLEAN_STACK_CHECK(L, 2);
		luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
		luaL_argcheck(L, lua_isboolean(L, 2), 2, "Expected boolean.");
		const char* str = lua_tostring(L, 1);
		bool value = lua_toboolean(L, 2);
		if (str) {
			valueChanged = ImGui::Checkbox(str, &value);
			lua_pushboolean(L, value);
			lua_pushboolean(L, valueChanged);
			return 2;
		}
	}
	return 0;
}

static int LuaSameLine(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	ImGui::SameLine();
	return 0;
}

static int LuaDrag(lua_State* L) noexcept 
{
	CLEAN_STACK_CHECK(L, 2);
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
	luaL_argcheck(L, lua_isnumber(L, 2) || lua_isinteger(L, 2), 2, "Expected number");
	float stepSize = 1.f;
	if (nargs >= 3) {
		luaL_argcheck(L, lua_isnumber(L, 3), 3, "Expected step size number");
		stepSize = lua_tonumber(L, 3);
	}

	const char* str = lua_tostring(L, 1);
	if (lua_isinteger(L, 2)) {
		int result = lua_tointeger(L, 2); // truncates to 32 bit
		valueChanged = ImGui::DragInt(str, &result, stepSize);
		lua_pushinteger(L, result);
	}
	else if (lua_isnumber(L, 2)) {
		float result = lua_tonumber(L, 2); // precision loss
		valueChanged = ImGui::DragFloat(str, &result, stepSize);
		lua_pushnumber(L, result);
	}

	lua_pushboolean(L, valueChanged);
	return 2;
}

static int LuaShowText(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		const char* str = lua_tostring(L, 1);
		if (str) ImGui::TextWrapped("%s", str);
	}
	return 0;
}

static int LuaButton(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	if (nargs >= 1) {
		CLEAN_STACK_CHECK(L, 1);
		luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
		const char* str = lua_tostring(L, 1);
		bool result = ImGui::Button(str);
		lua_pushboolean(L, result);
		return 1;
	}
	return 0;
}

static int LuaInput(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 2);
	int nargs = lua_gettop(L);
	bool valueChanged = false;
	luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
	luaL_argcheck(L, lua_isnumber(L, 2) || lua_isstring(L, 2) || lua_isinteger(L, 2), 2, "Expected string or number.");

	lua_Number stepSize = 1.0;

	if(nargs >= 3) {
		luaL_argcheck(L, lua_isnumber(L, 3) || lua_isinteger(L, 3) && !lua_isstring(L, 2), 3, "Expected step size.");
		stepSize = lua_tonumber(L, 3);
	}

	const char* str = lua_tostring(L, 1);
	if (lua_isinteger(L, 2)) {
		int result = lua_tointeger(L, 2); // this will truncate to 32 bit
		valueChanged = ImGui::InputInt(str, &result, stepSize);
		lua_pushinteger(L, result);
	}
	else if (lua_isnumber(L, 2)) {	
		lua_Number result = result = lua_tonumber(L, 2);
		valueChanged = ImGui::InputDouble(str, &result, stepSize);
		lua_pushnumber(L, result);
	}
	else if (lua_isstring(L, 2)) {
		char buffer[512];
		const char* result = lua_tostring(L, 2);
		strcpy(buffer, result);
		valueChanged = ImGui::InputText(str, buffer, sizeof(buffer));
		lua_pushstring(L, buffer);
	}

	lua_pushboolean(L, valueChanged);
	return 2;
}

inline static bool ScriptDataHelperCheckIfSelected(const Funscript::FunscriptData& data, FunscriptAction action) noexcept
{
	// TODO: awful perf
	return data.selection.find(action) != data.selection.end();
};

static void ActionGetterSetter(lua_State* L, Funscript::FunscriptData* scriptData) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto setter = [](lua_State* L) -> int {
		CLEAN_STACK_CHECK(L, 0);
		auto app = OpenFunscripter::ptr;
		int nargs = lua_gettop(L);
		assert(lua_isuserdata(L, lua_upvalueindex(1)));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, lua_upvalueindex(1));

		if (nargs == 3) {
			int actionIdx = (intptr_t)lua_touserdata(L, 1);
			assert(actionIdx >= 0 && actionIdx < scriptData->Actions.size());
			auto& action = scriptData->Actions[actionIdx];
			
			FunscriptAction newAction = action;
			const char* key = lua_tostring(L, 2);
			lua_Number setValue = lua_tonumber(L, 3);
			

			auto updateAction = [](Funscript::FunscriptData* scriptData, FunscriptAction& toBeUpdated, FunscriptAction newVal, bool isSelected) noexcept
			{
				if (isSelected) {
					scriptData->selection.erase(toBeUpdated);
					scriptData->selection.emplace(newVal);
				}
				toBeUpdated = newVal;
			};

			if (strcmp(key, "pos") == 0) {
				bool isSelected = ScriptDataHelperCheckIfSelected(*scriptData, action);
				newAction.pos = Util::Clamp(setValue, 0.0, 100.0);
				updateAction(scriptData, action, newAction, isSelected);
			}
			else if (strcmp(key, "at") == 0) {
				bool isSelected = ScriptDataHelperCheckIfSelected(*scriptData, action);
				newAction.atS = setValue / 1000.0;
				newAction.atS = std::max(newAction.atS, 0.f);
				updateAction(scriptData, action, newAction, isSelected);
			}
			else if (strcmp(key, "selected") == 0) {
				auto shouldSelect = lua_toboolean(L, 3);
				if(shouldSelect) {
					scriptData->selection.emplace(action);
				}
				else {
					scriptData->selection.erase(action);
				}
			}
		}
		return 0;
	};

	auto getter = [](lua_State* L) -> int {
		CLEAN_STACK_CHECK(L, 1);
		auto app = OpenFunscripter::ptr;
		int nargs = lua_gettop(L);
		assert(lua_isuserdata(L, lua_upvalueindex(1)));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, lua_upvalueindex(1));

		if (nargs >= 2) {
			int actionIdx = (intptr_t)lua_touserdata(L, 1);
			assert(actionIdx >= 0 && actionIdx < scriptData->Actions.size());
			const auto& action = scriptData->Actions[actionIdx];

			const char* key = lua_tostring(L, 2);
			if (strcmp(key, "pos") == 0) {
				lua_pushinteger(L, action.pos);
			}
			else if (strcmp(key, "at") == 0) {
				lua_pushnumber(L, (double)action.atS * 1000.0);
			}
			else if (strcmp(key, "selected") == 0) {
				lua_pushboolean(L, ScriptDataHelperCheckIfSelected(*scriptData, action));
			}
		}
		return 1;
	};

	lua_createtable(L, 0, 2);
	lua_pushlightuserdata(L, (void*)scriptData);
	lua_pushcclosure(L, getter, 1);
	lua_setfield(L, -2, "__index");

	lua_pushlightuserdata(L, (void*)scriptData);
	lua_pushcclosure(L, setter, 1);
	lua_setfield(L, -2, "__newindex");
}

static int LuaPlayerIsPlaying(lua_State* L) noexcept;
static int LuaPlayerSeek(lua_State* L) noexcept;
static int LuaPlayerPlay(lua_State* L) noexcept;
static int LuaPlayerCurrentTime(lua_State* L) noexcept;
static int LuaPlayerDuration(lua_State* L) noexcept;
static int LuaPlayerGetVideo(lua_State* L) noexcept;
static int LuaPlayerGetFPS(lua_State* L) noexcept;

static constexpr struct luaL_Reg playerLib[] = {
	{"Play", LuaPlayerPlay},
	{"Seek", LuaPlayerSeek},
	{"CurrentTime", LuaPlayerCurrentTime},
	{"Duration", LuaPlayerDuration},
	{"IsPlaying", LuaPlayerIsPlaying},
	{"CurrentVideo", LuaPlayerGetVideo},
	{"FPS", LuaPlayerGetFPS},
	{NULL, NULL}
};

static int LuaPlayerGetFPS(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	lua_pushnumber(L, app->player->getFps());
	return 1;
}

static int LuaPlayerGetVideo(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	// this actually creates a copy of the string in Lua
	lua_pushstring(L, app->player->getVideoPath());
	return 1;
}

static int LuaPlayerIsPlaying(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	lua_pushboolean(L, !app->player->isPaused());
	return 1;
}

static int LuaPlayerPlay(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	int nargs = lua_gettop(L);
	auto app = OpenFunscripter::ptr;
	bool play = app->player->isPaused(); // toggle by default
	if (nargs >= 1) {
		luaL_argcheck(L, lua_isboolean(L, 1), 1, "Expected boolean: playing true/false");
		play = lua_toboolean(L, 1);
	}

	app->player->setPaused(!play);
	return 0;
}

static int LuaPlayerSeek(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	if (nargs >= 1) {
		luaL_argcheck(L, lua_isnumber(L, 1), 1, "Expected time in seconds.");
		lua_Number time = lua_tonumber(L, 1);
		app->player->setPositionExact(time);
	}
	return 0;
}

static int LuaPlayerDuration(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	lua_Number duration = app->player->getDuration();
	lua_pushnumber(L, duration);
	return 1;
}

static int LuaPlayerCurrentTime(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	lua_Number time = app->player->getCurrentPositionSecondsInterp();
	lua_pushnumber(L, time);
	return 1;
}

static int LuaGetActiveIdx(lua_State* L) noexcept;
static int LuaGetScript(lua_State* L) noexcept;
static int LuaAddAction(lua_State* L) noexcept;
static int LuaClearScript(lua_State* L) noexcept;
static int LuaRemoveAction(lua_State* L) noexcept;
static int LuaRemoveSelected(lua_State* L) noexcept;
static int LuaBindFunction(lua_State* L) noexcept;
static int LuaScheduleTask(lua_State* L) noexcept;
static int LuaCommitChanges(lua_State* L) noexcept;
static int LuaUndo(lua_State* L) noexcept;
static int LuaHasSelection(lua_State* L) noexcept;
static int LuaSelectedIndices(lua_State* L) noexcept;
static int LuaGetExtensionDir(lua_State* L) noexcept;
static int LuaGetScriptTitle(lua_State* L) noexcept;
static int LuaClosestAction(lua_State* L) noexcept;
static int LuaClosestActionAfter(lua_State* L) noexcept;
static int LuaClosestActionBefore(lua_State* L) noexcept;
static int LuaSilentCmd(lua_State* L) noexcept;
static int LuaSaveScript(lua_State* L) noexcept;
static int LuaScriptPath(lua_State* L) noexcept;

static constexpr struct luaL_Reg ofsLib[] = {
	// core
	{"Task", LuaScheduleTask},
	{"Bind", LuaBindFunction},
	{"Undo", LuaUndo},
	{"ExtensionDir", LuaGetExtensionDir},
	{"SilentCmd", LuaSilentCmd},

	// funscript api
	{"Script", LuaGetScript},
	{"AddAction", LuaAddAction},
	{"RemoveAction", LuaRemoveAction},
	{"RemoveSelected", LuaRemoveSelected},
	{"ActiveIdx", LuaGetActiveIdx},
	{"ClearScript", LuaClearScript},
	{"HasSelection", LuaHasSelection},
	{"SelectedIndices", LuaSelectedIndices},
	{"Commit", LuaCommitChanges},
	{"ScriptTitle", LuaGetScriptTitle},
	{"ClosestAction", LuaClosestAction},
	{"ClosestActionAfter", LuaClosestActionAfter},
	{"ClosestActionBefore", LuaClosestActionBefore},
	{"SaveScript", LuaSaveScript},
	{"ScriptPath", LuaScriptPath},
	{NULL, NULL}
};

static int LuaSilentCmd(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	int nargs = lua_gettop(L);
	bool success = false;
	if(nargs >= 1) {
		bool runAsync = false;
		luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
		if(nargs >= 2) { 
			luaL_argcheck(L, lua_isboolean(L, 2), 2, "Expected boolean.");
			runAsync = lua_toboolean(L, 2);		
		}
		const char* cmd = lua_tostring(L, 1);
		#if defined(WIN32)
		std::wstringstream wstrm;
		wstrm << L"/c \"" << Util::Utf8ToUtf16(cmd) << L'"';
		auto wCmd = wstrm.str();

		if(!runAsync) {
			SHELLEXECUTEINFOW info = {0};
			info.cbSize = sizeof(SHELLEXECUTEINFOW);
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.hwnd = NULL;
			info.lpVerb = NULL;
			info.lpFile = L"cmd.exe";        
			info.lpParameters = wCmd.c_str();   
			info.lpDirectory = NULL;
			info.nShow = SW_HIDE;
			info.hInstApp = NULL; 
			success = ShellExecuteExW(&info);
			WaitForSingleObject(info.hProcess, INFINITE);
			CloseHandle(info.hProcess);
		}
		else {
			auto val = (INT_PTR)ShellExecuteW(NULL, L"open", L"cmd.exe", wCmd.c_str(), NULL, SW_HIDE);
			success = val > 32;
		}

		#else
		success = std::system(cmd);
		#endif
	}
	lua_pushboolean(L, success);
	return 1;
}

static int LuaGetScriptTitle(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	if(nargs >= 1) {
		CLEAN_STACK_CHECK(L, 1);
		luaL_argcheck(L, lua_isinteger(L, 1), 1, "Expected script index.");
		int scriptIndex = lua_tointeger(L, 1) - 1;
		luaL_argcheck(L, scriptIndex >= 0 && scriptIndex < app->LoadedFunscripts().size(), 1, "Script index invalid.");
		lua_pushstring(L, app->LoadedFunscripts()[scriptIndex]->Title.c_str());
		return 1;
	}
	return 0;
}

static int LuaGetExtensionDir(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	lua_getglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);
	assert(lua_isuserdata(L, -1));
	OFS_LuaExtension* ext = (OFS_LuaExtension*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop off GlobalExtensionPtr
	lua_pushstring(L, ext->Directory.c_str());
	return 1;
}

static int LuaHasSelection(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	bool hasSelection = false;
	if (nargs >= 1) {
		luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script");
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
		assert(lua_isuserdata(L, -1));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
		lua_pop(L, 1); // pop off ScriptDataUserdata
		hasSelection = !scriptData->selection.empty();
	}
	lua_pushboolean(L, hasSelection);
	return 1;
}

static int LuaSelectedIndices(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	int nargs = lua_gettop(L);
	luaL_argcheck(L, nargs >= 1, 0, "Not enough arguments.");
	luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script.");
	
	lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
	assert(lua_isuserdata(L, -1));
	auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop off ScriptDataUserdata


	lua_createtable(L, scriptData->selection.size(), 1);
	for(int i=0, size=scriptData->selection.size(); i < size; ++i) {
		auto& selected = scriptData->selection[i];
		auto it = scriptData->Actions.find(selected);
		auto index = std::distance(scriptData->Actions.begin(), it);
		lua_pushinteger(L, index+1);
		assert(lua_istable(L, -2));
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

static int LuaClosestAction(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	int nargs = lua_gettop(L);
	luaL_argcheck(L, nargs >= 2, 0, "Not enough arguments.");
	luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script.");
	luaL_argcheck(L, lua_isnumber(L, 2), 2, "Expected time in seconds.");

	lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
	assert(lua_isuserdata(L, -1));
	auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop ScriptUserdata

	lua_Number time = lua_tonumber(L, 2);

	{
		auto closestAction = Funscript::getActionAtTime(scriptData->Actions, time, std::numeric_limits<float>::max());
		if(closestAction) {
			auto i = std::distance(scriptData->Actions.begin(), closestAction);
			lua_pushinteger(L, i + 1);
			return 1;
		}
	}

	return 0;
}

static int LuaClosestActionAfter(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);	
	if(nargs >= 2) {
		CLEAN_STACK_CHECK(L, 1);
		luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script");
		luaL_argcheck(L, lua_isnumber(L, 2), 2, "Expected time in seconds");

		lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
		assert(lua_isuserdata(L, -1));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
		lua_pop(L, 1); // pop ScriptDataUserdata
		lua_Number time = lua_tonumber(L, 2);

		{
			if (scriptData->Actions.empty()) return 0;
			auto it = scriptData->Actions.upper_bound(FunscriptAction(time, 0));
			if(it != scriptData->Actions.end()) {
				int actionIndex = std::distance(scriptData->Actions.begin(), it);
				lua_pushinteger(L, actionIndex + 1);
				return 1;
			}
		}
	}	
	return 0;
}

static int LuaClosestActionBefore(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);	
	if(nargs >= 2) {
		CLEAN_STACK_CHECK(L, 1);
		luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script");
		luaL_argcheck(L, lua_isnumber(L, 2), 2, "Expected time in seconds");

		lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
		assert(lua_isuserdata(L, -1));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
		lua_pop(L, 1); // pop ScriptDataUserdata
		lua_Number time = lua_tonumber(L, 2);

		{
			if (scriptData->Actions.empty()) return 0;
			auto it = scriptData->Actions.lower_bound(FunscriptAction(time, 0));
			if(it-1 >= scriptData->Actions.begin()) {
				auto before = it - 1; 
				int actionIndex = std::distance(scriptData->Actions.begin(), before);
				lua_pushinteger(L, actionIndex + 1);
				return 1;
			}
		}
	}	

	return 0;
}

static int LuaClearScript(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script.");

	lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
	auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop ScriptDataUserdata
	*scriptData = Funscript::FunscriptData();

	lua_createtable(L, 0, 0);
	lua_setfield(L, 1, OFS_LuaExtensions::ScriptActionsField);
	return 0;
}

static int LuaUndo(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	bool undo = false;
	auto app = OpenFunscripter::ptr;
	if (app->undoSystem->MatchUndoTop(StateType::CUSTOM_LUA)) {
		undo = app->undoSystem->Undo();
	}
	lua_pushboolean(L, undo);
	return 1;
}



inline void CommitScriptChanges(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	lua_getfield(L, 1, OFS_LuaExtensions::ScriptIdxUserdata);
	assert(lua_isuserdata(L, -1));
	int scriptIdx = (intptr_t)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop script index

	lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
	assert(lua_isuserdata(L, -1));
	auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop ScriptDataUserdata

	if(scriptIdx >= 0 && scriptIdx < app->LoadedFunscripts().size()) {
		auto& script = app->LoadedFunscripts()[scriptIdx];
		app->undoSystem->Snapshot(StateType::CUSTOM_LUA, script);
		script->SetActions(scriptData->Actions);
		script->SetSelection(scriptData->selection, true);
	}
}

static int LuaCommitChanges(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script.");

	if(SDL_ThreadID() == OFS_LuaExtensions::MainThread) {
		CommitScriptChanges(L);
	}
	else if(nargs >= 1) {
		auto handle = EventSystem::ev().WaitableSingleShot([](void* ctx) noexcept {
			CommitScriptChanges((lua_State*)ctx);
		}, (void*)L);
		handle->wait();
	}
	return 0;
}

static int LuaGetActiveIdx(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 1);
	int nargs = lua_gettop(L);
	luaL_argcheck(L, nargs == 0, 1, "Expected no arguments.");
	auto app = OpenFunscripter::ptr;
	lua_pushinteger(L, app->ActiveFunscriptIndex() + 1);
	return 1;
}

static int LuaAddAction(lua_State* L) noexcept
{		
	CLEAN_STACK_CHECK(L, 0);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	bool selectAction = false;
	if (nargs >= 3) {
		luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script"); // script
		luaL_argcheck(L, lua_isnumber(L, 2), 2, "Expected time"); // timestamp
		luaL_argcheck(L, lua_isnumber(L, 3), 3, "Expected position"); // pos
		if (nargs >= 4) {
			luaL_argcheck(L, lua_isboolean(L, 4), 4, "Expected selected boolean"); // selected
			selectAction = lua_toboolean(L, 4);
		}
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
		assert(lua_isuserdata(L, -1));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
		lua_pop(L, 1); // pop ScriptDataUserdata

		double atTime = lua_tonumber(L, 2) / 1000.0;
		assert(atTime >= 0.f);
		lua_Number pos = lua_tonumber(L, 3);
		luaL_argcheck(L, pos >= 0.0 && pos <= 100.0, 3, "Position has to be 0 to 100.");
		FunscriptAction newAction(atTime, pos);

		scriptData->Actions.emplace(newAction);
		if (selectAction) {
			scriptData->selection.emplace(newAction);
		}

		auto actionCount = scriptData->Actions.size();
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptActionsField); 
		assert(lua_istable(L, -1));

		lua_pushlightuserdata(L, (void*)(intptr_t)(actionCount-1));
		lua_rawseti(L, -2, actionCount);
		lua_pop(L, 1); // pop ScriptActionsField
	}
	return 0;
}

static int LuaRemoveAction(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);

	if (nargs == 2) {
		luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script");
		luaL_argcheck(L, lua_isuserdata(L, 2), 2, "Expected action");
		
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata); // 3
		assert(lua_isuserdata(L, -1));
		auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
		lua_pop(L, 1); // pop ScriptDataUserdata

		int actionIdx = (intptr_t)lua_touserdata(L, 2);
		assert(actionIdx >= 0 && actionIdx < scriptData->Actions.size());

		{
			// deletes action & selection
			auto& action = scriptData->Actions[actionIdx];
			auto it = scriptData->Actions.find(action);
			if (it != scriptData->Actions.end()) {
				scriptData->Actions.erase(it);
				auto selIt = scriptData->selection.find(action);
				if(selIt != scriptData->selection.end()) {
					scriptData->selection.erase(selIt);
				}
			}
		}		

	
		// update actions
		lua_getfield(L, 1, OFS_LuaExtensions::ScriptActionsField); // 4
		assert(lua_istable(L, -1));
	
		// remove element
		int actionCount = scriptData->Actions.size();
		lua_pushnil(L);
		lua_rawseti(L, -2, actionCount+1);

		if (actionCount > 0) {
			for (int i = 0, size = actionCount; i < size; ++i) {
				lua_pushlightuserdata(L, (void*)(intptr_t)i);
				assert(lua_istable(L, -2));
				lua_rawseti(L, -2, i + 1);
			}
		}
		lua_pop(L, 1); // pop ScriptActionsField
	}
	return 0;
}

static int LuaRemoveSelected(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);
	luaL_argcheck(L, lua_istable(L, 1), 1, "Expected script.");
	
	lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata);
	assert(lua_isuserdata(L, -1));
	auto scriptData = (Funscript::FunscriptData*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop ScriptDataUserdata

	auto& removeActions = scriptData->selection;
	auto it = std::remove_if(scriptData->Actions.begin(), scriptData->Actions.end(),
		[&removeActions, end = removeActions.end()](auto action) {
			if (removeActions.find(action) != end) {
				return true;
			}
			return false;
		});
	scriptData->Actions.erase(it, scriptData->Actions.end());
	scriptData->selection.clear();
	return 0;
}

static int LuaSaveScript(lua_State* L) noexcept 
{
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	bool saved = false;
	if(nargs >= 1) {
		luaL_argcheck(L, lua_isnumber(L, 1), 1, "Expected script index.");
		int scriptIdx = lua_tonumber(L, 1) - 1;
		luaL_argcheck(L, scriptIdx >= 0 && scriptIdx < app->LoadedFunscripts().size(), 1, "Script index is invalid");
		auto& script = app->LoadedFunscripts()[scriptIdx];
		const char* savePath = script->Path().c_str();

		if(nargs >= 2) {
			luaL_argcheck(L, lua_isstring(L, 2), 2, "Expected path string.");
			savePath = lua_tostring(L, 2);
		}

		script->save(savePath, false);
		// HACK: when "script->save" returns the file might not exist yet.
		//		 to avoid race conditions we wait 2 ms.
		//       we need to add a synchronous funscript save
		SDL_Delay(2);
		saved = true;
	}
	lua_pushboolean(L, saved);
	return 1;
}

static int LuaScriptPath(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	if(nargs >= 1) {
		CLEAN_STACK_CHECK(L, 0);
		luaL_argcheck(L, lua_isnumber(L, 1), 1, "Expected script index.");
		int scriptIdx = lua_tonumber(L, 1) - 1;
		luaL_argcheck(L, scriptIdx >= 0 && scriptIdx < app->LoadedFunscripts().size(), 1, "Script index is invalid");
		auto& script = app->LoadedFunscripts()[scriptIdx];
		lua_pushstring(L, script->Path().c_str());
		return 1;
	}
	return 0;
}

static int LuaGetScript(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		CLEAN_STACK_CHECK(L, 1);
		assert(lua_isnumber(L, 1));
		lua_Integer index = lua_tointeger(L, 1);

		if (index >= 1 && index <= app->LoadedFunscripts().size()) {
			auto& script = app->LoadedFunscripts()[index - 1];
			lua_createtable(L, 0, 3); // 2

			lua_pushlightuserdata(L, (void*)(intptr_t)(index - 1));
			lua_setfield(L, -2, OFS_LuaExtensions::ScriptIdxUserdata); // pops off
			
			auto scriptData = new Funscript::FunscriptData(script->Data());
			lua_pushlightuserdata(L, (void*)scriptData);
			lua_setfield(L, -2, OFS_LuaExtensions::ScriptDataUserdata); // pops off

			auto gcFunc = [](lua_State* L) -> int {
				lua_getfield(L, 1, OFS_LuaExtensions::ScriptDataUserdata); // 3
				assert(lua_isuserdata(L, -1));
				auto data = (Funscript::FunscriptData*)lua_touserdata(L, -1);
				assert(data);
				if(data) {
					delete data;
				}
				return 0;
			};

			lua_createtable(L, 0, 1);
			lua_pushcfunction(L, gcFunc);
			lua_setfield(L, -2, "__gc");
			int f = lua_setmetatable(L, 2);

			ActionGetterSetter(L, scriptData);
			lua_setglobal(L, OFS_LuaExtensions::GlobalActionMetaTable); // this gets reused for every action

			lua_createtable(L, script->Actions().size(), 2); // 3
			for(int i=0, size=script->Actions().size(); i < size; ++i) {
				lua_pushlightuserdata(L, (void*)(intptr_t)i);
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
	CLEAN_STACK_CHECK(L, 1);
	auto app = OpenFunscripter::ptr;
	auto& ext = app->extensions;

	int nargs = lua_gettop(L);
	if (nargs >= 1) {
		luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected string.");
		const char* functionName = lua_tostring(L, 1);

		OFS_LuaTask task;
		task.L = L;
		task.Function = functionName;
		ext->Tasks.emplace(std::move(task));
	}

	return 0;
}

static int LuaBindFunction(lua_State* L) noexcept
{
	CLEAN_STACK_CHECK(L, 0);
	auto app = OpenFunscripter::ptr;
	int nargs = lua_gettop(L);
	luaL_argcheck(L, lua_isstring(L, 1), 1, "Expected function name.");
	const char* str = lua_tostring(L, 1);
	lua_getglobal(L, str);
	if (!lua_isfunction(L, -1)) {
		LOGF_ERROR("LUA: ofs.Bind: function: \"%s\" not found.", str);
		return 0;
	}
	lua_pop(L, 1);

	lua_getglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);
	assert(lua_isuserdata(L, -1));
	OFS_LuaExtension* ext = (OFS_LuaExtension*)lua_touserdata(L, -1);
	lua_pop(L, 1); // pop GlobalExtensionPtr

	OFS_BindableLuaFunction func;
	func.Name = str;
	func.GlobalName = Util::Format("%s::%s", ext->Name.c_str(), func.Name.c_str());

	if (nargs >= 2) {
		luaL_argcheck(L, lua_isstring(L, 2), 2, "Expected function description");
		func.Description = lua_tostring(L, 2);
	}

	auto [item, suc] = ext->Bindables.emplace(eastl::make_pair(func.GlobalName, std::move(func)));
	if (!suc) {
		item->second = func;
	}
	return 0;
}

void OFS_LuaExtensions::RemoveNonExisting() noexcept
{
	Extensions.erase(std::remove_if(Extensions.begin(), Extensions.end(), [](auto& ext) {
		return !Util::DirectoryExists(ext.Directory);
		}), Extensions.end());
}

void OFS_LuaExtensions::UpdateExtensionList() noexcept
{
	auto extensionDir = Util::Prefpath(ExtensionDir);
	std::error_code ec;
	std::filesystem::create_directories(extensionDir, ec);
	std::filesystem::directory_iterator dirIt(extensionDir, ec);
	
	RemoveNonExisting();

	for (auto it : dirIt) {
		if (it.is_directory()) {
			auto Name = it.path().filename().u8string();
			auto Directory = it.path().u8string();
			auto Hash = Util::Hash(Directory.c_str(), Directory.size());
			bool skip = std::any_of(Extensions.begin(), Extensions.end(), 
				[&](auto& a) {
				return a.Hash == Hash;
			});
			if (!skip) {
				auto& ext = Extensions.emplace_back();
				ext.Name = std::move(Name);
				ext.NameId = Util::Format("%s##_%s_", ext.Name.c_str(), ext.Name.c_str());
				ext.Directory = std::move(Directory);
				ext.Hash = Hash;
			}
		}
	}
}

OFS_LuaExtensions::OFS_LuaExtensions() noexcept
{
	load(Util::Prefpath("extension.json"));
	UpdateExtensionList();
	
	OFS_CoreExtension::setup();

	auto app = OpenFunscripter::ptr;
	app->keybinds.registerDynamicHandler(OFS_LuaExtensions::DynamicBindingHandler, [this](Binding* b) { HandleBinding(b); });

	for (auto& ext : Extensions) {
		if (ext.Active) ext.Load(ext.Directory);
	}
}

OFS_LuaExtensions::~OFS_LuaExtensions() noexcept
{
	save();
	for (auto& ext : Extensions) ext.Shutdown();
}

void OFS_LuaExtensions::load(const std::string& path) noexcept
{
	LastConfigPath = path;
	bool suc;
	auto json = Util::LoadJson(path, &suc);
	if (suc) {
		OFS::serializer::load(this, &json);
		RemoveNonExisting();		
	}
}

void OFS_LuaExtensions::save() noexcept
{
	nlohmann::json json;
	OFS::serializer::save(this, &json);
	Util::WriteJson(json, LastConfigPath, true);
}

void OFS_LuaExtensions::Update(float delta) noexcept
{
	for (auto& ext : this->Extensions) {
		if (!ext.Active || !ext.ExtensionError.empty() || this->TaskBusy) continue;
		auto startTime = std::chrono::high_resolution_clock::now();
		{
			CLEAN_STACK_CHECK(ext.L, 0);
			lua_getglobal(ext.L, OFS_LuaExtensions::UpdateFunction);
			lua_pushnumber(ext.L, delta);
			int result = Lua_Pcall(ext.L, 1, 0, 0); 
			if (result) {
				auto error = lua_tostring(ext.L, -1);
				LOG_ERROR(error);
				ext.Fail(error);
			}
		}
		std::chrono::duration<float> updateDuration = std::chrono::high_resolution_clock::now() - startTime;
		ext.UpdateTime = updateDuration.count();
		if (ext.UpdateTime > ext.MaxUpdateTime) {
			ext.MaxUpdateTime = ext.UpdateTime;
		}
	}
	
	if (!Tasks.empty() && !this->TaskBusy) {
		auto app = OpenFunscripter::ptr;
		auto taskData = std::make_unique<BlockingTaskData>();
		taskData->TaskDescription = "Lua extension";
		taskData->TaskThreadFunc = [](void* data)->int
		{
			BlockingTaskData* task = (BlockingTaskData*)data;
			OFS_LuaExtensions* ext = (OFS_LuaExtensions*)task->User;
			task->MaxProgress = ext->Tasks.size();
			while (!ext->Tasks.empty()) {
				OFS_LuaTask& lua = ext->Tasks.front();
				{
					CLEAN_STACK_CHECK(lua.L, 0);
					lua_getglobal(lua.L, lua.Function.c_str());
					if (lua_isfunction(lua.L, -1)) {
						int result = Lua_Pcall(lua.L, 0, 0, 0);
						if (result) {
							const char* error = lua_tostring(lua.L, -1);
							LOG_ERROR(error);
							lua_pop(lua.L, 1);
						}
					}
				}
				ext->Tasks.pop();
				++task->Progress;
			}
			ext->TaskBusy = false;
			return 0;
		};
		this->TaskBusy = true;
		taskData->User = this;
		taskData->DimBackground = false;
		app->blockingTask.DoTask(std::move(taskData));
	}
}

inline static void ShowExtensionLogWindow(bool* open) noexcept
{
	if(!*open) return;
	ExtensionLogBuffer.Draw("Extension Log Output", open);
}

void OFS_LuaExtensions::ShowExtensions() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto app = OpenFunscripter::ptr;
	ShowExtensionLogWindow(&OFS_LuaExtensions::ShowLogs);
	for (auto& ext : this->Extensions) {
		if (!ext.WindowOpen || !ext.Active || this->TaskBusy) continue;	

		ImGui::Begin(ext.NameId.c_str(), &ext.WindowOpen, ImGuiWindowFlags_None);
		if (!ext.ExtensionError.empty()) {
			ImGui::TextWrapped("Error:\n%s", ext.ExtensionError.c_str());
			if (ImGui::Button("Try reloading")) {
				ext.Load(Util::PathFromString(ext.Directory));
			}
			ImGui::End(); continue;
		}

		if (DevMode && ImGui::Button("Reload", ImVec2(-1.f, 0.f))) { 
			if (!ext.Load(Util::PathFromString(ext.Directory))) {
				ImGui::End();
				continue;
			}
		}
		ImGui::SetWindowSize(ImVec2(300.f, 200.f), ImGuiCond_FirstUseEver);
			
		auto startTime = std::chrono::high_resolution_clock::now();
		

		{		
			CLEAN_STACK_CHECK(ext.L, 0);
			lua_getglobal(ext.L, RenderGui);
			int result = Lua_Pcall(ext.L, 0, 0, 0);
			if (result) {
				const char* error = lua_tostring(ext.L, -1);
				LOG_ERROR(error);
				ext.Fail(error);
			}
		}

		if(DevMode && ext.L)
		{   // benchmark
			ImGui::Separator();
			std::chrono::duration<float> duration = std::chrono::high_resolution_clock::now() - startTime;
			if (duration.count() > ext.MaxGuiTime) ext.MaxGuiTime = duration.count();
			ImGui::Text("Memory usage: %s", Util::FormatBytes(ext.L->l_G->totalbytes));
			ImGui::Text("Update: %f ms", ext.UpdateTime * 1000.f);
			ImGui::Text("Update (slowest): %f ms", ext.MaxUpdateTime * 1000.f);
			ImGui::Text("GUI: %f ms", duration.count() * 1000.f);
			ImGui::Text("GUI (slowest): %f ms", ext.MaxGuiTime * 1000.f);
			#ifndef NDEBUG
			ImGui::Separator();
			ImGui::Text("Stack size: %lld bytes", (intptr_t)((intptr_t)ext.L->top - (intptr_t)ext.L->stack));
			ImGui::Text("Stack value size: %lld bytes", sizeof(StackValue));
			ImGui::Text("Stack values: %lld", ext.L->top - ext.L->stack);
			#endif
		}
		if (!ext.Bindables.empty()) {
			auto& style = ImGui::GetStyle();
			if (ImGui::CollapsingHeader("Bindable functions")) {
				for (auto& bindP : ext.Bindables) {
					auto& bind = bindP.second;
					ImGui::Text("%s:", bind.GlobalName.c_str()); ImGui::SameLine();
					if (!bind.Description.empty()) {
						ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
						ImGui::TextWrapped("%s", bind.Description.c_str());
						ImGui::PopStyleColor(1);
					}
					OFS::Tooltip(bind.Description.c_str());
				}
				ImGui::Separator();
			}
		}
		ImGui::End();
	}
}

void OFS_LuaExtensions::HandleBinding(Binding* binding) noexcept
{
	OFS_BindableLuaFunction tmp;
	tmp.GlobalName = binding->identifier;

	for (auto& ext : Extensions) {
		if (!ext.Active || !ext.L) continue;
		auto it = ext.Bindables.find(binding->identifier);
		if (it != ext.Bindables.end()) {
			OFS_LuaTask task;
			task.L = ext.L;
			task.Function = it->second.Name;
			Tasks.emplace(std::move(task));
			return;
		}
	}
}

void OFS_LuaExtensions::ReloadEnabledExtensions() noexcept
{
	for(auto& ext : Extensions) {
		if(ext.Active) {
			ext.Load(Util::PathFromString(ext.Directory));
		}
	}
}

// ============================================================ Extension

bool OFS_LuaExtension::Load(const std::filesystem::path& directory) noexcept
{
	auto mainFile = directory / OFS_LuaExtension::MainFile;
	Directory = directory.u8string(); 
	Hash = Util::Hash(Directory.c_str(), Directory.size());
	NameId = directory.filename().u8string();
	NameId = Util::Format("%s##_%s_", Name.c_str(), Name.c_str());
	ExtensionError.clear();

	std::vector<uint8_t> extensionText;
	if (!Util::ReadFile(mainFile.u8string().c_str(), extensionText)) {
		FUN_ASSERT(false, "no file");
		return false;
	}
	extensionText.emplace_back('\0');

	if (L) { lua_close(L); L = 0; }
	UpdateTime = 0.f;
	MaxUpdateTime = 0.f;
	MaxGuiTime = 0.f;
	Bindables.clear();

	L = luaL_newstate();
	luaL_openlibs(L);
	love::luasocket::__open(L);

	lua_getglobal(L, "_G");
	luaL_setfuncs(L, printlib, 0);

	// put all ofs functions into a ofs table
	lua_createtable(L, 0, sizeof(ofsLib) / sizeof(luaL_Reg) + sizeof(imguiLib) / sizeof(luaL_Reg));
	luaL_setfuncs(L, ofsLib, 0);
	luaL_setfuncs(L, imguiLib, 0);
	lua_setglobal(L, OFS_LuaExtensions::DefaultNamespace);

	lua_createtable(L, 0, sizeof(playerLib) / sizeof(luaL_Reg));
	luaL_setfuncs(L, playerLib, 0);
	lua_setglobal(L, OFS_LuaExtensions::PlayerNamespace);

	auto addToLuaPath = [](lua_State* L, const char* path)
	{
		lua_getglobal(L, "package");
		lua_getfield(L, -1, "path"); // get field "path" from table at top of stack (-1)
		std::string cur_path = lua_tostring(L, -1); // grab path string from top of stack
		cur_path.append(";"); // do your path magic here
		cur_path.append(path);
		lua_pop(L, 1); // get rid of the string on the stack we just pushed on line 5
		lua_pushstring(L, cur_path.c_str()); // push the new one
		lua_setfield(L, -2, "path"); // set the field "path" in table at -2 with value at top of stack
		lua_pop(L, 1); // get rid of package table from top of stack
	};
	{
		auto dirPath = Util::PathFromString(Directory);
		addToLuaPath(L, (dirPath / "?.lua").u8string().c_str());
		addToLuaPath(L, (dirPath / "lib" / "?.lua").u8string().c_str());
	}

	lua_pushlightuserdata(L, this);
	lua_setglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);

	int status = luaL_dostring(L, LuaDefaultFunctions);
	FUN_ASSERT(status == 0, "defaults failed");
	status = luaL_dostring(L, (const char*)extensionText.data());
	if (status != 0) {
		const char* error = lua_tostring(L, -1);
		LOG_ERROR(error);
		Fail(error);
		return false;
	}

	{
		CLEAN_STACK_CHECK(L, 0);
		lua_getglobal(L, OFS_LuaExtensions::InitFunction);
		status = Lua_Pcall(L, 0, 0, 0);
		if (status != 0) {
			const char* error = lua_tostring(L, -1);
			LOG_ERROR(error);
			Fail(error);
			return false;
		}
	}

	auto app = OpenFunscripter::ptr;
	for (auto& bindP : Bindables) {
		auto& bind = bindP.second;
		Binding binding(
			bind.GlobalName,
			bind.GlobalName,
			false,
			[](void* user) {} // this gets handled by OFS_LuaExtensions::HandleBinding
		);
		binding.dynamicHandlerId = OFS_LuaExtensions::DynamicBindingHandler;
		app->keybinds.addDynamicBinding(std::move(binding));
	}

	return true;
}