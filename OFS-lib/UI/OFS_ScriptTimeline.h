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

#include "OFS_Waveform.h"


class ScriptTimelineEvents {
public:
	static int32_t FfmpegAudioProcessingFinished;
	
	using ActionClickedEventArgs = std::tuple<SDL_Event, FunscriptAction>;
	static int32_t FunscriptActionClicked;

	static int32_t ScriptpositionWindowDoubleClick;
	static int32_t ActiveScriptChanged;

	enum class Mode : int32_t
	{
		All,
		Bottom,
		Middle,
		Top
	};

	struct SelectTime {
		int32_t start_ms;
		int32_t end_ms;
		ScriptTimelineEvents::Mode mode = Mode::All;
		bool clear;
	};
	static int32_t FunscriptSelectTime;

	static void RegisterEvents() noexcept;
};


class ScriptTimeline
{
public:
	float offsetMs;
	float visibleSizeMs;
	bool IsSelecting = false;
	bool IsMoving = false;
	bool PositionsItemHovered = false;
	float relX1 = 0.0f;
	float relX2 = 0.0f;

	ScriptTimelineEvents::ActionClickedEventArgs ActionClickEventData;
	ScriptTimelineEvents::SelectTime SelectTimeEventData = {0};

	std::unique_ptr<BaseOverlay> overlay;
	std::vector<std::pair<FunscriptAction, FunscriptAction>> RecordingBuffer;
	
	std::vector<float> WaveformLineBuffer;
	uint32_t WaveformTex = 0;
	std::unique_ptr<class WaveformShader> WaveShader;
	bool WaveformPartyMode = false;
	ImColor WaveformColor = IM_COL32(227, 66, 52, 255);
	ImGuiViewport* WaveformViewport = nullptr;
		
	const char* videoPath = nullptr;
	float frameTimeMs = 16.66667;
	
	const std::vector<std::shared_ptr<Funscript>>* Scripts = nullptr;
	
	int activeScriptIdx = 0;
	ImVec2 activeCanvasPos;
	ImVec2 activeCanvasSize;

	int hovereScriptIdx = 0;
	ImVec2 hoveredCanvasPos;
	ImVec2 hoveredCanvasSize;

	UndoSystem* undoSystem = nullptr;
private:
	void mousePressed(SDL_Event& ev) noexcept;
	void mouseReleased(SDL_Event& ev) noexcept;
	void mouseDrag(SDL_Event& ev) noexcept;
	void mouseScroll(SDL_Event& ev) noexcept;

	void videoLoaded(SDL_Event& ev) noexcept;

	inline static ImVec2 getPointForAction(const OverlayDrawingCtx& ctx, FunscriptAction action) noexcept {
		float relative_x = (float)(action.at - ctx.offset_ms) / ctx.visibleSizeMs;
		float x = (ctx.canvas_size.x) * relative_x;
		float y = (ctx.canvas_size.y) * (1 - (action.pos / 100.0));
		x += ctx.canvas_pos.x;
		y += ctx.canvas_pos.y;
		return ImVec2(x, y);
	}

	inline FunscriptAction getActionForPoint(ImVec2 canvas_pos, ImVec2 canvas_size, const ImVec2& point, float frameTime) noexcept {
		ImVec2 localCoord;
		localCoord = point - canvas_pos;
		float relative_x = localCoord.x / canvas_size.x;
		float relative_y = localCoord.y / canvas_size.y;
		float at_ms = offsetMs + (relative_x * visibleSizeMs);
		// fix frame alignment
		at_ms =  std::max<float>(std::round(at_ms / frameTime) * frameTime, 0.f);
		float pos = Util::Clamp<float>(100.f - (relative_y * 100.f), 0.f, 100.f);
		return FunscriptAction(at_ms, pos);
	}

	void updateSelection(ScriptTimelineEvents::Mode mode, bool clear) noexcept;
	void FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept;

	float WindowSizeSeconds = 5.f;
	int32_t startSelectionMs = -1;
	
	bool ShowAudioWaveform = false;
	float ScaleAudio = 1.f;
	OFS_Waveform waveform;
public:
	static constexpr const char* PositionsId = "Positions";

	static constexpr float MAX_WINDOW_SIZE = 300.f; // this limit is arbitrary and not enforced
	static constexpr float MIN_WINDOW_SIZE = 1.f; // this limit is also arbitrary and not enforced
	void setup(UndoSystem* undo);

	inline void ClearAudioWaveform() noexcept { ShowAudioWaveform = false; waveform.Clear(); }
	inline void setStartSelection(int32_t ms) noexcept { startSelectionMs = ms; }
	inline int32_t selectionStart() const noexcept { return startSelectionMs; }
	void ShowScriptPositions(bool* open, float currentPositionMs, float durationMs, float frameTimeMs, const std::vector<std::shared_ptr<Funscript>>* scripts, int activeScriptIdx) noexcept;

	void DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept;
};