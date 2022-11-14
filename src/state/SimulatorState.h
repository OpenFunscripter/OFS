#pragma once

#include "OFS_StateHandle.h"


struct SimulatorState
{
    static constexpr auto StateName = "SimulatorState";

    ImVec2 P1 = {600.f, 300.f};
    ImVec2 P2 = {600.f, 700.f};
    ImColor Text = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImColor Front = IM_COL32(0x01, 0xBA, 0xEF, 0xFF);
    ImColor Back = IM_COL32(0x10, 0x10, 0x10, 0xBF);
    ImColor Border = IM_COL32(0x0B, 0x4F, 0x6C, 0xFF);
    ImColor ExtraLines = IM_COL32(0x0B, 0x4F, 0x6C, 0xFF);
    ImColor Indicator = IM_COL32(0xFF, 0x4F, 0x6C, 0xFF);
    float Width = 120.f;
    float BorderWidth = 8.f;
    float ExtraLineWidth = 4.f;
    float LineWidth = 4.f;
    float GlobalOpacity = 0.75f;

    int32_t ExtraLinesCount = 0;

    bool EnableIndicators = true;
    bool EnablePosition = false;
    bool EnableHeightLines = true;
    bool LockedPosition = false;

    inline static SimulatorState& State(uint32_t stateHandle) noexcept
    {
        return OFS_ProjectState<SimulatorState>(stateHandle).Get();
    }
};

struct SimulatorDefaultConfigState
{
    static constexpr auto StateName = "SimulatorDefaultConfigState";
    SimulatorState defaultState;

    inline static SimulatorDefaultConfigState& StaticStateSlow() noexcept
    {
        // This shouldn't be done in hot paths but shouldn't be a problem otherwise.
        uint32_t handle = OFS_AppState<SimulatorDefaultConfigState>::Register(StateName);
        return OFS_AppState<SimulatorDefaultConfigState>(handle).Get();
    }
};

REFL_TYPE(SimulatorDefaultConfigState)
    REFL_FIELD(defaultState)
REFL_END

REFL_TYPE(SimulatorState)
	REFL_FIELD(P1)
	REFL_FIELD(P2)
	REFL_FIELD(Width)
	REFL_FIELD(BorderWidth)
	REFL_FIELD(LineWidth)
	REFL_FIELD(ExtraLineWidth)
	REFL_FIELD(Text)
	REFL_FIELD(Front)
	REFL_FIELD(Back)
	REFL_FIELD(Border)
	REFL_FIELD(ExtraLines)
	REFL_FIELD(Indicator)
	REFL_FIELD(GlobalOpacity)
	REFL_FIELD(EnableIndicators)
	REFL_FIELD(EnablePosition)
	REFL_FIELD(EnableHeightLines)
	REFL_FIELD(ExtraLinesCount)
	REFL_FIELD(LockedPosition)
REFL_END