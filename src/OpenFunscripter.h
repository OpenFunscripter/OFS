#pragma once

#include "SDL.h"
#include "glad/glad.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "ScriptingMode.h"
#include "KeybindingSystem.h"
#include "OpenFunscripterSettings.h"
#include "ScriptPositionsWindow.h"
#include "OpenFunscripterVideoplayer.h"
#include "UndoSystem.h"
#include "event/EventSystem.h"
#include "GradientBar.h"
#include "ScriptSimulator.h"
#include "Funscript.h"
#include "ControllerInput.h"
#include "GradientBar.h"
#include "SpecialFunctions.h"

#include <memory>
#include <array>
#include <chrono>

class OpenFunscripter {
private:
	SDL_Window* window;
	SDL_GLContext gl_context;
	bool exit_app = false;
	
	// TODO: move this into a bitset
	bool ShowMetadataEditor = false;
	bool Fullscreen = false;
	bool DebugMetrics = false;
	bool DebugDemo = false;
	bool ShowAbout = false;
	
	std::vector<FunscriptAction> CopiedSelection;

	std::chrono::system_clock::time_point last_save_time;
	std::chrono::system_clock::time_point last_backup;

	ImGradient TimelineGradient;
	bool updateTimelineGradient = false;
	char tmp_buf[2][32];

	int32_t ActiveFunscriptIdx = 0;

	void register_bindings();

	void update() noexcept;
	void new_frame() noexcept;
	void render() noexcept;
	void rollingBackup() noexcept;

	bool imgui_setup() noexcept;
	void process_events() noexcept;

	void FunscriptChanged(SDL_Event& ev) noexcept;
	void FunscriptActionClicked(SDL_Event& ev) noexcept;

	void DragNDrop(SDL_Event& ev) noexcept;

	void MpvVideoLoaded(SDL_Event& ev) noexcept;

	void cutSelection() noexcept;
	void copySelection() noexcept;
	void pasteSelection() noexcept;
	void equalizeSelection() noexcept;
	void invertSelection() noexcept;
	void isolateAction() noexcept;

	void saveScripts() noexcept;
	void saveHeatmap(const char* path, int width, int height);
	void updateTitle() noexcept;

	void removeAction(FunscriptAction action) noexcept;
	void removeAction() noexcept;
	void addEditAction(int pos) noexcept;

	void seekByTime(int32_t ms) noexcept;

	void showOpenFileDialog();
	void saveActiveScriptAs();
	bool openFile(const std::string& file);
	
	void SetFullscreen(bool fullscreen);

	void UpdateTimelineGradient(ImGradient& grad);

	void setupDefaultLayout(bool force) noexcept;

	void clearLoadedScripts() noexcept;

	// UI
	void CreateDockspace() noexcept;
	void ShowAboutWindow(bool* open) noexcept;
	void ShowStatisticsWindow(bool* open) noexcept;
	bool DrawTimelineWidget(const char* label, float* position) noexcept;
	void ShowMainMenuBar() noexcept;
	bool ShowMetadataEditorWindow(bool* open) noexcept;
public:
	static OpenFunscripter* ptr;
	static ImFont* DefaultFont2; // x2 size of default

	~OpenFunscripter();

	KeybindingSystem keybinds;
	ScriptPositionsWindow scriptPositions;
	VideoplayerWindow player;
	ScriptSimulator simulator;

	bool RollingBackup = true;

	std::unique_ptr<SpecialFunctionsWindow> specialFunctions;
	std::unique_ptr<ScriptingMode> scripting;
	std::unique_ptr<EventSystem> events;
	std::unique_ptr<ControllerInput> controllerInput;
	std::unique_ptr<OpenFunscripterSettings> settings;
	std::vector<std::unique_ptr<Funscript>> LoadedFunscripts;

	bool setup();
	int run() noexcept;
	void shutdown() noexcept;

	inline bool ScriptLoaded() const { return LoadedFunscripts.size() > 0; }
	inline std::unique_ptr<Funscript>& ActiveFunscript() noexcept { 
		FUN_ASSERT(ScriptLoaded(), "No script loaded");
		return LoadedFunscripts[ActiveFunscriptIdx]; 
	}

	void UpdateNewActiveScript(int32_t activeIndex) noexcept;
	inline int32_t ActiveFunscriptIndex() const { return ActiveFunscriptIdx; }

	static inline Funscript& script() noexcept { return *OpenFunscripter::ptr->ActiveFunscript(); }
	static void SetCursorType(ImGuiMouseCursor id) noexcept;
};
