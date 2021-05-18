#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include "OFS_Reflection.h"
#include "OFS_BinarySerialization.h"
#include "OFS_Util.h"
#include "OFS_Shader.h"

#include <string>
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
	bool redrawVideo = false;
	uint32_t framebufferObj = 0;
	uint32_t renderTexture = 0;
	char tmpBuf[32];

	std::unique_ptr<VrShader> vrShader;
	ImGuiViewport* playerViewport;
	
	ImGuiID videoImageId;
	ImVec2 videoDrawSize;
	ImVec2 viewportPos;
	ImVec2 windowPos;

	float lastVideoStep = 0.f;
	float baseScaleFactor = 1.f;
	float smoothTime = 0.f;
	bool correctPlaybackErrorActive = false;
	bool videoHovered = false;
	bool dragStarted = false;

	enum class LoopEnum : int8_t
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
		double duration = 1.0;
		double percentPos = 0.0;
		double realPercentPos = 0.0;
		double currentSpeed = 1.0;
		double fps = 30.0;
		double averageFrameTime = 1.0/fps;
		
		double abLoopA = 0;
		double abLoopB = 0;

		int64_t totalNumFrames = 0;
		int64_t paused = true;
		int64_t videoWidth = 0;
		int64_t videoHeight = 0;


		bool videoLoaded = false;
		const char* filePath = nullptr;
	} MpvData;


	static constexpr float ZoomMulti = 0.05f;
	void MpvEvents(SDL_Event& ev) noexcept;
	void MpvRenderUpdate(SDL_Event& ev) noexcept;

	void observeProperties() noexcept;
	void renderToTexture() noexcept;
	void updateRenderTexture() noexcept;
	void mouseScroll(SDL_Event& ev) noexcept;

	void setupVrMode() noexcept;

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
		ImVec2 currentVrRotation = ImVec2(0.5f, -0.5f);
		ImVec2 currentTranslation = ImVec2(0.0f, 0.0f);
		ImVec2 videoPos = ImVec2(0.0f, 0.0f);
		ImVec2 prevVrRotation = currentVrRotation;
		ImVec2 prevTranslation = currentTranslation;

		VideoMode activeMode = VideoMode::FULL;
		float vrZoom = 0.2f;
		float zoomFactor = 1.f;
		float volume = 0.5f;
		float playbackSpeed = 1.f;
		bool LockedPosition = false;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(activeMode, ar);
			activeMode = (VideoMode)Util::Clamp<int32_t>(activeMode, VideoMode::FULL, VideoMode::TOTAL_NUM_MODES - 1);
			OFS_REFLECT(volume, ar);
			OFS_REFLECT(playbackSpeed, ar);
			OFS_REFLECT(vrZoom, ar);
			OFS_REFLECT(currentVrRotation, ar);
			OFS_REFLECT(prevVrRotation, ar);
			OFS_REFLECT(currentTranslation, ar);
			OFS_REFLECT(prevTranslation, ar);
			OFS_REFLECT(videoPos, ar);
			OFS_REFLECT(LockedPosition, ar);
		}

		template<typename S>
		void serialize(S& s)
		{
			s.ext(*this, bitsery::ext::Growable{},
				[](S& s, OFS_VideoPlayerSettings& o) {
					s.object(o.currentVrRotation);
					s.object(o.prevVrRotation);
					s.object(o.currentTranslation);
					s.object(o.prevTranslation);
					s.object(o.videoPos);
					s.value4b(o.activeMode);
					o.activeMode = (VideoMode)Util::Clamp<int32_t>(o.activeMode, VideoMode::FULL, VideoMode::TOTAL_NUM_MODES - 1);
					s.value4b(o.vrZoom);
					s.value4b(o.zoomFactor);
					s.value4b(o.volume);
					s.value4b(o.playbackSpeed);
					s.value1b(o.LockedPosition);
				});
		}
	};

	static constexpr float MinPlaybackSpeed = 0.05f;
	static constexpr float MaxPlaybackSpeed = 3.0f;

	OFS_VideoPlayerSettings settings;

	bool setup(bool force_hw_decoding);
	void DrawVideoPlayer(bool* open, bool* draw_video) noexcept;

	inline void resetTranslationAndZoom() noexcept {
		if (settings.LockedPosition) return;
		settings.zoomFactor = 1.f;
		settings.prevTranslation = ImVec2(0.f, 0.f);
		settings.currentTranslation = ImVec2(0.f, 0.f); 
	}

	void setSpeed(float speed) noexcept;
	void addSpeed(float speed) noexcept;

	void openVideo(const std::string& file);
	void saveFrameToImage(const std::string& file);

	inline double getCurrentPositionSecondsInterp() const noexcept {
		OFS_PROFILE(__FUNCTION__);
		if (MpvData.paused) {
			return getCurrentPositionSeconds();
		}
		else {
			return getCurrentPositionSeconds() + smoothTime;
		}
	}

	inline double getCurrentPositionSeconds() const noexcept { 
		return MpvData.percentPos * MpvData.duration; 
	}

	inline double getRealCurrentPositionSeconds() const noexcept { return MpvData.realPercentPos * MpvData.duration;  }
	inline void syncWithRealTime() noexcept { MpvData.percentPos = MpvData.realPercentPos; }


	void setVolume(float volume) noexcept;
	
	inline void setPositionExact(float timeSeconds, bool pausesVideo = false) noexcept {
		timeSeconds = Util::Clamp<float>(timeSeconds, 0.f, getDuration());
		float relPos = ((float)timeSeconds) / getDuration();
		setPositionPercent(relPos, pausesVideo);
	}
	void setPositionPercent(float relPos, bool pausesVideo) noexcept;
	void seekRelative(float ms) noexcept;
	
	void setPaused(bool paused) noexcept;
	void nextFrame() noexcept;
	void previousFrame() noexcept;
	void relativeFrameSeek(int32_t seek) noexcept;
	void togglePlay() noexcept;
	void cycleSubtitles() noexcept;

	void cycleLoopAB() noexcept;

	inline float getFrameTime() const noexcept { return MpvData.averageFrameTime; }

	inline float getSpeed() const noexcept { return MpvData.currentSpeed; }
	inline double getDuration() const noexcept { return MpvData.duration; }
	inline int64_t getTotalNumFrames() const  noexcept { return MpvData.totalNumFrames; }
	inline bool isPaused() const noexcept { return MpvData.paused; };
	inline float getPosition() const noexcept { return MpvData.percentPos; }
	inline int64_t getCurrentFrameEstimate() const noexcept { return MpvData.percentPos * MpvData.totalNumFrames; }
	inline float getFps() const noexcept { return MpvData.fps; }
	inline bool isLoaded() const noexcept { return MpvData.videoLoaded; }
	
	inline double getCurrentPositionRel() const noexcept { return MpvData.percentPos; }

	inline bool LoopActive() const noexcept { return LoopState == LoopEnum::B_set; }
	inline double LoopASeconds() const noexcept { return MpvData.abLoopA; }
	inline double LoopBSeconds() const noexcept { return MpvData.abLoopB; }

	void closeVideo() noexcept;

	inline const char* getVideoPath() const noexcept { return (MpvData.filePath == nullptr) ? "" : MpvData.filePath; }

	inline uint32_t VideoWidth() const noexcept { return MpvData.videoWidth; }
	inline uint32_t VideoHeight() const noexcept { return MpvData.videoHeight; }

	inline void update(float delta) noexcept
	{
		if (!isPaused()) {
			OFS_PROFILE(__FUNCTION__);
			smoothTime += delta * MpvData.currentSpeed;
			const float realTime = getRealCurrentPositionSeconds() + (lastVideoStep / 2.f);
			const float minError = lastVideoStep; 
			const float estimateTime = getCurrentPositionSecondsInterp();
			const float error = realTime-estimateTime;
			const float absError = std::abs(error);
			if (absError >= minError) {
				correctPlaybackErrorActive = true;
			}

#ifndef NDEBUG
			static float displayVal = error;
			static int frameCount = 0;
			++frameCount;
			if (frameCount % 30 == 0) displayVal = error;
			ImGui::Text("Video sync error: %.3f s", displayVal);
			ImGui::Checkbox("Correcting", &correctPlaybackErrorActive);
#endif

			if (correctPlaybackErrorActive) {
				if (absError >= 0.0005f) {
					float bias = MpvData.currentSpeed > 1.f ? MpvData.currentSpeed * 4.f * delta : 4.f * delta;
					bias = Util::Clamp(bias, 0.f, 1.f);
					smoothTime += error * bias;
				}
				else {
					LOG_DEBUG("Done correcting.");
					correctPlaybackErrorActive = false;
				}
			}
		}
	}
};