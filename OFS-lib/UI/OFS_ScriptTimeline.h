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
	uint32_t overlayStateHandle = 0xFFFF'FFFF;
	float absSel1 = 0.f; // absolute selection start
	float relSel2 = 0.f; // relative selection end

	bool IsSelecting = false;
	bool PositionsItemHovered = false;
	int32_t IsMovingIdx = -1;

	ScriptTimelineEvents::ActionClickedEventArgs ActionClickEventData;
	ScriptTimelineEvents::ActionMovedEventArgs ActionMovedEventData;
	ScriptTimelineEvents::SelectTime SelectTimeEventData = {0};
	ScriptTimelineEvents::ActionCreatedEventArgs ActionCreatedEventData;
private:
	void mouseScroll(SDL_Event& ev) noexcept;
	void videoLoaded(SDL_Event& ev) noexcept;

	void handleSelectionScrolling(const OverlayDrawingCtx& ctx) noexcept;
	void handleTimelineHover(const OverlayDrawingCtx& ctx) noexcept;
	bool handleTimelineClicks(const OverlayDrawingCtx& ctx) noexcept;

	void updateSelection(const OverlayDrawingCtx& ctx, bool clear) noexcept;
	void FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept;

	std::string videoPath;
	uint32_t visibleTimeUpdate = 0;
	float nextVisisbleTime = 5.f;
	float previousVisibleTime = 5.f;

	float visibleTime = 5.f;
	float startSelectionTime = -1.f;
	
	bool ShowAudioWaveform = false;
	float ScaleAudio = 1.f;
public:
	OFS_WaveformLOD Wave;
	static constexpr const char* WindowId = "###POSITIONS";

	static constexpr float MaxVisibleTime = 300.f;
	static constexpr float MinVisibleTime = 1.f;

	void Init();
	inline void ClearAudioWaveform() noexcept { ShowAudioWaveform = false; Wave.data.Clear(); }
	inline void setStartSelection(float time) noexcept { startSelectionTime = time; }
	inline float selectionStart() const noexcept { return startSelectionTime; }
	void ShowScriptPositions(const OFS_Videoplayer* player, BaseOverlay* overlay, const std::vector<std::shared_ptr<Funscript>>& scripts, int activeScriptIdx) noexcept;

	void Update() noexcept;

	void DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept;
};