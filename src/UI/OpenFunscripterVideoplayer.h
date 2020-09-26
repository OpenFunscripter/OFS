#pragma once

#include "SDL.h"
#include "imgui.h"

#include <string>

struct mpv_handle;
struct mpv_render_context;

enum class VideoMode {
	FULL,
	LEFT_PANE,
	RIGHT_PANE,
	TOP_PANE,
	BOTTOM_PANE,
	VR_MODE
};

class VideoplayerWindow
{
public:
	~VideoplayerWindow();
private:
	mpv_handle* mpv;
	mpv_render_context* mpv_gl;
	bool redraw_video = false;
	uint32_t framebuffer_obj = 0;
	uint32_t render_texture = 0;
	
	struct ScreenshotSavingThreadData {
		int w;
		int h;
		uint8_t* dataBuffer = nullptr;
		std::string filename;
	};

	unsigned int vr_shader;
	ImGuiViewport* player_viewport;

	float vr_zoom = 1.f;
	ImVec2 prev_vr_rotation;
	ImVec2 current_vr_rotation;

	ImVec2 prev_translation;
	ImVec2 current_translation;
	
	ImVec2 video_draw_size;
	ImVec2 video_pos;
	ImVec2 viewport_pos;

	enum MpvPropertyGet : uint64_t {
		MpvDuration,
		MpvPosition,
		MpvTotalFrames,
		MpvSpeed,
		MpvVideoWidth,
		MpvVideoHeight,
		MpvPauseState,
		MpvFilePath,
		MpvHwDecoder,
	};

	struct MpvDataCache {
		double duration = 0.0;
		double percent_pos = 0.0;
		double current_speed = 1.0;

		double average_frame_time = 0.0167;

		int64_t total_num_frames = 0;
		int64_t paused = false;
		int64_t video_width = 0;
		int64_t video_height = 0;

		bool video_loaded = false;

		const char* file_path = nullptr;
	} MpvData;

	char tmp_buf[32];

	float base_scale_factor = 1.f;
	float zoom_factor = 1.f;
	const float zoom_multi = 0.15f;

	bool videoHovered = false;
	bool dragStarted = false;

	void MpvEvents(SDL_Event& ev);
	void MpvRenderUpdate(SDL_Event& ev);

	void observeProperties();
	void renderToTexture();
	void updateRenderTexture();
	void mouse_scroll(SDL_Event& ev);

	void setup_vr_mode();
public:
	VideoplayerWindow()
		: prev_translation(0.f, 0.f), current_translation(0.f, 0.f), activeMode(VideoMode::FULL)
	{}

	const float minPlaybackSpeed = 0.05f;
	const float maxPlaybackSpeed = 5.0f;
	float playbackSpeed = 1.f;
	float volume = 0.5f;
	VideoMode activeMode;
	bool setup();
	void DrawVideoPlayer(bool* open);

	inline void resetTranslationAndZoom() { vr_zoom = 1.f; zoom_factor = 1.f; prev_translation = ImVec2(0.f, 0.f); current_translation = ImVec2(0.f, 0.f); }

	void setSpeed(float speed);
	void addSpeed(float speed);

	bool openVideo(const std::string& file);
	void saveFrameToImage(const std::string& file);

	inline double getCurrentPositionMs() const { return getCurrentPositionSeconds() * 1000.0; }
	inline double getCurrentPositionSeconds() const { return MpvData.percent_pos * MpvData.duration; }

	void setVolume(float volume);
	inline void setPosition(int32_t time_ms) { float rel_pos = ((float)time_ms) / (getDuration() * 1000.f); setPosition(rel_pos); }
	void setPosition(float rel_pos);
	void setPaused(bool paused);
	void nextFrame();
	void previousFrame();
	void togglePlay();

	inline double getFrameTimeMs() const { return MpvData.average_frame_time * 1000.0; }
	inline double getSpeed() const { return MpvData.current_speed; }
	inline double getDuration() const { return MpvData.duration; }
	inline int getTotalNumFrames() const { return MpvData.total_num_frames; }
	inline bool isPaused() const { return MpvData.paused; };
	inline double getPosition() const { return MpvData.percent_pos; }

	inline bool isLoaded() const { return MpvData.video_loaded; }
	void closeVideo();

	inline const char* getVideoPath() const { return MpvData.file_path; }
};


