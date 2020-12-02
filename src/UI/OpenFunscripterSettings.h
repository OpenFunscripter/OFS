#pragma once

#include "nlohmann/json.hpp"

#include "KeybindingSystem.h"

#include <sstream>
#include <vector>
#include <string>

#include "ScriptSimulator.h"

#include "OFS_Reflection.h"
#include "OpenFunscripterUtil.h"
#include "imgui.h"

constexpr const char* CurrentSettingsVersion = "1";
class OpenFunscripterSettings
{
public:
	struct RecentFile {
		std::string name;
		std::string video_path;
		std::string script_path;
		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(name, ar);
			OFS_REFLECT(video_path, ar);
			OFS_REFLECT(script_path, ar);
		}
	};
private:
	struct ScripterSettingsData {
		std::string config_version = CurrentSettingsVersion;
		std::string last_path;
		int32_t default_font_size = 18;
		int32_t fast_step_amount = 6;
		bool always_show_bookmark_labels = false;
		bool draw_video= true;
		bool show_simulator = true;
		bool show_simulator_3d = false;
		bool show_statistics = true;
		bool show_history = true;
		bool show_special_functions = false;
		bool show_action_editor = false;
		bool force_hw_decoding = false;
		bool mirror_mode = false;

		struct HeatmapSettings {
			int32_t defaultWidth = 2000;
			int32_t defaultHeight = 50;
			std::string defaultPath = "./";
			template <class Archive>
			inline void reflect(Archive& ar) {
				OFS_REFLECT(defaultWidth, ar);
				OFS_REFLECT(defaultHeight, ar);
				OFS_REFLECT(defaultPath, ar);
			}
		} heatmapSettings;

		std::vector<RecentFile> recentFiles;
		ScriptSimulator::SimulatorSettings* simulator;
		template <class Archive>
		inline void reflect(Archive& ar)
		{
			OFS_REFLECT(config_version, ar);
			// checks configuration version and cancels if it doesn't match
			if (config_version != CurrentSettingsVersion) { 
				LOGF_WARN("Settings version: \"%s\" didn't match \"%s\". Settings are reset.", config_version.c_str(), CurrentSettingsVersion);
				config_version = CurrentSettingsVersion;
				return; 
			}
			OFS_REFLECT(last_path, ar);
			OFS_REFLECT(always_show_bookmark_labels, ar);
			OFS_REFLECT(draw_video, ar);
			OFS_REFLECT(show_simulator, ar);
			OFS_REFLECT(show_simulator_3d, ar);
			OFS_REFLECT(show_statistics, ar);
			OFS_REFLECT(show_history, ar);
			OFS_REFLECT(show_special_functions, ar);
			OFS_REFLECT(show_action_editor, ar);
			OFS_REFLECT(default_font_size, ar);
			OFS_REFLECT(fast_step_amount, ar);
			OFS_REFLECT(force_hw_decoding, ar);
			OFS_REFLECT(recentFiles, ar);
			OFS_REFLECT(heatmapSettings, ar);
			OFS_REFLECT(mirror_mode, ar);
			OFS_REFLECT_PTR(simulator, ar);
		}
	} scripterSettings;

	std::string keybinds_path;
	std::string config_path;

	const char* KeybindsStr = "keybinds";
	const char* ConfigStr = "config";
	nlohmann::json keybindsObj;
	nlohmann::json configObj;
	nlohmann::json& config() noexcept { return configObj[ConfigStr]; }

	void save_keybinds();
	void save_config();
	void load_config();
public:
	OpenFunscripterSettings(const std::string& keybinds, const std::string& config);
	ScripterSettingsData& data() noexcept { return scripterSettings; }
	void saveSettings();
	void saveKeybinds(const Keybindings& binding);
	Keybindings getKeybindings();

	inline void addRecentFile(RecentFile& recentFile) noexcept {
		auto it = std::find_if(scripterSettings.recentFiles.begin(), scripterSettings.recentFiles.end(),
			[&](auto& file) {
				return file.video_path == recentFile.video_path && file.script_path == recentFile.script_path;
		});
		if (it != scripterSettings.recentFiles.end()) {
			scripterSettings.recentFiles.erase(it);
		}
		scripterSettings.recentFiles.push_back(recentFile);
		if (scripterSettings.recentFiles.size() > 5) {
			scripterSettings.recentFiles.erase(scripterSettings.recentFiles.begin());
		}
	}

	bool ShowWindow = false;
	bool ShowPreferenceWindow();
};