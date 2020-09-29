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
#include "RawInput.h"

#include <memory>
#include <array>
#include <chrono>

class OpenFunscripter {
private:
	SDL_Window* window;
	SDL_GLContext gl_context;
	bool exit_app = false;

	bool ShowMetadataEditor = false;
	bool ShowStatistics = true;
	bool ShowHistory = true;
	bool RollingBackup = false;
	bool Fullscreen = false;
	bool DebugMetrics = false;
	bool DebugDemo = false;
	
	std::vector<FunscriptAction> CopiedSelection;

	std::chrono::system_clock::time_point last_save_time;

	ScriptingMode scripting;
	KeybindingSystem keybinds;
	UndoSystem undoRedoSystem;
	ScriptPositionsWindow scriptPositions;
	RawInput rawInput;
	ScriptSimulator simulator;

	bool updateTimelineGradient = false;
	char tmp_buf[2][32];

	void register_bindings();

	void update();
	void new_frame();
	void render();

	bool imgui_setup();
	void process_events();

	void FunscriptChanged(SDL_Event& ev);
	void FunscriptActionClicked(SDL_Event& ev);

	void FileDialogOpenEvent(SDL_Event& ev);
	void FileDialogSaveEvent(SDL_Event& ev);
	void DragNDrop(SDL_Event& ev);

	void cutSelection();
	void copySelection();
	void pasteSelection();
	void equalizeSelection();
	void invertSelection();

	void saveScript(const char* path = nullptr);


	void removeAction(const FunscriptAction& action);
	void removeAction();
	void addEditAction(int pos);


	void showOpenFileDialog();
	void showSaveFileDialog();
	bool openFile(const std::string& file);
	void updateTitle();

	void fireAlert(const std::string& msg);
	
	void SetFullscreen(bool fullscreen);

	void UpdateTimelineGradient(ImGradient& grad);

	// UI
	void CreateDockspace();
	void ShowStatisticsWindow(bool* open);
	void ShowUndoRedoHistory(bool* open);
	bool DrawTimelineWidget(const char* label, float* position);
	void ShowMainMenuBar();
	bool ShowMetadataEditorWindow(bool* open);
public:
	static OpenFunscripter* ptr;

	EventSystem events;
	VideoplayerWindow player;
	std::unique_ptr<OpenFunscripterSettings> settings;
	std::unique_ptr<Funscript> LoadedFunscript;

	const std::array<const char*, 6> SupportedVideoExtensions{
		".mp4",
		".avi",
		".m4v",
		".webm",
		".mkv",
		".wmv",
	};

	bool setup();
	int run();
	void shutdown();

	static inline Funscript& script() { return *OpenFunscripter::ptr->LoadedFunscript; }
};
