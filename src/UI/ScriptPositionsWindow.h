#pragma once

#include "Funscript.h"
#include "GradientBar.h"

#include "imgui.h"

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

	ImVec2 canvas_pos;
	ImVec2 canvas_size;
	int offset_ms;
	float frameSizeMs;
	bool selection = false;
	bool PositionsItemHovered = false;
	float rel_x1 = 0.0f;
	float rel_x2 = 0.0f;

	void mouse_pressed(SDL_Event& ev);
	void mouse_released(SDL_Event& ev);
	void mouse_drag(SDL_Event& ev);
	void mouse_scroll(SDL_Event& ev);

	inline ImVec2 getPointForAction(const FunscriptAction& action) {
		float relative_x = (float)(action.at - offset_ms) / frameSizeMs;
		float x = (canvas_size.x) * relative_x;
		float y = (canvas_size.y) * (1 - (action.pos / 100.0));
		x += canvas_pos.x;
		y += canvas_pos.y;
		return ImVec2(x, y);
	}
	void updateSelection(bool clear);

	std::vector<FunscriptAction> ActionPositionWindow;
	std::vector<ImVec2> ActionScreenCoordinates;
	
	void FfmpegAudioProcessingFinished(SDL_Event& ev);

	bool ShowRawActions = true;
	bool ShowRegularActions = true;
	bool ShowAudioWaveform = false;
	float ScaleAudio = 1.f;
	float WindowSizeSeconds = 5.f;
public:

	const float MAX_WINDOW_SIZE = 60.f; // this limit is arbitrary and not enforced
	const float MIN_WINDOW_SIZE = 1.f; // this limit is also arbitrary and not enforced
	void setup();

	inline void ClearAudioWaveform() { audio_waveform_avg.clear(); }
	void ShowScriptPositions(bool* open, float currentPositionMs);
};