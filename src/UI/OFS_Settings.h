#pragma once

#include "nlohmann/json.hpp"

#include <sstream>
#include <vector>
#include <string>

#include "OFS_ScriptSimulator.h"

#include "OFS_Reflection.h"
#include "OFS_Util.h"
#include "imgui.h"

#include "OFS_ScriptPositionsOverlays.h"

enum class OFS_Theme : uint32_t
{
	dark,
	light
};
constexpr const char* CurrentSettingsVersion = "1";
class OFS_Settings
{
public:
	struct RecentFile {
		std::string name;
		std::string projectPath;
		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(name, ar);
			OFS_REFLECT(projectPath, ar);
		}
	};
private:
	struct ScripterSettingsData {
		std::string config_version = CurrentSettingsVersion;
		std::string last_path;
		std::string font_override;
		std::string language_csv;

		int32_t default_font_size = 18;
		int32_t fast_step_amount = 6;
		OFS_Theme current_theme = OFS_Theme::dark;

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
		bool show_tcode = false;
		bool show_debug_log = false;
		bool show_meta_on_new = true;

		int32_t	vsync = 0;
		int32_t framerateLimit = 150;

		int32_t action_insert_delay_ms = 0;

		int32_t currentSpecialFunction = 0; // SpecialFunctions::RANGE_EXTENDER;

		int32_t buttonRepeatIntervalMs = 100;

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

		Funscript::Metadata defaultMetadata;
		ScriptSimulator::SimulatorSettings defaultSimulatorConfig;

		std::vector<RecentFile> recentFiles;
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
			OFS_REFLECT_PTR_NAMED("theme", (uint32_t*)&current_theme, ar);
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
			OFS_REFLECT(action_insert_delay_ms, ar);
			OFS_REFLECT(currentSpecialFunction, ar);
			OFS_REFLECT(vsync, ar);
			OFS_REFLECT(framerateLimit, ar);
			OFS_REFLECT(buttonRepeatIntervalMs, ar);
			OFS_REFLECT(font_override, ar);
			OFS_REFLECT(show_tcode, ar);
			OFS_REFLECT(defaultMetadata, ar);
			OFS_REFLECT(show_debug_log, ar);
			OFS_REFLECT(show_meta_on_new, ar);
			OFS_REFLECT_NAMED("SplineMode", BaseOverlay::SplineMode, ar);
			OFS_REFLECT_NAMED("SyncLineEnable", BaseOverlay::SyncLineEnable, ar);
			OFS_REFLECT(defaultSimulatorConfig, ar);
			OFS_REFLECT(language_csv, ar);

			OFS_REFLECT_NAMED("MaxSpeedHightlightEnabled", BaseOverlay::ShowMaxSpeedHighlight, ar);
			OFS_REFLECT_NAMED("MaxSpeedHightlightColor", BaseOverlay::MaxSpeedColor, ar);
			OFS_REFLECT_NAMED("MaxSpeedPerSecond", BaseOverlay::MaxSpeedPerSecond, ar);
		}
	} scripterSettings;

	std::string config_path;

	const char* ConfigStr = "config";
	nlohmann::json configObj;
	nlohmann::json& config() noexcept { return configObj[ConfigStr]; }

	std::vector<std::string> translationFiles;

	void save_config();
	void load_config();
public:
	OFS_Settings(const std::string& config) noexcept;
	ScripterSettingsData& data() noexcept { return scripterSettings; }
	void saveSettings();

	inline void addRecentFile(RecentFile& recentFile) noexcept {
		auto it = std::find_if(scripterSettings.recentFiles.begin(), scripterSettings.recentFiles.end(),
			[&](auto& file) {
				return file.projectPath == recentFile.projectPath;
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
	bool ShowPreferenceWindow() noexcept;
	void SetTheme(OFS_Theme theme) noexcept;
};