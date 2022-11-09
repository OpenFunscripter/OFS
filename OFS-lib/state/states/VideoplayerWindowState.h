#pragma once

#include "OFS_StateHandle.h"

enum VideoMode : int32_t {
	Full,
	LeftPane,
	RightPane,
	TopPane,
	BottomPane,
	VrMode,
	TotalNumModes,
};

struct VideoPlayerWindowState
{
    static constexpr auto StateName = "VideoPlayerWindowState";

    ImVec2 currentVrRotation = ImVec2(0.5f, -0.5f);
    ImVec2 currentTranslation = ImVec2(0.0f, 0.0f);
    ImVec2 videoPos = ImVec2(0.0f, 0.0f);
    ImVec2 prevVrRotation = currentVrRotation;
    ImVec2 prevTranslation = currentTranslation;

    VideoMode activeMode = VideoMode::Full;
    float vrZoom = 0.2f;
    float zoomFactor = 1.f;
    bool lockedPosition = false;

	inline static VideoPlayerWindowState& State(uint32_t stateHandle) noexcept {
		return OFS_ProjectState<VideoPlayerWindowState>(stateHandle).Get();
	}
};

REFL_TYPE(VideoPlayerWindowState)
	REFL_FIELD(currentVrRotation)
    REFL_FIELD(currentTranslation)
    REFL_FIELD(videoPos)
    REFL_FIELD(prevVrRotation)
    REFL_FIELD(prevTranslation)
    REFL_FIELD(activeMode, serializeEnum{})
    REFL_FIELD(vrZoom)
    REFL_FIELD(zoomFactor)
    REFL_FIELD(lockedPosition)
REFL_END
