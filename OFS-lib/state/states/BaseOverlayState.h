#pragma once

#include "OFS_StateHandle.h"

struct BaseOverlayState
{
    static constexpr auto StateName = "BaseOverlayState";

    ImColor MaxSpeedColor = ImColor(0, 0, 255, 255);
    float MaxSpeedPerSecond = 400.f;
    bool ShowMaxSpeedHighlight = false;
	bool SyncLineEnable = false;
	bool SplineMode = false;

    inline static uint32_t RegisterStatic() noexcept
    {
        return OFS_AppState<BaseOverlayState>::Register(StateName);
    }

    inline static BaseOverlayState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<BaseOverlayState>(stateHandle).Get();
    }
};

REFL_TYPE(BaseOverlayState)
    REFL_FIELD(MaxSpeedColor)
    REFL_FIELD(MaxSpeedPerSecond)
    REFL_FIELD(ShowMaxSpeedHighlight)
    REFL_FIELD(SyncLineEnable)
    REFL_FIELD(SplineMode)
REFL_END