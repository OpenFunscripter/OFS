#include "OFS_ScriptTimeline.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "SDL.h"
#include "stb_sprintf.h"

//#define MINIMP3_ONLY_SIMD
//#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#include <array>

int32_t ScriptTimelineEvents::FfmpegAudioProcessingFinished = 0;
int32_t ScriptTimelineEvents::ScriptpositionWindowDoubleClick = 0;
int32_t ScriptTimelineEvents::FunscriptActionClicked = 0;
int32_t ScriptTimelineEvents::FunscriptSelectTime = 0;

void ScriptTimelineEvents::RegisterEvents() noexcept
{
	FfmpegAudioProcessingFinished = SDL_RegisterEvents(1);
	FunscriptActionClicked = SDL_RegisterEvents(1);
	FunscriptSelectTime = SDL_RegisterEvents(1);
	ScriptpositionWindowDoubleClick = SDL_RegisterEvents(1);
}

void ScriptTimeline::updateSelection(bool clear)
{
	float min = std::min(rel_x1, rel_x2);
	float max = std::max(rel_x1, rel_x2);
	
	static ScriptTimelineEvents::SelectTime selection;
	selection.start_ms = offset_ms + (visibleSizeMs * min);
	selection.end_ms = offset_ms + (visibleSizeMs * max);
	selection.clear = clear;

	SDL_Event ev;
	ev.type = ScriptTimelineEvents::FunscriptSelectTime;
	ev.user.data1 = &selection;
	SDL_PushEvent(&ev);
}

void ScriptTimeline::FfmpegAudioProcessingFinished(SDL_Event& ev)
{
	ShowAudioWaveform = true;
	ffmpegInProgress = false;
	LOG_INFO("Audio processing complete.");
}

void ScriptTimeline::setup(EventSystem& events, VideoplayerWindow* player)
{
	this->player = player;
	events.Subscribe(SDL_MOUSEBUTTONDOWN, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouse_pressed));
	events.Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouse_scroll));
	events.Subscribe(SDL_MOUSEMOTION, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouse_drag));
	events.Subscribe(SDL_MOUSEBUTTONUP, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouse_released));
	events.Subscribe(ScriptTimelineEvents::FfmpegAudioProcessingFinished, EVENT_SYSTEM_BIND(this, &ScriptTimeline::FfmpegAudioProcessingFinished));
}

void ScriptTimeline::mouse_pressed(SDL_Event& ev)
{
	auto& button = ev.button;
	auto mousePos = ImGui::GetMousePos();
	auto modstate = SDL_GetModState();
	FunscriptAction* clickedAction = nullptr;

	if (PositionsItemHovered) {
		if (button.button == SDL_BUTTON_LEFT && button.clicks == 2) {
			// seek to position double click
			float rel_x = (mousePos.x - active_canvas_pos.x) / active_canvas_size.x;
			int32_t seekToMs = offset_ms + (visibleSizeMs * rel_x);

			SDL_Event ev;
			ev.type = ScriptTimelineEvents::ScriptpositionWindowDoubleClick;
			ev.user.data1 =(void*)(intptr_t)seekToMs;
			SDL_PushEvent(&ev);
			return;
		}

		// test if an action has been clicked
		int index = 0;
		for (auto& vert : overlay->ActionScreenCoordinates) {
			const ImVec2 size(10, 10);
			ImRect rect(vert - size, vert + size);
			if (rect.Contains(mousePos)) {
				clickedAction = &overlay->ActionPositionWindow[index];
				static FunscriptAction clickedActionStatic;
				clickedActionStatic = *clickedAction;
				
				SDL_Event ev;
				ev.type = ScriptTimelineEvents::FunscriptActionClicked;
				ev.user.data1 = &clickedActionStatic;
				SDL_PushEvent(&ev);
				break;
			}
			index++;
		}
	}

	if (button.button == SDL_BUTTON_LEFT) {
		if (modstate & KMOD_SHIFT && PositionsItemHovered) {
			//auto app = OpenFunscripter::ptr;
			if (clickedAction != nullptr) {
				// start move
				activeScript->ClearSelection();
				activeScript->SetSelection(*clickedAction, true);
				IsMoving = true;
				//app->undoSystem->Snapshot(StateType::MOUSE_MOVE_ACTION, false);
				return;
			}

			// shift click an action into the window
			auto action = getActionForPoint(active_canvas_pos, active_canvas_size, mousePos, player->getFrameTimeMs());
			auto edit = activeScript->GetActionAtTime(action.at, player->getFrameTimeMs());
			//app->undoSystem->Snapshot(StateType::ADD_ACTION, false);
			if (edit != nullptr) { activeScript->RemoveAction(*edit); }
			activeScript->AddAction(action);
		}
		// clicking an action  fires an event
		else if (PositionsItemHovered && clickedAction != nullptr) {
			static ActionClickedEventArgs args;
			args = std::tuple<SDL_Event, FunscriptAction>(ev, *clickedAction);
			SDL_Event notify;
			notify.type = ScriptTimelineEvents::FunscriptActionClicked;
			notify.user.data1 = &args;
			SDL_PushEvent(&notify);
		}
		// selecting only works in the active timeline
		else if (PositionsItemHovered) {
			ImRect rect(active_canvas_pos, active_canvas_pos + active_canvas_size);
			if (rect.Contains(ImGui::GetMousePos())) {
				// start drag selection
				IsSelecting = true;
				rel_x1 = (mousePos.x - active_canvas_pos.x) / rect.GetWidth();
				rel_x2 = rel_x1;
			}
		}
	}
	else if (button.button == SDL_BUTTON_MIDDLE) {
		activeScript->ClearSelection();
	}
}

void ScriptTimeline::mouse_released(SDL_Event& ev)
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

void ScriptTimeline::mouse_drag(SDL_Event& ev)
{
	auto& motion = ev.motion;
	if (IsSelecting) {
		rel_x2 = (ImGui::GetMousePos().x - active_canvas_pos.x) / active_canvas_size.x;
	}
	//else if (IsMoving) {
	//	auto app = OpenFunscripter::ptr;
	//	if (!app->script().HasSelection()) { IsMoving = false; return; }
	//	auto mousePos = ImGui::GetMousePos();
	//	auto frameTime = app->player.getFrameTimeMs();
	//	auto& toBeMoved = app->script().Selection()[0];
	//	auto newAction = getActionForPoint(active_canvas_pos, active_canvas_size, mousePos, frameTime);
	//	if (newAction.at != toBeMoved.at || newAction.pos != toBeMoved.pos) {
	//		const FunscriptAction* nearbyAction = nullptr;
	//		if ((newAction.at - toBeMoved.at) > 0) {
	//			nearbyAction = app->script().GetNextActionAhead(toBeMoved.at);
	//			if (nearbyAction != nullptr) {
	//				if (std::abs(nearbyAction->at - newAction.at) > frameTime) {
	//					nearbyAction = nullptr;
	//				}
	//			}
	//		}
	//		else if((newAction.at - toBeMoved.at) < 0) {
	//			nearbyAction = app->script().GetPreviousActionBehind(toBeMoved.at);
	//			if (nearbyAction != nullptr) {
	//				if (std::abs(nearbyAction->at - newAction.at) > frameTime) {
	//					nearbyAction = nullptr;
	//				}
 //				}
	//		}

	//		if (nearbyAction == nullptr) {
	//			app->script().RemoveAction(toBeMoved);
	//			app->script().ClearSelection();
	//			app->script().AddAction(newAction);
	//			app->script().SetSelection(newAction, true);
	//		}
	//		else {
	//			app->script().ClearSelection();
	//			IsMoving = false;
	//		}
	//	}
	//}
}

void ScriptTimeline::mouse_scroll(SDL_Event& ev)
{
	auto& wheel = ev.wheel;
	constexpr float scrollPercent = 0.10f;
	if (PositionsItemHovered) {
		WindowSizeSeconds *= 1 + (scrollPercent * -wheel.y);
		WindowSizeSeconds = Util::Clamp(WindowSizeSeconds, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE);
	}
}

void ScriptTimeline::ShowScriptPositions(bool* open, const std::vector<std::unique_ptr<Funscript>>& scripts, Funscript* activeScript) noexcept
{
	this->activeScript = activeScript;

	auto& style = ImGui::GetStyle();
	visibleSizeMs = WindowSizeSeconds * 1000.0;
	float currentPositionMs = player->getCurrentPositionMsInterp();
	offset_ms = currentPositionMs - (visibleSizeMs / 2.0);
	
	OverlayDrawingCtx drawingCtx;
	drawingCtx.offset_ms = offset_ms;
	drawingCtx.visibleSizeMs = visibleSizeMs;
	drawingCtx.totalDurationMs = player->getDuration() * 1000.f;
	
	ImGui::Begin(PositionsId, open, ImGuiWindowFlags_None);
	auto draw_list = ImGui::GetWindowDrawList();
	drawingCtx.draw_list = draw_list;
	PositionsItemHovered = ImGui::IsWindowHovered();

	drawingCtx.drawnScriptCount = 0;
	for (auto&& script : scripts) {
		if (script->Enabled) { drawingCtx.drawnScriptCount++; }
	}
	const auto availSize = ImGui::GetContentRegionAvail() - ImVec2(0.f , style.ItemSpacing.y*((float)drawingCtx.drawnScriptCount-1));
	const auto startCursor = ImGui::GetCursorScreenPos();

	
	for (auto&& scriptPtr : scripts) {
		if (!scriptPtr->Enabled) { continue; }
		auto& script = *scriptPtr;
		drawingCtx.canvas_pos = ImGui::GetCursorScreenPos();
		drawingCtx.canvas_size = ImVec2(availSize.x, availSize.y / (float)drawingCtx.drawnScriptCount);
		const bool IsActivated = drawingCtx.drawnScriptCount ? scriptPtr.get() == activeScript : false;
		
		if (drawingCtx.drawnScriptCount == 1) {
			active_canvas_pos = drawingCtx.canvas_pos;
			active_canvas_size = drawingCtx.canvas_size;
		} else if (IsActivated) {
			active_canvas_pos = drawingCtx.canvas_pos;
			active_canvas_size = drawingCtx.canvas_size;
			constexpr float activatedBorderThicknes = 5.f;
			draw_list->AddRect(
				drawingCtx.canvas_pos - ImVec2(activatedBorderThicknes, activatedBorderThicknes),
				ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y) + ImVec2(activatedBorderThicknes, activatedBorderThicknes),
				IM_COL32(0, 180, 0, 255),
				0.f, ImDrawCornerFlags_All,
				activatedBorderThicknes
			);
		}
		ImVec2 newCursor(drawingCtx.canvas_pos.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y + style.ItemSpacing.y);
		if (newCursor.y < (startCursor.y + availSize.y)) { ImGui::SetCursorScreenPos(newCursor); }
	}


	ImGui::SetCursorScreenPos(startCursor);
	for(int i=0; i < scripts.size(); i++) {
		auto& scriptPtr = scripts[i];
		auto& script = *scriptPtr.get();
		if (!script.Enabled) { continue; }
		
		drawingCtx.scriptIdx = i;
		drawingCtx.canvas_pos = ImGui::GetCursorScreenPos();
		drawingCtx.canvas_size = ImVec2(availSize.x, availSize.y / (float)drawingCtx.drawnScriptCount);
		const ImGuiID itemID = ImGui::GetID(script.metadata.title.c_str());
		ImRect itemBB(drawingCtx.canvas_pos, drawingCtx.canvas_pos + drawingCtx.canvas_size);
		ImGui::ItemAdd(itemBB, itemID);

		draw_list->AddRectFilledMultiColor(drawingCtx.canvas_pos, ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
			IM_COL32(0, 0, 50, 255), IM_COL32(0, 0, 50, 255),
			IM_COL32(0, 0, 20, 255), IM_COL32(0, 0, 20, 255)
		);

		auto startIt = std::find_if(script.Actions().begin(), script.Actions().end(),
		    [&](auto& act) { return act.at >= offset_ms; });
		if (startIt != script.Actions().begin()) {
		    startIt -= 1;
		}

		auto endIt = std::find_if(startIt, script.Actions().end(),
		    [&](auto& act) { return act.at >= offset_ms + visibleSizeMs; });
		if (endIt != script.Actions().end()) {
		    endIt += 1;
		}
		drawingCtx.actionFromIdx = std::distance(script.Actions().begin(), startIt);
		drawingCtx.actionToIdx = std::distance(script.Actions().begin(), endIt);
		drawingCtx.script = scriptPtr.get();

		// draws mode specific things in the timeline
		// by default it draws the frame and time dividers
		// DrawAudioWaveform called in scripting mode to control the draw order. spaghetti
		overlay->DrawScriptPositionContent(drawingCtx);

		// border
		constexpr float borderThicknes = 1.f;
		draw_list->AddRect(
			drawingCtx.canvas_pos,
			ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
			script.HasSelection() ? ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_SliderGrabActive]) : IM_COL32(255, 255, 255, 255),
			0.f, ImDrawCornerFlags_All,
			borderThicknes
		);

		
		// TODO: reimplement this as an overlay ???

		// render recordings
		const float frameTime = player->getFrameTimeMs();
		const FunscriptAction* prevAction = nullptr;
		auto& recording = RecordingBuffer;

		auto pathStroke = [](auto draw_list, uint32_t col) {
			// sort of a hack ...
			// PathStroke sets  _Path.Size = 0
			// so we reset it in order to draw the same path twice
			auto tmp = draw_list->_Path.Size;
			draw_list->PathStroke(IM_COL32(0, 0, 0, 255), false, 7.0f);
			draw_list->_Path.Size = tmp;
			draw_list->PathStroke(col, false, 5.f);
		};
		auto pathRawSection = [this, &drawingCtx](auto draw_list, auto rawActions, int32_t fromIndex, int32_t toIndex) {
			for (int i = fromIndex; i < toIndex; i++) {
				auto action = rawActions[i];
				if (action.at >= 0) {
					auto point = getPointForAction(drawingCtx.canvas_pos, drawingCtx.canvas_size, action);
					draw_list->PathLineTo(point);
				}
			}
		};
		int32_t startIndex = Util::Clamp<int32_t>((offset_ms / frameTime), 0, recording.size());
		int32_t endIndex = Util::Clamp<int32_t>(((float)offset_ms + visibleSizeMs) / frameTime, startIndex, recording.size());

		pathRawSection(draw_list, recording, startIndex, endIndex);
		pathStroke(draw_list, IM_COL32(0, 255, 0, 180));


		// current position indicator -> |
		draw_list->AddLine(
			drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x / 2.f, 0),
			drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x / 2.f, drawingCtx.canvas_size.y),
			IM_COL32(255, 255, 255, 255), 3.0f);

		// selection box
		constexpr auto selectColor = IM_COL32(3, 252, 207, 255);
		constexpr auto selectColorBackground = IM_COL32(3, 252, 207, 100);
		if (IsSelecting && (scriptPtr.get() == activeScript)) {
			draw_list->AddRectFilled(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * rel_x1, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * rel_x2, drawingCtx.canvas_size.y), selectColorBackground);
			draw_list->AddLine(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * rel_x1, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * rel_x1, drawingCtx.canvas_size.y), selectColor, 3.0f);
			draw_list->AddLine(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * rel_x2, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * rel_x2, drawingCtx.canvas_size.y), selectColor, 3.0f);
		}

		// TODO: refactor this
		// selectionStart currently used for controller select
		if (startSelectionMs >= 0) {
			float startSelectRel = (startSelectionMs - offset_ms) / visibleSizeMs;
			draw_list->AddLine(
				drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * startSelectRel, 0),
				drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * startSelectRel, drawingCtx.canvas_size.y),
				selectColor, 3.0f
			);
		}
		ImVec2 newCursor(drawingCtx.canvas_pos.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y + style.ItemSpacing.y);
		if (newCursor.y < (startCursor.y + availSize.y)) { ImGui::SetCursorScreenPos(newCursor); }


		// right click context menu
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::BeginMenu("Scripts")) {
				for (auto&& script : scripts) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, drawingCtx.drawnScriptCount == 1 && script->Enabled);
					if (ImGui::Checkbox(script->metadata.title.c_str(), &script->Enabled) && !script->Enabled) {
						if (script.get() == activeScript) {
							// find a enabled script which can be set active
							for (int i = 0; i < scripts.size(); i++) {
								if (scripts[i]->Enabled) {
									//app->UpdateNewActiveScript(i);
									FUN_ASSERT(false, "fukc");
									break;
								}
							}
						}
					}
					ImGui::PopItemFlag();
				}
				ImGui::EndMenu();
			}
#ifndef NDEBUG
			if (ImGui::BeginMenu("Rendering")) {
				ImGui::EndMenu();
			}
#endif
			if (ImGui::BeginMenu("Audio waveform")) {
				ImGui::SliderFloat("Waveform scale", &ScaleAudio, 0.25f, 10.f);
				if (ImGui::MenuItem("Enable waveform", NULL, &ShowAudioWaveform, !ffmpegInProgress)) {}
				if (ImGui::MenuItem(ffmpegInProgress ? "Processing audio..." : "Update waveform", NULL, false, !ffmpegInProgress)) {
					if (!ffmpegInProgress) {
						ShowAudioWaveform = false; // gets switched true after processing
						ffmpegInProgress = true;

						auto ffmpegThread = [](void* userData) -> int {
							auto& ctx = *((ScriptTimeline*)userData);

							std::error_code ec;
							auto base_path = Util::Basepath();
	#if WIN32
							auto ffmpeg_path = base_path / "ffmpeg.exe";
	#else
							auto ffmpeg_path = std::filesystem::path("ffmpeg");
	#endif
							auto output_path = Util::Prefpath("tmp");
							if (!Util::CreateDirectories(output_path)) {
								return 0;
							}
							output_path = (std::filesystem::path(output_path) / "audio.mp3").string();
							auto video_path = ctx.player->getVideoPath();

							bool succ = OutputAudioFile(ffmpeg_path.string().c_str(), video_path, output_path.c_str());
							if (!succ) {
								LOGF_ERROR("Failed to output mp3 from video. (ffmpeg_path: \"%s\")", ffmpeg_path.string().c_str());
								ctx.ShowAudioWaveform = false;
								ctx.ffmpegInProgress = false;
								return 0;
							}

							mp3dec_t mp3d;
							mp3dec_file_info_t info;

							if (mp3dec_load(&mp3d, output_path.c_str(), &info, NULL, NULL))
							{
								LOGF_ERROR("failed to load \"%s\"", output_path.c_str());
								ctx.ShowAudioWaveform = false;
								ctx.ffmpegInProgress = false;
								return 0;
							}

							const int samples_per_line = info.hz / 1024.f; // controls the resolution

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
								for (int i = offset; i < offset + sample_count; i++)
								{
									float sample = info.buffer[i];
									float abs_sample = std::abs(sample);
									average += abs_sample;
								}
								average /= sample_count;
								ctx.audio_waveform_avg.push_back(average);

								//float peak(0);
								//for (int i = offset; i < offset + sample_count; i++)
								//{
								//	float sample = info.buffer[i];
								//	peak = std::max(peak, sample);
								//}
								//ctx.audio_waveform_avg.push_back(peak);
							}

							auto map2range = [](float x, float in_min, float in_max, float out_min, float out_max)
							{
								return Util::Clamp<float>(
									out_min + (out_max - out_min) * (x - in_min) / (in_max - in_min),
									out_min,
									out_max
									);
							};

							ctx.audio_waveform_avg.shrink_to_fit();
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
							ev.type = ScriptTimelineEvents::FfmpegAudioProcessingFinished;
							SDL_PushEvent(&ev);
							return 0;
						};
						auto handle = SDL_CreateThread(ffmpegThread, "OpenFunscripterFfmpegThread", this);
						SDL_DetachThread(handle);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
	}

	// draw points on top of lines
	for (auto&& p : overlay->ActionScreenCoordinates) {
		draw_list->AddCircleFilled(p, 7.0, IM_COL32(0, 0, 0, 255), 8); // border
		draw_list->AddCircleFilled(p, 5.0, IM_COL32(255, 0, 0, 255), 8);
	}

	// draw selected points
	for (auto&& p : overlay->SelectedActionScreenCoordinates) {
		constexpr auto selectedDots = IM_COL32(11, 252, 3, 255);
		draw_list->AddCircleFilled(p, 5.0, selectedDots, 8);
	}

	ImGui::End();
}

void ScriptTimeline::DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept
{
	auto& canvas_pos = ctx.canvas_pos;
	auto& canvas_size = ctx.canvas_size;
	const auto draw_list = ctx.draw_list;
	if (ShowAudioWaveform) {
		const float durationMs = ctx.totalDurationMs;
		const float rel_start = offset_ms / durationMs;
		const float rel_end = ((float)(offset_ms)+visibleSizeMs) / durationMs;

		auto& audio_waveform = audio_waveform_avg;

		int32_t start_index = rel_start * (float)audio_waveform.size();
		int32_t end_index = rel_end * (float)audio_waveform.size();
		const int total_samples = end_index - start_index;
		const int line_merge = 1 + (total_samples / 2000);
		const int actual_total_samples = total_samples / line_merge;
		//LOGF_INFO("total_samples=%d actual_total_samples=%d", total_samples, actual_total_samples);

		const float line_width = ((1.f / ((float)actual_total_samples)) * canvas_size.x) + 0.75f;
		start_index -= start_index % line_merge;
		end_index -= end_index % line_merge;
		end_index += line_merge;

		for (int i = start_index; i < end_index - (line_merge + line_merge); i += line_merge) {
			const float total_pos_x = ((((float)i - start_index) / (float)total_samples)) * canvas_size.x;
			float total_len;
			if (i < 0 || (i + line_merge) >= audio_waveform.size()) {
				total_len = 0.f;
				continue;
			}
			else {
				float sample = audio_waveform[i];
				for (int x = 1; x < line_merge; x++) {
					// find peak
					sample = std::max(sample, audio_waveform[i + x]);
				}
				total_len = canvas_size.y * sample * ScaleAudio;
				if (total_len < 2.f) {
					continue;
				}
			}
			draw_list->AddLine(
				canvas_pos + ImVec2(total_pos_x, (canvas_size.y / 2.f) + (total_len / 2.f)),
				canvas_pos + ImVec2(total_pos_x, (canvas_size.y / 2.f) - (total_len / 2.f)),
				IM_COL32(227, 66, 52, 255), line_width);
		}
	}
}

bool OutputAudioFile(const char* ffmpeg_path, const char* video_path, const char* output_path) {
	char buffer[1024];
#if WIN32
	constexpr const char* fmt = "\"\"%s\" -i \"%s\" -b:a 320k -ac 1 -y \"%s\"\"";
#else
	constexpr const char* fmt = "\"%s\" -i \"%s\" -b:a 320k -ac 1 -y \"%s\"";
#endif
	int num = stbsp_snprintf(buffer, sizeof(buffer), fmt,
		ffmpeg_path,
		video_path,
		output_path);

	FUN_ASSERT(num <= sizeof(buffer), "buffer to small");

	if (num >= sizeof(buffer)) {
		return false;
	}

#if WIN32
	auto wide = Util::Utf8ToUtf16(buffer);
	bool success = _wsystem(wide.c_str()) == 0;
#else
	bool success = std::system(buffer) == 0;
#endif

	return success;
}