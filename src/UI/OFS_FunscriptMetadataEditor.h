#pragma once
#include <cstdint>

#include "OFS_Reflection.h"
#include "OFS_StateHandle.h"

#include "Funscript.h"

struct FunscriptMetadataState
{
    Funscript::Metadata defaultMetadata;

    static inline FunscriptMetadataState& State(uint32_t stateHandle) noexcept
    {
        return OFS_StateHandle<FunscriptMetadataState>(stateHandle).Get();
    }
};

REFL_TYPE(FunscriptMetadataState)
    REFL_FIELD(defaultMetadata)
REFL_END

class OFS_FunscriptMetadataEditor
{
public:
    static constexpr auto StateName = "FunscriptMetadata";
    inline uint32_t StateHandle() const noexcept { return stateHandle; }

    OFS_FunscriptMetadataEditor() noexcept;
    bool ShowMetadataEditor(bool* open, Funscript::Metadata& metadata) noexcept;
private:
    uint32_t stateHandle = 0xFFFF'FFFF;
};

