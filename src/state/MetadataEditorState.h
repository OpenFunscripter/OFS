#pragma once
#include "Funscript.h"
#include "OFS_StateHandle.h"

struct FunscriptMetadataState
{
    static constexpr auto StateName = "FunscriptMetadata";

    Funscript::Metadata defaultMetadata;

    static inline FunscriptMetadataState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<FunscriptMetadataState>(stateHandle).Get();
    }
};

REFL_TYPE(FunscriptMetadataState)
    REFL_FIELD(defaultMetadata)
REFL_END