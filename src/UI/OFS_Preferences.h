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

enum class OFS_Theme : int32_t
{
	Dark,
	Light
};
constexpr const char* CurrentSettingsVersion = "1";

struct RecentFile 
{
	std::string name;
	std::string projectPath;
};

struct ScripterSettingsData 
{
	std::string configVersion = CurrentSettingsVersion;
	std::string lastPath;
	std::string fontOverride;
	std::string languageCsv;

	int32_t defaultFontSize = 18;
	int32_t fastStepAmount = 6;
	int32_t currentTheme = static_cast<int32_t>(OFS_Theme::Dark);

	bool alwaysShowBookmarkLabels = false;
	bool drawVideo = true;
	bool showSimulator = true;
	bool showSimulator3d = false;
	bool showStatistics = true;
	bool showHistory = true;
	bool showSpecialFunctions = false;
	bool showActionEditor = false;
	bool forceHwDecoding = false;
	bool mirrorMode = false;
	bool showTCode = false;
	bool showDebugLog = false;
	bool showMetaOnNew = true;

	int32_t	vsync = 0;
	int32_t framerateLimit = 150;

	int32_t actionInsertDelayMs = 0;

	int32_t currentSpecialFunction = 0; // SpecialFunctions::RANGE_EXTENDER;

	int32_t buttonRepeatIntervalMs = 100;

	struct HeatmapSettings {
		int32_t defaultWidth = 2000;
		int32_t defaultHeight = 50;
		std::string defaultPath = "./";
	} heatmapSettings;

	Funscript::Metadata defaultMetadata;
	ScriptSimulator::SimulatorSettings defaultSimulatorConfig;

	std::vector<RecentFile> recentFiles;
};

REFL_TYPE(RecentFile)
	REFL_FIELD(name)
	REFL_FIELD(projectPath)
REFL_END

REFL_TYPE(ScripterSettingsData::HeatmapSettings)
	REFL_FIELD(defaultWidth)
	REFL_FIELD(defaultHeight)
	REFL_FIELD(defaultPath)
REFL_END

REFL_TYPE(ScripterSettingsData)
	REFL_FIELD(configVersion)
	REFL_FIELD(lastPath)
	REFL_FIELD(alwaysShowBookmarkLabels)
	REFL_FIELD(currentTheme)
	REFL_FIELD(drawVideo)
	REFL_FIELD(showSimulator)
	REFL_FIELD(showSimulator3d)
	REFL_FIELD(showStatistics)
	REFL_FIELD(showHistory)
	REFL_FIELD(showSpecialFunctions)
	REFL_FIELD(showActionEditor)
	REFL_FIELD(defaultFontSize)
	REFL_FIELD(fastStepAmount)
	REFL_FIELD(forceHwDecoding)
	REFL_FIELD(recentFiles)
	REFL_FIELD(heatmapSettings)
	REFL_FIELD(mirrorMode)
	REFL_FIELD(actionInsertDelayMs)
	REFL_FIELD(currentSpecialFunction)
	REFL_FIELD(vsync)
	REFL_FIELD(framerateLimit)
	REFL_FIELD(buttonRepeatIntervalMs)
	REFL_FIELD(fontOverride)
	REFL_FIELD(showTCode)
	REFL_FIELD(defaultMetadata)
	REFL_FIELD(showDebugLog)
	REFL_FIELD(showMetaOnNew)
	REFL_FIELD(defaultSimulatorConfig)
	REFL_FIELD(languageCsv)
	
	// FIXME
	// REFL_FIELD(BaseOverlay::ShowMaxSpeedHighlight)
	// REFL_FIELD(BaseOverlay::SplineMode)
	// REFL_FIELD(BaseOverlay::SyncLineEnable)
	// REFL_FIELD(BaseOverlay::MaxSpeedColor)
	// REFL_FIELD(BaseOverlay::MaxSpeedPerSecond)
REFL_END

class OFS_Settings
{
private:
	ScripterSettingsData scripterSettings;

	std::string config_path;

	const char* ConfigStr = "config";
	nlohmann::json configObj;
	nlohmann::json& config() noexcept { return configObj[ConfigStr]; }

	std::vector<std::string> translationFiles;

	void saveConfig() noexcept;
	void loadConfig() noexcept;
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
