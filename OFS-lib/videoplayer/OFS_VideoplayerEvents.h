#pragma once
#include <cstdint>
#include <string>

#include "OFS_Event.h"

class VideoLoadedEvent : public OFS_Event<VideoLoadedEvent>
{
	public:
	std::string videoPath;
	VideoLoadedEvent(const char* path) noexcept
		: videoPath(path) {}
	VideoLoadedEvent(const std::string& path) noexcept
		: videoPath(path) {}	
};

class PlayPauseChangeEvent : public OFS_Event<PlayPauseChangeEvent>
{
	public:
	bool paused = false;
	PlayPauseChangeEvent(bool pause) noexcept
		: paused(pause) {}
};