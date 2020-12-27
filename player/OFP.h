#pragma once

#include "glad/glad.h"
#include "OFS_Videoplayer.h"
#include "EventSystem.h"
#include "Funscript.h"
#include "KeybindingSystem.h"
#include "OFS_Tcode.h"
#include "OFS_Util.h"
#include "OFS_Reflection.h"
#include "OFS_VideoplayerControls.h"

#include "SDL_events.h"

#include <memory>
#include <cstdint>
#include <string>

struct OFP_Settings {
	std::string font_override = "";
	float default_font_size = 18.f;
	bool vsync = true;
	bool show_video = true;

	bool show_tcode = false;

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(font_override, ar);
		OFS_REFLECT(default_font_size, ar);
		OFS_REFLECT(vsync, ar);
		OFS_REFLECT(show_video, ar);
	}
};

struct OFP_ScriptSettings {
	
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

	bool Fullscreen = false;
	void set_fullscreen(bool full) noexcept;

	void set_default_layout(bool force) noexcept;

	void process_events() noexcept;
	void update() noexcept;
	void new_frame() noexcept;
	void render() noexcept;

	void ShowMainMenuBar() noexcept;
	void CreateDockspace() noexcept;
public:
	static ImFont* DefaultFont2; // x2 size of default

	OFP_Settings settings;
	KeybindingSystem keybinds;
	VideoplayerWindow player;
	OFS_VideoplayerControls playerControls;

	std::unique_ptr<EventSystem> events;
	std::unique_ptr<TCodePlayer> tcode;

	std::vector<std::unique_ptr<Funscript>> LoadedFunscripts;

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

	// events
	void DragNDrop(SDL_Event& ev) noexcept;
	void MpvVideoLoaded(SDL_Event& ev) noexcept;
	void MpvPlayPauseChange(SDL_Event& ev) noexcept;

	inline bool ScriptLoaded() const { return LoadedFunscripts.size() > 0; }
	inline std::unique_ptr<Funscript>& RootFunscript() noexcept {
		FUN_ASSERT(ScriptLoaded(), "No script loaded");
		return LoadedFunscripts[0];
	}
};