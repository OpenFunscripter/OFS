#pragma once
#include <cstdint>

class VideoEvents {
public:
	static int32_t VideoLoaded;
	static int32_t PlayPauseChanged;

	static void RegisterEvents() noexcept;
};