#include "OFS_ScriptTimeline.h"
#include "OFS_Profiling.h"
#include "OFS_VideoplayerEvents.h"

#include "stb_sprintf.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "OFS_ImGui.h"
#include "OFS_Shader.h"
#include "OFS_GL.h"

#include "state/states/BaseOverlayState.h"

#include "KeybindingSystem.h"
#include "SDL_events.h"
#include "SDL_timer.h"

int32_t ScriptTimelineEvents::FfmpegAudioProcessingFinished = 0;
int32_t ScriptTimelineEvents::SetTimePosition = 0;
int32_t ScriptTimelineEvents::FunscriptActionClicked = 0;
int32_t ScriptTimelineEvents::FunscriptSelectTime = 0;
int32_t ScriptTimelineEvents::ActiveScriptChanged = 0;
int32_t ScriptTimelineEvents::FunscriptActionMoved = 0;
int32_t ScriptTimelineEvents::FunscriptActionMoveStarted = 0;
int32_t ScriptTimelineEvents::FunscriptActionCreated = 0;

void ScriptTimelineEvents::RegisterEvents() noexcept
{
	FfmpegAudioProcessingFinished = SDL_RegisterEvents(1);
	FunscriptActionClicked = SDL_RegisterEvents(1);
	FunscriptSelectTime = SDL_RegisterEvents(1);
	SetTimePosition = SDL_RegisterEvents(1);
	ActiveScriptChanged = SDL_RegisterEvents(1);
	FunscriptActionMoved = SDL_RegisterEvents(1);
	FunscriptActionMoveStarted = SDL_RegisterEvents(1);
	FunscriptActionCreated = SDL_RegisterEvents(1);
}

void ScriptTimeline::updateSelection(bool clear) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	float relSel1 = (absSel1 - offsetTime) / visibleTime;
	float min = std::min(relSel1, relSel2);
	float max = std::max(relSel1, relSel2);

	SelectTimeEventData.startTime = offsetTime + (visibleTime * min);
	SelectTimeEventData.endTime = offsetTime + (visibleTime * max);
	SelectTimeEventData.clear = clear;

	EventSystem::PushEvent(ScriptTimelineEvents::FunscriptSelectTime, &SelectTimeEventData);
}

void ScriptTimeline::FfmpegAudioProcessingFinished(SDL_Event& ev) noexcept
{
	ShowAudioWaveform = true;
	LOG_INFO("Audio processing complete.");
}

void ScriptTimeline::Init()
{
	overlayStateHandle = BaseOverlayState::RegisterStatic();

	EventSystem::ev().Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &ScriptTimeline::mouseScroll));
	EventSystem::ev().Subscribe(ScriptTimelineEvents::FfmpegAudioProcessingFinished, EVENT_SYSTEM_BIND(this, &ScriptTimeline::FfmpegAudioProcessingFinished));
	EventSystem::ev().Subscribe(VideoEvents::VideoLoaded, EVENT_SYSTEM_BIND(this, &ScriptTimeline::videoLoaded));

	Wave.Init();
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
	videoPath = *(std::string*)ev.user.data1;
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

void ScriptTimeline::handleTimelineHover(const OverlayDrawingCtx& ctx) noexcept
{
	if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		auto mousePos = ImGui::GetMousePos();
		float relX = (mousePos.x - activeCanvasPos.x) / activeCanvasSize.x;
		float seekToTime = offsetTime + (visibleTime * relX);
		EventSystem::PushEvent(ScriptTimelineEvents::SetTimePosition, (void*)(*(intptr_t*)&seekToTime));
	}
	else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (hovereScriptIdx != activeScriptIdx) {
			EventSystem::PushEvent(ScriptTimelineEvents::ActiveScriptChanged, (void*)(intptr_t)hovereScriptIdx);
			activeScriptIdx = hovereScriptIdx;
			activeCanvasPos = hoveredCanvasPos;
			activeCanvasSize = hoveredCanvasSize;
		}
	}
	else if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
	{
		ctx.script->ClearSelection();
	}
}

void ScriptTimeline::handleActionClicks(const OverlayDrawingCtx& ctx) noexcept
{
	bool moveOrAddPointModifer = KeybindingSystem::PassiveModifier("move_or_add_point_modifier");
	auto mousePos = ImGui::GetMousePos();
	auto startIt = ctx.script->Actions().begin() + ctx.actionFromIdx;
	auto endIt = ctx.script->Actions().begin() + ctx.actionToIdx;

	bool pointWasClicked = false;
	for (; startIt != endIt; ++startIt) 
	{
		auto point = BaseOverlay::GetPointForAction(ctx, *startIt);
		const ImVec2 size(BaseOverlay::PointSize, BaseOverlay::PointSize);
		ImRect rect(point - size, point + size);
		bool pointClicked = rect.Contains(mousePos);
		pointWasClicked |= pointClicked;
		
		if (!moveOrAddPointModifer && pointClicked) {
			ActionClickEventData = *startIt;
			EventSystem::PushEvent(ScriptTimelineEvents::FunscriptActionClicked, &ActionClickEventData);
			break;
		}
		else if(moveOrAddPointModifer && !IsMoving && pointClicked)
		{
			// Start dragging action
			ctx.script->ClearSelection();
			ctx.script->SetSelected(*startIt, true);
			IsMoving = true;
			EventSystem::PushEvent(ScriptTimelineEvents::FunscriptActionMoveStarted, &ActionMovedEventData);
			break;
		}
	}

	if(moveOrAddPointModifer && !pointWasClicked)
	{
		auto newAction = getActionForPoint(ctx, mousePos);
		ActionCreatedEventData = newAction;
		EventSystem::PushEvent(ScriptTimelineEvents::FunscriptActionCreated, &ActionCreatedEventData);
	}
}

void ScriptTimeline::ShowScriptPositions(
	const OFS_Videoplayer* player,
	BaseOverlay* overlay,
	const std::vector<std::shared_ptr<Funscript>>& scripts,
	int activeScriptIdx) noexcept
{
	OFS_PROFILE(__FUNCTION__);

	const auto activeScript = scripts[activeScriptIdx].get();

	float currentTime = player->CurrentTime();
	float duration = player->Duration();

	auto& style = ImGui::GetStyle();
	offsetTime = currentTime - (visibleTime / 2.0);
	if(IsSelecting) handleSelectionScrolling();
	
	OverlayDrawingCtx drawingCtx;
	drawingCtx.offsetTime = offsetTime;
	drawingCtx.visibleTime = visibleTime;
	drawingCtx.totalDuration = duration;
	if (drawingCtx.totalDuration == 0.f) return;
	
	ImGui::Begin(TR_ID(WindowId, Tr::POSITIONS));
	drawingCtx.drawList = ImGui::GetWindowDrawList();
	PositionsItemHovered = ImGui::IsWindowHovered();

	drawingCtx.drawnScriptCount = 0;
	for (auto&& script : scripts) {
		if (script->Enabled) { drawingCtx.drawnScriptCount++; }
	}

	const float verticalSpacingBetweenScripts = style.ItemSpacing.y*2.f;
	const auto availSize = ImGui::GetContentRegionAvail() - ImVec2(0.f , verticalSpacingBetweenScripts*((float)drawingCtx.drawnScriptCount-1));
	const auto startCursor = ImGui::GetCursorScreenPos();

	ImGui::SetCursorScreenPos(startCursor);
	for(int i=0; i < scripts.size(); i += 1) 
	{
		auto script = scripts[i].get();
		if (!script->Enabled) { continue; }
		
		drawingCtx.scriptIdx = i;
		drawingCtx.canvasPos = ImGui::GetCursorScreenPos();
		drawingCtx.canvasSize = ImVec2(availSize.x, availSize.y / (float)drawingCtx.drawnScriptCount);
		const ImGuiID itemID = ImGui::GetID(script->Title.empty() ? "empty script" : script->Title.c_str());
		ImRect itemBB(drawingCtx.canvasPos, drawingCtx.canvasPos + drawingCtx.canvasSize);
		ImGui::ItemSize(itemBB);
		if (!ImGui::ItemAdd(itemBB, itemID)) {
			continue;
		}

		drawingCtx.drawList->PushClipRect(itemBB.Min - ImVec2(0.f, 1.f), itemBB.Max + ImVec2(0.f, 1.f));

		bool ItemIsHovered = ImGui::IsItemHovered();
		if (ItemIsHovered) {
			hovereScriptIdx = i;
			hoveredCanvasPos = drawingCtx.canvasPos;
			hoveredCanvasSize = drawingCtx.canvasSize;
		}

		const bool IsActivated = script == activeScript && drawingCtx.drawnScriptCount > 1;
		if (drawingCtx.drawnScriptCount == 1) {
			activeCanvasPos = drawingCtx.canvasPos;
			activeCanvasSize = drawingCtx.canvasSize;
		} else if (IsActivated) {
			activeCanvasPos = drawingCtx.canvasPos;
			activeCanvasSize = drawingCtx.canvasSize;
		}

		if (IsActivated) {
			drawingCtx.drawList->AddRectFilledMultiColor(
				drawingCtx.canvasPos, 
				ImVec2(drawingCtx.canvasPos.x + drawingCtx.canvasSize.x, drawingCtx.canvasPos.y + drawingCtx.canvasSize.y),
				IM_COL32(1.2f*50, 0, 1.2f*50, 255), IM_COL32(1.2f*50, 0, 1.2f*50, 255),
				IM_COL32(1.2f*20, 0, 1.2f*20, 255), IM_COL32(1.2f*20, 0, 1.2f*20, 255)
			);
		}
		else {
			drawingCtx.drawList->AddRectFilledMultiColor(drawingCtx.canvasPos, 
				ImVec2(drawingCtx.canvasPos.x + drawingCtx.canvasSize.x, drawingCtx.canvasPos.y + drawingCtx.canvasSize.y),
				IM_COL32(0, 0, 50, 255), IM_COL32(0, 0, 50, 255),
				IM_COL32(0, 0, 20, 255), IM_COL32(0, 0, 20, 255)
			);
		}

		if (ItemIsHovered) {
			drawingCtx.drawList->AddRectFilled(
				drawingCtx.canvasPos, 
				ImVec2(drawingCtx.canvasPos.x + drawingCtx.canvasSize.x, drawingCtx.canvasPos.y + drawingCtx.canvasSize.y),
				IM_COL32(255, 255, 255, 10)
			);
		}

		// FIXME: use the binary search here
		auto startIt = std::find_if(script->Actions().begin(), script->Actions().end(),
		    [this](auto act) { return act.atS >= offsetTime; });
		if (startIt != script->Actions().begin()) {
		    startIt -= 1;
		}

		auto endIt = std::find_if(startIt, script->Actions().end(),
		    [this](auto act) { return act.atS >= offsetTime + visibleTime; });
		if (endIt != script->Actions().end()) {
		    endIt += 1;
		}
		drawingCtx.actionFromIdx = std::distance(script->Actions().begin(), startIt);
		drawingCtx.actionToIdx = std::distance(script->Actions().begin(), endIt);
		drawingCtx.script = script;

		if(script->HasSelection())
		{
			auto startIt = std::find_if(script->Selection().begin(), script->Selection().end(),
				[&](auto act) { return act.atS >= drawingCtx.offsetTime; });
				if (startIt != script->Selection().begin())
					startIt -= 1;

			auto endIt = std::find_if(startIt, script->Selection().end(),
				[&](auto act) { return act.atS >= drawingCtx.offsetTime + drawingCtx.visibleTime; });
				if (endIt != script->Selection().end())
					endIt += 1;

			drawingCtx.selectionFromIdx = std::distance(script->Selection().begin(), startIt);
			drawingCtx.selectionToIdx = std::distance(script->Selection().begin(), endIt);
		}
		else 
		{
			drawingCtx.selectionFromIdx = 0;
			drawingCtx.selectionToIdx = 0;
		}

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
		if (script->HasSelection()) { 
			borderColor = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_SliderGrabActive]); 
		}
		drawingCtx.drawList->AddRect(
			drawingCtx.canvasPos,
			ImVec2(drawingCtx.canvasPos.x + drawingCtx.canvasSize.x, drawingCtx.canvasPos.y + drawingCtx.canvasSize.y),
			borderColor,
			0.f, ImDrawFlags_None,
			borderThicknes
		);

		// current position indicator -> |
		drawingCtx.drawList->AddTriangleFilled(
			drawingCtx.canvasPos + ImVec2((drawingCtx.canvasSize.x/2.f) - ImGui::GetFontSize(), 0.f),
			drawingCtx.canvasPos + ImVec2((drawingCtx.canvasSize.x/2.f) + ImGui::GetFontSize(), 0.f),
			drawingCtx.canvasPos + ImVec2((drawingCtx.canvasSize.x/2.f), ImGui::GetFontSize()/1.5f),
			IM_COL32(255, 255, 255, 255)
		);
		drawingCtx.drawList->AddLine(
			drawingCtx.canvasPos + ImVec2((drawingCtx.canvasSize.x/2.f)-0.5f, 0),
			drawingCtx.canvasPos + ImVec2((drawingCtx.canvasSize.x/2.f)-0.5f, drawingCtx.canvasSize.y-1.f),
			IM_COL32(255, 255, 255, 255),
		4.0f);

		// selection box
		constexpr auto selectColor = IM_COL32(3, 252, 207, 255);
		constexpr auto selectColorBackground = IM_COL32(3, 252, 207, 100);
		if (IsSelecting && IsActivated) {
			float relSel1 = (absSel1 - offsetTime) / visibleTime;
			drawingCtx.drawList->AddRectFilled(drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * relSel1, 0), drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * relSel2, drawingCtx.canvasSize.y), selectColorBackground);
			drawingCtx.drawList->AddLine(drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * relSel1, 0), drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * relSel1, drawingCtx.canvasSize.y), selectColor, 3.0f);
			drawingCtx.drawList->AddLine(drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * relSel2, 0), drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * relSel2, drawingCtx.canvasSize.y), selectColor, 3.0f);
		}

		// TODO: refactor this
		// selectionStart currently used for controller select
		if (startSelectionTime >= 0.f) {
			float startSelectRel = (startSelectionTime - offsetTime) / visibleTime;
			drawingCtx.drawList->AddLine(
				drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * startSelectRel, 0),
				drawingCtx.canvasPos + ImVec2(drawingCtx.canvasSize.x * startSelectRel, drawingCtx.canvasSize.y),
				selectColor, 3.0f
			);
		}


		// Handle action clicks
		if(ItemIsHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			handleActionClicks(drawingCtx);
		}
		else if(IsMoving)
		{
			if(ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) 
			{
				// Update dragged action
				auto mousePos = ImGui::GetMousePos();
				auto newAction = getActionForPoint(drawingCtx, mousePos);
				ActionMovedEventData = ScriptTimelineEvents::ActionMovedEventArgs{newAction, scripts[i]};
				EventSystem::PushEvent(ScriptTimelineEvents::FunscriptActionMoved, &ActionMovedEventData);
			}
			else
			{
				// Stop dragging
				IsMoving = false;
			}
		}
		else if(!IsMoving && ItemIsHovered)
		{
			handleTimelineHover(drawingCtx);
		}

		ImVec2 newCursor(drawingCtx.canvasPos.x, drawingCtx.canvasPos.y + drawingCtx.canvasSize.y + verticalSpacingBetweenScripts);
		if (newCursor.y < (startCursor.y + availSize.y)) { ImGui::SetCursorScreenPos(newCursor); }


		// right click context menu
		if (ImGui::BeginPopupContextItem(script->Title.c_str()))
		{
			if (ImGui::BeginMenu(TR_ID("SCRIPTS", Tr::SCRIPTS))) {
				for (auto& script : scripts) {
					if(script->Title.empty()) {
						ImGui::TextDisabled(TR(NONE));
						continue;
					}
					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, drawingCtx.drawnScriptCount == 1 && script->Enabled);
					if (ImGui::Checkbox(script->Title.c_str(), &script->Enabled) && !script->Enabled) {
						if (script.get() == activeScript) {
							// find a enabled script which can be set active
							for (int i = 0; i < scripts.size(); i++) {
								if (scripts[i]->Enabled) {									
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
				auto& overlayState = BaseOverlayState::State(overlayStateHandle);
				ImGui::MenuItem(TR(SHOW_ACTION_LINES), 0, &BaseOverlay::ShowLines);
				ImGui::MenuItem(TR(SPLINE_MODE), 0, &overlayState.SplineMode);
				ImGui::MenuItem(TR(SHOW_VIDEO_POSITION), 0, &overlayState.SyncLineEnable);
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
				else if(ImGui::MenuItem(TR(UPDATE_WAVEFORM), NULL, false, !Wave.data.BusyGenerating() && !videoPath.empty())) {
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

		drawingCtx.drawList->PopClipRect();
	}
	ImGui::End();
}

constexpr uint32_t HighRangeCol = IM_COL32(0xE3, 0x42, 0x34, 0xff);
constexpr uint32_t MidRangeCol = IM_COL32(0xE8, 0xD7, 0x5A, 0xff);
constexpr uint32_t LowRangeCol = IM_COL32(0xF7, 0x65, 0x38, 0xff); // IM_COL32(0xff, 0xba, 0x08, 0xff);

void ScriptTimeline::DrawAudioWaveform(const OverlayDrawingCtx& ctx) noexcept
{
	OFS_PROFILE(__FUNCTION__);

	if (ShowAudioWaveform && Wave.data.SampleCount() > 0) {
		Wave.WaveformViewport = ImGui::GetWindowViewport();
		auto renderWaveform = [](ScriptTimeline* timeline, const OverlayDrawingCtx& ctx) noexcept
		{
			OFS_PROFILE("DrawAudioWaveform::renderWaveform");
			
			timeline->Wave.Update(ctx);
			
			ctx.drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) noexcept {
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

			ctx.drawList->AddImage(0, ctx.canvasPos, ctx.canvasPos + ctx.canvasSize);
			ctx.drawList->AddCallback(ImDrawCallback_ResetRenderState, 0);
			ctx.drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) noexcept { glActiveTexture(GL_TEXTURE0); }, 0);
		};

		renderWaveform(this, ctx);
	}
}