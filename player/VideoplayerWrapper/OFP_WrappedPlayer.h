#pragma once

#include "OFS_Videoplayer.h"
#include "EventSystem.h"

#include <string>

class WrappedPlayer
{
public:
	VideoplayerWindow::OFS_VideoPlayerSettings* settings = nullptr;

	virtual bool setup() = 0;

	virtual void togglePlay() noexcept = 0;

	virtual void setPositionExact(int32_t ms, bool pause = false) noexcept = 0;
	virtual void setPositionRelative(float pos, bool pause = false) noexcept = 0;

	virtual void seekRelative(int32_t ms) noexcept = 0;
	virtual void previousFrame() noexcept = 0;
	virtual void nextFrame() noexcept = 0;

	virtual void setVolume(float volume) noexcept = 0;
	virtual void setPaused(bool paused) noexcept = 0;
	virtual void setSpeed(float speed) noexcept = 0;
	virtual void addSpeed(float val) noexcept = 0;

	virtual void closeVideo() noexcept = 0;
	virtual void openVideo(const std::string& path) noexcept = 0;

	virtual float getFrameTimeMs() const noexcept = 0;
	virtual bool isPaused() const noexcept = 0;
	virtual float getPosition() const noexcept = 0;
	virtual float getDuration() const noexcept = 0;
	virtual float getSpeed() const noexcept = 0;
	virtual float getCurrentPositionMsInterp() const noexcept = 0;
	virtual float getCurrentPositionSecondsInterp() const noexcept = 0;
	virtual const char* getVideoPath() const noexcept = 0;

	virtual void cycleSubtitles() noexcept = 0;
	virtual void saveFrameToImage(const std::string& path) noexcept = 0;
	virtual void resetTranslationAndZoom() noexcept = 0;

	virtual void DrawVideoPlayer(bool* open, bool* show_video) noexcept = 0;
};

class DefaultPlayer : public WrappedPlayer
{
public:
	VideoplayerWindow impl;

	DefaultPlayer() noexcept
	{
		settings = &impl.settings;
	}

	virtual bool setup() {	return impl.setup(false); }

	virtual void togglePlay() noexcept {
		impl.togglePlay();
	}

	virtual void setPositionExact(int32_t ms, bool pause = false) noexcept
	{
		impl.setPositionExact(ms, pause);
	}

	virtual void setPositionRelative(float pos, bool pause = false) noexcept
	{
		impl.setPositionRelative(pos, pause);
	}


	virtual void seekRelative(int32_t ms) noexcept
	{
		impl.seekRelative(ms);
	}

	virtual void previousFrame() noexcept
	{
		impl.previousFrame();
	}

	virtual void nextFrame() noexcept
	{
		impl.nextFrame();
	}

	virtual void setVolume(float volume) noexcept
	{
		impl.setVolume(volume);
	}

	virtual void setPaused(bool paused) noexcept
	{
		impl.setPaused(paused);
	}

	virtual void setSpeed(float speed) noexcept
	{
		impl.setSpeed(speed);
	}

	virtual void addSpeed(float val) noexcept
	{
		impl.addSpeed(val);
	}

	virtual void closeVideo() noexcept
	{
		impl.closeVideo();
	}

	virtual void openVideo(const std::string& path) noexcept
	{
		impl.openVideo(path);
	}

	virtual bool isPaused() const noexcept
	{
		return impl.isPaused();
	}

	virtual float getFrameTimeMs() const noexcept
	{
		return impl.getFrameTimeMs();
	}

	virtual float getPosition() const noexcept
	{
		return impl.getPosition();
	}

	virtual float getDuration() const noexcept
	{
		return impl.getDuration();
	}

	virtual float getSpeed() const noexcept 
	{
		return impl.getSpeed();
	}

	virtual float getCurrentPositionMsInterp() const noexcept
	{
		return impl.getCurrentPositionMsInterp();
	}

	virtual float getCurrentPositionSecondsInterp() const noexcept
	{
		return impl.getCurrentPositionSecondsInterp();
	}

	virtual const char* getVideoPath() const noexcept
	{
		return impl.getVideoPath();
	}

	virtual void cycleSubtitles() noexcept
	{
		impl.cycleSubtitles();
	}

	virtual void saveFrameToImage(const std::string& path) noexcept
	{
		impl.saveFrameToImage(path);
	}

	virtual void resetTranslationAndZoom() noexcept
	{
		impl.resetTranslationAndZoom();
	}

	virtual void DrawVideoPlayer(bool* open, bool* show_video) noexcept
	{
		impl.DrawVideoPlayer(open, show_video);
	}
};