#pragma once

#include "SDL.h"


#include "glad/glad.h"
#include "OFS_Simulator3D.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "ScriptingMode.h"
#include "KeybindingSystem.h"
#include "OpenFunscripterSettings.h"
#include "OFS_ScriptTimeline.h"
#include "OFS_Videoplayer.h"
#include "OFS_UndoSystem.h"
#include "FunscriptUndoSystem.h"
#include "EventSystem.h"
#include "GradientBar.h"
#include "ScriptSimulator.h"
#include "Funscript.h"
#include "OFS_ControllerInput.h"
#include "GradientBar.h"
#include "SpecialFunctions.h"
#include "OFS_ScriptSettings.h"
#include "OFS_Events.h"
#include "OFS_VideoplayerControls.h"
#include "OFS_TCode.h"
#include "OFS_Project.h"
#include "OFS_AsyncIO.h"

#include <memory>
#include <array>
#include <chrono>

class OpenFunscripter {
private:
	SDL_Window* window;
	SDL_GLContext gl_context;
	bool ShouldExit = false;
	
	// TODO: move this into a bitset
	bool ShowMetadataEditor = false;
	bool Fullscreen = false;
	bool DebugMetrics = false;
	bool DebugDemo = false;
	bool ShowAbout = false;
	
	std::vector<FunscriptAction> CopiedSelection;

	std::chrono::steady_clock::time_point lastBackup;

	bool updateTimelineGradient = false;
	char tmp_buf[2][32];

	int32_t ActiveFunscriptIdx = 0;

	void register_bindings();

	void update() noexcept;
	void new_frame() noexcept;
	void render() noexcept;
	void autoBackup() noexcept;

	void exitApp(bool force = false) noexcept;

	bool load_fonts(const char* font_override = nullptr) noexcept;
	bool imgui_setup() noexcept;
	void process_events() noexcept;

	void FunscriptChanged(SDL_Event& ev) noexcept;

	void DragNDrop(SDL_Event& ev) noexcept;

	void MpvVideoLoaded(SDL_Event& ev) noexcept;
	void MpvPlayPauseChange(SDL_Event& ev) noexcept;

	bool SetPlaybackSpeedController = false;
	void ControllerAxisPlaybackSpeed(SDL_Event& ev) noexcept;

	void ScriptTimelineActionClicked(SDL_Event& ev) noexcept;
	void ScriptTimelineDoubleClick(SDL_Event& ev) noexcept;
	void ScriptTimelineSelectTime(SDL_Event& ev) noexcept;
	void ScriptTimelineActiveScriptChanged(SDL_Event& ev) noexcept;


	void selectTopPoints() noexcept;
	void selectMiddlePoints() noexcept;
	void selectBottomPoints() noexcept;

	void cutSelection() noexcept;
	void copySelection() noexcept;
	void pasteSelection() noexcept;
	void pasteSelectionExact() noexcept;
	void equalizeSelection() noexcept;
	void invertSelection() noexcept;
	void isolateAction() noexcept;
	void repeatLastStroke() noexcept;

	void saveProject() noexcept;
	void quickExport() noexcept;

	bool closeProject() noexcept;

	void saveHeatmap(const char* path, int width, int height);
	void updateTitle() noexcept;

	void removeAction(FunscriptAction action) noexcept;
	void removeAction() noexcept;
	void addEditAction(int pos) noexcept;

	//void showOpenFileDialog();
	void saveActiveScriptAs();

	bool openFile(const std::string& file) noexcept;
	bool importFile(const std::string& file) noexcept;
	bool openProject(const std::string& file) noexcept;
	void initProject() noexcept;
	
	void SetFullscreen(bool fullscreen);


	void setupDefaultLayout(bool force) noexcept;
	//void clearLoadedScripts() noexcept;

	// UI
	void CreateDockspace() noexcept;
	void ShowAboutWindow(bool* open) noexcept;
	void ShowStatisticsWindow(bool* open) noexcept;
	void ShowMainMenuBar() noexcept;
	bool ShowMetadataEditorWindow(bool* open) noexcept;
public:
	static OpenFunscripter* ptr;
	static ImFont* DefaultFont2; // x2 size of default
	static std::array<const char*, 6> SupportedVideoExtensions;
	static std::array<const char*, 4> SupportedAudioExtensions;

	~OpenFunscripter();

	KeybindingSystem keybinds;
	ScriptTimeline scriptPositions;
	OFS_VideoplayerControls playerControls;
	ScriptSimulator simulator;
	TCodePlayer tcode;

	bool AutoBackup = true;

	std::unique_ptr<VideoplayerWindow> player;
	std::unique_ptr<SpecialFunctionsWindow> specialFunctions;
	std::unique_ptr<ScriptingMode> scripting;
	std::unique_ptr<EventSystem> events;
	std::unique_ptr<ControllerInput> controllerInput;
	std::unique_ptr<OpenFunscripterSettings> settings;
	std::unique_ptr<UndoSystem> undoSystem;

	std::unique_ptr<Simulator3D> sim3D;

	std::unique_ptr<OFS_Project> LoadedProject;
	std::unique_ptr<OFS_AsyncIO> IO;

	bool setup();
	int run() noexcept;
	void step() noexcept;
	void shutdown() noexcept;

	inline const std::vector<std::shared_ptr<Funscript>>& LoadedFunscripts() const noexcept
	{
		return LoadedProject->Funscripts;
	}

	inline bool ScriptLoaded() const { return LoadedFunscripts().size() > 0; }
	inline std::shared_ptr<Funscript>& ActiveFunscript() noexcept {
		FUN_ASSERT(ScriptLoaded(), "No script loaded");
		return LoadedProject->Funscripts[ActiveFunscriptIdx]; 
	}

	inline std::shared_ptr<Funscript>& RootFunscript() noexcept {
		FUN_ASSERT(ScriptLoaded(), "No script loaded");
		// when multiple funscripts are loaded the root funscript will store paths to the associated scripts
		return LoadedProject->Funscripts[0];
	}

	void UpdateNewActiveScript(int32_t activeIndex) noexcept;
	inline int32_t ActiveFunscriptIndex() const { return ActiveFunscriptIdx; }

	static inline Funscript& script() noexcept { return *OpenFunscripter::ptr->ActiveFunscript(); }
	static void SetCursorType(ImGuiMouseCursor id) noexcept;

	inline const std::vector<FunscriptAction>& FunscriptClipboard() const { return CopiedSelection; }

	inline bool LoadOverrideFont(const std::string& font) noexcept { return load_fonts(font.empty() ? nullptr : font.c_str()); }

	void Undo() noexcept;
	void Redo() noexcept;
};
