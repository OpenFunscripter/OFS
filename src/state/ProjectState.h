#pragma once

#include "OFS_StateHandle.h"
#include "Funscript.h"
#include "ProjectBookmarkState.h"

#include <vector>
#include <string>

/*
    This file contains all 'state' which is serialized into project files.
*/

struct TempoOverlayState
{
    static constexpr auto StateName = "TempoOverlayState";

    float bpm = 100.f;
    float beatOffsetSeconds = 0.f;
    uint32_t measureIndex = 0;

    inline static TempoOverlayState& State(uint32_t stateHandle) noexcept
    {
        return OFS_ProjectState<TempoOverlayState>(stateHandle).Get();
    }
};

REFL_TYPE(TempoOverlayState)
    REFL_FIELD(bpm)
    REFL_FIELD(beatOffsetSeconds)
    REFL_FIELD(measureIndex)
REFL_END

struct ProjectState
{
    static constexpr auto StateName = "ProjectState";

    Funscript::Metadata metadata;
    std::string relativeMediaPath;
    float activeTimer = 0.f;
    float lastPlayerPosition = 0.f;
    uint32_t activeScriptIdx = 0;
    bool nudgeMetadata = true;

    std::vector<uint8_t> binaryFunscriptData;

    inline static ProjectState& State(uint32_t stateHandle) noexcept
    {
        return OFS_ProjectState<ProjectState>(stateHandle).Get();
    }
};

REFL_TYPE(ProjectState)
    REFL_FIELD(metadata)
    REFL_FIELD(relativeMediaPath)
    REFL_FIELD(activeTimer)
    REFL_FIELD(lastPlayerPosition)
    REFL_FIELD(activeScriptIdx)
    REFL_FIELD(nudgeMetadata)
    REFL_FIELD(binaryFunscriptData)
REFL_END

