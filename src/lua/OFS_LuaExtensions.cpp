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
	Extensions.reserve(100); // NOTE: this is mitigate a relocation bug
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
	bool succ;
	auto jsonText = Util::ReadFileString(path.c_str());
	auto json = Util::ParseJson(jsonText, &succ);
	if (succ) {
		OFS::Serializer<false>::Deserialize(*this, json);
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
	for(auto& ext : Extensions)	{
		if(!ext.Active) continue;
		ext.ScriptChanged(scriptIdx);
	}
}

void OFS_LuaExtensions::save() noexcept
{
	nlohmann::json json;
	OFS::Serializer<false>::Serialize(*this, json);
	auto jsonText = Util::SerializeJson(json, true);
	Util::WriteFile(LastConfigPath.c_str(), jsonText.data(), jsonText.size());
}

void OFS_LuaExtensions::removeNonExisting() noexcept
{
	Extensions.erase(std::remove_if(Extensions.begin(), Extensions.end(), 
		[](auto& ext) {
			return !Util::DirectoryExists(ext.Directory);
		}), Extensions.end());
}

void OFS_LuaExtensions::UpdateExtensionList() noexcept
{
	auto extensionDir = Util::Prefpath(ExtensionDir);
	Util::CreateDirectories(extensionDir);
	std::error_code ec;
	std::filesystem::directory_iterator dirIt(extensionDir, ec);
	
	removeNonExisting();

	for (auto it : dirIt) {
		if (it.is_directory()) {
			auto Name = it.path().filename().u8string();
			auto Directory = it.path().u8string();
			bool skip = std::any_of(Extensions.begin(), Extensions.end(), 
				[&](auto& a) {
				return a.Name == Name;
			});
			if (!skip) {
				auto& ext = Extensions.emplace_back();
				ext.Name = std::move(Name);
				ext.NameId = Util::Format("%s##_%s_", ext.Name.c_str(), ext.Name.c_str());
				ext.Directory = std::move(Directory);
			}
		}
	}
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