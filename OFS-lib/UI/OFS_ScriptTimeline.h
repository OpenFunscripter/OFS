#pragma once
#include "imgui.h"
#include <vector>
#include <memory>
#include <tuple>

#include "OFS_Waveform.h"
#include "OFS_Shader.h"
#include "ScriptPositionsOverlayMode.h"

#include "SDL_events.h"

class ScriptTimelineEvents {
public:
	using ActionClickedEventArgs = std::tuple<SDL_Event, FunscriptAction>;
	static int32_t FfmpegAudioProcessingFinished;
	static int32_t FunscriptActionClicked;
	static int32_t SetTimePosition;
	static int32_t ActiveScriptChanged;

	enum class Mode : int32_t
	{
		All,
		Bottom,
		Middle,
		Top
	};

	struct SelectTime {
		float startTime;
		float endTime;
		ScriptTimelineEvents::Mode mode = Mode::All;
		bool clear;
	};
	static int32_t FunscriptSelectTime;

	static void RegisterEvents() noexcept;
};


class ScriptTimeline
{
public:
	float offsetTime;
	bool IsSelecting = false;
	bool IsMoving = false;
	bool PositionsItemHovered = false;
	
	float absSel1 = 0.f; // absolute selection start
	float relSel2 = 0.f; // relative selection end

	ScriptTimelineEvents::ActionClickedEventArgs ActionClickEventData;
	ScriptTimelineEvents::SelectTime SelectTimeEventData = {0};

	std::unique_ptr<BaseOverlay> overlay;
		
	const char* videoPath = nullptr;
	float frameTime = 16.66667;
	
	const std::vector<std::shared_ptr<Funscript>>* Scripts = nullptr;
	
	int activeScriptIdx = 0;
	ImVec2 activeCanvasPos;
	ImVec2 activeCanvasSize;

	int hovereScriptIdx = 0;
	ImVec2 hoveredCanvasPos;
	ImVec2 hoveredCanvasSize;

	class UndoSystem* undoSystem = nullptr;
private:
	void mousePressed(SDL_Event& ev) noexcept;
	void mouseReleased(SDL_Event& ev) noexcept;
	void mouseDrag(SDL_Event& ev) noexcept;
	void mouseScroll(SDL_Event& ev) noexcept;

	void videoLoaded(SDL_Event& ev) noexcept;

	inline static ImVec2 getPointForAction(const OverlayDrawingCtx& ctx, FunscriptAction action) noexcept {
		float relative_x = (float)(action.atS - ctx.offsetTime) / ctx.visibleTime;
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
		float atTime = offsetTime + (relative_x * visibleTime);
		// fix frame alignment
		//atTime =  std::max<float>(std::round(atTime / frameTime) * frameTime, 0.f);
		float pos = Util::Clamp<float>(100.f - (relative_y * 100.f), 0.f, 100.f);
		return FunscriptAction(atTime, pos);
	}

	void updateSelection(ScriptTimelineEvents::Mode mode, bool clear) noexcept;
	void FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept;

	uint32_t visibleTimeUpdate = 0;
	float nextVisisbleTime = 5.f;
	float previousVisibleTime = 5.f;

	float visibleTime = 5.f;
	float startSelectionTime = -1.f;
	
	bool ShowAudioWaveform = false;
	float ScaleAudio = 1.f;
	
	void handleSelectionScrolling() noexcept;

public:
	OFS_WaveformLOD Wave;
	static constexpr const char* WindowId = "###POSITIONS";

	static constexpr float MAX_WINDOW_SIZE = 300.f;
	static constexpr float MIN_WINDOW_SIZE = 1.f;
	void setup(UndoSystem* undo);

	inline void ClearAudioWaveform() noexcept { ShowAudioWaveform = false; Wave.data.Clear(); }
	inline void setStartSelection(float time) noexcept { startSelectionTime = time; }
	inline float selectionStart() const noexcept { return startSelectionTime; }
	void ShowScriptPositions(bool* open, float currentTime, float duration, float frameTime, const std::vector<std::shared_ptr<Funscript>>* scripts, int activeScriptIdx) noexcept;

	void Update() noexcept;

	void DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept;
};