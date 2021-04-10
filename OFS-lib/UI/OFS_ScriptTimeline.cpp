#include "OFS_ScriptTimeline.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "OFS_Profiling.h"

#include "OFS_Videoplayer.h"

#include "SDL.h"
#include "stb_sprintf.h"

#include <memory>
#include <array>

#include "OFS_Shader.h"
#include "glad/glad.h"

#include "KeybindingSystem.h"

int32_t ScriptTimelineEvents::FfmpegAudioProcessingFinished = 0;
int32_t ScriptTimelineEvents::ScriptpositionWindowDoubleClick = 0;
int32_t ScriptTimelineEvents::FunscriptActionClicked = 0;
int32_t ScriptTimelineEvents::FunscriptSelectTime = 0;
int32_t ScriptTimelineEvents::ActiveScriptChanged = 0;

void ScriptTimelineEvents::RegisterEvents() noexcept
{
	FfmpegAudioProcessingFinished = SDL_RegisterEvents(1);
	FunscriptActionClicked = SDL_RegisterEvents(1);
	FunscriptSelectTime = SDL_RegisterEvents(1);
	ScriptpositionWindowDoubleClick = SDL_RegisterEvents(1);
	ActiveScriptChanged = SDL_RegisterEvents(1);
}

void ScriptTimeline::updateSelection(bool clear) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	float min = std::min(relX1, relX2);
	float max = std::max(relX1, relX2);
	
	SelectTimeEventData.start_ms = offsetMs + (visibleSizeMs * min);
	SelectTimeEventData.end_ms = offsetMs + (visibleSizeMs * max);
	SelectTimeEventData.clear = clear;

	EventSystem::PushEvent(ScriptTimelineEvents::FunscriptSelectTime, &SelectTimeEventData);
}

void ScriptTimeline::FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept
{
	ShowAudioWaveform = true;
	LOG_INFO("Audio processing complete.");
}

void ScriptTimeline::setup(UndoSystem* undoSystem)
{
	this->undoSystem = undoSystem;
	EventSystem::ev().Subscribe(SDL_MOUSEBUTTONDOWN, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mousePressed));
	EventSystem::ev().Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouseScroll));
	EventSystem::ev().Subscribe(SDL_MOUSEMOTION, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouseDrag));
	EventSystem::ev().Subscribe(SDL_MOUSEBUTTONUP, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouseReleased));
	EventSystem::ev().Subscribe(ScriptTimelineEvents::FfmpegAudioProcessingFinished, EVENT_SYSTEM_BIND(this, &ScriptTimeline::FfmpegAudioProcessingFinished));
	EventSystem::ev().Subscribe(VideoEvents::MpvVideoLoaded, EVENT_SYSTEM_BIND(this, &ScriptTimeline::videoLoaded));

	glGenTextures(1, &WaveformTex);
	glBindTexture(GL_TEXTURE_1D, WaveformTex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	WaveShader = std::make_unique<WaveformShader>();
}

void ScriptTimeline::mousePressed(SDL_Event& ev) noexcept
{
	if (Scripts == nullptr || (*Scripts).size() <= activeScriptIdx) return;
	OFS_PROFILE(__FUNCTION__);

	auto& button = ev.button;
	auto mousePos = ImGui::GetMousePos();

	FunscriptAction* clickedAction = nullptr;

	if (PositionsItemHovered) {
		if (button.button == SDL_BUTTON_LEFT && button.clicks == 2) {
			// seek to position double click
			float relX = (mousePos.x - activeCanvasPos.x) / activeCanvasSize.x;
			int32_t seekToMs = offsetMs + (visibleSizeMs * relX);
			EventSystem::PushEvent(ScriptTimelineEvents::ScriptpositionWindowDoubleClick, (void*)(intptr_t)seekToMs);
			return;
		}
		else if (button.button == SDL_BUTTON_LEFT && button.clicks == 1)
		{
			// test if an action has been clicked
			int index = 0;
			for (auto& vert : overlay->ActionScreenCoordinates) {
				const ImVec2 size(20, 20);
				ImRect rect(vert - size, vert + size);
				if (rect.Contains(mousePos)) {
					clickedAction = &overlay->ActionPositionWindow[index];
					break;
				}
				index++;
			}

			if (hovereScriptIdx != activeScriptIdx) {
				EventSystem::PushEvent(ScriptTimelineEvents::ActiveScriptChanged, (void*)(intptr_t)hovereScriptIdx);
				activeScriptIdx = hovereScriptIdx;
				activeCanvasPos = hoveredCanvasPos;
				activeCanvasSize = hoveredCanvasSize;
			}
		}
	}
	
	if (undoSystem == nullptr) return;
	auto& activeScript = (*Scripts)[activeScriptIdx];
	bool moveOrAddPointModifer = KeybindingSystem::PassiveModifier("move_or_add_point_modifier");

	if (button.button == SDL_BUTTON_LEFT) {
		if (moveOrAddPointModifer && PositionsItemHovered) {
			if (clickedAction != nullptr) {
				// start move
				activeScript->ClearSelection();
				activeScript->SetSelected(*clickedAction, true);
				IsMoving = true;
				undoSystem->Snapshot(StateType::MOUSE_MOVE_ACTION, activeScript);
				return;
			}
			else {
				// click a point into existence
				auto action = getActionForPoint(activeCanvasPos, activeCanvasSize, mousePos, frameTimeMs);
				auto edit = activeScript->GetActionAtTime(action.at, frameTimeMs);
				undoSystem->Snapshot(StateType::ADD_ACTION, activeScript);
				if (edit != nullptr) { activeScript->RemoveAction(*edit); }
				activeScript->AddAction(action);
			}
		}
		// clicking an action fires an event
		else if (PositionsItemHovered && clickedAction != nullptr) {
			ActionClickEventData = std::make_tuple(ev, *clickedAction);
			EventSystem::PushEvent(ScriptTimelineEvents::FunscriptActionClicked, &ActionClickEventData);
		}
		// selecting only works in the active timeline
		else if (PositionsItemHovered) {
			ImRect rect(activeCanvasPos, activeCanvasPos + activeCanvasSize);
			if (rect.Contains(ImGui::GetMousePos())) {
				// start drag selection
				IsSelecting = true;
				relX1 = (mousePos.x - activeCanvasPos.x) / rect.GetWidth();
				relX2 = relX1;
			}
		}
	}
	else if (button.button == SDL_BUTTON_MIDDLE) {
		activeScript->ClearSelection();
	}
}

void ScriptTimeline::mouseReleased(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
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

void ScriptTimeline::mouseDrag(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (Scripts == nullptr || (*Scripts).size() <= activeScriptIdx) return;

	auto& motion = ev.motion;
	auto& activeScript = (*Scripts)[activeScriptIdx];

	if (IsSelecting) {
		relX2 = (ImGui::GetMousePos().x - activeCanvasPos.x) / activeCanvasSize.x;
	}
	else if (IsMoving) {
		if (!activeScript->HasSelection()) { IsMoving = false; return; }
		auto mousePos = ImGui::GetMousePos();
		auto& toBeMoved = activeScript->Selection()[0];
		auto newAction = getActionForPoint(activeCanvasPos, activeCanvasSize, mousePos, frameTimeMs);
		if (newAction.at != toBeMoved.at || newAction.pos != toBeMoved.pos) {
			const FunscriptAction* nearbyAction = nullptr;
			if ((newAction.at - toBeMoved.at) > 0) {
				nearbyAction = activeScript->GetNextActionAhead(toBeMoved.at);
				if (nearbyAction != nullptr) {
					if (std::abs(nearbyAction->at - newAction.at) < std::round(frameTimeMs)) {
						return;
					}
				}
			}
			else if((newAction.at - toBeMoved.at) < 0) {
				nearbyAction = activeScript->GetPreviousActionBehind(toBeMoved.at);
				if (nearbyAction != nullptr) {
					if (std::abs(nearbyAction->at - newAction.at) < std::round(frameTimeMs)) {
						return;
					}
 				}
			}

			activeScript->RemoveAction(toBeMoved);
			activeScript->ClearSelection();
			activeScript->AddAction(newAction);
			activeScript->SetSelected(newAction, true);
		}
	}
}

void ScriptTimeline::mouseScroll(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto& wheel = ev.wheel;
	constexpr float scrollPercent = 0.10f;
	if (PositionsItemHovered) {
		WindowSizeSeconds *= 1 + (scrollPercent * -wheel.y);
		WindowSizeSeconds = Util::Clamp(WindowSizeSeconds, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE);
	}
}

void ScriptTimeline::videoLoaded(SDL_Event& ev) noexcept
{
	videoPath = (const char*)ev.user.data1;
}

void ScriptTimeline::ShowScriptPositions(bool* open, float currentPositionMs, float durationMs, float frameTimeMs, const std::vector<std::shared_ptr<Funscript>>* scripts, int activeScriptIdx) noexcept
{
	if (open != nullptr && !*open) return;
	OFS_PROFILE(__FUNCTION__);

	FUN_ASSERT(scripts, "scripts is null");

	this->Scripts = scripts;
	this->activeScriptIdx = activeScriptIdx;
	this->frameTimeMs = frameTimeMs;

	const auto activeScript = (*Scripts)[activeScriptIdx].get();

	auto& style = ImGui::GetStyle();
	visibleSizeMs = WindowSizeSeconds * 1000.0;
	offsetMs = currentPositionMs - (visibleSizeMs / 2.0);
	
	OverlayDrawingCtx drawingCtx;
	drawingCtx.offset_ms = offsetMs;
	drawingCtx.visibleSizeMs = visibleSizeMs;
	drawingCtx.totalDurationMs = durationMs;
	if (drawingCtx.totalDurationMs == 0.f) return;
	
	ImGui::Begin(PositionsId, open, ImGuiWindowFlags_None /*ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse*/);
	auto draw_list = ImGui::GetWindowDrawList();
	drawingCtx.draw_list = draw_list;
	PositionsItemHovered = ImGui::IsWindowHovered();

	drawingCtx.drawnScriptCount = 0;
	for (auto&& script : *Scripts) {
		if (script->Enabled) { drawingCtx.drawnScriptCount++; }
	}

	const float verticalSpacingBetweenScripts = style.ItemSpacing.y*2.f;
	const auto availSize = ImGui::GetContentRegionAvail() - ImVec2(0.f , verticalSpacingBetweenScripts*((float)drawingCtx.drawnScriptCount-1));
	const auto startCursor = ImGui::GetCursorScreenPos();

	ImGui::SetCursorScreenPos(startCursor);
	for(int i=0; i < (*Scripts).size(); i++) {
		auto& scriptPtr = (*Scripts)[i];

		auto& script = *scriptPtr.get();
		if (!script.Enabled) { continue; }
		
		drawingCtx.scriptIdx = i;
		drawingCtx.canvas_pos = ImGui::GetCursorScreenPos();
		drawingCtx.canvas_size = ImVec2(availSize.x, (availSize.y - 1.f) / (float)drawingCtx.drawnScriptCount);
		const ImGuiID itemID = ImGui::GetID(script.Title.c_str());
		ImRect itemBB(drawingCtx.canvas_pos, drawingCtx.canvas_pos + drawingCtx.canvas_size);
		ImGui::ItemSize(itemBB);
		if (!ImGui::ItemAdd(itemBB, itemID)) {
			continue;
		}

		bool ItemIsHovered = ImGui::IsItemHovered();
		if (ItemIsHovered) {
			hovereScriptIdx = i;
			hoveredCanvasPos = drawingCtx.canvas_pos;
			hoveredCanvasSize = drawingCtx.canvas_size;
		}

		const bool IsActivated = scriptPtr.get() == activeScript && drawingCtx.drawnScriptCount > 1;
		if (drawingCtx.drawnScriptCount == 1) {
			activeCanvasPos = drawingCtx.canvas_pos;
			activeCanvasSize = drawingCtx.canvas_size;
		} else if (IsActivated) {
			activeCanvasPos = drawingCtx.canvas_pos;
			activeCanvasSize = drawingCtx.canvas_size;
		}

		if (IsActivated) {
			draw_list->AddRectFilledMultiColor(drawingCtx.canvas_pos, ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
				IM_COL32(1.2f*50, 0, 1.2f*50, 255), IM_COL32(1.2f*50, 0, 1.2f*50, 255),
				IM_COL32(1.2f*20, 0, 1.2f*20, 255), IM_COL32(1.2f*20, 0, 1.2f*20, 255)
			);
		}
		else {
			draw_list->AddRectFilledMultiColor(drawingCtx.canvas_pos, ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
				IM_COL32(0, 0, 50, 255), IM_COL32(0, 0, 50, 255),
				IM_COL32(0, 0, 20, 255), IM_COL32(0, 0, 20, 255)
			);
		}

		if (ItemIsHovered) {
			draw_list->AddRectFilled(
				drawingCtx.canvas_pos, 
				ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
				IM_COL32(255, 255, 255, 10)
			);
		}

		auto startIt = std::find_if(script.Actions().begin(), script.Actions().end(),
		    [this](auto act) { return act.at >= offsetMs; });
		if (startIt != script.Actions().begin()) {
		    startIt -= 1;
		}

		auto endIt = std::find_if(startIt, script.Actions().end(),
		    [this](auto act) { return act.at >= offsetMs + visibleSizeMs; });
		if (endIt != script.Actions().end()) {
		    endIt += 1;
		}
		drawingCtx.actionFromIdx = std::distance(script.Actions().begin(), startIt);
		drawingCtx.actionToIdx = std::distance(script.Actions().begin(), endIt);
		drawingCtx.script = scriptPtr.get();

		// draws mode specific things in the timeline
		// by default it draws the frame and time dividers
		// DrawAudioWaveform called in scripting mode to control the draw order. spaghetti
		{
			OFS_PROFILE("overlay->DrawScriptPositionContent(drawingCtx)");
			overlay->DrawScriptPositionContent(drawingCtx);
		}

		// border
		constexpr float borderThicknes = 1.f;
		uint32_t borderColor = IsActivated ? IM_COL32(0, 180, 0, 255) : IM_COL32(255, 255, 255, 255);
		if (script.HasSelection()) { borderColor = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_SliderGrabActive]); }
		draw_list->AddRect(
			drawingCtx.canvas_pos,
			ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
			borderColor,
			0.f, ImDrawFlags_None,
			borderThicknes
		);

		
		// TODO: reimplement this as an overlay ???

		// render recordings
		const FunscriptAction* prevAction = nullptr;
		auto& recording = RecordingBuffer;

		auto pathStroke = [](auto draw_list, uint32_t col) noexcept {
			OFS_PROFILE(__FUNCTION__);
			// sort of a hack ...
			// PathStroke sets  _Path.Size = 0
			// so we reset it in order to draw the same path twice
			// auto tmp = draw_list->_Path.Size;
			// draw_list->PathStroke(IM_COL32(0, 0, 0, 255), false, 7.0f);
			// draw_list->_Path.Size = tmp;
			draw_list->PathStroke(col, false, 5.f);
		};
		auto pathRawSection =
			[pathStroke](const OverlayDrawingCtx& ctx, const auto& rawActions, int32_t fromIndex, int32_t toIndex) noexcept {
			OFS_PROFILE(__FUNCTION__);
			for (int i = fromIndex; i <= toIndex; i++) {
				auto& action = rawActions[i].first;
				if (action.at >= 0) {
					auto point = getPointForAction(ctx, action);
					ctx.draw_list->PathLineTo(point);
				}
			}
			pathStroke(ctx.draw_list, IM_COL32(0, 255, 0, 180));

			for (int i = fromIndex; i <= toIndex; i++) {
				auto& action = rawActions[i].second;
				if (action.at >= 0) {
					auto point = getPointForAction(ctx, action);
					ctx.draw_list->PathLineTo(point);
				}
			}

			pathStroke(ctx.draw_list, IM_COL32(255, 255, 0, 180));
		};

		if (scriptPtr.get() == activeScript && recording.size() > 0) {
			int32_t startIndex = Util::Clamp<int32_t>((offsetMs / frameTimeMs), 0, recording.size() - 1);
			int32_t endIndex = Util::Clamp<int32_t>((offsetMs + visibleSizeMs) / frameTimeMs, startIndex, recording.size() - 1);
			pathRawSection(drawingCtx, recording, startIndex, endIndex);
		}


		// current position indicator -> |
		draw_list->AddLine(
			drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x / 2.f, 0),
			drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x / 2.f, drawingCtx.canvas_size.y),
			IM_COL32(255, 255, 255, 255), 3.0f);

		// selection box
		constexpr auto selectColor = IM_COL32(3, 252, 207, 255);
		constexpr auto selectColorBackground = IM_COL32(3, 252, 207, 100);
		if (IsSelecting && (scriptPtr.get() == activeScript)) {
			draw_list->AddRectFilled(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relX1, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relX2, drawingCtx.canvas_size.y), selectColorBackground);
			draw_list->AddLine(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relX1, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relX1, drawingCtx.canvas_size.y), selectColor, 3.0f);
			draw_list->AddLine(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relX2, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relX2, drawingCtx.canvas_size.y), selectColor, 3.0f);
		}

		// TODO: refactor this
		// selectionStart currently used for controller select
		if (startSelectionMs >= 0) {
			float startSelectRel = (startSelectionMs - offsetMs) / visibleSizeMs;
			draw_list->AddLine(
				drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * startSelectRel, 0),
				drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * startSelectRel, drawingCtx.canvas_size.y),
				selectColor, 3.0f
			);
		}
		ImVec2 newCursor(drawingCtx.canvas_pos.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y + verticalSpacingBetweenScripts);
		if (newCursor.y < (startCursor.y + availSize.y)) { ImGui::SetCursorScreenPos(newCursor); }


		// right click context menu
		if (ImGui::BeginPopupContextItem(script.Title.c_str()))
		{
			if (ImGui::BeginMenu("Scripts")) {
				for (auto&& script : *Scripts) {
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, drawingCtx.drawnScriptCount == 1 && script->Enabled);
					if (ImGui::Checkbox(script->Title.c_str(), &script->Enabled) && !script->Enabled) {
						if (script.get() == activeScript) {
							// find a enabled script which can be set active
							for (int i = 0; i < (*Scripts).size(); i++) {
								if ((*Scripts)[i]->Enabled) {									
									EventSystem::PushEvent(ScriptTimelineEvents::ActiveScriptChanged, (void*)(intptr_t)i);
									break;
								}
							}
						}
					}
					ImGui::PopItemFlag();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Rendering")) {
				ImGui::MenuItem("Show actions", 0, &BaseOverlay::ShowActions);
				ImGui::MenuItem("Spline mode", 0, &BaseOverlay::SplineMode);
				ImGui::EndMenu();
			}

			auto updateAudioWaveformThread = [](void* userData) -> int {
				auto& ctx = *((ScriptTimeline*)userData);
				std::error_code ec;
				auto basePath = Util::Basepath();
#if WIN32
				auto ffmpegPath = basePath / "ffmpeg.exe";
#else
				auto ffmpegPath = Util::PathFromString("ffmpeg");
#endif
				auto outputPath = Util::Prefpath("tmp");
				if (!Util::CreateDirectories(outputPath)) {
					return 0;
				}
				outputPath = (Util::PathFromString(outputPath) / "audio.mp3").u8string();

				bool succ = ctx.waveform.GenerateMP3(ffmpegPath.u8string(), std::string(ctx.videoPath), outputPath);
				if (!succ) { LOGF_ERROR("Failed to output mp3 from video. (ffmpeg_path: \"%s\")", ffmpegPath.u8string().c_str()); return 0;	}
				succ = ctx.waveform.LoadMP3(outputPath);
				if (!succ) { LOGF_ERROR("Failed load mp3. (path: \"%s\")", outputPath.c_str());	return 0; }
				EventSystem::PushEvent(ScriptTimelineEvents::FfmpegAudioProcessingFinished);
				return 0;
			};
			if (ImGui::BeginMenu("Audio waveform")) {
				ImGui::DragFloat("Scale", &ScaleAudio, 0.01f, 0.01f, 10.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::ColorEdit3("Color", &WaveformColor.Value.x, ImGuiColorEditFlags_None);
				if (ImGui::MenuItem("Enable waveform", NULL, &ShowAudioWaveform, !waveform.BusyGenerating())) {}
				if (ImGui::MenuItem(waveform.BusyGenerating() 
					? "Processing audio..." 
					: "Update waveform", NULL, false, !waveform.BusyGenerating() && videoPath != nullptr)) {
					if (!waveform.BusyGenerating()) {
						ShowAudioWaveform = false; // gets switched true after processing
						auto handle = SDL_CreateThread(updateAudioWaveformThread, "OFS_GenWaveform", this);
						SDL_DetachThread(handle);
					}
				}
				if (ShowAudioWaveform) { if (ImGui::MenuItem("Enable P-Mode " ICON_WARNING_SIGN, 0, &WaveformPartyMode)) {} }
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


constexpr uint32_t HighRangeCol = IM_COL32(0xE3, 0x42, 0x34, 0xff);
constexpr uint32_t MidRangeCol = IM_COL32(0xE8, 0xD7, 0x5A, 0xff);
constexpr uint32_t LowRangeCol = IM_COL32(0xF7, 0x65, 0x38, 0xff); // IM_COL32(0xff, 0xba, 0x08, 0xff);

void ScriptTimeline::DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept
{
	OFS_PROFILE(__FUNCTION__);

#if 0
	if (!ShowAudioWaveform) {
		waveform.LoadMP3(Util::Prefpath("tmp/audio.mp3"));
		EventSystem::PushEvent(ScriptTimelineEvents::FfmpegAudioProcessingFinished);
	}
#endif
	auto& canvas_pos = ctx.canvas_pos;
	auto& canvas_size = ctx.canvas_size;
	const auto draw_list = ctx.draw_list;
	if (ShowAudioWaveform && waveform.SampleCount() > 0) {
		const float durationMs = ctx.totalDurationMs;
		const float rel_start = offsetMs / durationMs;
		const float rel_end = (offsetMs + visibleSizeMs) / durationMs;
		int32_t start_index = rel_start * (float)waveform.SampleCount();
		int32_t end_index = rel_end * (float)waveform.SampleCount();
		const int total_samples = end_index - start_index;

		WaveformViewport = ImGui::GetWindowViewport();
		auto renderWaveform = [](const std::vector<float>& samples, ScriptTimeline* timeline, const OverlayDrawingCtx& ctx, int start_index, int end_index)
		{
			OFS_PROFILE(__FUNCTION__);
			const int total_samples = end_index - start_index;

			const int line_merge = 1 + (total_samples / ctx.canvas_size.x);

			timeline->WaveformLineBuffer.clear();
			float sample = 0.f;
			for (int i = start_index; i <= end_index; i++) {
				if (i >= 0 && i < samples.size()) {
					float s = samples[i];
					sample = std::max(sample, s);
				}

				if (i % line_merge == 0) {
					timeline->WaveformLineBuffer.emplace_back(sample);
					sample = 0.f;
				}
			}
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D, timeline->WaveformTex);
			glTexImage1D(GL_TEXTURE_1D, 0, GL_RED, timeline->WaveformLineBuffer.size(), 0, GL_RED, GL_FLOAT, timeline->WaveformLineBuffer.data());

			ctx.draw_list->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) noexcept {
				ScriptTimeline* ctx = (ScriptTimeline*)cmd->UserCallbackData;
				
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_1D, ctx->WaveformTex);
				ctx->WaveShader->use();
				auto draw_data = ctx->WaveformViewport->DrawData;
				float L = draw_data->DisplayPos.x;
				float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
				float T = draw_data->DisplayPos.y;
				float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
				const float ortho_projection[4][4] =
				{
					{ 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
					{ 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
					{ 0.0f, 0.0f, -1.0f, 0.0f },
					{ (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
				};
				ctx->WaveShader->ProjMtx(&ortho_projection[0][0]);
				ctx->WaveShader->AudioData(1);
				ctx->WaveShader->ScaleFactor(ctx->ScaleAudio);
				ctx->WaveShader->Time((SDL_GetTicks() / 1000.f));
				ctx->WaveShader->PartyMode(ctx->WaveformPartyMode);
				ctx->WaveShader->Color(&ctx->WaveformColor.Value.x);
			}, timeline);
			ctx.draw_list->AddImage(0, ctx.canvas_pos, ctx.canvas_pos + ctx.canvas_size);
			ctx.draw_list->AddCallback(ImDrawCallback_ResetRenderState, 0);
			ctx.draw_list->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) noexcept { glActiveTexture(GL_TEXTURE0); }, 0);
		};

#if 0 // TODO: this has bad perf
		renderWaveform(waveform.SamplesHigh, HighRangeCol, waveform.MidMax);
		renderWaveform(waveform.SamplesMid, MidRangeCol, waveform.LowMax);
		renderWaveform(waveform.SamplesLow, LowRangeCol, 0.f);
#else
		renderWaveform(waveform.SamplesHigh, this, ctx, start_index, end_index);
#endif
	}
}