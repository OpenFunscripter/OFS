#include "ScriptPositionsWindow.h"
#include "imgui_internal.h"

#include "OpenFunscripter.h"

#include "SDL.h"
#include "stb_sprintf.h"

//#define MINIMP3_ONLY_SIMD
//#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

void ScriptPositionsWindow::updateSelection(bool clear)
{
	float min = std::min(rel_x1, rel_x2);
	float max = std::max(rel_x1, rel_x2);
	int selection_start_ms = offset_ms + (frameSizeMs * min);
	int selection_end_ms = offset_ms + (frameSizeMs * max);
	OpenFunscripter::script().SelectTime(selection_start_ms, selection_end_ms, clear);
}

void ScriptPositionsWindow::FfmpegAudioProcessingFinished(SDL_Event& ev)
{
	ShowAudioWaveform = true;
	ffmpegInProgress = false;
	LOG_INFO("Audio processing complete.");
}

void ScriptPositionsWindow::setup()
{
	auto app = OpenFunscripter::ptr;
	app->events->Subscribe(SDL_MOUSEBUTTONDOWN, EVENT_SYSTEM_BIND(this, &ScriptPositionsWindow::mouse_pressed));
	app->events->Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &ScriptPositionsWindow::mouse_scroll));
	app->events->Subscribe(SDL_MOUSEMOTION, EVENT_SYSTEM_BIND(this, &ScriptPositionsWindow::mouse_drag));
	app->events->Subscribe(SDL_MOUSEBUTTONUP, EVENT_SYSTEM_BIND(this, &ScriptPositionsWindow::mouse_released));
	app->events->Subscribe(EventSystem::FfmpegAudioProcessingFinished, EVENT_SYSTEM_BIND(this, &ScriptPositionsWindow::FfmpegAudioProcessingFinished));

	std::array<ImColor, 4> heatColor{
		IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
		IM_COL32(0x66, 0xff, 0x00, 0xFF),
		IM_COL32(0xFF, 0xff, 0x00, 0xFF),
		IM_COL32(0xFF, 0x00, 0x00, 0xFF),
	};

	float pos = 0.0f;
	for (auto& col : heatColor) {
		speedGradient.addMark(pos, col);
		pos += (1.f / (heatColor.size() - 1));
	}
	speedGradient.refreshCache();

}

void ScriptPositionsWindow::mouse_pressed(SDL_Event& ev)
{
	auto& button = ev.button;
	auto mousePos = ImGui::GetMousePos();
	auto modstate = SDL_GetModState();
	FunscriptAction* clickedAction = nullptr;
	
	if (PositionsItemHovered) {
		if (button.clicks == 2) {
			// seek to position double click
			float rel_x = (mousePos.x - canvas_pos.x) / canvas_size.x;
			int32_t seekToMs = offset_ms + (frameSizeMs * rel_x);
			OpenFunscripter::ptr->player.setPosition(seekToMs, false);
			return;
		}

		// test if an action has been clicked
		int index = 0;
		for (auto& vert : ActionScreenCoordinates) {
			const ImVec2 size(10, 10);
			ImRect rect(vert - size, vert + size);
			if (rect.Contains(mousePos)) {
				clickedAction = &ActionPositionWindow[index];
				break;
			}
			index++;
		}
	}

	if (button.button == SDL_BUTTON_LEFT) {
		if (modstate & KMOD_SHIFT && PositionsItemHovered) {
			auto app = OpenFunscripter::ptr;
			if (clickedAction != nullptr) {
				// start move
				app->script().ClearSelection();
				app->script().SetSelection(*clickedAction, true);
				IsMoving = true;
				app->undoRedoSystem.Snapshot("Mouse move action");
				return;
			}

			// shift click an action into the window
			auto action = getActionForPoint(mousePos, app->player.getFrameTimeMs());
			auto edit = app->script().GetActionAtTime(action.at, app->player.getFrameTimeMs());
			app->undoRedoSystem.Snapshot("Added action");
			if (edit != nullptr) { app->script().RemoveAction(*edit); }
			app->script().AddAction(action);
		}
		else if (PositionsItemHovered) {
			if (clickedAction != nullptr) { 
				static ActionClickedEventArgs args;
				args = std::tuple<SDL_Event, FunscriptAction>(ev, *clickedAction);
				SDL_Event notify;
				notify.type = EventSystem::FunscriptActionClickedEvent;
				notify.user.data1 = &args;
				SDL_PushEvent(&notify);
				return; 
			}
			// start drag selection
			ImRect rect(canvas_pos, canvas_pos + canvas_size);
			IsSelecting = true;
			rel_x1 = (mousePos.x - canvas_pos.x) / rect.GetWidth();
			rel_x2 = rel_x1;
		}
	}
	else if (button.button == SDL_BUTTON_MIDDLE) {
		OpenFunscripter::script().ClearSelection();
	}
}

void ScriptPositionsWindow::mouse_released(SDL_Event& ev)
{
	auto& button = ev.button;
	if (IsMoving && button.button == SDL_BUTTON_LEFT) {
		IsMoving = false;
	}
	else if (IsSelecting && button.button == SDL_BUTTON_LEFT) {
		IsSelecting = false;
		auto modstate = SDL_GetModState();
		// regular select
		updateSelection(!(modstate & KMOD_CTRL));
	}
}

void ScriptPositionsWindow::mouse_drag(SDL_Event& ev)
{
	auto& motion = ev.motion;
	if (IsSelecting) {
		rel_x2 = (ImGui::GetMousePos().x - canvas_pos.x) / canvas_size.x;
	}
	else if (IsMoving) {
		auto app = OpenFunscripter::ptr;
		if (!app->script().HasSelection()) { IsMoving = false; return; }
		auto mousePos = ImGui::GetMousePos();
		auto frameTime = app->player.getFrameTimeMs();
		auto& toBeMoved = app->script().Selection()[0];
		auto newAction = getActionForPoint(mousePos, frameTime);
		if (newAction.at != toBeMoved.at || newAction.pos != toBeMoved.pos) {
			const FunscriptAction* nearbyAction = nullptr;
			if ((newAction.at - toBeMoved.at) > 0) {
				nearbyAction = app->script().GetNextActionAhead(toBeMoved.at);
				if (nearbyAction != nullptr) {
					if (std::abs(nearbyAction->at - newAction.at) > frameTime) {
						nearbyAction = nullptr;
					}
				}
			}
			else if((newAction.at - toBeMoved.at) < 0) {
				nearbyAction = app->script().GetPreviousActionBehind(toBeMoved.at);
				if (nearbyAction != nullptr) {
					if (std::abs(nearbyAction->at - newAction.at) > frameTime) {
						nearbyAction = nullptr;
					}
 				}
			}

			if (nearbyAction == nullptr) {
				app->script().RemoveAction(toBeMoved);
				app->script().ClearSelection();
				app->script().AddAction(newAction);
				app->script().SetSelection(newAction, true);
			}
			else {
				app->script().ClearSelection();
				IsMoving = false;
			}
		}
	}
}

void ScriptPositionsWindow::mouse_scroll(SDL_Event& ev)
{
	auto& wheel = ev.wheel;
	const float scrollPercent = 0.15f;
	ImRect rect(canvas_pos, canvas_pos + canvas_size);
	if (rect.Contains(ImGui::GetMousePos())) {
		WindowSizeSeconds *= 1 + (scrollPercent * -wheel.y);
		WindowSizeSeconds = Util::Clamp(WindowSizeSeconds, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE);
	}
}

void ScriptPositionsWindow::ShowScriptPositions(bool* open, float currentPositionMs)
{
	auto& script = OpenFunscripter::script();

	frameSizeMs = WindowSizeSeconds * 1000.0;
	offset_ms = currentPositionMs - (frameSizeMs / 2.0);

	ActionScreenCoordinates.clear();
	ActionPositionWindow.clear();

	ImGui::Begin(PositionsId, open, ImGuiWindowFlags_None);
	auto draw_list = ImGui::GetWindowDrawList();

	canvas_pos = ImGui::GetCursorScreenPos();
	canvas_size = ImGui::GetContentRegionAvail();
	auto& style = ImGui::GetStyle();
	const ImGuiID itemID = ImGui::GetID("##ScriptPositions");
	ImRect itemBB(canvas_pos, canvas_pos + canvas_size);
	ImGui::ItemAdd(itemBB, itemID);
	float frameTime = OpenFunscripter::ptr->player.getFrameTimeMs();
	bool IsPaused = OpenFunscripter::ptr->player.isPaused();

	draw_list->AddRectFilledMultiColor(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		IM_COL32(0, 0, 50, 255), IM_COL32(0, 0, 50, 255),
		IM_COL32(0, 0, 20, 255), IM_COL32(0, 0, 20, 255)
	);


	// height indicators
	for (int i = 0; i < 9; i++) {
		auto color = (i == 4) ? IM_COL32(150, 150, 150, 255) : IM_COL32(80, 80, 80, 255);
		auto thickness = (i == 4) ? 2.f : 1.0f;
		draw_list->AddLine(
			canvas_pos + ImVec2(0.0, (canvas_size.y / 10.f) * (i + 1)),
			canvas_pos + ImVec2(canvas_size.x, (canvas_size.y / 10.f) * (i + 1)),
			color,
			thickness
		);
	}

	float visibleFrames = frameSizeMs / frameTime;
	const float maxVisibleFrames = 400.f;
	if (visibleFrames <= (maxVisibleFrames * 0.75f)) {
		// render frame dividers
		float offset = -std::fmod(offset_ms, frameTime);
		const int lineCount = visibleFrames + 2;
		int alpha = 255 * (1.f - (visibleFrames / maxVisibleFrames));
		for (int i = 0; i < lineCount; i++) {
			draw_list->AddLine(
				canvas_pos + ImVec2(((offset + (i * frameTime)) / frameSizeMs) * canvas_size.x, 0.f),
				canvas_pos + ImVec2(((offset + (i * frameTime)) / frameSizeMs) * canvas_size.x, canvas_size.y),
				IM_COL32(80, 80, 80, alpha),
				1.f
			);
		}

		if (IsPaused || OpenFunscripter::ptr->player.getSpeed() <= 0.1) {
			float realFrameTime = OpenFunscripter::ptr->player.getRealCurrentPositionMs() - offset_ms;
			draw_list->AddLine(
				canvas_pos + ImVec2((realFrameTime / frameSizeMs) * canvas_size.x, 0.f),
				canvas_pos + ImVec2((realFrameTime / frameSizeMs) * canvas_size.x, canvas_size.y),
				IM_COL32(255, 0, 0, alpha),
				1.f
			);
		}
	}

	// border
	const float borderThicknes = 1.f;
	draw_list->AddRect(
		canvas_pos,
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		script.HasSelection() ? ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_SliderGrabActive]) : IM_COL32(255, 255, 255, 255),
		0.f, ImDrawCornerFlags_All,
		borderThicknes
	);

	if (ShowAudioWaveform) {
		const float durationMs = OpenFunscripter::ptr->player.getDuration() * 1000.f;
		const float rel_start = offset_ms / durationMs;
		const float rel_end = ((float)(offset_ms)+frameSizeMs) / durationMs;

		auto& audio_waveform = audio_waveform_avg;

		const float start_index = rel_start * audio_waveform.size();
		const float end_index = Util::Clamp<float>((rel_end * audio_waveform.size()), 0.f, audio_waveform.size());
		const int total_samples = end_index - start_index;

		const float line_width = (1.f / total_samples) * canvas_size.x + 0.75f; // 0.75 pixel padding prevents ugly spacing between lines
		for (int i = start_index; i < (int)end_index; i++) {
			const float total_pos_x = ((((float)i - start_index) / (float)total_samples)) * canvas_size.x;
			float total_len;
			if (i < 0)
				total_len = 0.f;
			else
				total_len = canvas_size.y * audio_waveform[i] * ScaleAudio;
			draw_list->AddLine(
				canvas_pos + ImVec2(total_pos_x, (canvas_size.y / 2.f) + (total_len / 2.f)),
				canvas_pos + ImVec2(total_pos_x, (canvas_size.y / 2.f) - (total_len / 2.f)),
				/*IM_COL32(245, 176, 66, 255)*/ IM_COL32(227, 66, 52, 255), line_width);
		}
	}

	// render raw actions
	const FunscriptAction* prevAction = nullptr;
	if (script.Raw().HasRecording() && RecordingMode != RecordingRenderMode::None) {
		auto pathStroke = [](auto draw_list, uint32_t col) {
			// sort of a hack ...
			// PathStroke sets  _Path.Size = 0
			// so we reset it in order to draw the same path twice
			auto tmp = draw_list->_Path.Size;
			draw_list->PathStroke(IM_COL32(0, 0, 0, 255), false, 7.0f);
			draw_list->_Path.Size = tmp;
			draw_list->PathStroke(col, false, 5.f);
		};
		auto pathRawSection = [this](auto draw_list, auto rawActions, int32_t fromIndex, int32_t toIndex) {
			float frameTimeMs = OpenFunscripter::ptr->player.getFrameTimeMs();
			for (int i = fromIndex; i < toIndex; i++) {
				auto action = rawActions[i];
				if (action.frame_no >= 0) {
					action.at = i * frameTimeMs;
					auto point = getPointForAction(FunscriptAction(action.at, action.pos));
					draw_list->PathLineTo(point);
				}
			}
		};

		switch (RecordingMode) {
		case RecordingRenderMode::All:
		{
			for(int i=0; i < script.Raw().Recordings.size(); i++) {
				auto& recording = script.Raw().Recordings[i];

				int32_t startIndex = Util::Clamp<int32_t>((offset_ms / frameTime), 0, recording.RawActions.size());
				int32_t endIndex = Util::Clamp<int32_t>(((float)offset_ms + frameSizeMs) / frameTime, startIndex, recording.RawActions.size());

				pathRawSection(draw_list, recording.RawActions, startIndex, endIndex);
				if (i != script.Raw().RecordingIdx) {
					pathStroke(draw_list, IM_COL32(255, 0, 0, 180));
				}
				else {
					pathStroke(draw_list, IM_COL32(0, 255, 0, 180));
				}
			}
			break;
		}
		case RecordingRenderMode::ActiveOnly: 
		{
			auto& recording = script.ActiveRecording();

			int32_t startIndex = Util::Clamp<int32_t>((offset_ms / frameTime), 0, recording.size());
			int32_t endIndex = Util::Clamp<int32_t>(((float)offset_ms + frameSizeMs) / frameTime, startIndex, recording.size());

			pathRawSection(draw_list, recording, startIndex, endIndex);
			pathStroke(draw_list, IM_COL32(0, 255, 0, 180));
			break;
		}
		}
	}

	if (script.Actions().size() > 0) {
		// render normal actions
		if (ShowRegularActions) {
			auto startIt = std::find_if(script.Actions().begin(), script.Actions().end(),
				[&](auto& act) { return act.at >= offset_ms; });
			if (startIt != script.Actions().begin())
				startIt -= 1;

			auto endIt = std::find_if(startIt, script.Actions().end(),
				[&](auto& act) { return act.at >= offset_ms + frameSizeMs; });
			if (endIt != script.Actions().end())
				endIt += 1;

			const FunscriptAction* prevAction = nullptr;
			for (; startIt != endIt; startIt++) {
				auto& action = *startIt;

				auto p1 = getPointForAction(action);
				ActionScreenCoordinates.emplace_back(p1);
				ActionPositionWindow.emplace_back(action);

				if (prevAction != nullptr) {
					// draw line
					auto p2 = getPointForAction(*prevAction);
					// calculate speed relative to maximum speed
					float rel_speed = Util::Clamp<float>((std::abs(action.pos - prevAction->pos) / ((action.at - prevAction->at) / 1000.0f)) / max_speed_per_seconds, 0.f, 1.f);
					ImColor speed_color;
					speedGradient.getColorAt(rel_speed, &speed_color.Value.x);
					speed_color.Value.w = 1.f;
					draw_list->AddLine(p1, p2, IM_COL32(0, 0, 0, 255), 7.0f); // border
					draw_list->AddLine(p1, p2, speed_color, 3.0f);
				}

				prevAction = &action;
			}
		}

		// draw points on top of lines
		for (auto& p : ActionScreenCoordinates) {
			draw_list->AddCircleFilled(p, 7.0, IM_COL32(0, 0, 0, 255), 12); // border
			draw_list->AddCircleFilled(p, 5.0, IM_COL32(255, 0, 0, 255), 12);
		}

		if (script.HasSelection()) {
			constexpr auto selectedDots = IM_COL32(11, 252, 3, 255);
			constexpr auto selectedLines = IM_COL32(3, 194, 252, 255);

			const FunscriptAction* prev_action = nullptr;
			for (int i = 0; i < script.Selection().size(); i++) {
				auto& action = script.Selection()[i];
				auto point = getPointForAction(action);

				if (prev_action != nullptr) {
					// draw highlight line
					draw_list->AddLine(getPointForAction(*prev_action), point, selectedLines, 3.0f);
				}
				// draw highlight point
				draw_list->AddCircleFilled(point, 5.0, selectedDots, 12);
				prev_action = &action;
			}
		}
	}

	char tmp[16];
	stbsp_snprintf(tmp, sizeof(tmp), "%.2f seconds", WindowSizeSeconds);
	auto textSize = ImGui::CalcTextSize(tmp);
	draw_list->AddText(
		canvas_pos + ImVec2(style.FramePadding.x, canvas_size.y - textSize.y - style.FramePadding.y),
		ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
		tmp
	);

	// current position indicator -> |
	draw_list->AddLine(
		canvas_pos + ImVec2(canvas_size.x / 2.f, 0),
		canvas_pos + ImVec2(canvas_size.x / 2.f, canvas_size.y),
		IM_COL32(255, 255, 255, 255), 3.0f);

	// selection box
	if (IsSelecting) {
		constexpr auto selectColor = IM_COL32(3, 252, 207, 255);
		constexpr auto selectColorBackground = IM_COL32(3, 252, 207, 100);
		draw_list->AddRectFilled(canvas_pos + ImVec2(canvas_size.x * rel_x1, 0), canvas_pos + ImVec2(canvas_size.x * rel_x2, canvas_size.y), selectColorBackground);
		draw_list->AddLine(canvas_pos + ImVec2(canvas_size.x * rel_x1, 0), canvas_pos + ImVec2(canvas_size.x * rel_x1, canvas_size.y), selectColor, 3.0f);
		draw_list->AddLine(canvas_pos + ImVec2(canvas_size.x * rel_x2, 0), canvas_pos + ImVec2(canvas_size.x * rel_x2, canvas_size.y), selectColor, 3.0f);
	}

	PositionsItemHovered = ImGui::IsWindowHovered(); //ImGui::IsItemHovered();

	// right click context menu
	if (ImGui::BeginPopupContextItem())
	{
		ImGui::MenuItem("Draw actions", NULL, &ShowRegularActions);
		ImGui::Combo("Recording", (int32_t*)&RecordingMode, "No\0All\0Active\0");
		ImGui::SliderFloat("Waveform scale", &ScaleAudio, 0.25f, 10.f);
		if (ImGui::MenuItem("Audio waveform", NULL, &ShowAudioWaveform, !ffmpegInProgress)) {}
		if (ImGui::MenuItem(ffmpegInProgress ? "Processing audio..." : "Update waveform", NULL, false, !ffmpegInProgress)) {
			if (!ffmpegInProgress) {
				ShowAudioWaveform = false; // gets switched true after processing
				ffmpegInProgress = true;

				auto ffmpegThread = [](void* userData) -> int {
					auto& ctx = *((ScriptPositionsWindow*)userData);
					std::error_code ec;

					auto base_path = Util::Basepath();
					auto ffmpeg_path = base_path / "ffmpeg.exe";
					auto output_path = base_path / "tmp";
					std::filesystem::create_directories(output_path, ec);

					output_path /= "audio.mp3";
					auto video_path = OpenFunscripter::ptr->player.getVideoPath();

					bool succ = OutputAudioFile(ffmpeg_path.string().c_str(), video_path,  output_path.string().c_str());
					if (!succ) {
						LOGF_ERROR("Failed to output mp3 from video. (ffmpeg_path: \"%s\")", ffmpeg_path.string().c_str());
						ctx.ShowAudioWaveform = false;
					}
				
					mp3dec_t mp3d;
					mp3dec_file_info_t info;

					if (mp3dec_load(&mp3d, output_path.string().c_str(), &info, NULL, NULL))
					{
						/* error */
						LOGF_ERROR("failed to load \"%s\"", output_path.string().c_str());
					}

					const int samples_per_line = info.hz / 256.f; // controls the resolution

					FUN_ASSERT(info.channels == 1, "expected one audio channels");
					// create one vector of floats for each requested channel
					ctx.audio_waveform_avg.clear();
					ctx.audio_waveform_avg.reserve((info.samples / samples_per_line) + 1);

					// for each requested channel
					for (size_t offset = 0; offset < info.samples; offset += samples_per_line)
					{
						int sample_count = (info.samples - offset >= samples_per_line) 
							? samples_per_line
							: info.samples - offset;

						float average(0);
						for (int i = offset; i < offset+sample_count; i++) 
						{
							float sample = info.buffer[i];
							float abs_sample = std::abs(sample);
							average += abs_sample;
						}
						average /= sample_count;
						ctx.audio_waveform_avg.push_back(average);
					}

					auto map2range = [](float x, float in_min, float in_max, float out_min, float out_max)
					{
						return Util::Clamp<float>(
							out_min + (out_max - out_min) * (x - in_min) / (in_max - in_min),
							out_min,
							out_max
							);
					};

					auto min = std::min_element(ctx.audio_waveform_avg.begin(), ctx.audio_waveform_avg.end());
					auto max = std::max_element(ctx.audio_waveform_avg.begin(), ctx.audio_waveform_avg.end());
					if (*min != 0.f || *max != 1.f) {
						for (auto& val : ctx.audio_waveform_avg)
						{
							val = map2range(val, *min, *max, 0.f, 1.f);
						}
					}
					free(info.buffer);
					SDL_Event ev;
					ev.type = EventSystem::FfmpegAudioProcessingFinished;
					SDL_PushEvent(&ev);
					return 0;
				};
				auto handle = SDL_CreateThread(ffmpegThread, "OpenFunscripterFfmpegThread", this);
				SDL_DetachThread(handle);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

bool OutputAudioFile(const char* ffmpeg_path, const char* video_path, const char* output_path) {
	char buffer[1024];
	int num = stbsp_snprintf(buffer, sizeof(buffer), "%s -i \"%s\" -b:a 320k -ac 1 -y \"%s\"",
		ffmpeg_path,
		video_path,
		output_path);
	FUN_ASSERT(num <= 1024, "buffer to small");
	bool success = std::system(buffer) == 0;

	return success;
}