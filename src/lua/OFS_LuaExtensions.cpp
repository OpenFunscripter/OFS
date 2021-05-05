#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_Serialization.h"
#include "OpenFunscripter.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <filesystem>
#include <algorithm>

//#ifdef WIN32
////used to obtain file age on windows
//#define WIN32_LEAN_AND_MEAN
//#include "windows.h"
//#endif

//static uint64_t GetWriteTime(const wchar_t* path) {
//	OFS_PROFILE(__FUNCTION__);
//	std::error_code ec;
//	uint64_t timestamp = 0;
//	HANDLE file = CreateFileW((wchar_t*)path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//	if (file == INVALID_HANDLE_VALUE) {	}
//	else {
//		FILETIME ftCreate;
//		GetFileTime(file, NULL, NULL, &ftCreate);
//		timestamp = *(uint64_t*)&ftCreate;
//		CloseHandle(file);
//	}
//	return timestamp;
//}


constexpr const char* LuaDefaultFunctions = R"(

function clamp(val, min, max)
	return math.min(max, math.max(val, min))
end

)";


#ifndef NDEBUG
bool OFS_LuaExtensions::DevMode = true;
#else
bool OFS_LuaExtensions::DevMode = false;
#endif
bool OFS_LuaExtensions::InMainThread = false;

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


static void ActionGetterSetter(lua_State* L, int scriptIndex) noexcept
{
	auto setter = [](lua_State* L) -> int {
		auto app = OpenFunscripter::ptr;
		int nargs = lua_gettop(L);
		assert(lua_isuserdata(L, lua_upvalueindex(1)));
		int scriptIndex = (intptr_t)lua_touserdata(L, lua_upvalueindex(1));
		if (nargs == 3 && scriptIndex >= 0 && scriptIndex < app->LoadedFunscripts().size()) {
			auto& script = app->LoadedFunscripts()[scriptIndex];
			FunscriptAction* action = (FunscriptAction*)lua_touserdata(L, 1);
			FunscriptAction newAction = *action;
			assert(action);
			const char* key = lua_tostring(L, 2);
			lua_Number value = lua_tonumber(L, 3);

			if (strcmp(key, "pos") == 0) {
				bool isSelected = script->IsSelected(*action); // TODO: awful perf
				newAction.pos = Util::Clamp(value, 0.0, 100.0);
				if (isSelected) {
					script->SetSelected(*action, false);
				}
				script->EditActionUnsafe(action, newAction);
				if (isSelected) {
					script->SetSelected(*action, true);
				}
			}
			else if (strcmp(key, "at") == 0) {
				bool isSelected = script->IsSelected(*action); // TODO: awful perf
				newAction.atS = value / 1000.0;
				if (isSelected) {
					script->SetSelected(*action, false);
				}
				script->EditActionUnsafe(action, newAction);
				if (isSelected) {
					script->SetSelected(*action, true);
				}
			}
			else if (strcmp(key, "selected") == 0) {
				script->SetSelected(*action, value);
			}
		}
		return 0;
	};

	auto getter = [](lua_State* L) -> int {
		auto app = OpenFunscripter::ptr;
		int nargs = lua_gettop(L);
		assert(lua_isuserdata(L, lua_upvalueindex(1)));
		int scriptIndex = (intptr_t)lua_touserdata(L, lua_upvalueindex(1));

		if (nargs == 2 && scriptIndex >= 0 && scriptIndex < app->LoadedFunscripts().size()) {
			auto& script = app->LoadedFunscripts()[scriptIndex];
			FunscriptAction* action = (FunscriptAction*)lua_touserdata(L, 1);
			assert(action);
			const char* key = lua_tostring(L, 2);
			if (strcmp(key, "pos") == 0) {
				lua_pushinteger(L, action->pos);
				return 1;
			}
			else if (strcmp(key, "at") == 0) {
				lua_pushnumber(L, (double)action->atS * 1000.0);
				return 1;
			}
			else if (strcmp(key, "selected") == 0) {
				lua_pushboolean(L, script->IsSelected(*action));
				return 1;
			}
		}
		return 0;
	};

	lua_createtable(L, 0, 2);
	lua_pushlightuserdata(L, (void*)(intptr_t)scriptIndex);
	lua_pushcclosure(L, getter, 1);
	//lua_pushcfunction(L, getter);
	lua_setfield(L, -2, "__index");

	lua_pushlightuserdata(L, (void*)(intptr_t)scriptIndex);
	lua_pushcclosure(L, setter, 1);
	//lua_pushcfunction(L, setter);
	lua_setfield(L, -2, "__newindex");

	//lua_setglobal(L, OFS_LuaExtensions::GlobalActionMetaTable);
}

int LuaGetActiveIdx(lua_State* L) noexcept;
int LuaGetScript(lua_State* L) noexcept;
int LuaAddAction(lua_State* L) noexcept;
int LuaRemoveAction(lua_State* L) noexcept;
int LuaBindFunction(lua_State* L) noexcept;
int LuaScheduleTask(lua_State* L) noexcept;
int LuaSnapshot(lua_State* L) noexcept;
static constexpr struct luaL_Reg ofsLib[] = {
	{"Script", LuaGetScript},
	{"AddAction", LuaAddAction},
	{"RemoveAction", LuaRemoveAction},
	{"ActiveIdx", LuaGetActiveIdx},
	{"Task", LuaScheduleTask},
	{"Snapshot", LuaSnapshot},
	{"Bind", LuaBindFunction},
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

			ActionGetterSetter(L, index - 1);
			lua_setglobal(L, OFS_LuaExtensions::GlobalActionMetaTable); // this gets reused for every action

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
		task.L = L;
		task.Function = functionName;
		ext->Tasks.emplace(std::move(task));
	}

	return 0;
}

static int LuaBindFunction(lua_State* L) noexcept
{
	auto app = OpenFunscripter::ptr;
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
	OFS_BindableLuaFunction func;
	func.Name = str;
	func.GlobalName = Util::Format("%s::%s", ext->Name.c_str(), func.Name.c_str());
	ext->Bindables.emplace(std::move(func));

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
				ext.NameId = Util::Format("%s##%s%c", ext.Name.c_str(), ext.Name.c_str(), 'X');
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

void OFS_LuaExtensions::ShowExtensions(bool* open) noexcept
{
	if (!*open) return;
	OFS_PROFILE(__FUNCTION__);
	
	auto app = OpenFunscripter::ptr;
	if (!app->blockingTask.Running) {
		for (auto& ext : this->Extensions) {
			if (!ext.Active || !ext.L) continue;
			ImGui::Begin(ext.NameId.c_str(), open, ImGuiWindowFlags_None);

			if (DevMode && !ext.Bindables.empty()) {
				ImGui::TextUnformatted("Bindable functions");
				for (auto& bind : ext.Bindables) {
					ImGui::TextDisabled("%s", bind.GlobalName.c_str());
				}
				ImGui::Separator();
			}

			if (DevMode && ImGui::Button("Reload", ImVec2(-1.f, 0.f))) { 
				ext.Load(Util::PathFromString(ext.Directory)); 
			}
			ImGui::SetWindowSize(ImVec2(300.f, 200.f), ImGuiCond_FirstUseEver);
			
			auto startTime = std::chrono::high_resolution_clock::now();
		
			lua_getglobal(ext.L, RenderGui);
			lua_pushnumber(ext.L, ImGui::GetIO().DeltaTime);
			OFS_LuaExtensions::InMainThread = true; // HACK
			int result = lua_pcall(ext.L, 1, 1, 0); // 1 arguments 1 result
			if (result) {
				const char* error = lua_tostring(ext.L, -1);
				LOG_ERROR(error);
				lua_pop(ext.L, 1);
				FUN_ASSERT(false, error);
			}

			if(DevMode)
			{   // benchmark
				std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - startTime;
				if (duration.count() > ext.MaxTime) ext.MaxTime = duration.count();
				ImGui::Text("Lua time: %lf ms", duration.count() * 1000.0);
				ImGui::Text("Lua slowest time: %lf ms", ext.MaxTime * 1000.0);
			}

			ImGui::End();
		}

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
					lua_getglobal(lua.L, lua.Function.c_str());
					if (lua_isfunction(lua.L, -1)) {
						OFS_LuaExtensions::InMainThread = false; // HACK
						int result = lua_pcall(lua.L, 0, 1, 0); // 0 arguments 1 result
						if (result) {
							const char* error = lua_tostring(lua.L, -1);
							LOG_ERROR(error);
							lua_pop(lua.L, 1);
						}
					}
					ext->Tasks.pop();
					++task->Progress;
				}
				return 0;
			};
			taskData->User = this;
			taskData->DimBackground = false;
			app->blockingTask.DoTask(std::move(taskData));
		}
	}
}

void OFS_LuaExtensions::HandleBinding(Binding* binding) noexcept
{
	OFS_BindableLuaFunction tmp;
	tmp.GlobalName = binding->identifier;

	for (auto& ext : Extensions) {
		if (!ext.Active || !ext.L) continue;
		auto it = ext.Bindables.find(tmp);
		if (it != ext.Bindables.end()) {
			auto& t = Tasks.emplace();
			t.L = ext.L;
			t.Function = it->Name;
			//lua_getglobal(ext.L, it->Name.c_str());
			//if (lua_isfunction(ext.L, -1)) {
			//	int status = lua_pcall(ext.L, 0, 1, 0); // 0 arguments 1 results
			//	if (status) {
			//		const char* error = lua_tostring(ext.L, -1);
			//		LOG_ERROR(error);
			//		lua_pop(ext.L, 1);
			//	}
			//}
			return;
		}
		// no idea how to use this...
		//auto it = ext.Bindables.find_as(binding->identifier,
		//	[](const auto& a,  const std::string& id) {
		//		return a.GlobalName < id;
		//	});
	}
}

// ============================================================ Extension

bool OFS_LuaExtension::Load(const std::filesystem::path& directory) noexcept
{
	auto mainFile = directory / OFS_LuaExtension::MainFile;
	Directory = directory.u8string(); 
	Hash = Util::Hash(Directory.c_str(), Directory.size());
	NameId = directory.filename().u8string();
	NameId = Util::Format("%s##%s%c", Name.c_str(), Name.c_str(), 'X');

	std::vector<uint8_t> extensionText;
	if (!Util::ReadFile(mainFile.u8string().c_str(), extensionText)) {
		FUN_ASSERT(false, "no file");
		return false;
	}
	extensionText.emplace_back('\0');

	if (L) { lua_close(L); L = 0; }

	L = luaL_newstate();
	luaL_openlibs(L);
	lua_getglobal(L, "_G");
	luaL_setfuncs(L, printlib, 0);

	// put all ofs functions into a ofs table
	lua_createtable(L, 0, sizeof(ofsLib) / sizeof(luaL_Reg) + sizeof(imguiLib) / sizeof(luaL_Reg));
	luaL_setfuncs(L, ofsLib, 0);
	luaL_setfuncs(L, imguiLib, 0);
	lua_setglobal(L, "ofs");

	lua_pushlightuserdata(L, this);
	lua_setglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);

	int status = luaL_dostring(L, LuaDefaultFunctions);
	FUN_ASSERT(status == 0, "defaults failed");
	status = luaL_dostring(L, (const char*)extensionText.data());
	if (status != 0) {
		const char* error = lua_tostring(L, -1);
		LOG_ERROR(error);
		lua_pop(L, 1);
		return false;
	}

	lua_getglobal(L, OFS_LuaExtensions::InitFunction);
	status = lua_pcall(L, 0, 1, 0); // 0 arguments 1 results
	if (status != 0) {
		const char* error = lua_tostring(L, -1);
		LOG_ERROR(error);
		lua_pop(L, 1);
		return false;
	}

	auto app = OpenFunscripter::ptr;
	for (auto& bind : Bindables) {
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