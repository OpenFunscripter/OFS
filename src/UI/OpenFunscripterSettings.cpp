#include "OpenFunscripterSettings.h"
#include "OpenFunscripterUtil.h"
#include "OpenFunscripter.h"

#include "OFS_Serialization.h"

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

	scripterSettings.simulator = &OpenFunscripter::ptr->simulator.simulator;
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

void OpenFunscripterSettings::load_config()
{
	OFS::serializer::load(&scripterSettings, &config());
}

void OpenFunscripterSettings::saveSettings()
{
	OFS::serializer::save(&scripterSettings, &config());
	save_config();
}

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

	if (ImGui::BeginPopupModal("Preferences", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::InputInt("Font size", (int*)&scripterSettings.default_font_size, 1, 1))
			save = true;
		Util::Tooltip("Requires program restart to take effect");
		if (ImGui::Checkbox("Force hardware decoding (Requires program restart)", &scripterSettings.force_hw_decoding))
			save = true;
		Util::Tooltip("Use this for really high resolution video 4K+ VR videos for example.");
		ImGui::EndPopup();
	}

	return save;
}