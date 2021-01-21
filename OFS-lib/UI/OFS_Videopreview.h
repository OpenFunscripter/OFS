#pragma once

#include "EventSystem.h"

#include <cstdint>

#include "SDL_events.h"

struct mpv_handle;
struct mpv_render_context;

class VideoPreviewEvents {
public:

	static int32_t PreviewWakeUpMpvEvents;
	static int32_t PreviewWakeUpMpvRender;

	static void RegisterEvents() noexcept;
};

class VideoPreview {
private:
	char tmp_buf[32];
	void updateRenderTexture() noexcept;
	void observeProperties() noexcept;
	float seek_to = 0.f;

	enum PreviewProps : int32_t {
		VideoWidthProp = 1,
		VideoHeightProp = 2,
		VideoPosProp = 3
	};

	int videoWidth = -1;
	int videoHeight = -1;
	float videoPos = 0.f;

	bool render_complete = false;
public:
	mpv_handle* mpv;
	mpv_render_context* mpv_gl;

	uint32_t framebuffer_obj = 0;
	uint32_t render_texture = 0;

	bool ready = false;
	bool loading = false;

	~VideoPreview();

	void setup(bool autoplay) noexcept;

	void setPosition(float pos) noexcept;
	void previewVideo(const std::string& path, float pos) noexcept;
	void play() noexcept;
	void pause() noexcept;
	void closeVideo() noexcept;

	void MpvEvents(SDL_Event& ev) noexcept;
	void MpvRenderUpdate(SDL_Event& ev) noexcept;
};