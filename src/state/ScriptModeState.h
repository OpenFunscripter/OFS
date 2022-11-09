#pragma once
#include "OFS_StateHandle.h"

struct ScriptingModeState
{
	static constexpr auto StateName = "ScriptingMode";

    int32_t actionInsertDelayMs = 0;

    inline static ScriptingModeState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<ScriptingModeState>(stateHandle).Get();
    }
};

REFL_TYPE(ScriptingModeState)
    REFL_FIELD(actionInsertDelayMs)
REFL_END
