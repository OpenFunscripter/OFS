#pragma once

#include "nlohmann/json.hpp"

#include "KeybindingSystem.h"

#include <sstream>
#include <vector>
#include <string>

class OpenFunscripterSettings
{
	struct ScripterSettingsData {
		std::string last_path;
		std::string last_opened_video;
		std::string last_opened_script;
		std::string screenshot_dir = "screenshot";
		bool always_show_bookmark_labels = false;
		bool draw_video= true;
		bool show_simulator = false;
		int64_t default_font_size = 18;
		bool force_hw_decoding = false;
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