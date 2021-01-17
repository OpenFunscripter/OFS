#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include "OFS_Reflection.h"
#include "OFS_Util.h"
#include "OFS_Shader.h"

#include <string>
#include <chrono>

#include "SDL_events.h"

struct mpv_handle;
struct mpv_render_context;


enum VideoMode : int32_t {
	FULL,
	LEFT_PANE,
	RIGHT_PANE,
	TOP_PANE,
	BOTTOM_PANE,
	VR_MODE,
	TOTAL_NUM_MODES,
};

class VideoEvents {
public:
	static int32_t WakeupOnMpvEvents;
	static int32_t WakeupOnMpvRenderUpdate;
	static int32_t MpvVideoLoaded;
	
	static int32_t PlayPauseChanged;

	static void RegisterEvents() noexcept;
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

	std::unique_ptr<class VrShader> vr_shader;
	ImGuiViewport* player_viewport;
	
	ImVec2 video_draw_size;
	ImVec2 viewport_pos;

	enum class LoopEnum : int32_t
	{
		A_set,
		B_set,
		Clear,
	};

	LoopEnum LoopState = LoopEnum::Clear;

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
		MpvFramesPerSecond,
		MpvAbLoopA,
		MpvAbLoopB,
	};

	enum MpvCommandIdentifier : uint64_t {
		//MpvSeekPlayingCommand = 1
	};

	struct MpvDataCache {
		double duration = 0.0;
		double percent_pos = 0.0;
		double real_percent_pos = 0.0;
		double current_speed = 1.0;
		double average_frame_time = 0.0167;
		double fps = 30.f;
		
		double ab_loop_a = 0;
		double ab_loop_b = 0;

		int64_t total_num_frames = 0;
		int64_t paused = true;
		int64_t video_width = 0;
		int64_t video_height = 0;


		bool video_loaded = false;
		const char* file_path = nullptr;
	} MpvData;

	char tmp_buf[32];

	float base_scale_factor = 1.f;

	const float zoom_multi = 0.1f;
	
	std::chrono::high_resolution_clock::time_point smooth_time;

	bool videoHovered = false;
	bool dragStarted = false;


	void MpvEvents(SDL_Event& ev) noexcept;
	void MpvRenderUpdate(SDL_Event& ev) noexcept;

	void observeProperties() noexcept;
	void renderToTexture() noexcept;
	void updateRenderTexture() noexcept;
	void mouse_scroll(SDL_Event& ev) noexcept;

	void setup_vr_mode() noexcept;

	void notifyVideoLoaded() noexcept;

	void drawVrVideo(ImDrawList* draw_list) noexcept;
	void draw2dVideo(ImDrawList* draw_list) noexcept;
	void videoRightClickMenu() noexcept;

	void showText(const char* text) noexcept;
	void clearLoop() noexcept;
public:
	static constexpr const char* PlayerId = "Player";
	ImDrawCallback OnRenderCallback = nullptr;

	struct OFS_VideoPlayerSettings {
		ImVec2 current_vr_rotation = ImVec2(0.5f, -0.5f);
		ImVec2 current_translation = ImVec2(0.0f, 0.0f);
		ImVec2 video_pos = ImVec2(0.0f, 0.0f);
		ImVec2 prev_vr_rotation;
		ImVec2 prev_translation;

		VideoMode activeMode = VideoMode::FULL;
		float vr_zoom = 0.2f;
		float zoom_factor = 1.f;
		float volume = 0.5f;
		float playback_speed = 1.f;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(activeMode, ar);
			activeMode = (VideoMode)Util::Clamp<int32_t>(activeMode, VideoMode::FULL, VideoMode::TOTAL_NUM_MODES - 1);
			OFS_REFLECT(volume, ar);
			OFS_REFLECT(playback_speed, ar);
			OFS_REFLECT(vr_zoom, ar);
			OFS_REFLECT(current_vr_rotation, ar);
			OFS_REFLECT(prev_vr_rotation, ar);
			OFS_REFLECT(current_translation, ar);
			OFS_REFLECT(prev_translation, ar);
			OFS_REFLECT(video_pos, ar);
		}
	};

	static constexpr float MinPlaybackSpeed = 0.05f;
	static constexpr float MaxPlaybackSpeed = 5.0f;

	OFS_VideoPlayerSettings settings;

	bool setup(bool force_hw_decoding);
	void DrawVideoPlayer(bool* open, bool* draw_video) noexcept;

	inline void resetTranslationAndZoom() noexcept {
		settings.zoom_factor = 1.f;
		settings.prev_translation = ImVec2(0.f, 0.f);
		settings.current_translation = ImVec2(0.f, 0.f); 
	}

	void setSpeed(float speed) noexcept;
	void addSpeed(float speed) noexcept;

	void openVideo(const std::string& file);
	void saveFrameToImage(const std::string& file);

	inline double getCurrentPositionMsInterp() const noexcept { return getCurrentPositionSecondsInterp() * 1000.0; }
	inline double getCurrentPositionSecondsInterp() const noexcept {
		if (MpvData.paused) {
			return getCurrentPositionSeconds();
		}
		else {
			std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - smooth_time;
			return getCurrentPositionSeconds() + (duration.count() * MpvData.current_speed);
		}
	}

	inline double getCurrentPositionMs() const noexcept { return getCurrentPositionSeconds() * 1000.0; }
	inline double getCurrentPositionSeconds() const noexcept { return MpvData.percent_pos * MpvData.duration; }

	inline double getRealCurrentPositionMs() const noexcept { return MpvData.real_percent_pos * MpvData.duration * 1000.0; }
	inline void syncWithRealTime() noexcept { MpvData.percent_pos = MpvData.real_percent_pos; }


	void setVolume(float volume) noexcept;
	
	inline void setPositionExact(int32_t time_ms, bool pausesVideo = false) noexcept {
		time_ms = Util::Clamp<int32_t>(time_ms, 0, getDuration() * 1000.f);
		float rel_pos = ((float)time_ms) / (getDuration() * 1000.f);
		setPositionRelative(rel_pos, pausesVideo); 
	}
	void setPositionRelative(float rel_pos, bool pausesVideo) noexcept;
	
	void seekRelative(int32_t ms) noexcept;
	
	void setPaused(bool paused) noexcept;
	void nextFrame() noexcept;
	void previousFrame() noexcept;
	void relativeFrameSeek(int32_t seek) noexcept;
	void togglePlay() noexcept;
	void cycleSubtitles() noexcept;

	void cycleLoopAB() noexcept;

	inline double getFrameTimeMs() const noexcept { return MpvData.average_frame_time * 1000.0; }
	inline double getSpeed() const noexcept { return MpvData.current_speed; }
	inline double getDuration() const noexcept { return MpvData.duration; }
	inline int64_t getTotalNumFrames() const  noexcept { return MpvData.total_num_frames; }
	inline bool isPaused() const noexcept { return MpvData.paused; };
	inline double getPosition() const noexcept { return MpvData.percent_pos; }
	inline int64_t getCurrentFrameEstimate() const noexcept { return MpvData.percent_pos * MpvData.total_num_frames; }
	inline double getFps() const noexcept { return MpvData.fps; }
	inline bool isLoaded() const noexcept { return MpvData.video_loaded; }
	
	inline bool LoopActive() const noexcept { return LoopState == LoopEnum::B_set; }
	inline double LoopASeconds() const noexcept { return MpvData.ab_loop_a; }
	inline double LoopBSeconds() const noexcept { return MpvData.ab_loop_b; }

	void closeVideo() noexcept;

	inline const char* getVideoPath() const noexcept { return (MpvData.file_path == nullptr) ? "" : MpvData.file_path; }
};