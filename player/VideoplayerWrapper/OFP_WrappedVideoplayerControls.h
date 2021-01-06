#pragma once

#include "OFP_WrappedPlayer.h"
#include "GradientBar.h"

#include <functional>

// ImDrawList* draw_list, const ImRect& frame_bb, bool item_hovered
using TimelineCustomDrawFunc = std::function<void(ImDrawList*, const ImRect&, bool)>;

class OFP_WrappedVideoplayerControls
{
private:
	float actualPlaybackSpeed = 1.f;
	float lastPlayerPosition = 0.0f;

	uint32_t measureStartTime = 0;
	bool mute = false;
	bool hasSeeked = false;
	bool dragging = false;
public:
	static constexpr const char* PlayerControlId = "Controls";
	static constexpr const char* PlayerTimeId = "Time";
	ImGradient TimelineGradient;

	OFP_WrappedVideoplayerControls() noexcept;

	bool DrawTimelineWidget(const char* label, float* position, WrappedPlayer* player, TimelineCustomDrawFunc&& customDraw) noexcept;

	void DrawTimeline(bool* open, WrappedPlayer* player, TimelineCustomDrawFunc&& customDraw = [](ImDrawList*, const ImRect&, bool) {}) noexcept;
	void DrawControls(bool* open, WrappedPlayer* player) noexcept;
};