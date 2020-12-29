#pragma once
#include <cstdint>
#include <array>

#include "Funscript.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "GradientBar.h"

struct OverlayDrawingCtx {
	Funscript* script;
	int32_t scriptIdx;
	int32_t drawnScriptCount;
	int32_t actionFromIdx;
	int32_t actionToIdx;
	ImDrawList* draw_list;
	float visibleSizeMs;
	float offset_ms;
	float totalDurationMs;
	ImVec2 canvas_pos;
	ImVec2 canvas_size;
};

class BaseOverlay {
protected:
	class ScriptTimeline* timeline;
public:
	struct ColoredLine {
		ImVec2 p1;
		ImVec2 p2;
		uint32_t color;
	};
	static std::vector<ColoredLine> ColoredLines;
	static std::vector<FunscriptAction> ActionPositionWindow;
	static std::vector<ImVec2> SelectedActionScreenCoordinates;
	static std::vector<ImVec2> ActionScreenCoordinates;
	static ImGradient speedGradient;
	// used for calculating stroke color with speedGradient
	static constexpr float max_speed_per_seconds = 530.f; // arbitrarily choosen maximum tuned for coloring
	static bool SplineLines;
	static float SplineEasing;

	BaseOverlay(class ScriptTimeline* timeline) noexcept;
	virtual ~BaseOverlay() noexcept {}
	virtual void DrawSettings() noexcept;

	virtual void update() noexcept;
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept {}
	virtual void nextFrame() noexcept {}
	virtual void previousFrame() noexcept {}

	static void DrawActionLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawSecondsLabel(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawHeightLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawScriptLabel(const OverlayDrawingCtx& ctx) noexcept;
};

class EmptyOverlay : public BaseOverlay {
public:
	EmptyOverlay(class ScriptTimeline* timeline) : BaseOverlay(timeline) {}
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
};


