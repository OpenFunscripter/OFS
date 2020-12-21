#pragma once

#include <cstdint>

class OFS_Events {
public:


	static int32_t FfmpegAudioProcessingFinished;


	static int32_t ControllerButtonRepeat;


	static void RegisterEvents() noexcept;
};