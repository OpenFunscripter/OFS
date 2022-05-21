#include "OpenFunscripter.h"
#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_LuaCoreExtension.h"

bool OFS_LuaExtensions::DevMode = false;
bool OFS_LuaExtensions::ShowLogs = false;

OFS::AppLog OFS_LuaExtensions::ExtensionLogBuffer;

inline static void ShowExtensionLogWindow(bool* open) noexcept
{
	if(!*open) return;
	OFS_LuaExtensions::ExtensionLogBuffer.Draw("Extension Log Output", open);
}

OFS_LuaExtensions::OFS_LuaExtensions() noexcept
{
	load(Util::Prefpath("extension.json"));
	UpdateExtensionList();
	
	OFS_CoreExtension::setup();

	auto app = OpenFunscripter::ptr;
	app->keybinds.registerDynamicHandler(OFS_LuaExtensions::DynamicBindingHandler, 
		[this](Binding* b) { HandleBinding(b); }
	);
}

OFS_LuaExtensions::~OFS_LuaExtensions() noexcept
{
	save();
	for (auto& ext : Extensions) {
        ext.Shutdown();
    }
}

bool OFS_LuaExtensions::Init() noexcept
{
	for (auto& ext : Extensions) {
		if (ext.Active) ext.Load();
	}
	return true;
}

void OFS_LuaExtensions::load(const std::string& path) noexcept
{
	LastConfigPath = path;
	bool suc;
	auto json = Util::LoadJson(path, &suc);
	if (suc) {
		OFS::serializer::load(this, &json);
		removeNonExisting();		
	}
}

void OFS_LuaExtensions::HandleBinding(Binding* b) noexcept
{
	auto it = Bindings.find(b->identifier);
	if(it != Bindings.end()) {
		for(auto& ext : Extensions) {
			if(!ext.Active) continue;
			if(ext.NameId == it->second.ExtensionId) {
				ext.Execute(it->second.Name);
				break;
			}
		}
	}
}

void OFS_LuaExtensions::ScriptChanged(uint32_t scriptIdx) noexcept
{
	for(auto& ext : Extensions)
	{
		if(!ext.Active) continue;
		ext.ScriptChanged(scriptIdx);
	}
}

void OFS_LuaExtensions::removeNonExisting() noexcept
{
	Extensions.erase(std::remove_if(Extensions.begin(), Extensions.end(), 
		[](auto& ext) {
			return !Util::DirectoryExists(ext.Directory);
		}), Extensions.end());
}

void OFS_LuaExtensions::save() noexcept
{
	nlohmann::json json;
	OFS::serializer::save(this, &json);
	Util::WriteJson(json, LastConfigPath, true);
}

void OFS_LuaExtensions::UpdateExtensionList() noexcept
{
}

void OFS_LuaExtensions::Update(float delta) noexcept
{
	for(auto& ext : Extensions) {
		ext.Update();
	}
}

void OFS_LuaExtensions::ShowExtensions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
	ShowExtensionLogWindow(&OFS_LuaExtensions::ShowLogs);
	for(auto& ext : Extensions) {
		ext.ShowWindow();
	}
}

void OFS_LuaExtensions::ReloadEnabledExtensions() noexcept
{
    for(auto& ext : Extensions) {
		if(ext.Active) {
			ext.Load();
		}
	}
}

void OFS_LuaExtensions::AddBinding(const std::string& extId, const std::string& uniqueId, const std::string& name) noexcept
{
	OFS_LuaBinding LuaBinding {uniqueId, extId, name};
	Bindings.emplace(std::make_pair(uniqueId, std::move(LuaBinding)));

	auto app = OpenFunscripter::ptr;
	Binding binding(
		uniqueId,
		uniqueId,
		true,
		[](void* user) {} // this gets handled by OFS_LuaExtensions::HandleBinding
	);
	binding.dynamicHandlerId = OFS_LuaExtensions::DynamicBindingHandler;
	app->keybinds.addDynamicBinding(std::move(binding));
}