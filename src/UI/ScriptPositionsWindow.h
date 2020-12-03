#pragma once

#include "Funscript.h"
#include "GradientBar.h"
#include "ScriptPositionsOverlayMode.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <memory>
#include <tuple>

#include "SDL_events.h"

using ActionClickedEventArgs = std::tuple<SDL_Event, FunscriptAction>;

static bool OutputAudioFile(const char* ffmpeg_path, const char* video_path, const char* output_path);

class ScriptPositionsWindow
{
	ImGradient speedGradient;
	// used for calculating stroke color with speedGradient
	const float max_speed_per_seconds = 530.f; // arbitrarily choosen maximum tuned for coloring
	
	std::vector<float> audio_waveform_avg;
	bool ffmpegInProgress = false;

public:
	ImVec2 active_canvas_pos;
	ImVec2 active_canvas_size;
	float offset_ms;
	float visibleSizeMs;
	bool IsSelecting = false;
	bool IsMoving = false;
	bool PositionsItemHovered = false;
	float rel_x1 = 0.0f;
	float rel_x2 = 0.0f;
private:

	void mouse_pressed(SDL_Event& ev);
	void mouse_released(SDL_Event& ev);
	void mouse_drag(SDL_Event& ev);
	void mouse_scroll(SDL_Event& ev);

	inline ImVec2 getPointForAction(ImVec2 canvas_pos, ImVec2 canvas_size, FunscriptAction action) noexcept {
		float relative_x = (float)(action.at - offset_ms) / visibleSizeMs;
		float x = (canvas_size.x) * relative_x;
		float y = (canvas_size.y) * (1 - (action.pos / 100.0));
		x += canvas_pos.x;
		y += canvas_pos.y;
		return ImVec2(x, y);
	}

	inline FunscriptAction getActionForPoint(ImVec2 canvas_pos, ImVec2 canvas_size, const ImVec2& point, float frameTime) noexcept {
		ImVec2 localCoord;
		localCoord = point - canvas_pos;
		float relative_x = localCoord.x / canvas_size.x;
		float relative_y = localCoord.y / canvas_size.y;
		float at_ms = offset_ms + (relative_x * visibleSizeMs);
		// fix frame alignment
		at_ms =  std::max<float>((int32_t)(at_ms / frameTime) * frameTime, 0.f);
		float pos = Util::Clamp<float>(100.f - (relative_y * 100.f), 0.f, 100.f);
		return FunscriptAction(at_ms, pos);
	}

	void updateSelection(bool clear);

	std::vector<FunscriptAction> ActionPositionWindow;
	std::vector<ImVec2> ActionScreenCoordinates;
	std::vector<ImVec2> SelectedActionScreenCoordinates;
	
	struct ColoredLine {
		ImVec2 p1;
		ImVec2 p2;
		uint32_t color;
	};
	std::vector<ColoredLine> ColoredLines;

	void FfmpegAudioProcessingFinished(SDL_Event& ev);

	// ATTENTION: no reordering
	enum RecordingRenderMode : int32_t {
		None,
		All,
		ActiveOnly,
	};

	RecordingRenderMode RecordingMode = RecordingRenderMode::All;
	bool ShowRegularActions = true;
	bool ShowAudioWaveform = false;
	float ScaleAudio = 1.f;
	float WindowSizeSeconds = 5.f;
	int32_t startSelectionMs = -1;
public:
	static constexpr const char* PositionsId = "Positions";

	const float MAX_WINDOW_SIZE = 60.f; // this limit is arbitrary and not enforced
	const float MIN_WINDOW_SIZE = 1.f; // this limit is also arbitrary and not enforced
	void setup();

	inline void ClearAudioWaveform() noexcept { audio_waveform_avg.clear(); }
	inline void setStartSelection(int32_t ms) noexcept { startSelectionMs = ms; }
	inline int32_t selectionStart() const noexcept { return startSelectionMs; }
	void ShowScriptPositions(bool* open, float currentPositionMs);

	void DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept;
	void DrawActions(const OverlayDrawingCtx& ctx) noexcept;
};