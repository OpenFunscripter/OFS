#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include "OFS_Reflection.h"
#include "OFS_BinarySerialization.h"
#include "OFS_Util.h"
#include "OFS_Shader.h"
#include "OFS_Localization.h"
#include "OFS_VideoplayerEvents.h"

#include <string>
#include "SDL_events.h"


class OFS_VideoplayerWindow
{
public:
	~OFS_VideoplayerWindow() noexcept;
	uint32_t StateHandle() const noexcept { return stateHandle; }
private:
	class OFS_Videoplayer* player = nullptr;
	std::unique_ptr<VrShader> vrShader;
	
	ImGuiID videoImageId;
	ImVec2 videoDrawSize;
	ImVec2 viewportPos;
	ImVec2 windowPos;

	uint32_t stateHandle = 0xFFFF'FFFF;

	float baseScaleFactor = 1.f;

	bool videoHovered = false;
	bool dragStarted = false;

	static constexpr float ZoomMulti = 0.05f;

	void mouseScroll(const OFS_SDL_Event* ev) noexcept;
	void drawVrVideo(ImDrawList* draw_list) noexcept;
	void draw2dVideo(ImDrawList* draw_list) noexcept;
	void videoRightClickMenu() noexcept;
public:
	static constexpr const char* WindowId = "###VIDEOPLAYER";
	ImDrawCallback OnRenderCallback = nullptr;

	bool Init(OFS_Videoplayer* player) noexcept;
	void DrawVideoPlayer(bool* open, bool* drawVideo) noexcept;

	void ResetTranslationAndZoom() noexcept;
};
