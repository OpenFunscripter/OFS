#pragma once
#include <cstdint>
#include <array>

#include "Funscript.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "GradientBar.h"

#include "state/states/BaseOverlayState.h"

struct OverlayDrawingCtx {
	Funscript* script;

	int32_t scriptIdx;
	int32_t drawnScriptCount;

	int32_t actionFromIdx;
	int32_t actionToIdx;

	int32_t selectionFromIdx;
	int32_t selectionToIdx;

	ImDrawList* drawList;

	ImVec2 canvasPos;
	ImVec2 canvasSize;
	float visibleTime;
	float offsetTime;
	float totalDuration;

	int32_t hoveredScriptIdx;
	int32_t activeScriptIdx;
};

class BaseOverlay {
protected:
	class ScriptTimeline* timeline;
	static uint32_t StateHandle;

	static void drawActionLinesSpline(const OverlayDrawingCtx& ctx, const BaseOverlayState& state) noexcept;
	static void drawActionLinesLinear(const OverlayDrawingCtx& ctx, const BaseOverlayState& state) noexcept;

public:
	inline static BaseOverlayState& State() noexcept
	{
		return BaseOverlayState::State(StateHandle);
	}

	struct ColoredLine {
		ImVec2 p1;
		ImVec2 p2;
		uint32_t color;
	};
	static std::vector<ColoredLine> ColoredLines;
	static float PointSize;
	
	static ImGradient speedGradient;
	static bool ShowLines;

	BaseOverlay(class ScriptTimeline* timeline) noexcept;
	virtual ~BaseOverlay() noexcept {}
	virtual void DrawSettings() noexcept;

	virtual void update() noexcept;
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept {}

	virtual void nextFrame(float realFrameTime) noexcept {}
	virtual void previousFrame(float realFrameTime) noexcept {}
	virtual float steppingIntervalForward(float realFrameTime, float fromTime) noexcept = 0;
	virtual float steppingIntervalBackward(float realFrameTime, float fromTime) noexcept = 0;
	virtual float logicalFrameTime(float realFrameTime) noexcept;

	static void DrawActionLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawActionPoints(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawSecondsLabel(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawHeightLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawScriptLabel(const OverlayDrawingCtx& ctx) noexcept;

	static ImVec2 GetPointForAction(const OverlayDrawingCtx& ctx, FunscriptAction action) noexcept;
};

class EmptyOverlay : public BaseOverlay {
public:
	EmptyOverlay(class ScriptTimeline* timeline) : BaseOverlay(timeline) {}
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
	virtual float steppingIntervalForward(float realFrameTime, float fromTime) noexcept override;
	virtual float steppingIntervalBackward(float realFrameTime, float fromTime) noexcept override;
};


