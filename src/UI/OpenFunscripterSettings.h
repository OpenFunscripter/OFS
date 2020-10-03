#pragma once

#include "nlohmann/json.hpp"

#include "KeybindingSystem.h"

#include <sstream>
#include <vector>
#include <string>

#include "ScriptSimulator.h"

#include "OFS_Reflection.h"

#include "imgui.h"

class OpenFunscripterSettings
{
	struct ScripterSettingsData {
		std::string last_path;
		std::string last_opened_video;
		std::string last_opened_script;
		std::string screenshot_dir = "screenshot";
		int32_t default_font_size = 18;
		bool always_show_bookmark_labels = false;
		bool draw_video= true;
		bool show_simulator = false;
		bool force_hw_decoding = false;

		ScriptSimulator::SimulatorSettings* simulator;

		template <class Archive>
		inline void reflect(Archive& ar)
		{
			OFS_REFLECT(last_path, ar);
			OFS_REFLECT(last_opened_video, ar);
			OFS_REFLECT(last_opened_script, ar);
			OFS_REFLECT(screenshot_dir, ar);
			OFS_REFLECT(always_show_bookmark_labels, ar);
			OFS_REFLECT(draw_video, ar);
			OFS_REFLECT(show_simulator, ar);
			OFS_REFLECT(default_font_size, ar);
			OFS_REFLECT(force_hw_decoding, ar);
			OFS_REFLECT_PTR(simulator, ar);
		}
	} scripterSettings;

	std::string keybinds_path;
	std::string config_path;

	const char* KeybindsStr = "keybinds";
	const char* ConfigStr = "config";
	nlohmann::json keybindsObj;
	nlohmann::json configObj;
	nlohmann::json& config() { return configObj[ConfigStr]; }

	void save_keybinds();
	void save_config();
	void load_config();
public:
	OpenFunscripterSettings(const std::string& keybinds, const std::string& config);
	ScripterSettingsData& data() { return scripterSettings; }
	void saveSettings();
	void saveKeybinds(const std::vector<Keybinding>& binding);
	std::vector<Keybinding> getKeybindings();

	bool ShowWindow = false;
	bool ShowPreferenceWindow();
};