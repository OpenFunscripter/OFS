#include "OFS_ScriptTimeline.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "OFS_Profiling.h"
#include "OFS_UndoSystem.h"
#include "OFS_VideoplayerWindow.h"

#include "SDL.h"
#include "stb_sprintf.h"

#include <memory>
#include <array>
#include <cmath>

#include "OFS_ImGui.h"
#include "OFS_Shader.h"
#include "OFS_GL.h"

#include "KeybindingSystem.h"
#include "SDL_events.h"

int32_t ScriptTimelineEvents::FfmpegAudioProcessingFinished = 0;
int32_t ScriptTimelineEvents::SetTimePosition = 0;
int32_t ScriptTimelineEvents::FunscriptActionClicked = 0;
int32_t ScriptTimelineEvents::FunscriptSelectTime = 0;
int32_t ScriptTimelineEvents::ActiveScriptChanged = 0;

void ScriptTimelineEvents::RegisterEvents() noexcept
{
	FfmpegAudioProcessingFinished = SDL_RegisterEvents(1);
	FunscriptActionClicked = SDL_RegisterEvents(1);
	FunscriptSelectTime = SDL_RegisterEvents(1);
	SetTimePosition = SDL_RegisterEvents(1);
	ActiveScriptChanged = SDL_RegisterEvents(1);
}

void ScriptTimeline::updateSelection(ScriptTimelineEvents::Mode mode, bool clear) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	float relSel1 = (absSel1 - offsetTime) / visibleTime;
	float min = std::min(relSel1, relSel2);
	float max = std::max(relSel1, relSel2);

	SelectTimeEventData.startTime = offsetTime + (visibleTime * min);
	SelectTimeEventData.endTime = offsetTime + (visibleTime * max);
	SelectTimeEventData.clear = clear;
	SelectTimeEventData.mode = mode;

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
	EventSystem::ev().Subscribe(VideoEvents::VideoLoaded, EVENT_SYSTEM_BIND(this, &ScriptTimeline::videoLoaded));

	Wave.Init();
}

void ScriptTimeline::mousePressed(SDL_Event& ev) noexcept
{
	if (Scripts == nullptr || (*Scripts).size() <= activeScriptIdx) return;
	OFS_PROFILE(__FUNCTION__);

	auto& button = ev.button;
	auto mousePos = ImGui::GetMousePos();

	const FunscriptAction* clickedAction = nullptr;

	if (PositionsItemHovered) {
		if (button.button == SDL_BUTTON_MIDDLE && button.clicks == 2) {
			// seek to position double click
			float relX = (mousePos.x - activeCanvasPos.x) / activeCanvasSize.x;
			float seekToTime = offsetTime + (visibleTime * relX);
			EventSystem::PushEvent(ScriptTimelineEvents::SetTimePosition, (void*)(*(intptr_t*)&seekToTime));
			return;
		}
		else if (button.button == SDL_BUTTON_LEFT && button.clicks == 1)	{
			// test if an action has been clicked
			if (overlay->PointSize > 0.f) {
				int index = 0;
				for (auto& vert : overlay->ActionScreenCoordinates) {
					const ImVec2 size(overlay->PointSize, overlay->PointSize);
					ImRect rect(vert - size, vert + size);
					if (rect.Contains(mousePos)) {
						clickedAction = &overlay->ActionPositionWindow[index];
						break;
					}
					index++;
				}
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

	if (button.button == SDL_BUTTON_LEFT) {
		bool moveOrAddPointModifer = KeybindingSystem::PassiveModifier("move_or_add_point_modifier");
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
				auto action = getActionForPoint(activeCanvasPos, activeCanvasSize, mousePos, frameTime);
				auto edit = activeScript->GetActionAtTime(action.atS, frameTime);
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
				float relSel1 = (mousePos.x - activeCanvasPos.x) / rect.GetWidth();
				relSel2 = relSel1;
				absSel1 = offsetTime + (visibleTime * relSel1);
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
		bool clearSelection = !(SDL_GetModState() & KMOD_CTRL);
		auto mode = ScriptTimelineEvents::Mode::All;
		if (KeybindingSystem::PassiveModifier("select_top_points_modifier")) {
			mode = ScriptTimelineEvents::Mode::Top;
		}
		else if (KeybindingSystem::PassiveModifier("select_bottom_points_modifier")) {
			mode = ScriptTimelineEvents::Mode::Bottom;
		}
		else if (KeybindingSystem::PassiveModifier("select_middle_points_modifier")) {
			mode = ScriptTimelineEvents::Mode::Middle;
		}
		updateSelection(mode, clearSelection);
	}
}

void ScriptTimeline::mouseDrag(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (Scripts == nullptr || (*Scripts).size() <= activeScriptIdx) return;

	auto& motion = ev.motion;
	auto& activeScript = (*Scripts)[activeScriptIdx];

	if (IsSelecting) {
		relSel2 = (ImGui::GetMousePos().x - activeCanvasPos.x) / activeCanvasSize.x;
		relSel2 = Util::Clamp(relSel2, 0.f, 1.f);
	}
	else if (IsMoving) {
		if (!activeScript->HasSelection()) { IsMoving = false; return; }
		auto mousePos = ImGui::GetMousePos();
		auto& toBeMoved = activeScript->Selection()[0];
		auto newAction = getActionForPoint(activeCanvasPos, activeCanvasSize, mousePos, frameTime);
		if (newAction.atS != toBeMoved.atS || newAction.pos != toBeMoved.pos) {
			const FunscriptAction* nearbyAction = nullptr;
			if ((newAction.atS - toBeMoved.atS) > 0.f) {
				nearbyAction = activeScript->GetNextActionAhead(toBeMoved.atS);
				if (nearbyAction != nullptr) {
					if (std::abs(nearbyAction->atS - newAction.atS) < frameTime) {
						return;
					}
				}
			}
			else if((newAction.atS - toBeMoved.atS) < 0.f) {
				nearbyAction = activeScript->GetPreviousActionBehind(toBeMoved.atS);
				if (nearbyAction != nullptr) {
					if (std::abs(nearbyAction->atS - newAction.atS) < frameTime) {
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
	else if(PositionsItemHovered) {
		if(ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
			float relX = -motion.xrel / hoveredCanvasSize.x;
			float seekToTime = (offsetTime + (visibleTime / 2.f)) + (visibleTime * relX);
			EventSystem::PushEvent(ScriptTimelineEvents::SetTimePosition, (void*)(*(intptr_t*)&seekToTime));
			return;
		}
	}
}

void ScriptTimeline::mouseScroll(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto& wheel = ev.wheel;
	constexpr float scrollPercent = 0.10f;
	if (PositionsItemHovered) {
		previousVisibleTime = visibleTime;
		nextVisisbleTime *= 1 + (scrollPercent * -wheel.y);
		nextVisisbleTime = Util::Clamp(nextVisisbleTime, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE);
		visibleTimeUpdate = SDL_GetTicks();
	}
}

inline static float easeOutExpo(float x) noexcept {
	return x >= 1.f ? 1.f : 1.f - powf(2, -10 * x);
}

void ScriptTimeline::Update() noexcept
{
	auto timePassed = Util::Clamp((SDL_GetTicks() - visibleTimeUpdate) / 150.f, 0.f, 1.f);
	timePassed = easeOutExpo(timePassed);
	visibleTime = Util::Lerp(previousVisibleTime, nextVisisbleTime, timePassed);
}

void ScriptTimeline::videoLoaded(SDL_Event& ev) noexcept
{
	videoPath = (const char*)ev.user.data1;
}

void ScriptTimeline::handleSelectionScrolling() noexcept
{
	constexpr float seekBorderMargin = 0.03f;
	constexpr float scrollSpeed = 80.f;
	if(relSel2 < seekBorderMargin || relSel2 > (1.f - seekBorderMargin)) {
		float seekToTime = offsetTime + (visibleTime / 2.f);
		seekToTime = Util::Max(0.f, seekToTime);
		float relSeek = (relSel2 < seekBorderMargin) 
			? -(seekBorderMargin - relSel2) 
			: relSel2 - (1.f - seekBorderMargin);

		relSeek *= ImGui::GetIO().DeltaTime * scrollSpeed;

		float seek = visibleTime * relSeek; 
		seekToTime += seek;
		EventSystem::PushEvent(ScriptTimelineEvents::SetTimePosition, (void*)(*(intptr_t*)&seekToTime));
	}
}

void ScriptTimeline::ShowScriptPositions(bool* open, float currentTime, float duration, float frameTime, const std::vector<std::shared_ptr<Funscript>>* scripts, int activeScriptIdx) noexcept
{
	if (open != nullptr && !*open) return;
	OFS_PROFILE(__FUNCTION__);

	FUN_ASSERT(scripts, "scripts is null");

	this->Scripts = scripts;
	this->activeScriptIdx = activeScriptIdx;
	this->frameTime = frameTime;

	const auto activeScript = (*Scripts)[activeScriptIdx].get();

	auto& style = ImGui::GetStyle();
	offsetTime = currentTime - (visibleTime / 2.0);
	if(IsSelecting) handleSelectionScrolling();
	
	OverlayDrawingCtx drawingCtx;
	drawingCtx.offsetTime = offsetTime;
	drawingCtx.visibleTime = visibleTime;
	drawingCtx.totalDuration = duration;
	if (drawingCtx.totalDuration == 0.f) return;
	
	ImGui::Begin(TR_ID(WindowId, Tr::POSITIONS), open, ImGuiWindowFlags_None);
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
		drawingCtx.canvas_size = ImVec2(availSize.x, availSize.y / (float)drawingCtx.drawnScriptCount);
		const ImGuiID itemID = ImGui::GetID(script.Title.empty() ? "empty script" : script.Title.c_str());
		ImRect itemBB(drawingCtx.canvas_pos, drawingCtx.canvas_pos + drawingCtx.canvas_size);
		ImGui::ItemSize(itemBB);
		if (!ImGui::ItemAdd(itemBB, itemID)) {
			continue;
		}

		draw_list->PushClipRect(itemBB.Min - ImVec2(0.f, 1.f), itemBB.Max + ImVec2(0.f, 1.f));

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
			draw_list->AddRectFilledMultiColor(
				drawingCtx.canvas_pos, 
				ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
				IM_COL32(1.2f*50, 0, 1.2f*50, 255), IM_COL32(1.2f*50, 0, 1.2f*50, 255),
				IM_COL32(1.2f*20, 0, 1.2f*20, 255), IM_COL32(1.2f*20, 0, 1.2f*20, 255)
			);
		}
		else {
			draw_list->AddRectFilledMultiColor(drawingCtx.canvas_pos, 
				ImVec2(drawingCtx.canvas_pos.x + drawingCtx.canvas_size.x, drawingCtx.canvas_pos.y + drawingCtx.canvas_size.y),
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
		    [this](auto act) { return act.atS >= offsetTime; });
		if (startIt != script.Actions().begin()) {
		    startIt -= 1;
		}

		auto endIt = std::find_if(startIt, script.Actions().end(),
		    [this](auto act) { return act.atS >= offsetTime + visibleTime; });
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

		// current position indicator -> |
		draw_list->AddTriangleFilled(
			drawingCtx.canvas_pos + ImVec2((drawingCtx.canvas_size.x/2.f) - ImGui::GetFontSize(), 0.f),
			drawingCtx.canvas_pos + ImVec2((drawingCtx.canvas_size.x/2.f) + ImGui::GetFontSize(), 0.f),
			drawingCtx.canvas_pos + ImVec2((drawingCtx.canvas_size.x/2.f), ImGui::GetFontSize()/1.5f),
			IM_COL32(255, 255, 255, 255)
		);
		draw_list->AddLine(
			drawingCtx.canvas_pos + ImVec2((drawingCtx.canvas_size.x/2.f)-0.5f, 0),
			drawingCtx.canvas_pos + ImVec2((drawingCtx.canvas_size.x/2.f)-0.5f, drawingCtx.canvas_size.y-1.f),
			IM_COL32(255, 255, 255, 255),
		4.0f);

		// selection box
		constexpr auto selectColor = IM_COL32(3, 252, 207, 255);
		constexpr auto selectColorBackground = IM_COL32(3, 252, 207, 100);
		if (IsSelecting && (scriptPtr.get() == activeScript)) {
			float relSel1 = (absSel1 - offsetTime) / visibleTime;
			draw_list->AddRectFilled(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relSel1, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relSel2, drawingCtx.canvas_size.y), selectColorBackground);
			draw_list->AddLine(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relSel1, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relSel1, drawingCtx.canvas_size.y), selectColor, 3.0f);
			draw_list->AddLine(drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relSel2, 0), drawingCtx.canvas_pos + ImVec2(drawingCtx.canvas_size.x * relSel2, drawingCtx.canvas_size.y), selectColor, 3.0f);
		}

		// TODO: refactor this
		// selectionStart currently used for controller select
		if (startSelectionTime >= 0.f) {
			float startSelectRel = (startSelectionTime - offsetTime) / visibleTime;
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
			if (ImGui::BeginMenu(TR_ID("SCRIPTS", Tr::SCRIPTS))) {
				for (auto& script : *Scripts) {
					if(script->Title.empty()) {
						ImGui::TextDisabled(TR(NONE));
						continue;
					}
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
			if (ImGui::BeginMenu(TR_ID("RENDERING", Tr::RENDERING))) {
				ImGui::MenuItem(TR(SHOW_ACTIONS), 0, &BaseOverlay::ShowActions);
				ImGui::MenuItem(TR(SPLINE_MODE), 0, &BaseOverlay::SplineMode);
				ImGui::MenuItem(TR(SHOW_VIDEO_POSITION), 0, &BaseOverlay::SyncLineEnable);
				OFS::Tooltip(TR(SHOW_VIDEO_POSITION_TOOLTIP));
				ImGui::EndMenu();
			}

			auto updateAudioWaveformThread = [](void* userData) -> int {
				auto& ctx = *((ScriptTimeline*)userData);
				std::error_code ec;
				auto ffmpegPath = Util::FfmpegPath();
				auto outputPath = Util::Prefpath("tmp");
				if (!Util::CreateDirectories(outputPath)) {
					return 0;
				}
				
				outputPath = (Util::PathFromString(outputPath) / "audio.flac").u8string();
				bool succ = ctx.Wave.data.GenerateAndLoadFlac(ffmpegPath.u8string(), ctx.videoPath, outputPath);
				EventSystem::PushEvent(ScriptTimelineEvents::FfmpegAudioProcessingFinished);
				return 0;
			};
			if (ImGui::BeginMenu(TR_ID("WAVEFORM", Tr::WAVEFORM))) {
				if(ImGui::BeginMenu(TR_ID("SETTINGS", Tr::SETTINGS))) {
					ImGui::DragFloat(TR(SCALE), &ScaleAudio, 0.01f, 0.01f, 10.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					ImGui::ColorEdit3(TR(COLOR), &Wave.WaveformColor.Value.x, ImGuiColorEditFlags_None);
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem(TR(ENABLE_WAVEFORM), NULL, &ShowAudioWaveform, !Wave.data.BusyGenerating())) {}

				if(Wave.data.BusyGenerating()) {
					ImGui::MenuItem(TR(PROCESSING_AUDIO), NULL, false, false);
					ImGui::SameLine();
					OFS::Spinner("##AudioSpin", ImGui::GetFontSize() / 3.f, 4.f, ImGui::GetColorU32(ImGuiCol_TabActive));
				}
				else if(ImGui::MenuItem(TR(UPDATE_WAVEFORM), NULL, false, !Wave.data.BusyGenerating() && videoPath != nullptr)) {
					if (!Wave.data.BusyGenerating()) {
						ShowAudioWaveform = false; // gets switched true after processing
						auto handle = SDL_CreateThread(updateAudioWaveformThread, "OFS_GenWaveform", this);
						SDL_DetachThread(handle);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		draw_list->PopClipRect();
	}


	// draw points on top of lines
	float opacity = 20.f / visibleTime;
	opacity = opacity > 1.f ? 1.f : opacity * opacity;
	overlay->PointSize = 7.f * opacity;
	if (opacity >= 0.25f) {
		draw_list->PushClipRect(startCursor - ImVec2(0.f, 20.f),
			(startCursor + ImGui::GetWindowSize() - (style.FramePadding*2.f) - (style.ItemInnerSpacing * 2.f))
			+ ImVec2(0.f, 20.f), true);
		int opcacityInt = 255 * opacity;
		for (auto&& p : overlay->ActionScreenCoordinates) {
			draw_list->AddCircleFilled(p, overlay->PointSize, IM_COL32(0, 0, 0, opcacityInt), 8); // border
			draw_list->AddCircleFilled(p, overlay->PointSize*0.7f, IM_COL32(255, 0, 0, opcacityInt), 8);
		}

		// draw selected points
		for (auto&& p : overlay->SelectedActionScreenCoordinates) {
			const auto selectedDots = IM_COL32(11, 252, 3, opcacityInt);
			draw_list->AddCircleFilled(p, overlay->PointSize * 0.7f, selectedDots, 8);
		}
		draw_list->PopClipRect();
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
		Wave.data.LoadFlac(Util::Prefpath("tmp/audio.flac"));
		EventSystem::PushEvent(ScriptTimelineEvents::FfmpegAudioProcessingFinished);
	}
#endif
	auto& canvas_pos = ctx.canvas_pos;
	auto& canvas_size = ctx.canvas_size;
	const auto draw_list = ctx.draw_list;
	if (ShowAudioWaveform && Wave.data.SampleCount() > 0) {
		FUN_ASSERT(Wave.data.SampleCount() < 16777217, "switch to doubles"); 

		Wave.WaveformViewport = ImGui::GetWindowViewport();
		auto renderWaveform = [](ScriptTimeline* timeline, const OverlayDrawingCtx& ctx) noexcept
		{
			OFS_PROFILE("DrawAudioWaveform::renderWaveform");
			
			timeline->Wave.Update(ctx);
			
			ctx.draw_list->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) noexcept {
				ScriptTimeline* ctx = (ScriptTimeline*)cmd->UserCallbackData;
				
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, ctx->Wave.WaveformTex);
				glActiveTexture(GL_TEXTURE0);
				ctx->Wave.WaveShader->use();
				auto draw_data = ctx->Wave.WaveformViewport->DrawData;
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
				ctx->Wave.WaveShader->ProjMtx(&ortho_projection[0][0]);
				ctx->Wave.WaveShader->AudioData(1);
				ctx->Wave.WaveShader->SampleOffset(ctx->Wave.samplingOffset);
				ctx->Wave.WaveShader->ScaleFactor(ctx->ScaleAudio);
				ctx->Wave.WaveShader->Color(&ctx->Wave.WaveformColor.Value.x);
			}, timeline);

			ctx.draw_list->AddImage(0, ctx.canvas_pos, ctx.canvas_pos + ctx.canvas_size);
			ctx.draw_list->AddCallback(ImDrawCallback_ResetRenderState, 0);
			ctx.draw_list->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) noexcept { glActiveTexture(GL_TEXTURE0); }, 0);
		};

		renderWaveform(this, ctx);
	}
}