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
	if (!keybindsObj[KeybindsStr].is_array())
		keybindsObj[KeybindsStr] = nlohmann::json::array();
	
	if (!configObj[ConfigStr].is_object())
		configObj[ConfigStr] = nlohmann::json::object();
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

void OpenFunscripterSettings::saveKeybinds(const std::vector<KeybindingGroup>& bindings)
{
	OFS::archiver ar(&keybindsObj);
	auto pair = reflect_member(KeybindsStr, (std::vector<KeybindingGroup>*) & bindings);
	ar << pair;
	save_keybinds();
}

std::vector<KeybindingGroup> OpenFunscripterSettings::getKeybindings()
{
	auto& keybinds = keybindsObj[KeybindsStr];
	std::vector<KeybindingGroup> bindings;
	OFS::unpacker upkg(&keybindsObj);
	auto pair = reflect_member(KeybindsStr, &bindings);
	upkg << pair;
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

		if (ImGui::InputInt("Fast frame step", &scripterSettings.fast_step_amount, 1, 1)) {
			save = true;
			scripterSettings.fast_step_amount = Util::Clamp<int32_t>(scripterSettings.fast_step_amount, 2, 30);
		}
		Util::Tooltip("Amount of frames to skip with fast step.");

		ImGui::EndPopup();
	}

	return save;
}