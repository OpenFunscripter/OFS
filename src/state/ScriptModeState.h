#pragma once
#include "OFS_StateHandle.h"

struct ScriptingModeState
{
    int32_t actionInsertDelayMs = 0;

    inline static ScriptingModeState& State(uint32_t stateHandle) noexcept
    {
        return OFS_StateHandle<ScriptingModeState>(stateHandle).Get();
    }
};

REFL_TYPE(ScriptingModeState)
    REFL_FIELD(actionInsertDelayMs)
REFL_END
