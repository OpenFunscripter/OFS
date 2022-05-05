#include "OpenFunscripter.h"
#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#include "LuaBridge/LuaBridge.h"

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
	
	//OFS_CoreExtension::setup();

	auto app = OpenFunscripter::ptr;
	//app->keybinds.registerDynamicHandler(OFS_LuaExtensions::DynamicBindingHandler, [this](Binding* b) { HandleBinding(b); });

	for (auto& ext : Extensions) {
		if (ext.Active) ext.Load();
	}
}

OFS_LuaExtensions::~OFS_LuaExtensions() noexcept
{
	save();
	for (auto& ext : Extensions) 
    {
        ext.Shutdown();
    }
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

void OFS_LuaExtensions::removeNonExisting() noexcept
{
	Extensions.erase(std::remove_if(Extensions.begin(), Extensions.end(), [](auto& ext) {
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
}

void OFS_LuaExtensions::ShowExtensions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
	ShowExtensionLogWindow(&OFS_LuaExtensions::ShowLogs);
	for(auto& ext : Extensions)
	{
		ext.ShowWindow();
	}
}

void OFS_LuaExtensions::ReloadEnabledExtensions() noexcept
{
    for(auto& ext : Extensions) {
		if(ext.Active) {
			//ext.Load(Util::PathFromString(ext.Directory));
		}
	}
}