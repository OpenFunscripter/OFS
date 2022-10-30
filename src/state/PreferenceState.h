#pragma once
#include <string>
#include "OFS_StateHandle.h"

enum class OFS_Theme : int32_t
{
	Dark,
	Light
};

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
		return OFS_AppState<PreferenceState>(stateHandle).Get();
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