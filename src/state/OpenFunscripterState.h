#pragma once

#include <string>
#include <vector>

#include "OFS_Reflection.h"
#include "OFS_StateHandle.h"

struct RecentFile 
{
	std::string name;
	std::string projectPath;
};

REFL_TYPE(RecentFile)
	REFL_FIELD(name)
	REFL_FIELD(projectPath)
REFL_END

struct OpenFunscripterState 
{
	std::vector<RecentFile> recentFiles;
    std::string lastPath;

	struct HeatmapSettings {
		int32_t defaultWidth = 2000;
		int32_t defaultHeight = 50;
		std::string defaultPath = "./";
	} heatmapSettings;

    bool showDebugLog = false;
    bool showVideo = true;

    bool showActionEditor = false;
    bool showStatistics = true;
    bool alwaysShowBookmarkLabels = false;
    bool showHistory = true;
    bool showSimulator = true;
    bool showSimulator3d = false;
    bool showSpecialFunctions = false;
    bool showTCode = false;

    inline static OpenFunscripterState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<OpenFunscripterState>(stateHandle).Get();
    }

    void addRecentFile(const RecentFile& recentFile) noexcept;

    static void RegisterAll() noexcept;
};

REFL_TYPE(OpenFunscripterState::HeatmapSettings)
	REFL_FIELD(defaultWidth)
	REFL_FIELD(defaultHeight)
	REFL_FIELD(defaultPath)
REFL_END

REFL_TYPE(OpenFunscripterState)
    REFL_FIELD(recentFiles)
    REFL_FIELD(lastPath)
    REFL_FIELD(showDebugLog)
    REFL_FIELD(showVideo)
    REFL_FIELD(alwaysShowBookmarkLabels)
    REFL_FIELD(showHistory)
    REFL_FIELD(showSimulator)
    REFL_FIELD(showSimulator3d)
    REFL_FIELD(showSpecialFunctions)
    REFL_FIELD(showTCode)
REFL_END