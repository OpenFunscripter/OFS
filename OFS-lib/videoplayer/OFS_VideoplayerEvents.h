#pragma once
#include <cstdint>
#include <string>

#include "OFS_Event.h"

class VideoLoadedEvent : public OFS_Event<VideoLoadedEvent>
{
	public:
	std::string videoPath;
	std::string playerName;

	VideoLoadedEvent(const char* path, const std::string& playerName) noexcept
		: videoPath(path), playerName(playerName) {}
	VideoLoadedEvent(const std::string& path, const std::string& playerName) noexcept
		: videoPath(path), playerName(playerName) {}	
};

class PlayPauseChangeEvent : public OFS_Event<PlayPauseChangeEvent>
{
	public:
	std::string playerName;
	bool paused = false;
	PlayPauseChangeEvent(bool pause, const std::string& playerName) noexcept
		: paused(pause), playerName(playerName) {}
};