#pragma once
#include "OFS_ScriptingMode.h"
#include "OFS_KeybindingSystem.h"
#include "OFS_Preferences.h"
#include "OFS_ScriptTimeline.h"
#include "OFS_UndoSystem.h"
#include "OFS_EventSystem.h"
#include "OFS_ScriptSimulator.h"
#include "OFS_ControllerInput.h"
#include "GradientBar.h"
#include "OFS_SpecialFunctions.h"
#include "OFS_VideoplayerControls.h"
#include "OFS_Project.h"
#include "OFS_BlockingTask.h"
#include "OFS_DynamicFontAtlas.h"
#include "OFS_LuaExtensions.h"
#include "OFS_Localization.h"
#include "OFS_StateManager.h"
#include "OFS_FunscriptMetadataEditor.h"

#include "OFS_Videoplayer.h"
#include "OFS_VideoplayerWindow.h"
#include "OFS_WebsocketApi.h"
#include "OFS_ChapterManager.h"

#include <memory>
#include <chrono>

enum OFS_Status : uint8_t {
    OFS_None = 0x0,
    OFS_ShouldExit = 0x1,
    OFS_Fullscreen = 0x1 << 1,
    OFS_GradientNeedsUpdate = 0x1 << 2,
    OFS_GamepadSetPlaybackSpeed = 0x1 << 3,
    OFS_AutoBackup = 0x1 << 4
};

class OpenFunscripter {
private:
    SDL_Window* window;
    SDL_GLContext glContext;

    uint32_t stateHandle = 0xFFFF'FFFF;
    bool ShowMetadataEditor = false;
    bool ShowProjectEditor = false;
#ifndef NDEBUG
    bool DebugDemo = false;
#endif
    bool DebugMetrics = false;
    bool ShowAbout = false;
    bool IdleMode = false;
    uint32_t IdleTimer = 0;

    FunscriptArray CopiedSelection;
    std::chrono::steady_clock::time_point lastBackup;

    char tmpBuf[2][32];

    void setIdle(bool idle) noexcept;
    void registerBindings();

    void update() noexcept;
    void newFrame() noexcept;
    void render() noexcept;
    void autoBackup() noexcept;

    void exitApp(bool force = false) noexcept;

    bool imguiSetup() noexcept;
    void processEvents() noexcept;

    void ExportClip(const class ExportClipForChapter* ev) noexcept;

    void FunscriptChanged(const FunscriptActionsChangedEvent* ev) noexcept;
    void DragNDrop(const OFS_SDL_Event* ev) noexcept;

    void VideoDuration(const DurationChangeEvent* ev) noexcept;
    void VideoLoaded(const VideoLoadedEvent* ev) noexcept;
    void PlayPauseChange(const PlayPauseChangeEvent* ev) noexcept;

    void ControllerAxisPlaybackSpeed(const OFS_SDL_Event* ev) noexcept;

    void ScriptTimelineActionCreated(const FunscriptActionShouldCreateEvent* ev) noexcept;
    void ScriptTimelineActionMoved(const FunscriptActionShouldMoveEvent* ev) noexcept;
    void ScriptTimelineActionClicked(const FunscriptActionClickedEvent* ev) noexcept;
    void ScriptTimelineDoubleClick(const ShouldSetTimeEvent* ev) noexcept;
    void ScriptTimelineSelectTime(const FunscriptShouldSelectTimeEvent* ev) noexcept;
    void ScriptTimelineActiveScriptChanged(const ShouldChangeActiveScriptEvent* ev) noexcept;

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
    void pickDifferentMedia() noexcept;

    void saveHeatmap(const char* path, int width, int height);
    void updateTitle() noexcept;

    void removeAction(FunscriptAction action) noexcept;
    void removeAction() noexcept;
    void addEditAction(int pos) noexcept;

    void saveActiveScriptAs();

    void openFile(const std::string& file) noexcept;
    void initProject() noexcept;
    bool closeProject(bool closeWithUnsavedChanges) noexcept;

    void SetFullscreen(bool fullscreen);
    void setupDefaultLayout(bool force) noexcept;

    template<typename OnCloseAction>
    void closeWithoutSavingDialog(OnCloseAction&& action) noexcept;

    // UI
    void CreateDockspace() noexcept;
    void ShowAboutWindow(bool* open) noexcept;
    void ShowStatisticsWindow(bool* open) noexcept;
    void ShowMainMenuBar() noexcept;
    bool ShowMetadataEditorWindow(bool* open) noexcept;

public:
    static OpenFunscripter* ptr;
    uint8_t Status = OFS_Status::OFS_AutoBackup;

    ~OpenFunscripter() noexcept;

    ScriptTimeline scriptTimeline;
    OFS_VideoplayerControls playerControls;
    ScriptSimulator simulator;
    OFS_BlockingTask blockingTask;

    std::unique_ptr<OFS_Videoplayer> player;
    std::unique_ptr<OFS_VideoplayerWindow> playerWindow;
    std::unique_ptr<OFS_KeybindingSystem> keys;
    std::unique_ptr<SpecialFunctionsWindow> specialFunctions;
    std::unique_ptr<ScriptingMode> scripting;
    std::unique_ptr<ControllerInput> controllerInput;
    std::unique_ptr<OFS_Preferences> preferences;
    std::unique_ptr<UndoSystem> undoSystem;
    std::unique_ptr<OFS_LuaExtensions> extensions;
    std::unique_ptr<OFS_FunscriptMetadataEditor> metadataEditor;
    std::unique_ptr<OFS_WebsocketApi> webApi;
    std::unique_ptr<OFS_ChapterManager> chapterMgr;

    std::unique_ptr<OFS_Project> LoadedProject;

    bool Init(int argc, char* argv[]);
    int Run() noexcept;
    void Step() noexcept;
    void Shutdown() noexcept;

    inline const std::vector<std::shared_ptr<Funscript>>& LoadedFunscripts() const noexcept
    {
        return LoadedProject->Funscripts;
    }

    inline std::shared_ptr<Funscript>& ActiveFunscript() noexcept
    {
        return LoadedProject->ActiveScript();
    }

    void UpdateNewActiveScript(uint32_t activeIndex) noexcept;

    inline const FunscriptArray& FunscriptClipboard() const { return CopiedSelection; }

    inline void LoadOverrideFont(const std::string& font) noexcept
    {
        OFS_DynFontAtlas::FontOverride = font;
        OFS_DynFontAtlas::ptr->forceRebuild = true;
    }
    void Undo() noexcept;
    void Redo() noexcept;
};

template<typename OnCloseAction>
inline void OpenFunscripter::closeWithoutSavingDialog(OnCloseAction&& onProjectCloseHandler) noexcept
{
    if (LoadedProject->HasUnsavedEdits()) {
        Util::YesNoCancelDialog(TR(PROJECT_HAS_UNSAVED_EDITS),
            TR(CLOSE_WITHOUT_SAVING_MSG),
            [this, onProjectCloseHandler = std::move(onProjectCloseHandler)](Util::YesNoCancel result) mutable {
                if (result == Util::YesNoCancel::Yes) {
                    LoadedProject->Save(true);
                    closeProject(true);
                    onProjectCloseHandler();
                }
                else if (result == Util::YesNoCancel::No) {
                    /* don't save */
                    closeProject(true);
                    onProjectCloseHandler();
                }
                /* do nothing on cancel */
            });
    }
    else {
        // the project has no edits and can be closed
        closeProject(true);
        onProjectCloseHandler();
    }
}
