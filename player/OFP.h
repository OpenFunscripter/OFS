#pragma once

#include "EventSystem.h"
#include "Funscript.h"
#include "KeybindingSystem.h"
#include "OFS_TCode.h"
#include "OFS_Util.h"
#include "OFS_Reflection.h"
#include "OFS_ControllerInput.h"
#include "OFS_Simulator3D.h"
#include "OFS_ScriptTimeline.h"

#include "OFP_WrappedVideoplayerControls.h"
#include "OFP_WrappedPlayer.h"

#include "OFP_Videobrowser.h"
#include "OFP_Settings.h"
#include "SDL_events.h"

#include <memory>
#include <cstdint>
#include <string>
#include <cstdint>

struct AutoHideTime 
{
	uint32_t StartTime = 0;
	static constexpr uint32_t HideAfterTime = 5000;

	inline void reset() { StartTime = SDL_GetTicks(); }
	inline bool hidden() const { return SDL_GetTicks() >= (StartTime + HideAfterTime); }
};

struct OFP_ScriptSettings {
	
	TChannel ScriptChannel;

	template <class Archive>
	inline void reflect(Archive& ar) {
		//OFS_RELFECT(..., ar);
	}
};

class OFP {
private:
	SDL_Window* window;
	SDL_GLContext gl_context;
	bool exit_app = false;


#ifndef NDEBUG
	bool DebugMetrics = false;
	bool DebugDemo = false;
#endif

#ifdef WIN32
	bool Whirligig = false;
#endif
	bool Fullscreen = false;
	void set_fullscreen(bool full) noexcept;

	void set_default_layout(bool force) noexcept;

	void process_events() noexcept;
	void update() noexcept;
	void new_frame() noexcept;
	void render() noexcept;

	void ShowMainMenuBar() noexcept;
	void CreateDockspace(bool withMenuBar) noexcept;

	void SetNavigationMode(bool enable) noexcept;
	void SetActiveScene(OFP_Scene scene) noexcept;
	void ToggleVrMode() noexcept;
public:
	static ImFont* DefaultFont2; // x2 size of default

	OFP_Settings settings;
	KeybindingSystem keybinds;
	OFP_WrappedVideoplayerControls playerControls;
	ScriptTimeline scriptTimeline;
	AutoHideTime timer;

	std::unique_ptr<WrappedPlayer> player;

	std::unique_ptr<EventSystem> events;
	std::unique_ptr<TCodePlayer> tcode;
	std::unique_ptr<ControllerInput> controllerInput;

	std::unique_ptr<Simulator3D> sim3d;

	std::unique_ptr<Videobrowser> videobrowser;
	std::vector<std::shared_ptr<Funscript>> LoadedFunscripts;

	~OFP() noexcept;

	bool load_fonts(const char* font_override) noexcept;
	bool imgui_setup() noexcept;

	void register_bindings() noexcept;

	bool setup();
	int run() noexcept;
	void step() noexcept;
	void shutdown() noexcept;

	void updateTitle() noexcept;
	bool openFile(const std::string& file) noexcept;
	void clearLoadedScripts() noexcept;


	void FilebrowserScene() noexcept;
	void PlayerScene() noexcept;

	// events
	void DragNDrop(SDL_Event& ev) noexcept;
	void MpvVideoLoaded(SDL_Event& ev) noexcept;
	void MpvPlayPauseChange(SDL_Event& ev) noexcept;

	void VideobrowserItemClicked(SDL_Event& ev) noexcept;

	ImVec2 rotateVR;
	float zoomVR = 1.f;
	void ControllerAxis(SDL_Event& ev) noexcept;

	inline bool ScriptLoaded() const { return LoadedFunscripts.size() > 0; }
	inline std::shared_ptr<Funscript>& RootFunscript() noexcept {
		FUN_ASSERT(ScriptLoaded(), "No script loaded");
		return LoadedFunscripts[0];
	}
};