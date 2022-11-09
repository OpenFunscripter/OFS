#pragma once
#include <cstdint>

#include "OFS_Reflection.h"
#include "OFS_StateHandle.h"

#include "Funscript.h"

class OFS_FunscriptMetadataEditor
{
public:
    inline uint32_t StateHandle() const noexcept { return stateHandle; }

    OFS_FunscriptMetadataEditor() noexcept;
    bool ShowMetadataEditor(bool* open, Funscript::Metadata& metadata) noexcept;
private:
    uint32_t stateHandle = 0xFFFF'FFFF;
};

