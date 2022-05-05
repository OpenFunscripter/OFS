#include "OFS_LuaExtension.h"
#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"

#include "EASTL/string.h"
#include "LuaBridge/LuaBridge.h"

constexpr const char* LuaDefaultFunctions = R"(
function clamp(val, min, max)
	return math.min(max, math.max(val, min))
end
)";

static int LuaPrint(lua_State* L) noexcept
{
	int nargs = lua_gettop(L);

	eastl::string logMsg;
	logMsg.reserve(1024);

	lua_getglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);
	FUN_ASSERT(lua_isuserdata(L, -1), "Missing extension pointer.");
	auto ext = (OFS_LuaExtension*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	logMsg.append_sprintf("[%s]: ", ext->Name.c_str());
	for (int i = 1; i <= nargs; ++i) {
		const char* str = lua_tostring(L, i);
		if (str != nullptr) {
			logMsg.append(str);
		}
	}

	logMsg.append(1, '\n');
	OFS_LuaExtensions::ExtensionLogBuffer.AddLog(logMsg.c_str());
	return 0;
}

void OFS_LuaExtension::Toggle() noexcept
{
    if (this->Active && !this->L) {
        this->Active = this->Load();
    }
    else if (!this->Active) { 
        this->Shutdown(); 
    }
}

void OFS_LuaExtension::ShowWindow() noexcept
{
	if(!WindowOpen || !Active) return;
	ImGui::Begin(NameId.c_str(), &WindowOpen, ImGuiWindowFlags_None);
	if(!Error.empty())
	{
		ImGui::TextWrapped("Error:\n%s", Error.c_str());
		if (ImGui::Button("Try reloading")) {
			Load();
		}
	}
	ImGui::End();
}

bool OFS_LuaExtension::Load() noexcept
{
    auto directory = Util::PathFromString(this->Directory);
    auto mainFile = directory / OFS_LuaExtension::MainFile;
	//Directory = directory.u8string(); 
	//Hash = Util::Hash(Directory.c_str(), Directory.size());

	NameId = directory.filename().u8string();
	NameId = Util::Format("%s##_%s_", Name.c_str(), Name.c_str());
	ClearError();

	std::vector<uint8_t> extensionText;
	if (!Util::ReadFile(mainFile.u8string().c_str(), extensionText)) {
		FUN_ASSERT(false, "no file");
		return false;
	}
	extensionText.emplace_back('\0');

	if (L) { 
        lua_close(L); 
        L = nullptr; 
    }
	//UpdateTime = 0.f;
	//MaxUpdateTime = 0.f;
	//MaxGuiTime = 0.f;
	//Bindables.clear();

	L = luaL_newstate();
	luaL_openlibs(L);
	{
		auto addToLuaPath = [](lua_State* L, const char* path) noexcept
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
		auto dirPath = Util::PathFromString(Directory);
		addToLuaPath(L, (dirPath / "?.lua").u8string().c_str());
		addToLuaPath(L, (dirPath / "lib" / "?.lua").u8string().c_str());
	}

	api = std::make_unique<OFS_ExtensionAPI>(L);

	luabridge::getGlobalNamespace(L)
		.addFunction("print", LuaPrint);
		

	lua_pushlightuserdata(L, this);
	lua_setglobal(L, OFS_LuaExtensions::GlobalExtensionPtr);

	int status = luaL_dostring(L, LuaDefaultFunctions);
	FUN_ASSERT(status == 0, "defaults failed");

	// put all ofs functions into a ofs table
	//lua_createtable(L, 0, sizeof(ofsLib) / sizeof(luaL_Reg) + sizeof(imguiLib) / sizeof(luaL_Reg));
	//luaL_setfuncs(L, ofsLib, 0);
	//luaL_setfuncs(L, imguiLib, 0);
	//lua_setglobal(L, OFS_LuaExtensions::DefaultNamespace);

	//lua_createtable(L, 0, sizeof(playerLib) / sizeof(luaL_Reg));
	//luaL_setfuncs(L, playerLib, 0);
	//lua_setglobal(L, OFS_LuaExtensions::PlayerNamespace);

	status = luaL_dostring(L, (const char*)extensionText.data());
	if (status != 0) {
		const char* error = lua_tostring(L, -1);
		LOG_ERROR(error);
		SetError(error);
		return false;
	}

	{
		auto init = luabridge::getGlobal(L, OFS_LuaExtensions::InitFunction);
		if(init.isNil())
		{
			SetError("No init() function found.");
			return false;
		}

		auto res = luabridge::call(init);
		if(!res.wasOk())
		{
			LOG_ERROR(res.errorMessage().c_str());
			SetError(res.errorMessage().c_str());
			return false;
		}
	}

	//auto app = OpenFunscripter::ptr;
	//for (auto& bindP : Bindables) {
	//	auto& bind = bindP.second;
	//	Binding binding(
	//		bind.GlobalName,
	//		bind.GlobalName,
	//		false,
	//		[](void* user) {} // this gets handled by OFS_LuaExtensions::HandleBinding
	//	);
	//	binding.dynamicHandlerId = OFS_LuaExtensions::DynamicBindingHandler;
	//	app->keybinds.addDynamicBinding(std::move(binding));
	//}

	return true;
}

void OFS_LuaExtension::Shutdown() noexcept
{

}