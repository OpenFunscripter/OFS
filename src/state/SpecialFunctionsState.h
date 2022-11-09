#pragma once

#include "OFS_StateHandle.h"

// ATTENTION: no reordering
enum class SpecialFunctionType : int32_t
{
	RangeExtender,
	RamerDouglasPeucker, 
	TotalFunctionCount
};

struct SpecialFunctionState
{
    static constexpr auto StateName = "SpecialFunctionState";

    SpecialFunctionType selectedFunction = SpecialFunctionType::RangeExtender;

    static inline SpecialFunctionState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<SpecialFunctionState>(stateHandle).Get();
    }
};

REFL_TYPE(SpecialFunctionState)
    REFL_FIELD(selectedFunction, serializeEnum{})
REFL_END