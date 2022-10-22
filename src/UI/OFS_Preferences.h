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

#include "OFS_StateHandle.h"

enum class OFS_Theme : int32_t
{
	Dark,
	Light
};

// FIXME
// REFL_FIELD(BaseOverlay::ShowMaxSpeedHighlight)
// REFL_FIELD(BaseOverlay::SplineMode)
// REFL_FIELD(BaseOverlay::SyncLineEnable)
// REFL_FIELD(BaseOverlay::MaxSpeedColor)
// REFL_FIELD(BaseOverlay::MaxSpeedPerSecond)

struct PreferenceState 
{
	std::string languageCsv;
	std::string fontOverride;

	int32_t defaultFontSize = 18;
	int32_t currentTheme = static_cast<int32_t>(OFS_Theme::Dark);

	int32_t fastStepAmount = 6;

	int32_t	vsync = 0;
	int32_t framerateLimit = 150;

	bool forceHwDecoding = false;
	bool showMetaOnNew = true;

	static inline PreferenceState& State(uint32_t stateHandle) noexcept {
		return OFS_StateHandle<PreferenceState>(stateHandle).Get();
	}
};

REFL_TYPE(PreferenceState)
	REFL_FIELD(languageCsv)
	REFL_FIELD(fontOverride)
	REFL_FIELD(defaultFontSize)
	REFL_FIELD(currentTheme)
	REFL_FIELD(fastStepAmount)
	REFL_FIELD(vsync)
	REFL_FIELD(framerateLimit)
	REFL_FIELD(forceHwDecoding)
	REFL_FIELD(showMetaOnNew)
REFL_END

class OFS_Preferences
{
private:
	uint32_t prefStateHandle = 0xFFFF'FFFF;
	std::vector<std::string> translationFiles;
public:
	static constexpr auto StateName = "Preferences";
	inline uint32_t StateHandle() const noexcept { return prefStateHandle; }
public:
	bool ShowWindow = false;
	OFS_Preferences() noexcept;
	bool ShowPreferenceWindow() noexcept;
	void SetTheme(OFS_Theme theme) noexcept;
};
