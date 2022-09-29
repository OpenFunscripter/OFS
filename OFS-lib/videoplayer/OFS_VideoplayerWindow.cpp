#include "OFS_VideoplayerWindow.h"

#include "stb_sprintf.h"
#include "stb_image_write.h"
#include "OFS_GL.h"
#include "OFS_Localization.h"

#include "EventSystem.h"
#include "OFS_ImGui.h"
#include "OFS_Profiling.h"
#include "OFS_Shader.h"

#include "OFS_Videoplayer.h"


bool OFS_VideoplayerWindow::Init(OFS_Videoplayer* player) noexcept
{
	this->player = player;
	this->vrShader = std::make_unique<VrShader>();
	EventSystem::ev().Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &OFS_VideoplayerWindow::mouseScroll));

	
	videoImageId = ImGui::GetIDWithSeed("videoImage", 0, rand());
	return true;
}

OFS_VideoplayerWindow::~OFS_VideoplayerWindow() noexcept
{
	EventSystem::ev().UnsubscribeAll(this);
}

void OFS_VideoplayerWindow::mouseScroll(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (settings.LockedPosition) return;

	auto& scroll = ev.wheel;
	if (videoHovered) {
		auto mousePosInVid = ImGui::GetMousePos() - viewportPos - windowPos - settings.videoPos;
		float zoomPointX = (mousePosInVid.x - (videoDrawSize.x/2.f)) / videoDrawSize.x;
		float zoomPointY = (mousePosInVid.y - (videoDrawSize.y/2.f)) / videoDrawSize.y;

		float vidWidth = player->VideoWidth();
		float vidHeight = player->VideoHeight();

		switch (settings.activeMode) {
		case VideoMode::LEFT_PANE:
		case VideoMode::RIGHT_PANE:
			vidWidth /= 2.f;
			break;
		case VideoMode::TOP_PANE:
		case VideoMode::BOTTOM_PANE:
			vidHeight /= 2.f;
			break;
		}
		zoomPointX *= vidWidth;
		zoomPointY *= vidHeight;

		const float oldScale = settings.zoomFactor;
		// apply zoom
		if (settings.activeMode == VideoMode::VR_MODE) {
			settings.vrZoom *= ((1+(ZoomMulti * scroll.y)));
			settings.vrZoom = Util::Clamp(settings.vrZoom, 0.05f, 2.0f);
			return;
		}

		settings.zoomFactor *= 1 + (ZoomMulti * scroll.y);
		settings.zoomFactor = Util::Clamp(settings.zoomFactor, 0.0f, 10.f);

		const float scaleChange = (settings.zoomFactor - oldScale) * baseScaleFactor;
		const float offsetX = -(zoomPointX * scaleChange);
		const float offsetY = -(zoomPointY * scaleChange);

		settings.prevTranslation.x += offsetX;
		settings.prevTranslation.y += offsetY;

		if (!dragStarted) {
			settings.currentTranslation = settings.prevTranslation;
		}
	}
}

void OFS_VideoplayerWindow::drawVrVideo(ImDrawList* draw_list) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (!settings.LockedPosition && videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
		dragStarted = true;
	}
	// apply drag to translation
	else if (dragStarted && videoHovered)
	{
		settings.currentVrRotation =
			settings.prevVrRotation
			+ (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left) 
				/ ImVec2((10000.f * settings.vrZoom), (videoDrawSize.y / videoDrawSize.x) * 10000.f * settings.vrZoom));
	}

	playerViewport = ImGui::GetCurrentWindowRead()->Viewport;
	draw_list->AddCallback(
		[](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
			auto& ctx = *(OFS_VideoplayerWindow*)cmd->UserCallbackData;

			auto draw_data = ctx.playerViewport->DrawData;
			ctx.vrShader->use();

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
			ctx.vrShader->ProjMtx(&ortho_projection[0][0]);
			ctx.vrShader->Rotation(&ctx.settings.currentVrRotation.x);
			ctx.vrShader->Zoom(ctx.settings.vrZoom);
			ctx.vrShader->AspectRatio(ctx.videoDrawSize.x / ctx.videoDrawSize.y);

			if (ctx.player->VideoHeight() > 0) {
				ctx.vrShader->VideoAspectRatio((float)ctx.player->VideoWidth() /(float)ctx.player->VideoHeight());
			}
		}, this);

	OFS::ImageWithId(videoImageId, (void*)(intptr_t)player->FrameTexture(), ImGui::GetContentRegionAvail(), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));
	videoRightClickMenu();
	videoDrawSize = ImGui::GetItemRectSize();
}

void OFS_VideoplayerWindow::draw2dVideo(ImDrawList* draw_list) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	ImVec2 videoSize(player->VideoWidth(), player->VideoHeight());
	ImVec2 dst = ImGui::GetContentRegionAvail();
	baseScaleFactor = std::min(dst.x / videoSize.x, dst.y / videoSize.y);
	videoSize.x *= baseScaleFactor;
	videoSize.y *= baseScaleFactor;

	ImVec2 uv0(0.f, 0.f);
	ImVec2 uv1(1.f, 1.f);

	switch (settings.activeMode) {
	case VideoMode::LEFT_PANE:
		videoSize.x /= 2.f;
		uv1.x = 0.5f;
		break;
	case VideoMode::RIGHT_PANE:
		videoSize.x /= 2.f;
		uv0.x = 0.5f;
		break;
	case VideoMode::TOP_PANE:
		videoSize.y /= 2.f;
		uv1.y = 0.5;
		break;
	case VideoMode::BOTTOM_PANE:
		videoSize.y /= 2.f;
		uv0.y = 0.5f;
		break;
	case VideoMode::VR_MODE:
	case VideoMode::FULL:
	default:
		// nothing
		break;
	}

	videoSize = videoSize * ImVec2(settings.zoomFactor, settings.zoomFactor);
	settings.videoPos = (ImGui::GetWindowSize() - videoSize) * 0.5f + settings.currentTranslation;
	ImGui::SetCursorPos(settings.videoPos);
	// the videoHovered is one frame old but moving this up prevents flicker while dragging and zooming at the same time
	// start video dragging
	if (!settings.LockedPosition && videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
		dragStarted = true;
	}
	// apply drag to translation
	else if(dragStarted && videoHovered)
	{
		settings.currentTranslation = settings.prevTranslation + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
	}

	playerViewport = ImGui::GetCurrentWindowRead()->Viewport;
	OFS::ImageWithId(videoImageId, (void*)(intptr_t)player->FrameTexture(), videoSize, uv0, uv1);
	videoRightClickMenu();
}

void OFS_VideoplayerWindow::videoRightClickMenu() noexcept
{
	if (ImGui::BeginPopupContextItem())	{
		ImGui::MenuItem(TR(LOCK), NULL, &settings.LockedPosition);
		ImGui::EndPopup();
	}
}

void OFS_VideoplayerWindow::DrawVideoPlayer(bool* open, bool* drawVideo) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (open != nullptr && !*open) return;
	
	ImGui::Begin(TR_ID("VIDEOPLAYER", Tr::VIDEOPLAYER), open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

	if (!player->VideoLoaded()) {
		ImGui::End();
		return;
	}

	if (*drawVideo) {
		viewportPos = ImGui::GetWindowViewport()->Pos;
		auto drawList = ImGui::GetWindowDrawList();
		if (settings.activeMode != VideoMode::VR_MODE) {
			draw2dVideo(drawList);
		}
		else {
			drawVrVideo(drawList);
		}
		if (OnRenderCallback != nullptr) { drawList->AddCallback(OnRenderCallback, this); }
		drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	
		videoHovered = ImGui::IsItemHovered() && ImGui::IsWindowHovered();
		videoDrawSize = ImGui::GetItemRectSize();
	
		// cancel drag
		if ((dragStarted && !videoHovered) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			dragStarted = false;
			settings.prevTranslation = settings.currentTranslation;
			settings.prevVrRotation = settings.currentVrRotation;
		}
	
		// recenter
		if (videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			ResetTranslationAndZoom();
		}
	}
	else
	{
		if (ImGui::Button(TR(CLICK_TO_ENABLE_VIDEO))) {
			*drawVideo = true;
		}
	}
	windowPos = ImGui::GetWindowPos() - viewportPos;
	ImGui::End();
}