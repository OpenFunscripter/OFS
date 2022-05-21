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
	float visibleTime;
	float offsetTime;
	float totalDuration;
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
	static float PointSize;
	
	static bool ShowMaxSpeedHighlight;
	static float MaxSpeedPerSecond;
	static ImColor MaxSpeedColor;

	static ImGradient speedGradient;
	static bool SplineMode;
	static bool ShowActions;
	static bool SyncLineEnable;

	BaseOverlay(class ScriptTimeline* timeline) noexcept;
	virtual ~BaseOverlay() noexcept {}
	virtual void DrawSettings() noexcept;

	virtual void update() noexcept;
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept {}
	virtual void nextFrame() noexcept {}
	virtual void previousFrame() noexcept {}

	virtual float steppingIntervalForward(float fromTime) noexcept = 0;
	virtual float steppingIntervalBackward(float fromTime) noexcept = 0;

	static void DrawActionLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawSecondsLabel(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawHeightLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawScriptLabel(const OverlayDrawingCtx& ctx) noexcept;
};

class EmptyOverlay : public BaseOverlay {
public:
	EmptyOverlay(class ScriptTimeline* timeline) : BaseOverlay(timeline) {}
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
	virtual float steppingIntervalForward(float fromTime) noexcept override;
	virtual float steppingIntervalBackward(float fromTime) noexcept override;
};


