#pragma once

#include "Funscript.h"
#include "GradientBar.h"
#include "ScriptPositionsOverlayMode.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <memory>
#include <tuple>

#include "OFS_UndoSystem.h"
#include "EventSystem.h"
#include "SDL_events.h"


class ScriptTimelineEvents {
public:
	static int32_t FfmpegAudioProcessingFinished;
	static int32_t FunscriptActionClicked;
	static int32_t ScriptpositionWindowDoubleClick;
	static int32_t ActiveScriptChanged;

	struct SelectTime {
		int32_t start_ms;
		int32_t end_ms;
		bool clear;
	};
	static int32_t FunscriptSelectTime;

	static void RegisterEvents() noexcept;
};

using ActionClickedEventArgs = std::tuple<SDL_Event, FunscriptAction>;

static bool OutputAudioFile(const char* ffmpeg_path, const char* video_path, const char* output_path);

class ScriptTimeline
{
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

	std::unique_ptr<BaseOverlay> overlay;

	std::vector<FunscriptAction> RecordingBuffer;
	
	const char* videoPath = nullptr;
	float frameTimeMs = 16.66667;
	Funscript* activeScript = nullptr;
	UndoSystem* undoSystem = nullptr;
private:

	void mouse_pressed(SDL_Event& ev) noexcept;
	void mouse_released(SDL_Event& ev) noexcept;
	void mouse_drag(SDL_Event& ev) noexcept;
	void mouse_scroll(SDL_Event& ev) noexcept;

	void videoLoaded(SDL_Event& ev) noexcept;

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
	void FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept;

	bool ShowAudioWaveform = false;
	float ScaleAudio = 1.f;
	float WindowSizeSeconds = 5.f;
	int32_t startSelectionMs = -1;

public:
	static constexpr const char* PositionsId = "Positions";

	const float MAX_WINDOW_SIZE = 300.f; // this limit is arbitrary and not enforced
	const float MIN_WINDOW_SIZE = 1.f; // this limit is also arbitrary and not enforced
	void setup(UndoSystem* undo);

	inline void ClearAudioWaveform() noexcept { audio_waveform_avg.clear(); }
	inline void setStartSelection(int32_t ms) noexcept { startSelectionMs = ms; }
	inline int32_t selectionStart() const noexcept { return startSelectionMs; }
	void ShowScriptPositions(bool* open, float currentPositionMs, float durationMs, float frameTimeMs, const std::vector<std::shared_ptr<Funscript>>& scripts, Funscript* activeScript) noexcept;

	void DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept;
};