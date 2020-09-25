#include "OpenFunscripterSettings.h"
#include "OpenFunscripterUtil.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>


OpenFunscripterSettings::OpenFunscripterSettings(const std::string& keybinds, const std::string& config)
	: keybinds_path(keybinds), config_path(config)
{
	if (Util::FileExists(keybinds)) {
		keybindsObj = Util::LoadJson(keybinds);
	}
	if (Util::FileExists(config)) {
		configObj = Util::LoadJson(config);
	}

	if(!keybindsObj[KeybindsStr].is_array())
		keybindsObj[KeybindsStr] = nlohmann::json().array();
	
	if (!configObj[ConfigStr].is_object())
		configObj[ConfigStr] = nlohmann::json().object();
	else
		load_config();
}

void OpenFunscripterSettings::save_keybinds()
{
	Util::WriteJson(keybindsObj, keybinds_path, true);
}

void OpenFunscripterSettings::save_config()
{
	Util::WriteJson(configObj, config_path, true);
}

#define LOAD_CONFIG(member) if (config().contains( #member ))\
 {\
	auto ptr = config()[ #member ].get_ptr<decltype( ScripterSettingsData::member )*>();\
	if(ptr != nullptr) scripterSettings.member = *ptr;\
 }
void OpenFunscripterSettings::load_config()
{
	LOAD_CONFIG(last_path)
	LOAD_CONFIG(last_opened_file)
	LOAD_CONFIG(draw_video)
	LOAD_CONFIG(show_simulator)
	LOAD_CONFIG(force_hw_decoding)
}
#undef LOAD_CONFIG

#define SAVE_CONFIG(member) config()[ #member ] = scripterSettings.member;
void OpenFunscripterSettings::saveSettings()
{
	SAVE_CONFIG(last_path)
	SAVE_CONFIG(last_opened_file)
	SAVE_CONFIG(draw_video)
	SAVE_CONFIG(show_simulator)
	SAVE_CONFIG(force_hw_decoding)
	save_config();
}
#undef SAVE_CONFIG

void OpenFunscripterSettings::saveKeybinds(const std::vector<Keybinding>& bindings)
{
	auto& keybinds = keybindsObj[KeybindsStr];
	for (auto& binding : bindings) {
		auto it = std::find_if(keybinds.begin(), keybinds.end(), 
			[&](auto& obj) { return obj["identifier"] == binding.identifier; }
		);
		nlohmann::json* ptr = nullptr;
		if (it != keybinds.end()) {
			auto& ref = *it;
			ref.clear();
			ptr = &ref;
		}
		else {
			nlohmann::json bindingObj;
			keybinds.push_back(bindingObj);
			ptr = &keybinds.back();
		}
		(*ptr)["identifier"] = binding.identifier;
		(*ptr)["ignore_repeats"] = binding.ignore_repeats;
		(*ptr)["key"] = binding.key;
		(*ptr)["modifiers"] = binding.modifiers;
	}
		
	save_keybinds();
}

std::vector<Keybinding> OpenFunscripterSettings::getKeybindings()
{
	auto& keybinds = keybindsObj[KeybindsStr];
	std::vector<Keybinding> bindings;
	bindings.reserve(keybinds.size());
	for (auto& bind : keybinds) {
		Keybinding b;
		if (bind.contains("ignore_repeats")
			&& bind.contains("identifier")
			&& bind.contains("key")
			&& bind.contains("modifiers")) {
			b.ignore_repeats = bind["ignore_repeats"];
			b.identifier = bind["identifier"].get<std::string>();
			b.key = bind["key"];
			b.modifiers = bind["modifiers"];
			bindings.push_back(b);
		}
	}

	return bindings;
}

bool OpenFunscripterSettings::ShowPreferenceWindow()
{
	bool save = false;
	if (ShowWindow)
		ImGui::OpenPopup("Preferences");


	if (ImGui::BeginPopupModal("Preferences", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		if (ImGui::Checkbox("Force hardware decoding", &scripterSettings.force_hw_decoding))
			save = true;
		Util::Tooltip("Use this for really high resolution video 4K+ VR videos for example.");
		ImGui::EndPopup();
	}

	return save;
}