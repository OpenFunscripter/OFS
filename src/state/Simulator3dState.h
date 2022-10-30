#pragma once

#include "OFS_StateHandle.h"

struct Simulator3dState
{
	Serializable<glm::mat4> Translation;
	float Distance = 3.f;

    static inline auto& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<Simulator3dState>(stateHandle).Get();
    }
};

REFL_TYPE(Simulator3dState)
	REFL_FIELD(Translation)
	REFL_FIELD(Distance)
REFL_END