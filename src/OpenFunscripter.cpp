#include "OpenFunscripter.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "GradientBar.h"
#include "FunscriptHeatmap.h"
#include "OFS_DownloadFfmpeg.h"
#include "OFS_Shader.h"
#include "OFS_MpvLoader.h"
#include "OFS_Localization.h"

#include "state/OpenFunscripterState.h"
#include "state/states/VideoplayerWindowState.h"
#include "state/states/BaseOverlayState.h"
#include "state/states/ChapterState.h"

#include <filesystem>

#include "stb_sprintf.h"

#include "imgui_stdlib.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "SDL.h"
#include "asap.h"
#include "OFS_GL.h"

// TODO: Use ImGui tables API in keybinding UI
// TODO: extend "range extender" functionality ( only extend bottom/top, range reducer )
// TODO: render simulator relative to video position & zoom
// TODO: make speed coloring configurable
// TODO: OFS_ScriptTimeline selections cause alot of unnecessary overdraw

OpenFunscripter* OpenFunscripter::ptr = nullptr;
static constexpr const char* GlslVersion = "#version 330 core";

static ImGuiID MainDockspaceID;
static constexpr const char* StatisticsWindowId = "###STATISTICS";
static constexpr const char* ActionEditorWindowId = "###ACTION_EDITOR";

static constexpr int DefaultWidth = 1920;
static constexpr int DefaultHeight = 1080;

static constexpr int AutoBackupIntervalSeconds = 60;

bool OpenFunscripter::imguiSetup() noexcept
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigViewportsNoDecoration = false;
    io.ConfigViewportsNoAutoMerge = false;
    io.ConfigViewportsNoTaskBarIcon = false;
    io.ConfigDockingTransparentPayload = true;

    static auto imguiIniPath = Util::Prefpath("imgui.ini");
    io.IniFilename = imguiIniPath.c_str();

    // NOTE: OFS_Preferences::OFS_Preferences() sets OFS_DynFontAtlas::FontOverride
    OFS_DynFontAtlas::Init();
    OFS_Translator::Init();
    auto& prefState = PreferenceState::State(preferences->StateHandle());
    if (!prefState.languageCsv.empty()) {
        if (OFS_Translator::ptr->LoadTranslation(prefState.languageCsv.c_str())) {
            OFS_DynFontAtlas::AddTranslationText();
        }
    }


    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    LOGF_DEBUG("init imgui with glsl: %s", GlslVersion);
    ImGui_ImplOpenGL3_Init(GlslVersion);

    // hook into paste for the dynamic atlas
    if (io.GetClipboardTextFn) {
        static auto OriginalSDL2_GetClipboardFunc = io.GetClipboardTextFn;
        io.GetClipboardTextFn = [](void* d) noexcept -> const char* {
            auto clipboard = OriginalSDL2_GetClipboardFunc(d);
            OFS_DynFontAtlas::AddText(clipboard);
            return clipboard;
        };
    }

    return true;
}

static void SaveState() noexcept
{
    auto stateJson = OFS_StateManager::Get()->SerializeAppAll(true);
    auto stateBin = Util::SerializeCBOR(stateJson);
    auto statePath = Util::Prefpath("state.ofs");
    Util::WriteFile(statePath.c_str(), stateBin.data(), stateBin.size());
}

OpenFunscripter::~OpenFunscripter() noexcept
{
    // needs a certain destruction order
    scripting.reset();
    controllerInput.reset();
    specialFunctions.reset();
    LoadedProject.reset();
    playerWindow.reset();
}

bool OpenFunscripter::Init(int argc, char* argv[])
{
    OFS_FileLogger::Init();
    Util::InMainThread();
    Util::InitRandom();
    
    FUN_ASSERT(!ptr, "there can only be one instance");
    ptr = this;

    auto prefPath = Util::Prefpath("");
    Util::CreateDirectories(prefPath);

    OFS_StateManager::Init();
    {
        auto stateMgr = OFS_StateManager::Get();
        std::vector<uint8_t> fileData;
        auto statePath = Util::Prefpath("state.ofs");
        if (Util::ReadFile(statePath.c_str(), fileData) > 0) {
            bool succ;
            auto cbor = Util::ParseCBOR(fileData, &succ);
            if (succ) {
                stateMgr->DeserializeAppAll(cbor, true);
            }
        }
    }

    stateHandle = OFS_AppState<OpenFunscripterState>::Register(OpenFunscripterState::StateName);
    const auto& ofsState = OpenFunscripterState::State(stateHandle);

    preferences = std::make_unique<OFS_Preferences>();
    const auto& prefState = PreferenceState::State(preferences->StateHandle());

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_ERROR(SDL_GetError());
        return false;
    }
    if (!OFS_MpvLoader::Load()) {
        LOG_ERROR("Failed to load mpv library.");
        return false;
    }

#if __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac according to imgui example
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0 /*| SDL_GL_CONTEXT_DEBUG_FLAG*/);
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    // antialiasing
    // this caused problems in my linux testing
#ifdef WIN32
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window = SDL_CreateWindow(
        "OpenFunscripter " OFS_LATEST_GIT_TAG "@" OFS_LATEST_GIT_HASH,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DefaultWidth, DefaultHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);

    SDL_Rect display;
    int windowDisplay = SDL_GetWindowDisplayIndex(window);
    SDL_GetDisplayBounds(windowDisplay, &display);
    if (DefaultWidth >= display.w || DefaultHeight >= display.h) {
        SDL_MaximizeWindow(window);
    }

    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(prefState.vsync);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        LOG_ERROR("Failed to load glad.");
        return false;
    }

    if (!imguiSetup()) {
        LOG_ERROR("Failed to setup ImGui");
        return false;
    }

    preferences->SetTheme(static_cast<OFS_Theme>(prefState.currentTheme));

    EV::Init();
    LoadedProject = std::make_unique<OFS_Project>();

    player = std::make_unique<OFS_Videoplayer>(VideoplayerType::Main);
    if (!player->Init(prefState.forceHwDecoding)) {
        LOG_ERROR("Failed to initialize videoplayer.");
        return false;
    }
    player->SetPaused(true);

    playerWindow = std::make_unique<OFS_VideoplayerWindow>();
    if (!playerWindow->Init(player.get())) {
        LOG_ERROR("Failed to init videoplayer window");
        return false;
    }

    playerControls.Init(player.get(), prefState.forceHwDecoding);
    undoSystem = std::make_unique<UndoSystem>();

    keys = std::make_unique<OFS_KeybindingSystem>();
    registerBindings();

    scriptTimeline.Init();

    scripting = std::make_unique<ScriptingMode>();
    scripting->Init();

    EV::Queue().appendListener(FunscriptActionsChangedEvent::EventType,
        FunscriptActionsChangedEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptChanged)));
    EV::Queue().appendListener(SDL_DROPFILE,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::DragNDrop)));
    EV::Queue().appendListener(SDL_CONTROLLERAXISMOTION,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ControllerAxisPlaybackSpeed)));
    EV::Queue().appendListener(VideoLoadedEvent::EventType,
        VideoLoadedEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::VideoLoaded)));
    EV::Queue().appendListener(DurationChangeEvent::EventType,
        DurationChangeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::VideoDuration)));
    EV::Queue().appendListener(PlayPauseChangeEvent::EventType,
        PlayPauseChangeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::PlayPauseChange)));
    EV::Queue().appendListener(FunscriptActionShouldMoveEvent::EventType,
        FunscriptActionShouldMoveEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActionMoved)));
    EV::Queue().appendListener(FunscriptActionClickedEvent::EventType,
        FunscriptActionClickedEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActionClicked)));
    EV::Queue().appendListener(FunscriptActionShouldCreateEvent::EventType,
        FunscriptActionShouldCreateEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActionCreated)));
    EV::Queue().appendListener(ShouldSetTimeEvent::EventType,
        ShouldSetTimeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineDoubleClick)));
    EV::Queue().appendListener(FunscriptShouldSelectTimeEvent::EventType,
        FunscriptShouldSelectTimeEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineSelectTime)));
    EV::Queue().appendListener(ShouldChangeActiveScriptEvent::EventType,
        ShouldChangeActiveScriptEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActiveScriptChanged)));
    EV::Queue().appendListener(ExportClipForChapter::EventType,
        ExportClipForChapter::HandleEvent(EVENT_SYSTEM_BIND(this, &OpenFunscripter::ExportClip)));

    specialFunctions = std::make_unique<SpecialFunctionsWindow>();
    controllerInput = std::make_unique<ControllerInput>();
    controllerInput->Init();
    simulator.Init();

    FunscriptHeatmap::Init();
    extensions = std::make_unique<OFS_LuaExtensions>();
    extensions->Init();
    metadataEditor = std::make_unique<OFS_FunscriptMetadataEditor>();

    webApi = std::make_unique<OFS_WebsocketApi>();
    webApi->Init();

    chapterMgr = std::make_unique<OFS_ChapterManager>();
#ifdef WIN32
    OFS_DownloadFfmpeg::FfmpegMissing = !Util::FileExists(Util::FfmpegPath().u8string());
#endif

    closeProject(true);
    if (argc > 1) {
        const char* path = argv[1];
        openFile(path);
    }
    else if (!ofsState.recentFiles.empty()) {
        auto& project = ofsState.recentFiles.back().projectPath;
        if (!project.empty()) {
            openFile(project);
        }
    }

    // Load potentially missing glyphs of recent files
    for (auto& recentFile : ofsState.recentFiles) {
        OFS_DynFontAtlas::AddText(recentFile.name.c_str());
    }

    SDL_ShowWindow(window);
    return true;
}

void OpenFunscripter::setupDefaultLayout(bool force) noexcept
{
    MainDockspaceID = ImGui::GetID("MainAppDockspace");
    OFS_DownloadFfmpeg::ModalId = ImGui::GetID(OFS_DownloadFfmpeg::WindowId);

    auto imgui_ini = ImGui::GetIO().IniFilename;
    bool imgui_ini_found = Util::FileExists(imgui_ini);
    if (force || !imgui_ini_found) {
        if (!imgui_ini_found) {
            LOG_INFO("imgui.ini was not found...");
            LOG_INFO("Setting default layout.");
        }

        ImGui::ClearIniSettings();

        ImGui::DockBuilderRemoveNode(MainDockspaceID); // Clear out existing layout
        ImGui::DockBuilderAddNode(MainDockspaceID, ImGuiDockNodeFlags_DockSpace); // Add empty node
        ImGui::DockBuilderSetNodeSize(MainDockspaceID, ImVec2(DefaultWidth, DefaultHeight));

        ImGuiID dock_player_center_id;
        ImGuiID opposite_node_id;
        auto dock_time_bottom_id = ImGui::DockBuilderSplitNode(MainDockspaceID, ImGuiDir_Down, 0.1f, NULL, &dock_player_center_id);
        auto dock_positions_id = ImGui::DockBuilderSplitNode(dock_player_center_id, ImGuiDir_Down, 0.15f, NULL, &dock_player_center_id);
        auto dock_mode_right_id = ImGui::DockBuilderSplitNode(dock_player_center_id, ImGuiDir_Right, 0.15f, NULL, &dock_player_center_id);
        auto dock_simulator_right_id = ImGui::DockBuilderSplitNode(dock_mode_right_id, ImGuiDir_Down, 0.15f, NULL, &dock_mode_right_id);
        auto dock_action_right_id = ImGui::DockBuilderSplitNode(dock_mode_right_id, ImGuiDir_Down, 0.38f, NULL, &dock_mode_right_id);
        auto dock_stats_right_id = ImGui::DockBuilderSplitNode(dock_mode_right_id, ImGuiDir_Down, 0.38f, NULL, &dock_mode_right_id);
        auto dock_undo_right_id = ImGui::DockBuilderSplitNode(dock_mode_right_id, ImGuiDir_Down, 0.5f, NULL, &dock_mode_right_id);

        auto dock_player_control_id = ImGui::DockBuilderSplitNode(dock_time_bottom_id, ImGuiDir_Left, 0.15f, &dock_time_bottom_id, &dock_time_bottom_id);

        ImGui::DockBuilderGetNode(dock_player_center_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::DockBuilderGetNode(dock_positions_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::DockBuilderGetNode(dock_time_bottom_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::DockBuilderGetNode(dock_player_control_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;

        ImGui::DockBuilderDockWindow(OFS_VideoplayerWindow::WindowId, dock_player_center_id);
        ImGui::DockBuilderDockWindow(OFS_VideoplayerControls::TimeId, dock_time_bottom_id);
        ImGui::DockBuilderDockWindow(OFS_VideoplayerControls::ControlId, dock_player_control_id);
        ImGui::DockBuilderDockWindow(ScriptTimeline::WindowId, dock_positions_id);
        ImGui::DockBuilderDockWindow(ScriptingMode::WindowId, dock_mode_right_id);
        ImGui::DockBuilderDockWindow(ScriptSimulator::WindowId, dock_simulator_right_id);
        ImGui::DockBuilderDockWindow(ActionEditorWindowId, dock_action_right_id);
        ImGui::DockBuilderDockWindow(StatisticsWindowId, dock_stats_right_id);
        ImGui::DockBuilderDockWindow(UndoSystem::WindowId, dock_undo_right_id);
        simulator.CenterSimulator();
        ImGui::DockBuilderFinish(MainDockspaceID);
    }
}

void OpenFunscripter::registerBindings()
{
    keys->RegisterGroup("Actions", Tr::ACTIONS_BINDING_GROUP);
    {
        // DELETE ACTION
        keys->RegisterAction(
            { "remove_action",
                [this]() { removeAction(); } },
            Tr::ACTION_REMOVE_ACTION, "Actions",
            { { ImGuiMod_None, ImGuiKey_Delete },
                { ImGuiMod_None, ImGuiKey_GamepadFaceRight } });
        // ADD ACTIONS
        keys->RegisterAction(
            { "action_0",
                [this]() { addEditAction(0); } },
            Tr::ACTION_ACTION_0, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad0 },
            });
        keys->RegisterAction(
            { "action_10",
                [this]() { addEditAction(10); } },
            Tr::ACTION_ACTION_10, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad1 },
            });
        keys->RegisterAction(
            { "action_20",
                [this]() { addEditAction(20); } },
            Tr::ACTION_ACTION_20, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad2 },
            });
        keys->RegisterAction(
            { "action_30",
                [this]() { addEditAction(30); } },
            Tr::ACTION_ACTION_30, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad3 },
            });
        keys->RegisterAction(
            { "action_40",
                [this]() { addEditAction(40); } },
            Tr::ACTION_ACTION_40, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad4 },
            });
        keys->RegisterAction(
            { "action_50",
                [this]() { addEditAction(50); } },
            Tr::ACTION_ACTION_50, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad5 },
            });
        keys->RegisterAction(
            { "action_60",
                [this]() { addEditAction(60); } },
            Tr::ACTION_ACTION_60, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad6 },
            });
        keys->RegisterAction(
            { "action_70",
                [this]() { addEditAction(70); } },
            Tr::ACTION_ACTION_70, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad7 },
            });
        keys->RegisterAction(
            { "action_80",
                [this]() { addEditAction(80); } },
            Tr::ACTION_ACTION_80, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad8 },
            });
        keys->RegisterAction(
            { "action_90",
                [this]() { addEditAction(90); } },
            Tr::ACTION_ACTION_90, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_Keypad9 },
            });
        keys->RegisterAction(
            { "action_100",
                [this]() { addEditAction(100); } },
            Tr::ACTION_ACTION_100, "Actions",
            {
                { ImGuiMod_None, ImGuiKey_KeypadDivide },
            });
    }

    keys->RegisterGroup("Core", Tr::CORE_BINDING_GROUP);
    {
        // SAVE
        keys->RegisterAction(
            { "save_project",
                [this]() { saveProject(); } },
            Tr::ACTION_SAVE_PROJECT, "Core",
            {
                { ImGuiMod_Ctrl, ImGuiKey_S },
            });

        keys->RegisterAction(
            { "quick_export",
                [this]() { quickExport(); } },
            Tr::ACTION_QUICK_EXPORT, "Core",
            {
                { ImGuiMod_Ctrl | ImGuiMod_Shift, ImGuiKey_S },
            });

        keys->RegisterAction(
            { "sync_timestamps",
                [this]() { player->SyncWithPlayerTime(); } },
            Tr::ACTION_SYNC_TIME_WITH_PLAYER, "Core",
            {
                { ImGuiMod_None, ImGuiKey_S },
            });

        keys->RegisterAction(
            { "cycle_loaded_forward_scripts",
                [this]() { 
                    auto activeIdx = LoadedProject->ActiveIdx();
                    do {
                        activeIdx++;
                        activeIdx %= LoadedFunscripts().size();
                    } while (!LoadedFunscripts()[activeIdx]->Enabled);
                    UpdateNewActiveScript(activeIdx); } },
            Tr::ACTION_CYCLE_FORWARD_LOADED_SCRIPTS, "Core",
            {
                { ImGuiMod_None, ImGuiKey_PageDown },
            });

        keys->RegisterAction(
            { "cycle_loaded_backward_scripts",
                [this]() {
                    auto activeIdx = LoadedProject->ActiveIdx();
                    do {
                        activeIdx--;
                        activeIdx %= LoadedFunscripts().size();
                    } while (!LoadedFunscripts()[activeIdx]->Enabled);
                    UpdateNewActiveScript(activeIdx);
                } },
            Tr::ACTION_CYCLE_BACKWARD_LOADED_SCRIPTS, "Core",
            {
                { ImGuiMod_None, ImGuiKey_PageUp },
            });

        keys->RegisterAction(
            { "reload_translation_csv",
                [this]() {
                    const auto& prefState = PreferenceState::State(preferences->StateHandle());
                    if (!prefState.languageCsv.empty()) {
                        if (OFS_Translator::ptr->LoadTranslation(prefState.languageCsv.c_str())) {
                            OFS_DynFontAtlas::AddTranslationText();
                        }
                    }
                } },
            Tr::ACTION_RELOAD_TRANSLATION, "Core");
    }

    keys->RegisterGroup("Navigation", Tr::NAVIGATION_BINDING_GROUP);
    {
        // JUMP BETWEEN ACTIONS
        keys->RegisterAction(
            { "prev_action",
                [this]() {
                    auto action = ActiveFunscript()->GetPreviousActionBehind(player->CurrentTime() - 0.001f);
                    if (action != nullptr) player->SetPositionExact(action->atS);
                },
                false },
            Tr::ACTION_PREVIOUS_ACTION, "Navigation",
            { { ImGuiKey_None, ImGuiKey_DownArrow, true },
                { ImGuiKey_None, ImGuiKey_GamepadDpadDown, true } });

        keys->RegisterAction(
            { "next_action",
                [this]() {
                    auto action = ActiveFunscript()->GetNextActionAhead(player->CurrentTime() + 0.001f);
                    if (action != nullptr) player->SetPositionExact(action->atS);
                },
                false },
            Tr::ACTION_NEXT_ACTION, "Navigation",
            { { ImGuiKey_None, ImGuiKey_UpArrow, true },
                { ImGuiKey_None, ImGuiKey_GamepadDpadUp, true } });

        keys->RegisterAction(
            { "prev_action_multi",
                [this]() {
                    bool foundAction = false;
                    float closestTime = std::numeric_limits<float>::max();
                    float currentTime = player->CurrentTime();

                    for (int i = 0; i < LoadedFunscripts().size(); i++) {
                        auto& script = LoadedFunscripts()[i];
                        auto action = script->GetPreviousActionBehind(currentTime - 0.001f);
                        if (action != nullptr) {
                            if (std::abs(currentTime - action->atS) < std::abs(currentTime - closestTime)) {
                                foundAction = true;
                                closestTime = action->atS;
                            }
                        }
                    }
                    if (foundAction) {
                        player->SetPositionExact(closestTime);
                    }
                },
                false },
            Tr::ACTION_PREVIOUS_ACTION_MULTI, "Navigation",
            {
                { ImGuiMod_Ctrl, ImGuiKey_DownArrow, true },
            });

        keys->RegisterAction(
            { "next_action_multi",
                [this]() {
                    bool foundAction = false;
                    float closestTime = std::numeric_limits<float>::max();
                    float currentTime = player->CurrentTime();
                    for (int i = 0; i < LoadedFunscripts().size(); i++) {
                        auto& script = LoadedFunscripts()[i];
                        auto action = script->GetNextActionAhead(currentTime + 0.001f);
                        if (action != nullptr) {
                            if (std::abs(currentTime - action->atS) < std::abs(currentTime - closestTime)) {
                                foundAction = true;
                                closestTime = action->atS;
                            }
                        }
                    }
                    if (foundAction) {
                        player->SetPositionExact(closestTime);
                    }
                },
                false },
            Tr::ACTION_NEXT_ACTION_MULTI, "Navigation",
            {
                { ImGuiMod_Ctrl, ImGuiKey_UpArrow, true },
            });

        // FRAME CONTROL
        keys->RegisterAction(
            { "prev_frame",
                [this]() {
                    if (player->IsPaused()) {
                        scripting->PreviousFrame();
                    }
                },
                false },
            Tr::ACTION_PREV_FRAME, "Navigation",
            {
                { ImGuiMod_None, ImGuiKey_LeftArrow, true },
                { ImGuiMod_None, ImGuiKey_GamepadDpadLeft, true },
            });

        keys->RegisterAction(
            { "next_frame",
                [this]() {
                    if (player->IsPaused()) {
                        scripting->NextFrame();
                    }
                },
                false },
            Tr::ACTION_NEXT_FRAME, "Navigation",
            {
                { ImGuiMod_None, ImGuiKey_RightArrow, true },
                { ImGuiMod_None, ImGuiKey_GamepadDpadRight, true },
            });

        keys->RegisterAction(
            { "fast_step",
                [this]() {
                    const auto& prefState = PreferenceState::State(preferences->StateHandle());
                    player->SeekFrames(prefState.fastStepAmount);
                },
                false },
            Tr::ACTION_FAST_STEP, "Navigation",
            { { ImGuiMod_Ctrl, ImGuiKey_RightArrow, true } });

        keys->RegisterAction(
            { "fast_backstep",
                [this]() {
                    const auto& prefState = PreferenceState::State(preferences->StateHandle());
                    player->SeekFrames(-prefState.fastStepAmount);
                },
                false },
            Tr::ACTION_FAST_BACKSTEP, "Navigation",
            { { ImGuiMod_Ctrl, ImGuiKey_LeftArrow, true } });
    }

    keys->RegisterGroup("Utility", Tr::UTILITY_BINDING_GROUP);
    {
        // UNDO / REDO
        keys->RegisterAction(
            { "undo",
                [this]() {
                    Undo();
                },
                false },
            Tr::ACTION_UNDO, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_Z, true } });

        keys->RegisterAction(
            { "redo",
                [this]() {
                    Redo();
                },
                false },
            Tr::ACTION_REDO, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_Y, true } });

        // COPY / PASTE
        keys->RegisterAction(
            { "copy",
                [this]() {
                    copySelection();
                },
                false },
            Tr::ACTION_COPY, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_C } });

        keys->RegisterAction(
            { "paste",
                [this]() {
                    pasteSelection();
                },
                false },
            Tr::ACTION_PASTE, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_V } });

        keys->RegisterAction(
            { "paste_exact",
                [this]() {
                    pasteSelectionExact();
                },
                false },
            Tr::ACTION_PASTE_EXACT, "Utility",
            { { ImGuiMod_Ctrl | ImGuiMod_Shift, ImGuiKey_V } });

        keys->RegisterAction(
            { "cut",
                [this]() {
                    cutSelection();
                },
                false },
            Tr::ACTION_CUT, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_X } });

        keys->RegisterAction(
            { "select_all",
                [this]() {
                    ActiveFunscript()->SelectAll();
                },
                false },
            Tr::ACTION_SELECT_ALL, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_A } });

        keys->RegisterAction(
            { "deselect_all",
                [this]() {
                    ActiveFunscript()->ClearSelection();
                },
                false },
            Tr::ACTION_DESELECT_ALL, "Utility",
            { { ImGuiMod_Ctrl, ImGuiKey_D } });

        keys->RegisterAction(
            { "select_all_left",
                [this]() {
                    ActiveFunscript()->SelectTime(0, player->CurrentTime());
                },
                false },
            Tr::ACTION_SELECT_ALL_LEFT, "Utility",
            { { ImGuiMod_Ctrl | ImGuiMod_Alt, ImGuiKey_LeftArrow } });

        keys->RegisterAction(
            { "select_all_right",
                [this]() {
                    ActiveFunscript()->SelectTime(player->CurrentTime(), player->Duration());
                },
                false },
            Tr::ACTION_SELECT_ALL_RIGHT, "Utility",
            { { ImGuiMod_Ctrl | ImGuiMod_Alt, ImGuiKey_RightArrow } });

        keys->RegisterAction(
            { "select_top_points",
                [this]() {
                    selectTopPoints();
                },
                false },
            Tr::ACTION_SELECT_TOP, "Utility");

        keys->RegisterAction(
            { "select_middle_points",
                [this]() {
                    selectMiddlePoints();
                },
                false },
            Tr::ACTION_SELECT_MID, "Utility");

        keys->RegisterAction(
            { "select_bottom_points",
                [this]() {
                    selectBottomPoints();
                },
                false },
            Tr::ACTION_SELECT_BOTTOM, "Utility");

        // SCREENSHOT VIDEO
        keys->RegisterAction(
            { "save_frame_as_image",
                [this]() {
                    auto screenshotDir = Util::Prefpath("screenshot");
                    player->SaveFrameToImage(screenshotDir);
                },
                false },
            Tr::ACTION_SAVE_FRAME, "Utility",
            { { ImGuiMod_None, ImGuiKey_F2 } });

        // CHANGE SUBTITLES
        keys->RegisterAction(
            { "cycle_subtitles",
                [this]() {
                    player->CycleSubtitles();
                },
                false },
            Tr::ACTION_CYCLE_SUBTITLES, "Utility",
            { { ImGuiMod_None, ImGuiKey_J } });

        // FULLSCREEN
        keys->RegisterAction(
            { "fullscreen_toggle",
                [this]() {
                    Status ^= OFS_Status::OFS_Fullscreen;
                    SetFullscreen(Status & OFS_Status::OFS_Fullscreen);
                },
                false },
            Tr::ACTION_TOGGLE_FULLSCREEN, "Utility",
            { { ImGuiMod_None, ImGuiKey_F10 } });
    }

    // MOVE LEFT/RIGHT
    auto move_actions_horizontal = [](bool forward) {
        auto app = OpenFunscripter::ptr;

        if (app->ActiveFunscript()->HasSelection()) {

            auto time = forward
                ? app->scripting->SteppingIntervalForward(app->ActiveFunscript()->Selection().front().atS)
                : app->scripting->SteppingIntervalBackward(app->ActiveFunscript()->Selection().front().atS);

            app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
            app->ActiveFunscript()->MoveSelectionTime(time, app->scripting->LogicalFrameTime());
        }
        else {
            auto closest = ptr->ActiveFunscript()->GetClosestAction(app->player->CurrentTime());
            if (closest != nullptr) {
                auto time = forward
                    ? app->scripting->SteppingIntervalForward(closest->atS)
                    : app->scripting->SteppingIntervalBackward(closest->atS);

                FunscriptAction moved(closest->atS + time, closest->pos);
                auto closestInMoveRange = app->ActiveFunscript()->GetActionAtTime(moved.atS, app->scripting->LogicalFrameTime());
                if (closestInMoveRange == nullptr
                    || (forward && closestInMoveRange->atS < moved.atS)
                    || (!forward && closestInMoveRange->atS > moved.atS)) {
                    app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
                    app->ActiveFunscript()->EditAction(*closest, moved);
                }
            }
        }
    };
    auto move_actions_horizontal_with_video = [](bool forward) {
        auto app = OpenFunscripter::ptr;
        if (app->ActiveFunscript()->HasSelection()) {
            auto time = forward
                ? app->scripting->SteppingIntervalForward(app->ActiveFunscript()->Selection().front().atS)
                : app->scripting->SteppingIntervalBackward(app->ActiveFunscript()->Selection().front().atS);

            app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
            app->ActiveFunscript()->MoveSelectionTime(time, app->scripting->LogicalFrameTime());
            auto closest = ptr->ActiveFunscript()->GetClosestActionSelection(app->player->CurrentTime());
            if (closest != nullptr) {
                app->player->SetPositionExact(closest->atS);
            }
            else {
                app->player->SetPositionExact(app->ActiveFunscript()->Selection().front().atS);
            }
        }
        else {
            auto closest = app->ActiveFunscript()->GetClosestAction(ptr->player->CurrentTime());
            if (closest != nullptr) {
                auto time = forward
                    ? app->scripting->SteppingIntervalForward(closest->atS)
                    : app->scripting->SteppingIntervalBackward(closest->atS);

                FunscriptAction moved(closest->atS + time, closest->pos);
                auto closestInMoveRange = app->ActiveFunscript()->GetActionAtTime(moved.atS, app->scripting->LogicalFrameTime());

                if (closestInMoveRange == nullptr
                    || (forward && closestInMoveRange->atS < moved.atS)
                    || (!forward && closestInMoveRange->atS > moved.atS)) {
                    app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
                    app->ActiveFunscript()->EditAction(*closest, moved);
                    app->player->SetPositionExact(moved.atS);
                }
            }
        }
    };

    keys->RegisterGroup("Moving", Tr::MOVING_BINDING_GROUP);
    {
        keys->RegisterAction(
            { "move_actions_up_ten",
                [this]() {
                    if (ActiveFunscript()->HasSelection()) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->MoveSelectionPosition(10);
                    }
                    else {
                        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                        if (closest != nullptr) {
                            undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                            ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos + 10, 0, 100)));
                        }
                    }
                },
                false },
            Tr::ACTION_MOVE_UP_10, "Moving");

        keys->RegisterAction(
            { "move_actions_down_ten",
                [this]() {
                    if (ActiveFunscript()->HasSelection()) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->MoveSelectionPosition(-10);
                    }
                    else {
                        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                        if (closest != nullptr) {
                            undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                            ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos - 10, 0, 100)));
                        }
                    }
                },
                false },
            Tr::ACTION_MOVE_DOWN_10, "Moving");

        keys->RegisterAction(
            { "move_actions_up_five",
                [this]() {
                    if (ActiveFunscript()->HasSelection()) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->MoveSelectionPosition(5);
                    }
                    else {
                        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                        if (closest != nullptr) {
                            undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                            ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos + 5, 0, 100)));
                        }
                    }
                },
                false },
            Tr::ACTION_MOVE_UP_5, "Moving");

        keys->RegisterAction(
            { "move_actions_down_five",
                [this]() {
                    if (ActiveFunscript()->HasSelection()) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->MoveSelectionPosition(-5);
                    }
                    else {
                        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                        if (closest != nullptr) {
                            undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                            ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos - 5, 0, 100)));
                        }
                    }
                },
                false },
            Tr::ACTION_MOVE_DOWN_5, "Moving");

        keys->RegisterAction(
            { "move_actions_left_snapped",
                [move_actions_horizontal_with_video]() {
                    move_actions_horizontal_with_video(false);
                },
                false },
            Tr::ACTION_MOVE_ACTIONS_LEFT_SNAP, "Moving",
            { { ImGuiMod_Ctrl | ImGuiMod_Shift, ImGuiKey_LeftArrow, true } });

        keys->RegisterAction(
            { "move_actions_right_snapped",
                [move_actions_horizontal_with_video]() {
                    move_actions_horizontal_with_video(true);
                },
                false },
            Tr::ACTION_MOVE_ACTIONS_RIGHT_SNAP, "Moving",
            { { ImGuiMod_Ctrl | ImGuiMod_Shift, ImGuiKey_RightArrow, true } });

        keys->RegisterAction(
            { "move_actions_left",
                [move_actions_horizontal]() {
                    move_actions_horizontal(false);
                },
                false },
            Tr::ACTION_MOVE_ACTIONS_LEFT, "Moving",
            { { ImGuiMod_Shift, ImGuiKey_LeftArrow, true } });

        keys->RegisterAction(
            { "move_actions_right",
                [move_actions_horizontal]() {
                    move_actions_horizontal(true);
                },
                false },
            Tr::ACTION_MOVE_ACTIONS_RIGHT, "Moving",
            { { ImGuiMod_Shift, ImGuiKey_RightArrow, true } });

        // MOVE SELECTION UP/DOWN
        keys->RegisterAction(
            { "move_actions_up",
                [this]() {
                    if (ActiveFunscript()->HasSelection()) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->MoveSelectionPosition(1);
                    }
                    else {
                        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                        if (closest != nullptr) {
                            FunscriptAction moved(closest->atS, closest->pos + 1);
                            if (moved.pos <= 100 && moved.pos >= 0) {
                                undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                                ActiveFunscript()->EditAction(*closest, moved);
                            }
                        }
                    }
                },
                false },
            Tr::ACTION_MOVE_ACTIONS_UP, "Moving",
            { { ImGuiMod_Shift, ImGuiKey_UpArrow, true } });

        keys->RegisterAction(
            { "move_actions_down",
                [this]() {
                    if (ActiveFunscript()->HasSelection()) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->MoveSelectionPosition(-1);
                    }
                    else {
                        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                        if (closest != nullptr) {
                            FunscriptAction moved(closest->atS, closest->pos - 1);
                            if (moved.pos <= 100 && moved.pos >= 0) {
                                undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                                ActiveFunscript()->EditAction(*closest, moved);
                            }
                        }
                    }
                },
                false },
            Tr::ACTION_MOVE_ACTIONS_DOWN, "Moving",
            { { ImGuiMod_Shift, ImGuiKey_DownArrow, true } });

        keys->RegisterAction(
            { "move_action_to_current_pos",
                [this]() {
                    auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::MOVE_ACTION_TO_CURRENT_POS, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(player->CurrentTime(), closest->pos));
                    }
                },
                false },
            Tr::ACTION_MOVE_TO_CURRENT_POSITION, "Moving",
            { { ImGuiMod_None, ImGuiKey_End } });
    }
    // FUNCTIONS
    keys->RegisterGroup("Special", Tr::SPECIAL_BINDING_GROUP);
    {
        keys->RegisterAction(
            { "equalize_actions",
                [this]() {
                    equalizeSelection();
                },
                false },
            Tr::ACTION_EQUALIZE_ACTIONS, "Special",
            { { ImGuiMod_None, ImGuiKey_E } });

        keys->RegisterAction(
            { "invert_actions",
                [this]() {
                    invertSelection();
                },
                false },
            Tr::ACTION_INVERT_ACTIONS, "Special",
            { { ImGuiMod_None, ImGuiKey_I } });

        keys->RegisterAction(
            { "isolate_action",
                [this]() {
                    isolateAction();
                },
                false },
            Tr::ACTION_ISOLATE_ACTION, "Special",
            { { ImGuiMod_None, ImGuiKey_R } });

        keys->RegisterAction(
            { "repeat_stroke",
                [this]() {
                    repeatLastStroke();
                },
                false },
            Tr::ACTION_REPEAT_STROKE, "Special",
            { { ImGuiMod_None, ImGuiKey_Home } });
    }

    // VIDEO CONTROL
    keys->RegisterGroup("Videoplayer", Tr::VIDEOPLAYER_BINDING_GROUP);
    {
        keys->RegisterAction(
            { "toggle_play",
                [this]() { player->TogglePlay(); },
                false },
            Tr::ACTION_TOGGLE_PLAY, "Videoplayer",
            { { ImGuiKey_None, ImGuiKey_Space },
                { ImGuiKey_None, ImGuiKey_GamepadStart } });

        // PLAYBACK SPEED
        keys->RegisterAction(
            { "decrement_speed",
                [this]() { player->AddSpeed(-0.10); },
                false },
            Tr::ACTION_REDUCE_PLAYBACK_SPEED, "Videoplayer",
            {
                { ImGuiKey_None, ImGuiKey_KeypadSubtract },
            });

        keys->RegisterAction(
            { "increment_speed",
                [this]() { player->AddSpeed(0.10); },
                false },
            Tr::ACTION_INCREASE_PLAYBACK_SPEED, "Videoplayer",
            {
                { ImGuiKey_None, ImGuiKey_KeypadAdd },
            });

        keys->RegisterAction(
            { "goto_start",
                [this]() { player->SetPositionPercent(0.f, false); },
                false },
            Tr::ACTION_GO_TO_START, "Videoplayer");

        keys->RegisterAction(
            { "goto_end",
                [this]() { player->SetPositionPercent(1.f, false); },
                false },
            Tr::ACTION_GO_TO_END, "Videoplayer");
    }

    keys->RegisterGroup("Extensions", Tr::EXTENSIONS_BINDING_GROUP);
    {
        keys->RegisterAction(
            { "reload_enabled_extensions",
                [this]() { extensions->ReloadEnabledExtensions(); },
                false },
            Tr::ACTION_RELOAD_ENABLED_EXTENSIONS, "Extensions");
    }

    keys->RegisterGroup("Controller", Tr::CONTROLLER_BINDING_GROUP);
    {
        keys->RegisterAction(
            { "toggle_controller_navmode",
                [this]() {
                    auto& io = ImGui::GetIO();
                    io.ConfigFlags ^= ImGuiConfigFlags_NavEnableGamepad;
                },
                false },
            Tr::ACTION_TOGGLE_CONTROLLER_NAV, "Controller",
            { { ImGuiMod_None, ImGuiKey_GamepadL3 } });

        keys->RegisterAction(
            { "seek_forward_second",
                [this]() {
                    player->SeekRelative(1);
                },
                false },
            Tr::ACTION_SEEK_FORWARD_1, "Controller",
            { { ImGuiMod_None, ImGuiKey_GamepadR1 } });

        keys->RegisterAction(
            { "seek_backward_second",
                [this]() {
                    player->SeekRelative(-1);
                },
                false },
            Tr::ACTION_SEEK_BACKWARD_1, "Controller",
            { { ImGuiMod_None, ImGuiKey_GamepadL1 } });

        keys->RegisterAction(
            { "add_action_controller",
                [this]() {
                    addEditAction(100);
                },
                false },
            Tr::ACTION_ADD_ACTION_CONTROLLER, "Controller",
            { { ImGuiMod_None, ImGuiKey_GamepadFaceDown } });

        keys->RegisterAction(
            { "toggle_recording_mode",
                [this]() {
                    static ScriptingModeEnum prevMode = ScriptingModeEnum::RECORDING;
                    if (scripting->ActiveMode() != ScriptingModeEnum::RECORDING) {
                        prevMode = scripting->ActiveMode();
                        scripting->SetMode(ScriptingModeEnum::RECORDING);
                        ScriptingModeBase* mode = scripting->Mode().get();
                        static_cast<RecordingMode*>(mode)->setRecordingMode(RecordingMode::RecordingType::Controller);
                    }
                    else {
                        scripting->SetMode(prevMode);
                    }
                },
                false },
            Tr::ACTION_TOGGLE_RECORDING_MODE, "Controller");

        keys->RegisterAction(
            { "set_selection_controller",
                [this]() {
                    if (scriptTimeline.selectionStart() < 0) {
                        scriptTimeline.setStartSelection(player->CurrentTime());
                    }
                    else {
                        auto tmp = player->CurrentTime();
                        auto [min, max] = std::minmax<float>(scriptTimeline.selectionStart(), tmp);
                        ActiveFunscript()->SelectTime(min, max);
                        scriptTimeline.setStartSelection(-1);
                    }
                },
                false },
            Tr::ACTION_CONTROLLER_SELECT, "Controller",
            { { ImGuiMod_None, ImGuiKey_GamepadR3 } });

        keys->RegisterAction(
            { "set_current_playbackspeed_controller",
                [this]() {
                    Status |= OFS_Status::OFS_GamepadSetPlaybackSpeed;
                },
                false },
            Tr::ACTION_SET_PLAYBACK_SPEED, "Controller",
            { { ImGuiMod_None, ImGuiKey_GamepadFaceLeft } });
    }

    // Group where all dynamic actions are placed.
    // Lua functions for example.
    keys->RegisterGroup("Dynamic", Tr::DYNAMIC_BINDING_GROUP);
}


void OpenFunscripter::newFrame() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    if (OFS_DynFontAtlas::NeedsRebuild()) {
        const auto& prefState = PreferenceState::State(preferences->StateHandle());
        OFS_DynFontAtlas::RebuildFont(prefState.defaultFontSize);
    }
    ImGui::NewFrame();
}

void OpenFunscripter::render() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImGui::Render();

    OFS_ImGui::CurrentlyRenderedViewport = ImGui::GetMainViewport();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    OFS_ImGui::CurrentlyRenderedViewport = nullptr;

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        {
            // ImGui::RenderPlatformWindowsDefault();
            // Skip the main viewport (index 0), which is always fully handled by the application!
            ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
            for (int i = 1; i < platform_io.Viewports.Size; i++) {
                ImGuiViewport* viewport = platform_io.Viewports[i];
                if (viewport->Flags & ImGuiViewportFlags_Minimized)
                    continue;
                OFS_ImGui::CurrentlyRenderedViewport = viewport;
                if (platform_io.Platform_RenderWindow) platform_io.Platform_RenderWindow(viewport, nullptr);
                if (platform_io.Renderer_RenderWindow) platform_io.Renderer_RenderWindow(viewport, nullptr);
            }
            OFS_ImGui::CurrentlyRenderedViewport = nullptr;
            for (int i = 1; i < platform_io.Viewports.Size; i++) {
                ImGuiViewport* viewport = platform_io.Viewports[i];
                if (viewport->Flags & ImGuiViewportFlags_Minimized)
                    continue;
                if (platform_io.Platform_SwapBuffers) platform_io.Platform_SwapBuffers(viewport, nullptr);
                if (platform_io.Renderer_SwapBuffers) platform_io.Renderer_SwapBuffers(viewport, nullptr);
            }
        }

        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }
    glFlush();
    glFinish();
}

void OpenFunscripter::processEvents() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto wrappedEvent = EV::MakeTyped<OFS_SDL_Event>();
    auto& event = wrappedEvent->sdl;
    bool IsExiting = false;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type) {
            case SDL_QUIT: {
                if (!IsExiting) {
                    exitApp();
                    IsExiting = true;
                }
                break;
            }
            case SDL_WINDOWEVENT: {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                    if (!IsExiting) {
                        exitApp();
                        IsExiting = true;
                    }
                }
                break;
            }
            case SDL_TEXTINPUT: {
                OFS_DynFontAtlas::AddText(event.text.text);
                break;
            }
        }

        switch (event.type) {
            case SDL_CONTROLLERAXISMOTION:
                if (std::abs(event.caxis.value) < 2000) break;
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:
            case SDL_TEXTINPUT:
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERBUTTONDOWN:
                IdleTimer = SDL_GetTicks();
                setIdle(false);
                break;
        }

        // This is a slight hack in order to avoid creating a bunch of SDL_Event wrapper classes
        OFS_SDL_Event::EventType = event.type;
        EV::Queue().directDispatch(OFS_SDL_Event::EventType, wrappedEvent);
    }
    EV::Process();
}

void OpenFunscripter::ExportClip(const ExportClipForChapter* ev) noexcept
{
    const auto& ofsState = OpenFunscripterState::State(stateHandle);
    Util::OpenDirectoryDialog(TR(CHOOSE_OUTPUT_DIR), ofsState.lastPath,
        [chapter = ev->chapter](auto& result) {
            if (!result.files.empty()) {
                OFS_ChapterManager::ExportClip(chapter, result.files[0]);
            }
        });
}

void OpenFunscripter::FunscriptChanged(const FunscriptActionsChangedEvent* ev) noexcept
{
    // the event passes the address of the Funscript
    // by searching for the funscript with the same address
    // the index can be retrieved
    auto ptr = ev->Script;
    for (int i = 0, size = LoadedFunscripts().size(); i < size; i += 1) {
        if (LoadedFunscripts()[i].get() == ptr) {
            extensions->ScriptChanged(i);
            break;
        }
    }

    Status = Status | OFS_Status::OFS_GradientNeedsUpdate;
}

void OpenFunscripter::ScriptTimelineActionClicked(const FunscriptActionClickedEvent* ev) noexcept
{
    if (SDL_GetModState() & KMOD_CTRL) {
        if (auto script = ev->script.lock()) {
            script->SelectAction(ev->action);
        }
    }
    else {
        player->SetPositionExact(ev->action.atS);
    }
}

void OpenFunscripter::ScriptTimelineActionCreated(const FunscriptActionShouldCreateEvent* ev) noexcept
{
    if (auto script = ev->script.lock()) {
        undoSystem->Snapshot(StateType::ADD_ACTION, script);
        script->AddEditAction(ev->newAction, scripting->LogicalFrameTime());
    }
}

void OpenFunscripter::ScriptTimelineActionMoved(const FunscriptActionShouldMoveEvent* ev) noexcept
{
    if (auto script = ev->script.lock()) {
        if (ev->moveStarted) {
            undoSystem->Snapshot(StateType::ACTIONS_MOVED, script);
        }
        else {
            if (script->SelectionSize() == 1) {
                script->RemoveSelectedActions();
                script->AddAction(ev->action);
                script->SelectAction(ev->action);
            }
        }
    }
}

void OpenFunscripter::DragNDrop(const OFS_SDL_Event* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);

    std::string dragNDropFile = ev->sdl.drop.file;
    closeWithoutSavingDialog([this, dragNDropFile]() {
        openFile(dragNDropFile);
    });
    // NOTE: currently there is just one DragNDrop handler
    // If another one would be added this SDL_free would be problematic
    SDL_free(ev->sdl.drop.file);
}

void OpenFunscripter::VideoDuration(const DurationChangeEvent* ev) noexcept
{
    auto& projectState = LoadedProject->State();
    projectState.metadata.duration = player->Duration();
    player->SetPositionExact(projectState.lastPlayerPosition);
    Status |= OFS_Status::OFS_GradientNeedsUpdate;
}

void OpenFunscripter::VideoLoaded(const VideoLoadedEvent* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
}

void OpenFunscripter::PlayPauseChange(const PlayPauseChangeEvent* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!ev->paused) {
        IdleTimer = SDL_GetTicks();
        setIdle(false);
    }
}

void OpenFunscripter::update() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    const float delta = ImGui::GetIO().DeltaTime;
    keys->ProcessKeybindings();
    extensions->Update(delta);
    player->Update(delta);
    playerControls.videoPreview->Update(delta);
    ControllerInput::UpdateControllers();
    scripting->Update();
    scriptTimeline.Update();

    if (LoadedProject->IsValid()) {
        LoadedProject->Update(delta, IdleMode);
    }

    if (Status & OFS_Status::OFS_AutoBackup) {
        autoBackup();
    }

    webApi->Update();
}

void OpenFunscripter::autoBackup() noexcept
{
    if (!LoadedProject->IsValid()) {
        return;
    }
    std::chrono::duration<float> timeSinceBackup = std::chrono::steady_clock::now() - lastBackup;
    if (timeSinceBackup.count() < AutoBackupIntervalSeconds) {
        return;
    }
    OFS_PROFILE(__FUNCTION__);
    lastBackup = std::chrono::steady_clock::now();

    auto backupDir = Util::PathFromString(Util::Prefpath("backup"));
    auto name = Util::Filename(player->VideoPath());
    name = Util::trim(name); // this needs to be trimmed because trailing spaces

    static auto BackupStartPoint = asap::now();
    name = Util::Format("%s_%02d%02d%02d_%02d%02d%02d",
        name.c_str(), BackupStartPoint.year(),
        BackupStartPoint.month() + 1,
        BackupStartPoint.mday(),
        BackupStartPoint.hour(), BackupStartPoint.minute(), BackupStartPoint.second());

#ifdef WIN32
    backupDir /= Util::Utf8ToUtf16(name);
#else
    backupDir /= name;
#endif
    if (!Util::CreateDirectories(backupDir)) {
        return;
    }

    std::error_code ec;
    auto iterator = std::filesystem::directory_iterator(backupDir, ec);
    for (auto it = std::filesystem::begin(iterator); it != std::filesystem::end(iterator); ++it) {
        if (it->path().has_extension()) {
            if (it->path().extension() == ".backup") {
                LOGF_INFO("Removing \"%s\"", it->path().u8string().c_str());
                std::filesystem::remove(it->path(), ec);
                if (ec) {
                    LOGF_ERROR("%s", ec.message().c_str());
                }
            }
        }
    }

    auto time = asap::now();
    auto fileName = Util::PathFromString(Util::Format("%s_%02d-%02d-%02d" OFS_PROJECT_EXT ".backup", name.c_str(), time.hour(), time.minute(), time.second()));
    auto savePath = backupDir / fileName;
    LOGF_INFO("Backup at \"%s\"", savePath.u8string().c_str());
    LoadedProject->Save(savePath.u8string(), false);
}

void OpenFunscripter::exitApp(bool force) noexcept
{
    if (force) {
        Status |= OFS_Status::OFS_ShouldExit;
        return;
    }

    bool unsavedChanges = LoadedProject->HasUnsavedEdits();

    if (unsavedChanges) {
        Util::YesNoCancelDialog(TR(UNSAVED_CHANGES), TR(UNSAVED_CHANGES_MSG),
            [&](Util::YesNoCancel result) {
                if (result == Util::YesNoCancel::Yes) {
                    saveProject();
                    Status |= OFS_Status::OFS_ShouldExit;
                }
                else if (result == Util::YesNoCancel::No) {
                    Status |= OFS_Status::OFS_ShouldExit;
                }
                else {
                    // cancel does nothing
                    Status &= ~(OFS_Status::OFS_ShouldExit);
                }
            });
    }
    else {
        Status |= OFS_Status::OFS_ShouldExit;
    }
}

void OpenFunscripter::setIdle(bool idle) noexcept
{
    if (idle == IdleMode) return;
    if (idle && !player->IsPaused()) return; // can't idle while player is playing
    IdleMode = idle;
}

void OpenFunscripter::Step() noexcept
{
    OFS_BEGINPROFILING();
    {
        OFS_PROFILE(__FUNCTION__);
        processEvents();
        newFrame();
        update();
        {
            OFS_PROFILE("ImGui");
            // IMGUI HERE
            CreateDockspace();
            blockingTask.ShowBlockingTask();

            auto& ofsState = OpenFunscripterState::State(stateHandle);
#ifdef WIN32
            if (OFS_DownloadFfmpeg::FfmpegMissing) {
                ImGui::OpenPopup(OFS_DownloadFfmpeg::ModalId);
                OFS_DownloadFfmpeg::DownloadFfmpegModal();
            }
#endif

            auto& overlayState = BaseOverlay::State();
            ShowAboutWindow(&ShowAbout);

            specialFunctions->ShowFunctionsWindow(&ofsState.showSpecialFunctions);
            undoSystem->ShowUndoRedoHistory(&ofsState.showHistory);
            simulator.ShowSimulator(&ofsState.showSimulator, ActiveFunscript(), player->CurrentTime(), overlayState.SplineMode);

            if (ShowMetadataEditor) {
                auto& projectState = LoadedProject->State();
                projectState.metadata.duration = player->Duration();
                if (metadataEditor->ShowMetadataEditor(&ShowMetadataEditor, projectState.metadata)) {
                    EV::Enqueue<MetadataChanged>();
                }
            }

            webApi->ShowWindow(&ofsState.showWsApi);
            scripting->DrawScriptingMode(NULL);
            LoadedProject->ShowProjectWindow(&ShowProjectEditor);

            extensions->ShowExtensions();

            OFS_FileLogger::DrawLogWindow(&ofsState.showDebugLog);

            keys->RenderKeybindingWindow();

            if (preferences->ShowPreferenceWindow()) {}

            playerControls.DrawControls();

            if (Status & OFS_GradientNeedsUpdate) {
                Status &= ~(OFS_GradientNeedsUpdate);
                playerControls.UpdateHeatmap(player->Duration(), ActiveFunscript()->Actions());
            }

            playerControls.DrawTimeline();

            scriptTimeline.ShowScriptPositions(player.get(),
                scripting->Overlay().get(),
                LoadedFunscripts(),
                LoadedProject->ActiveIdx());

            ShowStatisticsWindow(&ofsState.showStatistics);

            if (ofsState.showActionEditor) {
                ImGui::Begin(TR_ID(ActionEditorWindowId, Tr::ACTION_EDITOR), &ofsState.showActionEditor);
                OFS_PROFILE(ActionEditorWindowId);

                ImGui::Columns(1, 0, false);
                if (ImGui::Button("100", ImVec2(-1, 0))) {
                    addEditAction(100);
                }
                for (int i = 9; i != 0; i--) {
                    if (i % 3 == 0) {
                        ImGui::Columns(3, 0, false);
                    }
                    sprintf(tmpBuf[0], "%d", i * 10);
                    if (ImGui::Button(tmpBuf[0], ImVec2(-1, 0))) {
                        addEditAction(i * 10);
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1, 0, false);
                if (ImGui::Button("0", ImVec2(-1, 0))) {
                    addEditAction(0);
                }

                if (player->IsPaused()) {
                    ImGui::Spacing();
                    auto scriptAction = ActiveFunscript()->GetActionAtTime(player->CurrentTime(), scripting->LogicalFrameTime());
                    if (!scriptAction) {
                        // create action
                        static int newActionPosition = 0;
                        ImGui::SetNextItemWidth(-1.f);
                        ImGui::SliderInt("##Position", &newActionPosition, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::Button(TR(ADD_ACTION), ImVec2(-1.f, 0.f))) {
                            addEditAction(newActionPosition);
                        }
                    }
                }
                ImGui::End();
            }

#ifndef NDEBUG
            if (DebugDemo) {
                ImGui::ShowDemoWindow(&DebugDemo);
            }
#endif
            if (DebugMetrics) {
                ImGui::ShowMetricsWindow(&DebugMetrics);
            }

            playerWindow->DrawVideoPlayer(NULL, &ofsState.showVideo);
        }

        render();
    }

    OFS_FileLogger::Flush();
    OFS_ENDPROFILING();
    SDL_GL_SwapWindow(window);
    player->NotifySwap();
}

int OpenFunscripter::Run() noexcept
{
    newFrame();
    setupDefaultLayout(false);
    render();

    const uint64_t PerfFreq = SDL_GetPerformanceFrequency();
    while (!(Status & OFS_Status::OFS_ShouldExit)) {

        uint64_t FrameStart = SDL_GetPerformanceCounter();
        Step();
        uint64_t FrameEnd = SDL_GetPerformanceCounter();

        const auto& prefState = PreferenceState::State(preferences->StateHandle());
        float frameLimit = IdleMode ? 10.f : (float)prefState.framerateLimit;
        const float minFrameTime = (float)PerfFreq / frameLimit;

        int32_t sleepMs = ((minFrameTime - (float)(FrameEnd - FrameStart)) / minFrameTime) * (1000.f / frameLimit);
        if (!IdleMode) sleepMs -= 1;
        if (sleepMs > 0) SDL_Delay(sleepMs);

        if (!prefState.vsync) {
            FrameEnd = SDL_GetPerformanceCounter();
            while ((FrameEnd - FrameStart) < minFrameTime) {
                OFS_PAUSE_INTRIN();
                FrameEnd = SDL_GetPerformanceCounter();
            }
        }

        if (SDL_GetTicks() - IdleTimer > 3000) {
            setIdle(true);
        }
    }
    return 0;
}

void OpenFunscripter::Shutdown() noexcept
{
    SaveState();

    OFS_DynFontAtlas::Shutdown();
    OFS_Translator::Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // These players need to be freed before unloading mpv
    // NOTE: Do not free the GL context before these players
    player.reset();
    playerControls.videoPreview.reset();
    OFS_MpvLoader::Unload();
    OFS_FileLogger::Shutdown();
    webApi->Shutdown();
    controllerInput->Shutdown();
    
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void OpenFunscripter::Undo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (undoSystem->Undo()) scripting->Undo();
}

void OpenFunscripter::Redo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (undoSystem->Redo()) scripting->Redo();
}

void OpenFunscripter::openFile(const std::string& file) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!Util::FileExists(file)) {
        Util::MessageBoxAlert(TR(FILE_NOT_FOUND), std::string(TR(COULDNT_FIND_FILE)) + "\n" + file);
        return;
    }

    // If a project with the same name exists, it's opened instead.
    auto testProjectPath = Util::PathFromString(file);
    if (testProjectPath.extension().u8string() != OFS_Project::Extension) {
        testProjectPath.replace_extension(OFS_Project::Extension);
        if (Util::FileExists(testProjectPath.u8string())) {
            openFile(testProjectPath.u8string());
            return;
        }
    }

    closeWithoutSavingDialog(
        [this, file]() noexcept {
            auto filePath = Util::PathFromString(file);
            auto fileExtension = filePath.extension().u8string();
            LoadedProject = std::make_unique<OFS_Project>();
            OFS_StateManager::Get()->ClearProjectAll();

            if (fileExtension == OFS_Project::Extension) {
                // It's a project
                LoadedProject->Load(file);
            }
            else if (fileExtension == Funscript::Extension) {
                // It's a funscript it should be imported into a new project
                LoadedProject->ImportFromFunscript(file);
            }
            else {
                // Assume it's some kind of media file
                LoadedProject->ImportFromMedia(file);
            }

            if (LoadedProject->IsValid()) {
                initProject();
            }
            else {
                Util::MessageBoxAlert("Failed to open file.", LoadedProject->NotValidError());
            }
        });
}

void OpenFunscripter::initProject() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (LoadedProject->IsValid()) {
        auto& projectState = LoadedProject->State();
        if (projectState.nudgeMetadata) {
            const auto& prefState = PreferenceState::State(preferences->StateHandle());
            ShowMetadataEditor = prefState.showMetaOnNew;
            projectState.nudgeMetadata = false;
        }

        if (Util::FileExists(LoadedProject->MediaPath())) {
            player->OpenVideo(LoadedProject->MediaPath());
        }
        else {
            pickDifferentMedia();
        }
    }
    updateTitle();

    auto lastPath = Util::PathFromString(LoadedProject->Path());
    lastPath.remove_filename();

    auto& ofsState = OpenFunscripterState::State(stateHandle);
    ofsState.lastPath = lastPath.u8string();

    lastBackup = std::chrono::steady_clock::now();
    EV::Enqueue<ProjectLoadedEvent>();
}

void OpenFunscripter::UpdateNewActiveScript(uint32_t activeIndex) noexcept
{
    LoadedProject->SetActiveIdx(activeIndex);
    updateTitle();
    Status = Status | OFS_Status::OFS_GradientNeedsUpdate;
}

void OpenFunscripter::updateTitle() noexcept
{
    const char* title = "OFS";
    if (LoadedProject->IsValid()) {
        title = Util::Format("OpenFunscripter %s@%s - \"%s\"",
            OFS_LATEST_GIT_TAG,
            OFS_LATEST_GIT_HASH,
            LoadedProject->Path().c_str());
    }
    else {
        title = Util::Format("OpenFunscripter %s@%s",
            OFS_LATEST_GIT_TAG,
            OFS_LATEST_GIT_HASH);
    }
    SDL_SetWindowTitle(window, title);
}

void OpenFunscripter::saveProject() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& projectState = LoadedProject->State();
    projectState.lastPlayerPosition = player->CurrentTime();
    LoadedProject->Save(true);

    auto& ofsState = OpenFunscripterState::State(stateHandle);
    auto recentFile = RecentFile{ Util::PathFromString(LoadedProject->Path()).filename().u8string(), LoadedProject->Path() };
    ofsState.addRecentFile(recentFile);
}

void OpenFunscripter::quickExport() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    LoadedProject->ExportFunscripts();
}

bool OpenFunscripter::closeProject(bool closeWithUnsavedChanges) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!closeWithUnsavedChanges && LoadedProject->HasUnsavedEdits()) {
        FUN_ASSERT(false, "this branch should ideally never be taken");
        return false;
    }
    else {
        UpdateNewActiveScript(0);
        LoadedProject = std::make_unique<OFS_Project>();
        player->CloseVideo();
        playerControls.videoPreview->CloseVideo();
        updateTitle();
    }
    return true;
}

void OpenFunscripter::pickDifferentMedia() noexcept
{
    if (LoadedProject->IsValid()) {
        auto& projectState = LoadedProject->State();
        Util::OpenFileDialog(
            TR(PICK_DIFFERENT_MEDIA), LoadedProject->MediaPath(),
            [this](auto& result) {
                auto& projectState = LoadedProject->State();
                if (!result.files.empty() && Util::FileExists(result.files[0])) {
                    projectState.relativeMediaPath = LoadedProject->MakePathRelative(result.files[0]);
                    player->OpenVideo(LoadedProject->MediaPath());
                }
            },
            false);
    }
}

void OpenFunscripter::saveHeatmap(const char* path, int width, int height, bool withChapters)
{
    OFS_PROFILE(__FUNCTION__);
    if (withChapters) {
        auto bitmap = playerControls.RenderHeatmapToBitmapWithChapters(width, height, height);
        Util::SavePNG(path, bitmap.data(), width, height + height, 4);
    }
    else {
        auto bitmap = playerControls.Heatmap->RenderToBitmap(width, height);
        Util::SavePNG(path, bitmap.data(), width, height, 4);
    }
}

void OpenFunscripter::removeAction(FunscriptAction action) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    undoSystem->Snapshot(StateType::REMOVE_ACTION, ActiveFunscript());
    ActiveFunscript()->RemoveAction(action);
}

void OpenFunscripter::removeAction() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (ActiveFunscript()->HasSelection()) {
        undoSystem->Snapshot(StateType::REMOVE_SELECTION, ActiveFunscript());
        ActiveFunscript()->RemoveSelectedActions();
    }
    else {
        auto action = ActiveFunscript()->GetClosestAction(player->CurrentTime());
        if (action != nullptr) {
            removeAction(*action); // snapshoted in here
        }
    }
}

void OpenFunscripter::addEditAction(int pos) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS, ActiveFunscript());
    scripting->AddEditAction(FunscriptAction(player->CurrentTime(), pos));
}

void OpenFunscripter::cutSelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (ActiveFunscript()->HasSelection()) {
        copySelection();
        undoSystem->Snapshot(StateType::CUT_SELECTION, ActiveFunscript());
        ActiveFunscript()->RemoveSelectedActions();
    }
}

void OpenFunscripter::copySelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (ActiveFunscript()->HasSelection()) {
        CopiedSelection.clear();
        for (auto action : ActiveFunscript()->Selection()) {
            CopiedSelection.emplace(action);
        }
    }
}

void OpenFunscripter::pasteSelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (CopiedSelection.empty()) return;
    undoSystem->Snapshot(StateType::PASTE_COPIED_ACTIONS, ActiveFunscript());
    // paste CopiedSelection relatively to position
    // NOTE: assumes CopiedSelection is ordered by time
    float currentTime = player->CurrentTime();
    float offsetTime = currentTime - CopiedSelection.begin()->atS;

    ActiveFunscript()->RemoveActionsInInterval(
        currentTime - 0.0005f,
        currentTime + (CopiedSelection.back().atS - CopiedSelection.front().atS + 0.0005f));

    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->AddAction(FunscriptAction(action.atS + offsetTime, action.pos));
    }
    float newPosTime = (CopiedSelection.end() - 1)->atS + offsetTime;
    player->SetPositionExact(newPosTime);
}

void OpenFunscripter::pasteSelectionExact() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (CopiedSelection.empty()) return;

    undoSystem->Snapshot(StateType::PASTE_COPIED_ACTIONS, ActiveFunscript());
    if (CopiedSelection.size() >= 2) {
        ActiveFunscript()->RemoveActionsInInterval(CopiedSelection.front().atS, CopiedSelection.back().atS);
    }

    // paste without altering timestamps
    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->AddAction(action);
    }
}

void OpenFunscripter::equalizeSelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!ActiveFunscript()->HasSelection()) {
        undoSystem->Snapshot(StateType::EQUALIZE_ACTIONS, ActiveFunscript());
        // this is a small hack
        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
        if (closest != nullptr) {
            auto behind = ActiveFunscript()->GetPreviousActionBehind(closest->atS);
            if (behind != nullptr) {
                auto front = ActiveFunscript()->GetNextActionAhead(closest->atS);
                if (front != nullptr) {
                    ActiveFunscript()->SelectAction(*behind);
                    ActiveFunscript()->SelectAction(*closest);
                    ActiveFunscript()->SelectAction(*front);
                    ActiveFunscript()->EqualizeSelection();
                    ActiveFunscript()->ClearSelection();
                }
            }
        }
    }
    else if (ActiveFunscript()->Selection().size() >= 3) {
        undoSystem->Snapshot(StateType::EQUALIZE_ACTIONS, ActiveFunscript());
        ActiveFunscript()->EqualizeSelection();
    }
}

void OpenFunscripter::invertSelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!ActiveFunscript()->HasSelection()) {
        // same hack as above
        auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
        if (closest != nullptr) {
            undoSystem->Snapshot(StateType::INVERT_ACTIONS, ActiveFunscript());
            ActiveFunscript()->SelectAction(*closest);
            ActiveFunscript()->InvertSelection();
            ActiveFunscript()->ClearSelection();
        }
    }
    else if (ActiveFunscript()->Selection().size() >= 3) {
        undoSystem->Snapshot(StateType::INVERT_ACTIONS, ActiveFunscript());
        ActiveFunscript()->InvertSelection();
    }
}

void OpenFunscripter::isolateAction() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
    if (closest != nullptr) {
        undoSystem->Snapshot(StateType::ISOLATE_ACTION, ActiveFunscript());
        auto prev = ActiveFunscript()->GetPreviousActionBehind(closest->atS - 0.001f);
        auto next = ActiveFunscript()->GetNextActionAhead(closest->atS + 0.001f);
        if (prev != nullptr && next != nullptr) {
            auto tmp = *next; // removing prev will invalidate the pointer
            ActiveFunscript()->RemoveAction(*prev);
            ActiveFunscript()->RemoveAction(tmp);
        }
        else if (prev != nullptr) {
            ActiveFunscript()->RemoveAction(*prev);
        }
        else if (next != nullptr) {
            ActiveFunscript()->RemoveAction(*next);
        }
    }
}

void OpenFunscripter::repeatLastStroke() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto stroke = ActiveFunscript()->GetLastStroke(player->CurrentTime());
    if (stroke.size() > 1) {
        auto offsetTime = player->CurrentTime() - stroke.back().atS;
        undoSystem->Snapshot(StateType::REPEAT_STROKE, ActiveFunscript());
        auto action = ActiveFunscript()->GetActionAtTime(player->CurrentTime(), scripting->LogicalFrameTime());
        // if we are on top of an action we ignore the first action of the last stroke
        if (action != nullptr) {
            for (int i = stroke.size() - 2; i >= 0; i--) {
                auto action = stroke[i];
                action.atS += offsetTime;
                ActiveFunscript()->AddAction(action);
            }
        }
        else {
            for (int i = stroke.size() - 1; i >= 0; i--) {
                auto action = stroke[i];
                action.atS += offsetTime;
                ActiveFunscript()->AddAction(action);
            }
        }
        player->SetPositionExact(stroke.front().atS + offsetTime);
    }
}

void OpenFunscripter::saveActiveScriptAs()
{
    Util::SaveFileDialog(TR(SAVE),
        LoadedProject->MakePathAbsolute(ActiveFunscript()->RelativePath()),
        [this](auto& result) {
            if (result.files.size() > 0) {
                LoadedProject->ExportFunscript(result.files[0], LoadedProject->ActiveIdx());
                auto dir = Util::PathFromString(result.files[0]);
                dir.remove_filename();
                auto& ofsState = OpenFunscripterState::State(stateHandle);
                ofsState.lastPath = dir.u8string();
            }
        },
        { "Funscript", "*.funscript" });
}

void OpenFunscripter::ShowMainMenuBar() noexcept
{
#define BINDING_STRING(binding) nullptr // TODO: keybinds.getBindingString(binding)
    OFS_PROFILE(__FUNCTION__);
    ImColor alertCol = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
    std::chrono::duration<float> saveDuration;
    bool unsavedEdits = LoadedProject->HasUnsavedEdits();
    if (player->VideoLoaded() && unsavedEdits) {
        saveDuration = std::chrono::system_clock::now() - ActiveFunscript()->EditTime();
        const float timeUnit = saveDuration.count() / 60.f;
        if (timeUnit >= 5.f) {
            alertCol = ImLerp(alertCol.Value, ImColor(IM_COL32(184, 33, 22, 255)).Value, std::max(std::sin(saveDuration.count()), 0.f));
        }
    }

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, alertCol.Value);
    if (ImGui::BeginMainMenuBar()) {
        auto region = ImGui::GetContentRegionAvail();
        auto& ofsState = OpenFunscripterState::State(stateHandle);
        if (ImGui::BeginMenu(TR_ID("FILE", Tr::FILE))) {
            if (ImGui::MenuItem(TR(GENERIC_OPEN))) {
                Util::OpenFileDialog(
                    TR(GENERIC_OPEN), ofsState.lastPath,
                    [this](auto& result) {
                        if (result.files.size() > 0) {
                            auto& file = result.files[0];
                            openFile(file);
                        }
                    },
                    false);
            }
            if (LoadedProject->IsValid() && ImGui::MenuItem(TR(CLOSE_PROJECT), NULL, false, LoadedProject->IsValid())) {
                closeWithoutSavingDialog([]() {});
            }
            ImGui::Separator();
            if (ImGui::BeginMenu(TR_ID("RECENT_FILES", Tr::RECENT_FILES))) {
                if (ofsState.recentFiles.empty()) {
                    ImGui::TextDisabled("%s", TR(NO_RECENT_FILES));
                }
                auto& recentFiles = ofsState.recentFiles;
                for (auto it = recentFiles.rbegin(); it != recentFiles.rend(); ++it) {
                    auto& recent = *it;
                    if (ImGui::MenuItem(recent.name.c_str())) {
                        if (!recent.projectPath.empty()) {
                            closeWithoutSavingDialog([this, clickedFile = recent.projectPath]() {
                                openFile(clickedFile);
                            });
                            break;
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem(TR(CLEAR_RECENT_FILES))) {
                    ofsState.recentFiles.clear();
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (ImGui::MenuItem(TR(SAVE_PROJECT), BINDING_STRING("save_project"), false, LoadedProject->IsValid())) {
                saveProject();
            }
            if (ImGui::BeginMenu(TR_ID("EXPORT_MENU", Tr::EXPORT_MENU), LoadedProject->IsValid())) {
                if (ImGui::MenuItem(FMT(ICON_SHARE " %s", TR(QUICK_EXPORT)), BINDING_STRING("quick_export"))) {
                    quickExport();
                }
                OFS::Tooltip(TR(QUICK_EXPORT_TOOLTIP));
                if (ImGui::MenuItem(FMT(ICON_SHARE " %s", TR(EXPORT_ACTIVE_SCRIPT)))) {
                    saveActiveScriptAs();
                }
                if (ImGui::MenuItem(FMT(ICON_SHARE " %s", TR(EXPORT_ALL)))) {
                    if (LoadedFunscripts().size() == 1) {
                        auto savePath = Util::PathFromString(ofsState.lastPath) / (ActiveFunscript()->Title() + ".funscript");
                        Util::SaveFileDialog(TR(EXPORT_MENU), savePath.u8string(),
                            [this](auto& result) {
                                if (result.files.size() > 0) {
                                    LoadedProject->ExportFunscript(result.files[0], LoadedProject->ActiveIdx());
                                    std::filesystem::path dir = Util::PathFromString(result.files[0]);
                                    dir.remove_filename();
                                    auto& ofsState = OpenFunscripterState::State(stateHandle);
                                    ofsState.lastPath = dir.u8string();
                                }
                            },
                            { "Funscript", "*.funscript" });
                    }
                    else if (LoadedFunscripts().size() > 1) {
                        Util::OpenDirectoryDialog(TR(EXPORT_MENU), ofsState.lastPath,
                            [this](auto& result) {
                                if (result.files.size() > 0) {
                                    LoadedProject->ExportFunscripts(result.files[0]);
                                }
                            });
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            bool autoBackupTmp = Status & OFS_Status::OFS_AutoBackup;
            if (ImGui::MenuItem(autoBackupTmp && LoadedProject->IsValid() ? FMT(TR(AUTO_BACKUP_TIMER_FMT), AutoBackupIntervalSeconds - std::chrono::duration_cast<std::chrono::seconds>((std::chrono::steady_clock::now() - lastBackup)).count())
                                                                          : TR(AUTO_BACKUP),
                    NULL, &autoBackupTmp)) {
                Status = autoBackupTmp
                    ? Status | OFS_Status::OFS_AutoBackup
                    : Status ^ OFS_Status::OFS_AutoBackup;
            }
            if (ImGui::MenuItem(TR(OPEN_BACKUP_DIR))) {
                Util::OpenFileExplorer(Util::Prefpath("backup").c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("PROJECT", Tr::PROJECT), LoadedProject->IsValid())) {
            if (ImGui::MenuItem(TR(CONFIGURE), NULL, &ShowProjectEditor)) {}
            ImGui::Separator();
            if (ImGui::MenuItem(TR(PICK_DIFFERENT_MEDIA))) {
                pickDifferentMedia();
            }
            if (ImGui::BeginMenu(TR(ADD_MENU), LoadedProject->IsValid())) {
                auto fileAlreadyLoaded = [](const std::string& path) noexcept -> bool {
                    auto app = OpenFunscripter::ptr;
                    auto it = std::find_if(app->LoadedFunscripts().begin(), app->LoadedFunscripts().end(),
                        [filename = Util::PathFromString(path).filename().u8string()](auto& script) {
                            return Util::PathFromString(script->RelativePath()).filename().u8string() == filename;
                        });
                    return it != app->LoadedFunscripts().end();
                };
                auto addNewShortcut = [this, fileAlreadyLoaded](const char* axisExt) noexcept {
                    if (ImGui::MenuItem(axisExt)) {
                        std::string newScriptPath;
                        {
                            auto root = Util::PathFromString(
                                LoadedProject->MakePathAbsolute(LoadedFunscripts()[0]->RelativePath()));
                            root.replace_extension(Util::Format(".%s.funscript", axisExt));
                            newScriptPath = root.u8string();
                        }

                        if (!fileAlreadyLoaded(newScriptPath)) {
                            LoadedProject->AddFunscript(newScriptPath);
                        }
                    }
                };
                if (ImGui::BeginMenu(TR(ADD_SHORTCUTS))) {
                    for (auto axis : Funscript::AxisNames) {
                        addNewShortcut(axis);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(TR(ADD_NEW))) {
                    Util::SaveFileDialog(TR(ADD_NEW_FUNSCRIPT), ofsState.lastPath,
                        [fileAlreadyLoaded](auto& result) noexcept {
                            if (result.files.size() > 0) {
                                auto app = OpenFunscripter::ptr;
                                if (!fileAlreadyLoaded(result.files[0])) {
                                    app->LoadedProject->AddFunscript(result.files[0]);
                                }
                            }
                        },
                        { "Funscript", "*.funscript" });
                }
                if (ImGui::MenuItem(TR(ADD_EXISTING))) {
                    Util::OpenFileDialog(
                        TR(ADD_EXISTING_FUNSCRIPTS), ofsState.lastPath,
                        [fileAlreadyLoaded](auto& result) noexcept {
                            if (result.files.size() > 0) {
                                for (auto& scriptPath : result.files) {
                                    auto app = OpenFunscripter::ptr;
                                    if (!fileAlreadyLoaded(scriptPath)) {
                                        app->LoadedProject->AddFunscript(scriptPath);
                                    }
                                }
                            }
                        },
                        true, { "*.funscript" }, "Funscript");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(TR(REMOVE), !LoadedFunscripts().empty())) {
                int unloadIndex = -1;
                for (int i = 0; i < LoadedFunscripts().size(); i++) {
                    if (ImGui::MenuItem(LoadedFunscripts()[i]->Title().c_str())) {
                        unloadIndex = i;
                    }
                }
                if (unloadIndex >= 0) {
                    Util::YesNoCancelDialog(TR(REMOVE_SCRIPT),
                        TR(REMOVE_SCRIPT_CONFIRM_MSG),
                        [this, unloadIndex](Util::YesNoCancel result) {
                            if (result == Util::YesNoCancel::Yes) {
                                LoadedProject->RemoveFunscript(unloadIndex);
                                auto activeIdx = LoadedProject->ActiveIdx();
                                if (activeIdx > 0) {
                                    activeIdx--;
                                    UpdateNewActiveScript(activeIdx);
                                }
                            }
                        });
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("EDIT", Tr::EDIT))) {
            if (ImGui::MenuItem(TR(SAVE_FRAME_AS_IMAGE), BINDING_STRING("save_frame_as_image"))) {
                auto screenshotDir = Util::Prefpath("screenshot");
                player->SaveFrameToImage(screenshotDir);
            }
            if (ImGui::MenuItem(TR(OPEN_SCREENSHOT_DIR))) {
                auto screenshotDir = Util::Prefpath("screenshot");
                Util::CreateDirectories(screenshotDir);
                Util::OpenFileExplorer(screenshotDir.c_str());
            }

            ImGui::Separator();

            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##width", &ofsState.heatmapSettings.defaultWidth);
            ImGui::SameLine();
            ImGui::TextUnformatted("x");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##height", &ofsState.heatmapSettings.defaultHeight);
            if (ImGui::MenuItem(TR(SAVE_HEATMAP))) {
                std::string filename = ActiveFunscript()->Title() + "_Heatmap.png";
                auto defaultPath = Util::PathFromString(ofsState.heatmapSettings.defaultPath);
                Util::ConcatPathSafe(defaultPath, filename);
                Util::SaveFileDialog(
                    TR(SAVE_HEATMAP), defaultPath.u8string(),
                    [this](auto& result) {
                        if (result.files.size() > 0) {
                            auto savePath = Util::PathFromString(result.files.front());
                            if (savePath.has_filename()) {
                                auto& ofsState = OpenFunscripterState::State(stateHandle);
                                saveHeatmap(result.files.front().c_str(), ofsState.heatmapSettings.defaultWidth, ofsState.heatmapSettings.defaultHeight, false);
                                savePath.remove_filename();
                                ofsState.heatmapSettings.defaultPath = savePath.u8string();
                            }
                        }
                    },
                    { "*.png" }, "PNG");
            }
            if (ImGui::MenuItem(TR(SAVE_HEATMAP_WITH_CHAPTERS))) {
                std::string filename = ActiveFunscript()->Title() + "_Heatmap.png";
                auto defaultPath = Util::PathFromString(ofsState.heatmapSettings.defaultPath);
                Util::ConcatPathSafe(defaultPath, filename);
                Util::SaveFileDialog(
                    TR(SAVE_HEATMAP), defaultPath.u8string(),
                    [this](auto& result) {
                        if (result.files.size() > 0) {
                            auto savePath = Util::PathFromString(result.files.front());
                            if (savePath.has_filename()) {
                                auto& ofsState = OpenFunscripterState::State(stateHandle);
                                saveHeatmap(result.files.front().c_str(), ofsState.heatmapSettings.defaultWidth, ofsState.heatmapSettings.defaultHeight, true);
                                savePath.remove_filename();
                                ofsState.heatmapSettings.defaultPath = savePath.u8string();
                            }
                        }
                    },
                    { "*.png" }, "PNG");
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR(UNDO), BINDING_STRING("undo"), false, !undoSystem->UndoEmpty())) {
                this->Undo();
            }
            if (ImGui::MenuItem(TR(REDO), BINDING_STRING("redo"), false, !undoSystem->RedoEmpty())) {
                this->Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR(CUT), BINDING_STRING("cut"), false, ActiveFunscript()->HasSelection())) {
                cutSelection();
            }
            if (ImGui::MenuItem(TR(COPY), BINDING_STRING("copy"), false, ActiveFunscript()->HasSelection())) {
                copySelection();
            }
            if (ImGui::MenuItem(TR(PASTE), BINDING_STRING("paste"), false, CopiedSelection.size() > 0)) {
                pasteSelection();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR(SELECT))) {
            if (ImGui::MenuItem(TR(SELECT_ALL), BINDING_STRING("select_all"), false)) {
                ActiveFunscript()->SelectAll();
            }
            if (ImGui::MenuItem(TR(DESELECT_ALL), BINDING_STRING("deselect_all"), false)) {
                ActiveFunscript()->ClearSelection();
            }

            if (ImGui::BeginMenu(TR(SPECIAL))) {
                if (ImGui::MenuItem(TR(SELECT_ALL_LEFT), BINDING_STRING("select_all_left"), false)) {
                    ActiveFunscript()->SelectTime(0, player->CurrentTime());
                }
                if (ImGui::MenuItem(TR(SELECT_ALL_RIGHT), BINDING_STRING("select_all_right"), false)) {
                    ActiveFunscript()->SelectTime(player->CurrentTime(), player->Duration());
                }
                ImGui::Separator();
                static int32_t selectionPoint = -1;
                if (ImGui::MenuItem(TR(SET_SELECTION_START))) {
                    if (selectionPoint == -1) {
                        selectionPoint = player->CurrentTime();
                    }
                    else {
                        ActiveFunscript()->SelectTime(player->CurrentTime(), selectionPoint);
                        selectionPoint = -1;
                    }
                }
                if (ImGui::MenuItem(TR(SET_SELECTION_END))) {
                    if (selectionPoint == -1) {
                        selectionPoint = player->CurrentTime();
                    }
                    else {
                        ActiveFunscript()->SelectTime(selectionPoint, player->CurrentTime());
                        selectionPoint = -1;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR(TOP_POINTS_ONLY), BINDING_STRING("select_top_points"), false)) {
                if (ActiveFunscript()->HasSelection()) {
                    selectTopPoints();
                }
            }
            if (ImGui::MenuItem(TR(MID_POINTS_ONLY), BINDING_STRING("select_middle_points"), false)) {
                if (ActiveFunscript()->HasSelection()) {
                    selectMiddlePoints();
                }
            }
            if (ImGui::MenuItem(TR(BOTTOM_POINTS_ONLY), BINDING_STRING("select_bottom_points"), false)) {
                if (ActiveFunscript()->HasSelection()) {
                    selectBottomPoints();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR(EQUALIZE), BINDING_STRING("equalize_actions"), false)) {
                equalizeSelection();
            }
            if (ImGui::MenuItem(TR(INVERT), BINDING_STRING("invert_actions"), false)) {
                invertSelection();
            }
            if (ImGui::MenuItem(TR(ISOLATE), BINDING_STRING("isolate_action"))) {
                isolateAction();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("VIEW_MENU", Tr::VIEW_MENU))) {
#ifndef NDEBUG
            // this breaks the layout after restarting for some reason
            if (ImGui::MenuItem("Reset layout")) {
                setupDefaultLayout(true);
            }
            ImGui::Separator();
#endif
            if (ImGui::MenuItem(TR(STATISTICS), NULL, &ofsState.showStatistics)) {}
            if (ImGui::MenuItem(TR(UNDO_REDO_HISTORY), NULL, &ofsState.showHistory)) {}
            if (ImGui::MenuItem(TR(SIMULATOR), NULL, &ofsState.showSimulator)) {}
            if (ImGui::MenuItem(TR(METADATA), NULL, &ShowMetadataEditor)) {}
            if (ImGui::MenuItem(TR(ACTION_EDITOR), NULL, &ofsState.showActionEditor)) {}
            if (ImGui::MenuItem(TR(SPECIAL_FUNCTIONS), NULL, &ofsState.showSpecialFunctions)) {}
            if (ImGui::MenuItem(TR(WEBSOCKET_API), NULL, &ofsState.showWsApi)) {}


            ImGui::Separator();

            if (ImGui::MenuItem(TR(DRAW_VIDEO), NULL, &ofsState.showVideo)) {}
            if (ImGui::MenuItem(TR(RESET_VIDEO_POS), NULL)) {
                playerWindow->ResetTranslationAndZoom();
            }

            auto videoModeToString = [](VideoMode mode) noexcept -> const char* {
                switch (mode) {
                    case VideoMode::Full: return TR(VIDEO_MODE_FULL);
                    case VideoMode::LeftPane: return TR(VIDEO_MODE_LEFT_PANE);
                    case VideoMode::RightPane: return TR(VIDEO_MODE_RIGHT_PANE);
                    case VideoMode::TopPane: return TR(VIDEO_MODE_TOP_PANE);
                    case VideoMode::BottomPane: return TR(VIDEO_MODE_BOTTOM_PANE);
                    case VideoMode::VrMode: return TR(VIDEO_MODE_VR);
                }
                return "";
            };

            auto& videoWindow = VideoPlayerWindowState::State(playerWindow->StateHandle());
            if (ImGui::BeginCombo(TR(VIDEO_MODE), videoModeToString(videoWindow.activeMode))) {
                auto& mode = videoWindow.activeMode;
                if (ImGui::Selectable(TR(VIDEO_MODE_FULL), mode == VideoMode::Full)) {
                    mode = VideoMode::Full;
                }
                if (ImGui::Selectable(TR(VIDEO_MODE_LEFT_PANE), mode == VideoMode::LeftPane)) {
                    mode = VideoMode::LeftPane;
                }
                if (ImGui::Selectable(TR(VIDEO_MODE_RIGHT_PANE), mode == VideoMode::RightPane)) {
                    mode = VideoMode::RightPane;
                }
                if (ImGui::Selectable(TR(VIDEO_MODE_TOP_PANE), mode == VideoMode::TopPane)) {
                    mode = VideoMode::TopPane;
                }
                if (ImGui::Selectable(TR(VIDEO_MODE_BOTTOM_PANE), mode == VideoMode::BottomPane)) {
                    mode = VideoMode::BottomPane;
                }
                if (ImGui::Selectable(TR(VIDEO_MODE_VR), mode == VideoMode::VrMode)) {
                    mode = VideoMode::VrMode;
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            if (ImGui::BeginMenu(TR(DEBUG))) {
                if (ImGui::MenuItem(TR(METRICS), NULL, &DebugMetrics)) {}
                if (ImGui::MenuItem(TR(LOG_OUTPUT), NULL, &ofsState.showDebugLog)) {}
#ifndef NDEBUG
                if (ImGui::MenuItem("ImGui Demo", NULL, &DebugDemo)) {}
#endif
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR(OPTIONS))) {
            if (ImGui::MenuItem(TR(KEYS))) {
                keys->ShowModal();
            }
            bool fullscreenTmp = Status & OFS_Status::OFS_Fullscreen;
            if (ImGui::MenuItem(TR(FULLSCREEN), BINDING_STRING("fullscreen_toggle"), &fullscreenTmp)) {
                SetFullscreen(fullscreenTmp);
                Status = fullscreenTmp
                    ? Status | OFS_Status::OFS_Fullscreen
                    : Status ^ OFS_Status::OFS_Fullscreen;
            }
            if (ImGui::MenuItem(TR(PREFERENCES), nullptr, &preferences->ShowWindow)) {}
            if (ImGui::BeginMenu(TR(CONTROLLER), ControllerInput::AnythingConnected())) {
                ImGui::TextColored(ImColor(IM_COL32(0, 255, 0, 255)), "%s", TR(CONTROLLER_CONNECTED));
                ImGui::TextUnformatted(ControllerInput::Controllers[0].GetName());
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("EXTENSIONS", Tr::EXTENSIONS_MENU))) {
            if (ImGui::IsWindowAppearing()) {
                extensions->UpdateExtensionList();
            }
            if (ImGui::MenuItem(TR(DEV_MODE), NULL, &OFS_LuaExtensions::DevMode)) {}
            OFS::Tooltip(TR(DEV_MODE_TOOLTIP));
            if (ImGui::MenuItem(TR(SHOW_LOGS), NULL, &OFS_LuaExtensions::ShowLogs)) {}
            if (ImGui::MenuItem(TR(EXTENSION_DIR))) {
                Util::OpenFileExplorer(Util::Prefpath(OFS_LuaExtensions::ExtensionDir));
            }
            ImGui::Separator();
            for (auto& ext : extensions->Extensions) {
                if (ImGui::BeginMenu(ext.NameId.c_str())) {
                    bool isActive = ext.Active;
                    if (ImGui::MenuItem(TR(ENABLED), NULL, &isActive)) {
                        ext.Toggle();
                        if (ext.HasError()) {
                            Util::MessageBoxAlert(TR(UNKNOWN_ERROR), ext.Error);
                        }
                    }
                    if (ImGui::MenuItem(Util::Format(TR(SHOW_WINDOW), ext.NameId.c_str()), NULL, &ext.WindowOpen, ext.Active)) {}
                    if (ImGui::MenuItem(Util::Format(TR(OPEN_DIRECTORY), ext.NameId.c_str()), NULL)) {
                        Util::OpenFileExplorer(ext.Directory);
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("?##About")) {
            ImGui::CloseCurrentPopup();
            ImGui::EndMenu();
        }
        if (ImGui::IsItemClicked()) ShowAbout = true;

        ImGui::Separator();
        ImGui::Spacing();
        if (ControllerInput::AnythingConnected()) {
            bool navmodeActive = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad;
            ImGui::Text(ICON_GAMEPAD " " ICON_LONG_ARROW_RIGHT " %s", (navmodeActive) ? TR(NAVIGATION) : TR(SCRIPTING));
        }
        ImGui::Spacing();
        if (IdleMode) {
            ImGui::TextUnformatted(ICON_LEAF);
        }
        if (player->VideoLoaded() && unsavedEdits) {
            const float timeUnit = saveDuration.count() / 60.f;
            ImGui::SameLine(region.x - ImGui::GetFontSize() * 13.5f);
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_Text], TR(UNSAVED_CHANGES_FMT), (int)(timeUnit));
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(1);
#undef BINDING_STRING
}

void OpenFunscripter::SetFullscreen(bool fullscreen)
{
    static SDL_Rect restoreRect = { 0, 0, 1280, 720 };
    if (fullscreen) {
        SDL_GetWindowPosition(window, &restoreRect.x, &restoreRect.y);
        SDL_GetWindowSize(window, &restoreRect.w, &restoreRect.h);

        SDL_SetWindowResizable(window, SDL_FALSE);
        SDL_SetWindowBordered(window, SDL_FALSE);
        SDL_SetWindowPosition(window, 0, 0);
        int display = SDL_GetWindowDisplayIndex(window);
        SDL_Rect bounds;
        SDL_GetDisplayBounds(display, &bounds);

#ifdef WIN32
        // +1 pixel to the height because windows is dumb
        // when the window has the exact size as the screen windows will do some
        // bs that causes the screen to flash black when focusing a different window,file picker, etc.
        SDL_SetWindowSize(window, bounds.w, bounds.h + 1);
#else
        SDL_SetWindowSize(window, bounds.w, bounds.h);
#endif
    }
    else {
        SDL_SetWindowResizable(window, SDL_TRUE);
        SDL_SetWindowBordered(window, SDL_TRUE);
        SDL_SetWindowPosition(window, restoreRect.x, restoreRect.y);
        SDL_SetWindowSize(window, restoreRect.w, restoreRect.h);
    }
}

void OpenFunscripter::CreateDockspace() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    constexpr bool opt_fullscreen_persistant = true;
    constexpr bool opt_fullscreen = opt_fullscreen_persistant;
    constexpr ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;

    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = /*ImGuiWindowFlags_MenuBar |*/ ImGuiWindowFlags_NoDocking;
    if constexpr (opt_fullscreen) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if constexpr ((bool)(dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)) window_flags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("MainDockSpace", 0, window_flags);
    ImGui::PopStyleVar();

    if constexpr (opt_fullscreen) ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpace(MainDockspaceID, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    ShowMainMenuBar();

    ImGui::End();
}

void OpenFunscripter::ShowAboutWindow(bool* open) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);
    ImGui::Begin(TR(ABOUT), open, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("OpenFunscripter " OFS_LATEST_GIT_TAG);
    ImGui::Text("%s: %s", TR(GIT_COMMIT), OFS_LATEST_GIT_HASH);

    if (ImGui::Button(FMT("%s " ICON_GITHUB, TR(LATEST_RELEASE)), ImVec2(-1.f, 0.f))) {
        Util::OpenUrl("https://github.com/OpenFunscripter/OFS/releases/latest");
    }
    ImGui::End();
}

void OpenFunscripter::ShowStatisticsWindow(bool* open) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);
    ImGui::Begin(TR_ID(StatisticsWindowId, Tr::STATISTICS), open, ImGuiWindowFlags_None);

    const float currentTime = player->CurrentTime();
    const FunscriptAction* front = ActiveFunscript()->GetActionAtTime(currentTime, 0.001f);
    const FunscriptAction* behind = nullptr;
    if (front != nullptr) {
        behind = ActiveFunscript()->GetPreviousActionBehind(front->atS);
    }
    else {
        behind = ActiveFunscript()->GetPreviousActionBehind(currentTime);
        front = ActiveFunscript()->GetNextActionAhead(currentTime);
    }

    if (behind != nullptr) {
        FUN_ASSERT(((double)currentTime - behind->atS) * 1000.0 > 0.001, "This maybe a bug");

        ImGui::Text("%s: %.2lf ms", TR(INTERVAL), ((double)currentTime - behind->atS) * 1000.0);
        if (front != nullptr) {
            auto duration = front->atS - behind->atS;
            int32_t length = front->pos - behind->pos;
            ImGui::Text("%s: %.02lf units/s", TR(SPEED), std::abs(length) / duration);
            ImGui::Text("%s: %.2lf ms", TR(DURATION), (double)duration * 1000.0);
            if (length > 0) {
                ImGui::Text("%3d " ICON_LONG_ARROW_RIGHT " %3d"
                            " = %3d " ICON_LONG_ARROW_UP,
                    behind->pos, front->pos, length);
            }
            else {
                ImGui::Text("%3d " ICON_LONG_ARROW_RIGHT " %3d"
                            " = %3d " ICON_LONG_ARROW_DOWN,
                    behind->pos, front->pos, -length);
            }
        }
    }

    ImGui::End();
}

void OpenFunscripter::ControllerAxisPlaybackSpeed(const OFS_SDL_Event* ev) noexcept
{
    static Uint8 lastAxis = 0;
    OFS_PROFILE(__FUNCTION__);
    auto& caxis = ev->sdl.caxis;
    if ((Status & OFS_Status::OFS_GamepadSetPlaybackSpeed) && caxis.axis == lastAxis && caxis.value <= 0) {
        Status &= ~(OFS_Status::OFS_GamepadSetPlaybackSpeed);
        return;
    }

    if (caxis.value < 0) {
        return;
    }
    if (Status & OFS_Status::OFS_GamepadSetPlaybackSpeed) {
        return;
    }
    auto app = OpenFunscripter::ptr;
    if (caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
        float speed = 1.f - (caxis.value / (float)std::numeric_limits<int16_t>::max());
        app->player->SetSpeed(speed);
        lastAxis = caxis.axis;
    }
    else if (caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        float speed = 1.f + (caxis.value / (float)std::numeric_limits<int16_t>::max());
        app->player->SetSpeed(speed);
        lastAxis = caxis.axis;
    }
}

void OpenFunscripter::ScriptTimelineDoubleClick(const ShouldSetTimeEvent* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    player->SetPositionExact(ev->newTime);
}

void OpenFunscripter::ScriptTimelineSelectTime(const FunscriptShouldSelectTimeEvent* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (auto script = ev->script.lock()) {
        script->SelectTime(ev->startTime, ev->endTime, ev->clearSelection);
    }
}

void OpenFunscripter::ScriptTimelineActiveScriptChanged(const ShouldChangeActiveScriptEvent* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    UpdateNewActiveScript(ev->activeIdx);
}

void OpenFunscripter::selectTopPoints() noexcept
{
    undoSystem->Snapshot(StateType::TOP_POINTS_ONLY, ActiveFunscript());
    ActiveFunscript()->SelectTopActions();
}

void OpenFunscripter::selectMiddlePoints() noexcept
{
    undoSystem->Snapshot(StateType::MID_POINTS_ONLY, ActiveFunscript());
    ActiveFunscript()->SelectMidActions();
}

void OpenFunscripter::selectBottomPoints() noexcept
{
    undoSystem->Snapshot(StateType::BOTTOM_POINTS_ONLY, ActiveFunscript());
    ActiveFunscript()->SelectBottomActions();
}
