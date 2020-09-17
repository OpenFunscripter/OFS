#pragma once

#include "SDL.h"
#include "glad/glad.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "ScriptingMode.h"
#include "KeybindingSystem.h"
#include "OpenFunscripterSettings.h"
#include "ScriptPositionsWindow.h"
#include "OpenFunscripterVideoplayer.h"
#include "UndoSystem.h"
#include "event/EventSystem.h"
#include "UI/GradientBar.h"

#include "Funscript.h"
#include "RawInput.h"

#include <memory>
#include <array>

class OpenFunscripter {
private:
	SDL_Window* window;
	SDL_GLContext gl_context;
	bool exit_app = false;

	bool ShowStatistics = true;
	bool ShowHistory = true;
	bool RollingBackup = false;
	bool Fullscreen = false;
	bool DebugMetrics = false;
	bool DebugDemo = false;
	std::vector<FunscriptAction> CopiedSelection;

	ScriptingMode scripting;
	KeybindingSystem keybinds;
	UndoSystem undoRedoSystem;
	ScriptPositionsWindow scriptPositions;
	RawInput rawInput;
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

	inline void formatTime(char* buffer, size_t buf_size, float time_seconds) {
		if (std::isinf(time_seconds) || std::isnan(time_seconds)) time_seconds = 0.f;
		auto duration = std::chrono::duration<double>(time_seconds);
		std::time_t t = duration.count();
		std::tm timestamp = *std::gmtime(&t);

		int ms = (time_seconds - (int)time_seconds) * 1000.0;
		std::strftime(buffer, buf_size, "%H:%M:%S", &timestamp);
	}

	void cutSelection();
	void copySelection();
	void pasteSelection();
	void setPosition(float ms);

	void saveScript(const char* path = nullptr);


	void removeAction(const FunscriptAction& action);
	void removeAction();
	void addEditAction(int pos);


	void showOpenFileDialog();
	void showSaveFileDialog();
	bool openFile(const std::string& file);
	bool openFunscript(const std::string& file);
	void updateTitle(const std::string& title);

	void fireAlert(const std::string& msg);
	
	void SetFullscreen(bool fullscreen);

	void UpdateTimelineGradient(ImGradient& grad);

	// UI
	void CreateDockspace();
	void ShowStatisticsWindow(bool* open);
	void ShowUndoRedoHistory(bool* open);
	void ShowSimulatorWindow(bool* open);
	bool DrawTimelineWidget(const char* label, float* position);
	void ShowMainMenuBar();
public:
	static OpenFunscripter* ptr;

	EventSystem events;
	std::unique_ptr<OpenFunscripterSettings> settings;
	OpenFunscripterVideoplayerWindow player;
	std::shared_ptr<Funscript> LoadedFunscript;

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
};
