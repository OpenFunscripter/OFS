#pragma once

#include "OFS_StateHandle.h"
#include "OFS_BinarySerialization.h"

#include <vector>
#include <cstdint>

struct WaveformState
{
    static constexpr auto StateName = "WaveformState";
    std::string Filename;
    std::vector<uint8_t> BinSamples;

    std::vector<float> GetSamples() noexcept
    {
        std::vector<float> samples;
        OFS_Binary::Deserialize(BinSamples, samples);
        return samples;
    }

    void SetSamples(const std::vector<float>& samples)
    {
        BinSamples.clear();
        auto size = OFS_Binary::Serialize(BinSamples, samples);
        BinSamples.resize(size);
    }

    inline static WaveformState& StaticStateSlow() noexcept
    {
        // This shouldn't be done in hot paths but shouldn't be a problem otherwise.
        uint32_t handle = OFS_AppState<WaveformState>::Register(StateName);
        return OFS_AppState<WaveformState>(handle).Get();
    }

    inline static WaveformState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<WaveformState>(stateHandle).Get();
    }
};

REFL_TYPE(WaveformState)
    REFL_FIELD(Filename)
    REFL_FIELD(BinSamples)
REFL_END