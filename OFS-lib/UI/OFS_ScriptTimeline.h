#pragma once
#include "imgui.h"

#include <vector>
#include <memory>
#include <string>

#include "OFS_Waveform.h"
#include "OFS_Shader.h"
#include "ScriptPositionsOverlayMode.h"
#include "OFS_Videoplayer.h"

#include "SDL_events.h"

class ScriptTimelineEvents {
public:
	using ActionClickedEventArgs = FunscriptAction;
	static int32_t FunscriptActionClicked;
	
	using ActionMovedEventArgs = std::tuple<FunscriptAction, std::weak_ptr<Funscript>>;
	static int32_t FunscriptActionMoved;
	static int32_t FunscriptActionMoveStarted;

	using ActionCreatedEventArgs = FunscriptAction;
	static int32_t FunscriptActionCreated;

	static int32_t FfmpegAudioProcessingFinished;
	static int32_t SetTimePosition;
	static int32_t ActiveScriptChanged;

	struct SelectTime {
		float startTime;
		float endTime;
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

	uint32_t overlayStateHandle = 0xFFFF'FFFF;

	ScriptTimelineEvents::ActionClickedEventArgs ActionClickEventData;
	ScriptTimelineEvents::ActionMovedEventArgs ActionMovedEventData;
	ScriptTimelineEvents::SelectTime SelectTimeEventData = {0};
	ScriptTimelineEvents::ActionCreatedEventArgs ActionCreatedEventData;

	int activeScriptIdx = 0;
	ImVec2 activeCanvasPos;
	ImVec2 activeCanvasSize;

	int hovereScriptIdx = 0;
	ImVec2 hoveredCanvasPos;
	ImVec2 hoveredCanvasSize;
private:
	void mouseScroll(SDL_Event& ev) noexcept;
	void videoLoaded(SDL_Event& ev) noexcept;

	void handleTimelineHover(const OverlayDrawingCtx& ctx) noexcept;
	void handleActionClicks(const OverlayDrawingCtx& ctx) noexcept;

	inline static FunscriptAction getActionForPoint(const OverlayDrawingCtx& ctx, const ImVec2& point) noexcept {
		auto localCoord = point - ctx.canvasPos;
		float relativeX = localCoord.x / ctx.canvasSize.x;
		float relativeY = localCoord.y / ctx.canvasSize.y;
		float atTime = ctx.offsetTime + (relativeX * ctx.visibleTime);
		float pos = Util::Clamp<float>(100.f - (relativeY * 100.f), 0.f, 100.f);
		return FunscriptAction(atTime, pos);
	}

	void updateSelection(bool clear) noexcept;
	void FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept;

	std::string videoPath;
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

	void Init();
	inline void ClearAudioWaveform() noexcept { ShowAudioWaveform = false; Wave.data.Clear(); }
	inline void setStartSelection(float time) noexcept { startSelectionTime = time; }
	inline float selectionStart() const noexcept { return startSelectionTime; }
	void ShowScriptPositions(const OFS_Videoplayer* player, BaseOverlay* overlay, const std::vector<std::shared_ptr<Funscript>>& scripts, int activeScriptIdx) noexcept;

	void Update() noexcept;

	void DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept;
};