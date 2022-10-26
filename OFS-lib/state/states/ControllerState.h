#pragma once

#include "OFS_StateHandle.h"

struct ControllerInputState
{
	int32_t buttonRepeatIntervalMs = 100;

	inline static ControllerInputState& State(uint32_t stateHandle) noexcept {
		return OFS_StateHandle<ControllerInputState>(stateHandle).Get();
	}
};

REFL_TYPE(ControllerInputState)
	REFL_FIELD(buttonRepeatIntervalMs)
REFL_END
