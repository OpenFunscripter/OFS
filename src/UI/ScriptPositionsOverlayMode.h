#pragma once
#include <cstdint>
#include <array>

#include "Funscript.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "GradientBar.h"

// ATTENTION: no reordering
enum ScriptingOverlayModes : int32_t {
	FRAME,
	TEMPO,
	EMPTY,
};

struct OverlayDrawingCtx {
	int32_t scriptIdx;
	int32_t drawnScriptCount;
	int32_t actionFromIdx;
	int32_t actionToIdx;
	ImDrawList* draw_list;
	float visibleSizeMs;
	float offset_ms;
	ImVec2 canvas_pos;
	ImVec2 canvas_size;
};

class BaseOverlay {
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

	BaseOverlay() noexcept;
	virtual ~BaseOverlay() noexcept {}
	virtual void DrawSettings() noexcept;

	virtual void update() noexcept;
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept {}
	virtual void nextFrame() noexcept;
	virtual void previousFrame() noexcept;

	static void DrawActionLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawSecondsLabel(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawHeightLines(const OverlayDrawingCtx& ctx) noexcept;
	static void DrawScriptLabel(const OverlayDrawingCtx& ctx) noexcept;
};

class EmptyOverlay : public BaseOverlay {
public:
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
};


class FrameOverlay : public BaseOverlay {

public:
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
};

class TempoOverlay : public BaseOverlay {
private:
	static constexpr std::array<float, 10> beatMultiples{
		4.f * 1.f,
		4.f * (1.f / 2.f),
		4.f * (1.f / 4.f),
		4.f * (1.f / 8.f),
		4.f * (1.f / 12.f),
		4.f * (1.f / 16.f),
		4.f * (1.f / 24.f),
		4.f * (1.f / 32.f),
		4.f * (1.f / 48.f),
		4.f * (1.f / 64.f),
	};
	static constexpr std::array<uint32_t, 10> beatMultipleColor{
		IM_COL32(0xbb, 0xbe, 0xbc, 0xFF), // 1st ???

		IM_COL32(0x53, 0xd3, 0xdf, 0xFF), // 2nds
		IM_COL32(0xc1, 0x65, 0x77, 0xFF), // 4ths
		IM_COL32(0x24, 0x54, 0x99, 0xFF), // 8ths
		IM_COL32(0xc8, 0x86, 0xee, 0xFF), // 12ths
		IM_COL32(0xd2, 0xcc, 0x23, 0xFF), // 16ths
		IM_COL32(0xea, 0x8d, 0xe0, 0xFF), // 24ths
		IM_COL32(0xe7, 0x97, 0x5c, 0xFF), // 32nds
		IM_COL32(0xeb, 0x38, 0x99, 0xFF), // 48ths
		IM_COL32(0x23, 0xd2, 0x54, 0xFF), // 64ths
	};
	static constexpr std::array<const char*, 10> beatMultiplesStrings{
		"whole measures", //???

		"2nd measures",
		"4th measures",
		"8th measures",
		"12th measures",
		"16th measures",
		"24th measures",
		"32nd measures",
		"48th measures",
		"64th measures",
	};
public:
	virtual void DrawSettings() noexcept override;
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
	virtual void nextFrame() noexcept override;
	virtual void previousFrame() noexcept override;
};