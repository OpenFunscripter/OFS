#include "OFS_LuaExtension.h"
#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OpenFunscripter.h"

#include <string>

void OFS_LuaExtension::Toggle() noexcept
{
    if (!this->Active) {
        this->Active = this->Load();
    }
    else { 
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
		ImGui::End();
		return;
	}

	if(OFS_LuaExtensions::DevMode) {
		if(ImGui::Button(TR(RELOAD), ImVec2(-1.f, 0.f))) {
			Load();
			ImGui::End();
			return;
		}
		ImGui::Separator();
	}

	auto gui = L.get<sol::protected_function>(OFS_LuaExtensions::RenderGui);
	auto res = gui();
	if(res.status() != sol::call_status::ok) {
		auto err = sol::stack::get_traceback_or_errors(L.lua_state());
		AddError(err.what());
	}
	if(!api->guiAPI->Validate()) {
		AddError(api->guiAPI->Error().c_str());
	}
	ImGui::End();
}

void OFS_LuaExtension::Update() noexcept
{
	if(!Active) return;
	auto update = L.get<sol::protected_function>(OFS_LuaExtensions::UpdateFunction);
	auto res = update(ImGui::GetIO().DeltaTime);
	if(res.status() != sol::call_status::ok)
	{
		auto err = sol::stack::get_traceback_or_errors(L.lua_state());
		AddError(err.what());
	}
}

bool OFS_LuaExtension::Load() noexcept
{
    auto directory = Util::PathFromString(this->Directory);
    auto mainFile = directory / OFS_LuaExtension::MainFile;

	NameId = directory.filename().u8string();
	NameId = Util::Format("%s##_%s_", Name.c_str(), Name.c_str());
	ClearError();

	std::string extensionText;
	{
		std::vector<uint8_t> dataBuf;
		if (!Util::ReadFile(mainFile.u8string().c_str(), dataBuf)) {
			FUN_ASSERT(false, "no file");
			return false;
		}
		extensionText = std::string((char*)dataBuf.data(), dataBuf.size());
	}

	//UpdateTime = 0.f;
	//MaxUpdateTime = 0.f;
	//MaxGuiTime = 0.f;
	//Bindables.clear();

	L = sol::state();
	L.open_libraries(
		sol::lib::base,
		sol::lib::package,
		sol::lib::coroutine,
		sol::lib::string,
		sol::lib::os,
		sol::lib::table,
		sol::lib::math,
		sol::lib::utf8,
		sol::lib::io
	);

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
		addToLuaPath(L.lua_state(), (dirPath / "?.lua").u8string().c_str());
		addToLuaPath(L.lua_state(), (dirPath / "lib" / "?.lua").u8string().c_str());
	}

	auto ofs = L.new_usertype<OFS_ExtensionAPI>("ofs");
	ofs["Version"] = []() noexcept { return OFS_ExtensionAPI::VersionAPI; };
	ofs["ExtensionDir"] = [this]() noexcept { return this->Directory.c_str(); };
	ofs["ScriptCount"] = []() noexcept { return OpenFunscripter::ptr->LoadedFunscripts().size(); };
	ofs["ScriptName"] = [](lua_Integer idx) noexcept -> const char* {
		auto app = OpenFunscripter::ptr;
		idx -= 1;
		if(idx >= 0 && idx < app->LoadedFunscripts().size()) {
			return app->LoadedFunscripts()[idx]->Title.c_str();
		}
		return nullptr;
	};

	api = std::make_unique<OFS_ExtensionAPI>(ofs);
	
	L[OFS_LuaExtensions::GlobalExtensionPtr] = this;

#ifndef NDEBUG
	L.set_exception_handler([](lua_State* L, auto optEx, std::string_view msg)
	{
		__debugbreak();
		return 0;
	});

	L.set_panic([](lua_State* L)
	{
		__debugbreak();
		return 0;
	});
#endif

	try
	{
		auto res = L.safe_script(extensionText);
		FUN_ASSERT(res.valid(), "what");

		auto init = L.get<sol::protected_function>(OFS_LuaExtensions::InitFunction);
		res = init();
		if(res.status() != sol::call_status::ok) {
			auto err = sol::stack::get_traceback_or_errors(L.lua_state());
			AddError(err.what());
			return false;
		}
	}
	catch(const std::exception& e)
	{
		AddError(e.what());
		return false;
	}

	sol::table btable = L[OFS_LuaExtension::BindingTable];
	if(btable.valid()) {
		auto app = OpenFunscripter::ptr;
		for(auto binding : btable) {
			auto key = sol::stack::push_pop(binding.first);
			const char* keyStr = lua_tostring(binding.first.lua_state(), key.m_index);
			std::string name = keyStr;
			std::string globalName = Util::Format("%s::%s", Name.c_str(), keyStr);
			app->extensions->AddBinding(NameId, globalName, name);
		}
	}

	return true;
}

void OFS_LuaExtension::Execute(const std::string& func) noexcept
{
	sol::protected_function bind = L[OFS_LuaExtension::BindingTable][func];
	if(bind.valid()) {
		auto res = bind();
		if(res.status() != sol::call_status::ok) {
			auto err = sol::stack::get_traceback_or_errors(L.lua_state());
			AddError(err.what());
		}
	}
}

void OFS_LuaExtension::ScriptChanged(uint32_t scriptIdx) noexcept
{
	sol::protected_function change = L[OFS_LuaExtension::ScriptChangeFunction];
	if(change.valid()) {
		auto res = change(scriptIdx + 1);
		if(res.status() != sol::call_status::ok) {
			auto err = sol::stack::get_traceback_or_errors(L.lua_state());
			AddError(err.what());
		}
	}
}

void OFS_LuaExtension::Shutdown() noexcept
{
	// UpdateTime = 0.f;
	// MaxUpdateTime = 0.f;
	// MaxGuiTime = 0.f;
	// Bindables.clear();
	L = sol::state();
	Active = false;
}