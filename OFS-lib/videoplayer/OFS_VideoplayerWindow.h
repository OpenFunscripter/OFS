#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include "OFS_Reflection.h"
#include "OFS_BinarySerialization.h"
#include "OFS_Util.h"
#include "OFS_Shader.h"
#include "OFS_Localization.h"
#include "OFS_VideoplayerEvents.h"

#include <string>
#include "SDL_events.h"


enum VideoMode : int32_t {
	FULL,
	LEFT_PANE,
	RIGHT_PANE,
	TOP_PANE,
	BOTTOM_PANE,
	VR_MODE,
	TOTAL_NUM_MODES,
};

class OFS_VideoplayerWindow
{
public:
	~OFS_VideoplayerWindow() noexcept;
private:
	class OFS_Videoplayer* player = nullptr;
	std::unique_ptr<VrShader> vrShader;
	ImGuiViewport* playerViewport;
	
	ImGuiID videoImageId;
	ImVec2 videoDrawSize;
	ImVec2 viewportPos;
	ImVec2 windowPos;

	float baseScaleFactor = 1.f;

	bool videoHovered = false;
	bool dragStarted = false;

	static constexpr float ZoomMulti = 0.05f;

	void mouseScroll(SDL_Event& ev) noexcept;
	void drawVrVideo(ImDrawList* draw_list) noexcept;
	void draw2dVideo(ImDrawList* draw_list) noexcept;
	void videoRightClickMenu() noexcept;
public:
	static constexpr const char* WindowId = "###VIDEOPLAYER";
	ImDrawCallback OnRenderCallback = nullptr;

	// FIXME: player related settings don't work anymore
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

	OFS_VideoPlayerSettings settings;

	bool Init(OFS_Videoplayer* player) noexcept;
	void DrawVideoPlayer(bool* open, bool* drawVideo) noexcept;

	inline void resetTranslationAndZoom() noexcept {
		if (settings.LockedPosition) return;
		settings.zoomFactor = 1.f;
		settings.prevTranslation = ImVec2(0.f, 0.f);
		settings.currentTranslation = ImVec2(0.f, 0.f); 
	}

	//inline void syncWithRealTime() noexcept { MpvData.percentPos = MpvData.realPercentPos; }
};
