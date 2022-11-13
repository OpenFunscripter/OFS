#pragma once

#include "GradientBar.h"
#include "OFS_Videopreview.h"
#include "FunscriptHeatmap.h"

#include <functional>

// ImDrawList* draw_list, const ImRect& frame_bb, bool item_hovered
using TimelineCustomDrawFunc = std::function<void(ImDrawList*, const struct ImRect&, bool)>;

class OFS_VideoplayerControls
{
private:
	float actualPlaybackSpeed = 1.f;
	float lastPlayerPosition = 0.0f;

	uint32_t measureStartTime = 0;
	bool mute = false;
	bool hasSeeked = false;
	bool dragging = false;
	
	static constexpr int32_t PreviewUpdateMs = 1000;
	uint32_t lastPreviewUpdate = 0;
	class OFS_Videoplayer* player = nullptr;
	std::unique_ptr<FunscriptHeatmap> Heatmap;

	void VideoLoaded(SDL_Event& ev) noexcept;
	bool DrawTimelineWidget(const char* label, float* position, TimelineCustomDrawFunc&& customDraw) noexcept;
public:
	static constexpr const char* ControlId = "###CONTROLS";
	static constexpr const char* TimeId = "###TIME";
	std::unique_ptr<VideoPreview> videoPreview;

	OFS_VideoplayerControls() noexcept {}
	void Init(class OFS_Videoplayer* player) noexcept;
	inline void Destroy() noexcept { videoPreview.reset(); }

	inline void UpdateHeatmap(float totalDuration, const FunscriptArray& actions) noexcept
	{
		Heatmap->Update(totalDuration, actions);
	}

	void DrawTimeline(bool* open, TimelineCustomDrawFunc&& customDraw = [](ImDrawList*, const ImRect&, bool) {}) noexcept;
	void DrawControls(bool* open) noexcept;
};