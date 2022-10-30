#include "OpenFunscripter.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "OFS_Simulator3D.h"
#include "GradientBar.h"
#include "FunscriptHeatmap.h"
#include "OFS_DownloadFfmpeg.h"
#include "OFS_Shader.h"
#include "OFS_MpvLoader.h"
#include "OFS_Localization.h"

#include <filesystem>

#include "stb_sprintf.h"

#include "imgui_stdlib.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "SDL.h"
#include "ImGuizmo.h"
#include "asap.h"
#include "OFS_GL.h"

// FIX: Add type checking to the deserialization. It does crash if a field type doesn't match.
// TODO: Use ImGui tables API in keybinding UI
// TODO: extend "range extender" functionality ( only extend bottom/top, range reducer )
// TODO: render simulator relative to video position & zoom
// TODO: make speed coloring configurable
// TODO: higher level representation for selections (array of intervals)
//       use that to add the ability to cycle between selection modes
// TODO: OFS_ScriptTimeline selections cause alot of unnecessary overdraw. find a way to not have any overdraw
// TODO: binding to toggle video player fullscreen
// BUG: audio files don't have a frame count which will cause recording mode to fail horribly

// the video player supports a lot more than these
// these are the ones looked for when importing funscripts
std::array<const char*, 6> OpenFunscripter::SupportedVideoExtensions {
    ".mp4",
    ".mkv",
    ".webm",
    ".wmv",
    ".avi",
    ".m4v",
};

std::array<const char*, 4> OpenFunscripter::SupportedAudioExtensions{
    ".mp3",
    ".ogg",
    ".flac",
    ".wav",
};

OpenFunscripter* OpenFunscripter::ptr = nullptr;
constexpr const char* GlslVersion = "#version 300 es";

static ImGuiID MainDockspaceID;
constexpr const char* StatisticsWindowId = "###STATISTICS";
constexpr const char* ActionEditorWindowId = "###ACTION_EDITOR";

constexpr int DefaultWidth = 1920;
constexpr int DefaultHeight= 1080;

constexpr int AutoBackupIntervalSeconds = 60;

bool OpenFunscripter::imguiSetup() noexcept
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    if(!ImGui::CreateContext()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigViewportsNoDecoration = false;
    io.ConfigViewportsNoAutoMerge = false;
    io.ConfigViewportsNoTaskBarIcon = false;
    io.ConfigDockingTransparentPayload = true;

    static auto imguiIniPath = Util::Prefpath("imgui.ini");
    io.IniFilename = imguiIniPath.c_str();
    
    settings->SetTheme(settings->data().current_theme);

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    LOGF_DEBUG("init imgui with glsl: %s", GlslVersion);
    ImGui_ImplOpenGL3_Init(GlslVersion);

    OFS_DynFontAtlas::FontOverride = settings->data().font_override;
    OFS_DynFontAtlas::Init();
    OFS_Translator::Init();
    if(!settings->data().language_csv.empty()) {
        if(OFS_Translator::ptr->LoadTranslation(settings->data().language_csv.c_str())) {
            OFS_DynFontAtlas::AddTranslationText();
        }
    }

    {
		// hook into paste for the dynamic atlas
		auto& io = ImGui::GetIO();
		if(io.GetClipboardTextFn) {
			static auto OriginalSDL2_GetClipboardFunc = io.GetClipboardTextFn;
			io.GetClipboardTextFn = [](void* d) -> const char* {
				auto clipboard = OriginalSDL2_GetClipboardFunc(d);
				OFS_DynFontAtlas::AddText(clipboard);
				return clipboard;
			};
		}
	}
  
    return true;
}

OpenFunscripter::~OpenFunscripter()
{
    tcode->save();

    // needs a certain destruction order
    playerControls.Destroy();
    scripting.reset();
    controllerInput.reset();
    specialFunctions.reset();
    LoadedProject.reset();

    settings->saveSettings();
    keybinds.save();
    
    playerWindow.reset();
    player.reset();
    events.reset();
}

bool OpenFunscripter::Init(int argc, char* argv[])
{
    OFS_FileLogger::Init();
    Util::InMainThread();
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    ptr = this;
    auto prefPath = Util::Prefpath("");
    Util::CreateDirectories(prefPath);

    settings = std::make_unique<OFS_Settings>(Util::Prefpath("config.json"));
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_ERROR(SDL_GetError());
        return false;
    }
    if(!OFS_MpvLoader::Load()) {
        LOG_ERROR("Failed to load mpv library.");
        return false;
    }

#if __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac according to imgui example
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0 /*| SDL_GL_CONTEXT_DEBUG_FLAG*/);
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

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
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN
    );

    SDL_Rect display;
    int windowDisplay = SDL_GetWindowDisplayIndex(window);
    SDL_GetDisplayBounds(windowDisplay, &display);
    if (DefaultWidth >= display.w || DefaultHeight >= display.h) {
        SDL_MaximizeWindow(window);
    }
    
    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(settings->data().vsync);

    if (!gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress)) {
        LOG_ERROR("Failed to load glad.");
        return false;
    }
    
    if (!imguiSetup()) {
        LOG_ERROR("Failed to setup ImGui");
        return false;
    }

    events = std::make_unique<EventSystem>();
    events->setup();
    // register custom events with sdl
    OFS_Events::RegisterEvents();
    FunscriptEvents::RegisterEvents();
    VideoEvents::RegisterEvents();
    KeybindingEvents::RegisterEvents();
    ScriptTimelineEvents::RegisterEvents();

    LoadedProject = std::make_unique<OFS_Project>();
    
    player = std::make_unique<OFS_Videoplayer>();
    if(!player->Init(settings->data().force_hw_decoding)) {
        LOG_ERROR("Failed to initialize videoplayer.");
        return false;
    }
    player->SetPaused(true);

    playerWindow = std::make_unique<OFS_VideoplayerWindow>();
    if (!playerWindow->Init(player.get())) {
        LOG_ERROR("Failed to init videoplayer window");
        return false;
    }

    OFS_ScriptSettings::player = &playerWindow->settings;
    playerControls.Init(player.get());
    closeProject(true);

    undoSystem = std::make_unique<UndoSystem>(&LoadedProject->Funscripts);

    keybinds.setup(*events);
    registerBindings(); // needs to happen before setBindings
    keybinds.load(Util::Prefpath("keybinds.json"));

    scriptTimeline.setup(undoSystem.get());

    scripting = std::make_unique<ScriptingMode>();
    scripting->Init();
    events->Subscribe(FunscriptEvents::FunscriptActionsChangedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptChanged));
    events->Subscribe(SDL_DROPFILE, EVENT_SYSTEM_BIND(this, &OpenFunscripter::DragNDrop));
    events->Subscribe(VideoEvents::VideoLoaded, EVENT_SYSTEM_BIND(this, &OpenFunscripter::VideoLoaded));
    events->Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ControllerAxisPlaybackSpeed));
    events->Subscribe(ScriptTimelineEvents::FunscriptActionClicked, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActionClicked));
    events->Subscribe(ScriptTimelineEvents::SetTimePosition, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineDoubleClick));
    events->Subscribe(ScriptTimelineEvents::FunscriptSelectTime, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineSelectTime));
    events->Subscribe(VideoEvents::PlayPauseChanged, EVENT_SYSTEM_BIND(this, &OpenFunscripter::PlayPauseChange));
    events->Subscribe(ScriptTimelineEvents::ActiveScriptChanged, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActiveScriptChanged));

    // hook up settings
    OFS_Project::ProjSettings::Simulator = &simulator.simulator;

    if (argc > 1) {
        const char* path = argv[1];
        openFile(path);
    } else if (!settings->data().recentFiles.empty()) {
        auto& project = settings->data().recentFiles.back().projectPath;
        if (!project.empty()) {
            openProject(project);           
        }
    }

    specialFunctions = std::make_unique<SpecialFunctionsWindow>();
    controllerInput = std::make_unique<ControllerInput>();
    controllerInput->setup(*events);
    simulator.setup();

    sim3D = std::make_unique<Simulator3D>();
    sim3D->setup();

    // callback that renders the simulator right after the video
    playerWindow->OnRenderCallback = [](const ImDrawList * parent_list, const ImDrawCmd * cmd) {
        auto app = OpenFunscripter::ptr;
        if (app->settings->data().show_simulator_3d) {
            app->sim3D->renderSim();
        }
    };

    HeatmapGradient::Init();
    tcode = std::make_unique<TCodePlayer>();
    tcode->loadSettings(Util::Prefpath("tcode.json"));
    extensions = std::make_unique<OFS_LuaExtensions>();
    extensions->Init();

#ifdef WIN32
    OFS_DownloadFfmpeg::FfmpegMissing = !Util::FileExists(Util::FfmpegPath().u8string());
#endif

    // Load potentially missing glyphs of recent files
    for(auto& recentFile : settings->data().recentFiles) {
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
    {
        KeybindingGroup group("Actions", Tr::ACTIONS_BINDING_GROUP);
        // DELETE ACTION
        auto& remove_action = group.bindings.emplace_back(
            "remove_action",
            Tr::ACTION_REMOVE_ACTION,
            true,
            [&](void*) { removeAction(); }
        );
        remove_action.key = Keybinding(
            SDLK_DELETE,
            0
        );
        remove_action.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_B,
            false
        );

        //ADD ACTIONS
        auto& action_0 = group.bindings.emplace_back(
            "action 0",
            Tr::ACTION_ACTION_0,
            true,
            [&](void*) { addEditAction(0); }
        );
        action_0.key = Keybinding(
            SDLK_KP_0,
            0
        );

        auto& action_10 = group.bindings.emplace_back(
            "action 10",
            Tr::ACTION_ACTION_10,
            true,
            [&](void*) { addEditAction(10); }
        );
        action_10.key = Keybinding(
            SDLK_KP_1,
            0
        );

        auto& action_20 = group.bindings.emplace_back(
            "action 20",
            Tr::ACTION_ACTION_20,
            true,
            [&](void*) { addEditAction(20); }
        );
        action_20.key = Keybinding(
            SDLK_KP_2,
            0
        );

        auto& action_30 = group.bindings.emplace_back(
            "action 30",
            Tr::ACTION_ACTION_30,
            true,
            [&](void*) { addEditAction(30); }
        );
        action_30.key = Keybinding(
            SDLK_KP_3,
            0
        );

        auto& action_40 = group.bindings.emplace_back(
            "action 40",
            Tr::ACTION_ACTION_40,
            true,
            [&](void*) { addEditAction(40); }
        );
        action_40.key = Keybinding(
            SDLK_KP_4,
            0
        );

        auto& action_50 = group.bindings.emplace_back(
            "action 50",
            Tr::ACTION_ACTION_50,
            true,
            [&](void*) { addEditAction(50); }
        );
        action_50.key = Keybinding(
            SDLK_KP_5,
            0
        );

        auto& action_60 = group.bindings.emplace_back(
            "action 60",
            Tr::ACTION_ACTION_60,
            true,
            [&](void*) { addEditAction(60); }
        );
        action_60.key = Keybinding(
            SDLK_KP_6,
            0
        );

        auto& action_70 = group.bindings.emplace_back(
            "action 70",
            Tr::ACTION_ACTION_70,
            true,
            [&](void*) { addEditAction(70); }
        );
        action_70.key = Keybinding(
            SDLK_KP_7,
            0
        );

        auto& action_80 = group.bindings.emplace_back(
            "action 80",
            Tr::ACTION_ACTION_80,
            true,
            [&](void*) { addEditAction(80); }
        );
        action_80.key = Keybinding(
            SDLK_KP_8,
            0
        );

        auto& action_90 = group.bindings.emplace_back(
            "action 90",
            Tr::ACTION_ACTION_90,
            true,
            [&](void*) { addEditAction(90); }
        );
        action_90.key = Keybinding(
            SDLK_KP_9,
            0
        );

        auto& action_100 = group.bindings.emplace_back(
            "action 100",
            Tr::ACTION_ACTION_100,
            true,
            [&](void*) { addEditAction(100); }
        );
        action_100.key = Keybinding(
            SDLK_KP_DIVIDE,
            0
        );

        keybinds.registerBinding(std::move(group));
    }

    {
        KeybindingGroup group("Core", Tr::CORE_BINDING_GROUP);
        // SAVE
        auto& save_project = group.bindings.emplace_back(
            "save_project",
            Tr::ACTION_SAVE_PROJECT,
            true,
            [&](void*) { saveProject(); }
        );
        save_project.key = Keybinding(
            SDLK_s,
            KMOD_CTRL
        );

        auto& quick_export = group.bindings.emplace_back(
            "quick_export",
            Tr::ACTION_QUICK_EXPORT,
            true,
            [&](void*) { quickExport(); }
        );
        quick_export.key = Keybinding(
            SDLK_s,
            KMOD_CTRL | KMOD_SHIFT
        );

        auto& sync_timestamp = group.bindings.emplace_back(
            "sync_timestamps",
            Tr::ACTION_SYNC_TIME_WITH_PLAYER,
            true,
            [&](void*) { player->SyncWithPlayerTime(); }
        );
        sync_timestamp.key = Keybinding(
            SDLK_s,
            0
        );

        auto& cycle_loaded_forward_scripts = group.bindings.emplace_back(
            "cycle_loaded_forward_scripts",
            Tr::ACTION_CYCLE_FORWARD_LOADED_SCRIPTS,
            true,
            [&](void*) {
                do {
                    ActiveFunscriptIdx++;
                    ActiveFunscriptIdx %= LoadedFunscripts().size();
                } while (!ActiveFunscript()->Enabled);
                UpdateNewActiveScript(ActiveFunscriptIdx);
            }
        );
        cycle_loaded_forward_scripts.key = Keybinding(
            SDLK_PAGEDOWN,
            0
        );

        auto& cycle_loaded_backward_scripts = group.bindings.emplace_back(
            "cycle_loaded_backward_scripts",
            Tr::ACTION_CYCLE_BACKWARD_LOADED_SCRIPTS,
            true,
            [&](void*) {
                do {
                    ActiveFunscriptIdx--;
                    ActiveFunscriptIdx %= LoadedFunscripts().size();
                } while (!ActiveFunscript()->Enabled);
                UpdateNewActiveScript(ActiveFunscriptIdx);
            }
        );
        cycle_loaded_backward_scripts.key = Keybinding(
            SDLK_PAGEUP,
            0
        );

        auto& reload_translation = group.bindings.emplace_back(
            "reload_translation_csv",
            Tr::ACTION_RELOAD_TRANSLATION,
            true,
            [&](void*)
            {
                if(!settings->data().language_csv.empty())
                {
                    if(OFS_Translator::ptr->LoadTranslation(settings->data().language_csv.c_str()))
                    {
                        OFS_DynFontAtlas::AddTranslationText();
                    }
                }
            }
        );

        keybinds.registerBinding(std::move(group));
    }
    {
        KeybindingGroup group("Navigation", Tr::NAVIGATION_BINDING_GROUP);
        // JUMP BETWEEN ACTIONS
        auto& prev_action = group.bindings.emplace_back(
            "prev_action",
            Tr::ACTION_PREVIOUS_ACTION,
            false,
            [&](void*) {
                auto action = ActiveFunscript()->GetPreviousActionBehind(player->CurrentTime() - 0.001f);
                if (action != nullptr) player->SetPositionExact(action->atS);
            }
        );
        prev_action.key = Keybinding(
            SDLK_DOWN,
            0
        );
        prev_action.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_DPAD_DOWN,
            false
        );

        auto& next_action = group.bindings.emplace_back(
            "next_action",
            Tr::ACTION_NEXT_ACTION,
            false,
            [&](void*) {
                auto action = ActiveFunscript()->GetNextActionAhead(player->CurrentTime() + 0.001f);
                if (action != nullptr) player->SetPositionExact(action->atS);
            }
        );
        next_action.key = Keybinding(
            SDLK_UP,
            0
        );
        next_action.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_DPAD_UP,
            false
        );

        auto& prev_action_multi = group.bindings.emplace_back(
            "prev_action_multi",
            Tr::ACTION_PREVIOUS_ACTION_MULTI,
            false,
            [&](void*) {
                bool foundAction = false;
                float closestTime = std::numeric_limits<float>::max();
                float currentTime = player->CurrentTime();

                for(int i=0; i < LoadedFunscripts().size(); i++) {
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
            }
        );
        prev_action_multi.key = Keybinding(
            SDLK_DOWN,
            KMOD_CTRL
        );

        auto& next_action_multi = group.bindings.emplace_back(
            "next_action_multi",
            Tr::ACTION_NEXT_ACTION_MULTI,
            false,
            [&](void*) {
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
            }
        );
        next_action_multi.key = Keybinding(
            SDLK_UP,
            KMOD_CTRL
        );

        // FRAME CONTROL
        auto& prev_frame = group.bindings.emplace_back(
            "prev_frame",
            Tr::ACTION_PREV_FRAME,
            false,
            [&](void*) { 
                if (player->IsPaused()) {
                    scripting->PreviousFrame(); 
                }
            }
        );
        prev_frame.key = Keybinding(
            SDLK_LEFT,
            0
        );
        prev_frame.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_DPAD_LEFT,
            false
        );

        auto& next_frame = group.bindings.emplace_back(
            "next_frame",
            Tr::ACTION_NEXT_FRAME,
            false,
            [&](void*) { 
                if (player->IsPaused()) {
                    scripting->NextFrame(); 
                }
            }
        );
        next_frame.key = Keybinding(
            SDLK_RIGHT,
            0
        );
        next_frame.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
            false
        );


        auto& fast_step = group.bindings.emplace_back(
            "fast_step",
            Tr::ACTION_FAST_STEP,
            false,
            [&](void*) {
                int32_t frameStep = settings->data().fast_step_amount;
                player->SeekFrames(frameStep);
            }
        );
        fast_step.key = Keybinding(
            SDLK_RIGHT,
            KMOD_CTRL
        );

        auto& fast_backstep = group.bindings.emplace_back(
            "fast_backstep",
            Tr::ACTION_FAST_BACKSTEP,
            false,
            [&](void*) {
                int32_t frameStep = settings->data().fast_step_amount;
                player->SeekFrames(-frameStep);
            }
        );
        fast_backstep.key = Keybinding(
            SDLK_LEFT,
            KMOD_CTRL
        );
        keybinds.registerBinding(std::move(group));
    }
    
    {
        KeybindingGroup group("Utility", Tr::UTILITY_BINDING_GROUP);
        // UNDO / REDO
        auto& undo = group.bindings.emplace_back(
            "undo",
            Tr::ACTION_UNDO,
            false,
            [&](void*) { 
                this->Undo();
            }
        );
        undo.key = Keybinding(
            SDLK_z,
            KMOD_CTRL
        );

        auto& redo = group.bindings.emplace_back(
            "redo",
            Tr::ACTION_REDO,
            false,
            [&](void*) { 
                this->Redo();
            }
        ); 
        redo.key = Keybinding(
            SDLK_y,
            KMOD_CTRL
        );

        // COPY / PASTE
        auto& copy = group.bindings.emplace_back(
            "copy",
            Tr::ACTION_COPY,
            true,
            [&](void*) { copySelection(); }
        );
        copy.key = Keybinding(
            SDLK_c,
            KMOD_CTRL
        );

        auto& paste = group.bindings.emplace_back(
            "paste",
            Tr::ACTION_PASTE,
            true,
            [&](void*) { pasteSelection(); }
        );
        paste.key = Keybinding(
            SDLK_v,
            KMOD_CTRL
        );

        auto& paste_exact = group.bindings.emplace_back(
            "paste_exact",
            Tr::ACTION_PASTE_EXACT,
            true,
            [&](void*) {pasteSelectionExact(); }
        );
        paste_exact.key = Keybinding(
            SDLK_v,
            KMOD_CTRL | KMOD_SHIFT
        );

        auto& cut = group.bindings.emplace_back(
            "cut",
            Tr::ACTION_CUT,
            true,
            [&](void*) { cutSelection(); }
        );
        cut.key = Keybinding(
            SDLK_x,
            KMOD_CTRL
        );

        auto& select_all = group.bindings.emplace_back(
            "select_all",
            Tr::ACTION_SELECT_ALL,
            true,
            [&](void*) { ActiveFunscript()->SelectAll(); }
        );
        select_all.key = Keybinding(
            SDLK_a,
            KMOD_CTRL
        );

        auto& deselect_all = group.bindings.emplace_back(
            "deselect_all",
            Tr::ACTION_DESELECT_ALL,
            true,
            [&](void*) { ActiveFunscript()->ClearSelection(); }
        );
        deselect_all.key = Keybinding(
            SDLK_d,
            KMOD_CTRL
        );

        auto& select_all_left = group.bindings.emplace_back(
            "select_all_left",
            Tr::ACTION_SELECT_ALL_LEFT,
            true,
            [&](void*) { ActiveFunscript()->SelectTime(0, player->CurrentTime()); }
        );
        select_all_left.key = Keybinding(
            SDLK_LEFT,
            KMOD_CTRL | KMOD_ALT
        );

        auto& select_all_right = group.bindings.emplace_back(
            "select_all_right",
            Tr::ACTION_SELECT_ALL_RIGHT,
            true,
            [&](void*) { ActiveFunscript()->SelectTime(player->CurrentTime(), player->Duration()); }
        );
        select_all_right.key = Keybinding(
            SDLK_RIGHT,
            KMOD_CTRL | KMOD_ALT
        );

        auto& select_top_points = group.bindings.emplace_back(
            "select_top_points",
            Tr::ACTION_SELECT_TOP,
            true,
            [&](void*) { selectTopPoints(); }
        );
        
        auto& select_middle_points = group.bindings.emplace_back(
            "select_middle_points",
            Tr::ACTION_SELECT_MID,
            true,
            [&](void*) { selectMiddlePoints(); }
        );
        
        auto& select_bottom_points = group.bindings.emplace_back(
            "select_bottom_points",
            Tr::ACTION_SELECT_BOTTOM,
            true,
            [&](void*) { selectBottomPoints(); }
        );

        auto& toggle_mirror_mode = group.bindings.emplace_back(
            "toggle_mirror_mode",
            Tr::ACTION_TOGGLE_MIRROR_MODE,
            true,
            [&](void*) { if (LoadedFunscripts().size() > 1) { settings->data().mirror_mode = !settings->data().mirror_mode; }}
        );
        toggle_mirror_mode.key = Keybinding(
            SDLK_PRINTSCREEN,
            0
        );

        // SCREENSHOT VIDEO
        auto& save_frame_as_image = group.bindings.emplace_back(
            "save_frame_as_image",
            Tr::ACTION_SAVE_FRAME,
            true,
            [&](void*) { 
                auto screenshotDir = Util::Prefpath("screenshot");
                player->SaveFrameToImage(screenshotDir);
            }
        );
        save_frame_as_image.key = Keybinding(
            SDLK_F2,
            0
        );

        // CHANGE SUBTITLES
        auto& cycle_subtitles = group.bindings.emplace_back(
            "cycle_subtitles",
            Tr::ACTION_CYCLE_SUBTITLES,
            true,
            [&](void*) { player->CycleSubtitles(); }
        );
        cycle_subtitles.key = Keybinding(
            SDLK_j,
            0
        );

        // FULLSCREEN
        auto& fullscreen_toggle = group.bindings.emplace_back(
            "fullscreen_toggle",
            Tr::ACTION_TOGGLE_FULLSCREEN,
            true,
            [&](void*) { Status ^= OFS_Status::OFS_Fullscreen; SetFullscreen(Status & OFS_Status::OFS_Fullscreen); }
        );
        fullscreen_toggle.key = Keybinding(
            SDLK_F10,
            0
        );
        keybinds.registerBinding(std::move(group));
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
            if (closest != nullptr) { app->player->SetPositionExact(closest->atS); }
            else { app->player->SetPositionExact(app->ActiveFunscript()->Selection().front().atS); }
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
    {
        KeybindingGroup group("Moving", Tr::MOVING_BINDING_GROUP);
        auto& move_actions_up_ten = group.bindings.emplace_back(
            "move_actions_up_ten",
            Tr::ACTION_MOVE_UP_10,
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(10);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos + 10, 0, 100)));
                    }
                }
            }
        );

        auto& move_actions_down_ten = group.bindings.emplace_back(
            "move_actions_down_ten",
            Tr::ACTION_MOVE_DOWN_10,
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(-10);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos - 10, 0, 100)));
                    }
                }
            }
        );


        auto& move_actions_up_five = group.bindings.emplace_back(
            "move_actions_up_five",
            Tr::ACTION_MOVE_UP_5,
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(5);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos + 5, 0, 100)));
                    }
                }
            }
        );

        auto& move_actions_down_five = group.bindings.emplace_back(
            "move_actions_down_five",
            Tr::ACTION_MOVE_DOWN_5,
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(-5);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos - 5, 0, 100)));
                    }
                }
            }
        );


        auto& move_actions_left_snapped = group.bindings.emplace_back(
            "move_actions_left_snapped",
            Tr::ACTION_MOVE_ACTIONS_LEFT_SNAP,
            false,
            [&](void*) {
                move_actions_horizontal_with_video(false);
            }
        );
        move_actions_left_snapped.key = Keybinding(
            SDLK_LEFT,
            KMOD_CTRL | KMOD_SHIFT
        );

        auto& move_actions_right_snapped = group.bindings.emplace_back(
            "move_actions_right_snapped",
            Tr::ACTION_MOVE_ACTIONS_RIGHT_SNAP,
            false,
            [&](void*) {
                move_actions_horizontal_with_video(true);
            }
        );
        move_actions_right_snapped.key = Keybinding(
            SDLK_RIGHT,
            KMOD_CTRL | KMOD_SHIFT
        );

        auto& move_actions_left = group.bindings.emplace_back(
            "move_actions_left",
            Tr::ACTION_MOVE_ACTIONS_LEFT,
            false,
            [&](void*) {
                move_actions_horizontal(false);
            }
        );
        move_actions_left.key = Keybinding(
            SDLK_LEFT,
            KMOD_SHIFT
        );

        auto& move_actions_right = group.bindings.emplace_back(
            "move_actions_right",
            Tr::ACTION_MOVE_ACTIONS_RIGHT,
            false,
            [&](void*) {
                move_actions_horizontal(true);
            }
        );
        move_actions_right.key = Keybinding(
            SDLK_RIGHT,
            KMOD_SHIFT
        );

        // MOVE SELECTION UP/DOWN
        auto& move_actions_up = group.bindings.emplace_back(
            "move_actions_up",
            Tr::ACTION_MOVE_ACTIONS_UP,
            false,
            [&](void*) {
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
            }
        );
        move_actions_up.key = Keybinding(
            SDLK_UP,
            KMOD_SHIFT
        );
        auto& move_actions_down = group.bindings.emplace_back(
            "move_actions_down",
            Tr::ACTION_MOVE_ACTIONS_DOWN,
            false,
            [&](void*) {
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
            }
        );
        move_actions_down.key = Keybinding(
            SDLK_DOWN,
            KMOD_SHIFT
        );

        auto& move_action_to_current_pos = group.bindings.emplace_back(
            "move_action_to_current_pos",
            Tr::ACTION_MOVE_TO_CURRENT_POSITION,
            true,
            [&](void*) {
                auto closest = ActiveFunscript()->GetClosestAction(player->CurrentTime());
                if (closest != nullptr) {
                    undoSystem->Snapshot(StateType::MOVE_ACTION_TO_CURRENT_POS, ActiveFunscript());
                    ActiveFunscript()->EditAction(*closest, FunscriptAction(player->CurrentTime(), closest->pos));
                }
            }
        );
        move_action_to_current_pos.key = Keybinding(
            SDLK_END,
            0
        );

        keybinds.registerBinding(std::move(group));
    }
    // FUNCTIONS
    {
        KeybindingGroup group("Special", Tr::SPECIAL_BINDING_GROUP);
        auto& equalize = group.bindings.emplace_back(
            "equalize_actions",
            Tr::ACTION_EQUALIZE_ACTIONS,
            true,
            [&](void*) { equalizeSelection(); }
        );
        equalize.key = Keybinding(
            SDLK_e,
            0
        );

        auto& invert = group.bindings.emplace_back(
            "invert_actions",
            Tr::ACTION_INVERT_ACTIONS,
            true,
            [&](void*) { invertSelection(); }
        );
        invert.key = Keybinding(
            SDLK_i,
            0
        );
        auto& isolate = group.bindings.emplace_back(
            "isolate_action",
            Tr::ACTION_ISOLATE_ACTION,
            true,
            [&](void*) { isolateAction(); }
        );
        isolate.key = Keybinding(
            SDLK_r,
            0
        );

        auto& repeat_stroke = group.bindings.emplace_back(
            "repeat_stroke",
            Tr::ACTION_REPEAT_STROKE,
            true,
            [&](void*) { repeatLastStroke(); }
        );
        repeat_stroke.key = Keybinding(
            SDLK_HOME,
            0
        );

        keybinds.registerBinding(std::move(group));
    }

    // VIDEO CONTROL
    {
        KeybindingGroup group("Videoplayer", Tr::VIDEOPLAYER_BINDING_GROUP);
        // PLAY / PAUSE
        auto& toggle_play = group.bindings.emplace_back(
            "toggle_play",
            Tr::ACTION_TOGGLE_PLAY,
            true,
            [&](void*) { player->TogglePlay(); }
        );
        toggle_play.key = Keybinding(
            SDLK_SPACE,
            0
        );
        toggle_play.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_START,
            false
        );
        // PLAYBACK SPEED
        auto& decrement_speed = group.bindings.emplace_back(
            "decrement_speed",
            Tr::ACTION_REDUCE_PLAYBACK_SPEED,
            true,
            [&](void*) { player->AddSpeed(-0.10); }
        );
        decrement_speed.key = Keybinding(
            SDLK_KP_MINUS,
            0
        );
        auto& increment_speed = group.bindings.emplace_back(
            "increment_speed",
            Tr::ACTION_INCREASE_PLAYBACK_SPEED,
            true,
            [&](void*) { player->AddSpeed(0.10); }
        );
        increment_speed.key = Keybinding(
            SDLK_KP_PLUS,
            0
        );

        auto& goto_start = group.bindings.emplace_back(
            "goto_start",
            Tr::ACTION_GO_TO_START,
            true,
            [&](void*) {
                player->SetPositionPercent(0.f, false); 
            }
        );
        goto_start.key = Keybinding(
            0,
            0
        );

        auto& goto_end = group.bindings.emplace_back(
            "goto_end",
            Tr::ACTION_GO_TO_END,
            true,
            [&](void*) {
                player->SetPositionPercent(1.f, false);
            }
        );
        goto_end.key = Keybinding(
            0,
            0
        );

        keybinds.registerBinding(std::move(group));
    }

    {
        KeybindingGroup group("Extensions", Tr::EXTENSIONS_BINDING_GROUP);
        auto& reload_enabled_extensions = group.bindings.emplace_back(
            "reload_enabled_extensions",
            Tr::ACTION_RELOAD_ENABLED_EXTENSIONS,
            true,
            [&](void*) { 
                extensions->ReloadEnabledExtensions();
            }
        );
        keybinds.registerBinding(std::move(group));
    }

    {
        KeybindingGroup group("Controller", Tr::CONTROLLER_BINDING_GROUP);
        auto& toggle_nav_mode = group.bindings.emplace_back(
            "toggle_controller_navmode",
            Tr::ACTION_TOGGLE_CONTROLLER_NAV,
            true,
            [&](void*) { 
                auto& io = ImGui::GetIO();
                io.ConfigFlags ^= ImGuiConfigFlags_NavEnableGamepad;
            }
        );
        toggle_nav_mode.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_LEFTSTICK,
            true
        );

        auto& seek_forward_second = group.bindings.emplace_back(
            "seek_forward_second",
            Tr::ACTION_SEEK_FORWARD_1,
            false,
            [&](void*) { player->SeekRelative(1); }
        );
        seek_forward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
            false
        );

        auto& seek_backward_second = group.bindings.emplace_back(
            "seek_backward_second",
            Tr::ACTION_SEEK_BACKWARD_1,
            false,
            [&](void*) { player->SeekRelative(-1); }
        );
        seek_backward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            false
        );

        auto& add_action_controller = group.bindings.emplace_back(
            "add_action_controller",
            Tr::ACTION_ADD_ACTION_CONTROLLER,
            true,
            [&](void*) { addEditAction(100); }
        );
        add_action_controller.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_A,
            false
        );

        auto& toggle_recording_mode = group.bindings.emplace_back(
            "toggle_recording_mode",
            Tr::ACTION_TOGGLE_RECORDING_MODE,
            true,
            [&](void*) {
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
            }
        );
        toggle_recording_mode.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_BACK,
            false
        );

        auto& controller_select = group.bindings.emplace_back(
            "set_selection_controller",
            Tr::ACTION_CONTROLLER_SELECT,
            true,
            [&](void*) {
                if (scriptTimeline.selectionStart() < 0) {
                    scriptTimeline.setStartSelection(player->CurrentTime());
                }
                else {
                    auto tmp = player->CurrentTime();
                    auto [min, max] = std::minmax<float>(scriptTimeline.selectionStart(), tmp);
                    ActiveFunscript()->SelectTime(min, max);
                    scriptTimeline.setStartSelection(-1);
                }
            }
        );
        controller_select.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSTICK,
            false
        );

        auto& set_playbackspeed_controller = group.bindings.emplace_back(
            "set_current_playbackspeed_controller",
            Tr::ACTION_SET_PLAYBACK_SPEED,
            true,
            [&](void*) {
                Status |= OFS_Status::OFS_GamepadSetPlaybackSpeed;
            }
        );
        set_playbackspeed_controller.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_X,
            false
        );
        keybinds.registerBinding(std::move(group));
    }

    // passive modifiers
    {
        PassiveBindingGroup group("Point timeline", Tr::PASSIVE_GROUP_TIMELINE);
        auto& move_or_add_point_modifier = group.bindings.emplace_back(
            "move_or_add_point_modifier",
            Tr::MOD_MOVE_OR_ADD_POINT
        );
        move_or_add_point_modifier.key = Keybinding(
            0,
            KMOD_SHIFT
        );

        //auto& select_top_points_modifier = group.bindings.emplace_back(
        //    "select_top_points_modifier",
        //    "Select top points",
        //    false
        //);
        //select_top_points_modifier.key = Keybinding(
        //    0,
        //    KMOD_ALT
        //);
        //
        //auto& select_bottom_points_modifier = group.bindings.emplace_back(
        //    "select_bottom_points_modifier",
        //    "Select bottom points",
        //    false
        //);
        //select_bottom_points_modifier.key = Keybinding(
        //    0,
        //    KMOD_ALT
        //);
        //
        //auto& select_middle_points_modifier = group.bindings.emplace_back(
        //    "select_middle_points_modifier",
        //    "Select middle points",
        //    false
        //);
        //select_middle_points_modifier.key = Keybinding(
        //    0,
        //    KMOD_ALT
        //);
           
        keybinds.registerPassiveBindingGroup(std::move(group));
    }

    {
        PassiveBindingGroup group("Simulator", Tr::PASSIVE_GROUP_SIMULATOR);
        auto& click_add_point_simulator = group.bindings.emplace_back(
            "click_add_point_simulator",
            Tr::MOD_CLICK_SIM_ADD_PONT
        );
        click_add_point_simulator.key = Keybinding(
            0,
            KMOD_SHIFT
        );

        keybinds.registerPassiveBindingGroup(std::move(group));
    }
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
    if(OFS_DynFontAtlas::NeedsRebuild()) {
		OFS_DynFontAtlas::RebuildFont(settings->data().default_font_size);
	}
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void OpenFunscripter::render() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }
    glFlush();
    glFinish();
}

void OpenFunscripter::processEvents() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    SDL_Event event;
    bool IsExiting = false;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
            {
                if (!IsExiting) {
                    exitApp();
                    IsExiting = true;
                }
                break;
            }
            case SDL_WINDOWEVENT:
            {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                    if (!IsExiting) {
                        exitApp();
                        IsExiting = true;
                    }
                }
                break;
            }
            case SDL_TEXTINPUT: 
            {
                OFS_DynFontAtlas::AddText(event.text.text);
                break;
            }
        }
        switch(event.type) {
            case SDL_CONTROLLERAXISMOTION:
                if(std::abs(event.caxis.value) < 2000) break;
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
        events->Propagate(event);
    }
}

void OpenFunscripter::FunscriptChanged(SDL_Event& ev) noexcept
{
    // the event passes the address of the Funscript
    // by searching for the funscript with the same address 
    // the index can be retrieved
    void* ptr = ev.user.data1;
    for(int i=0, size=LoadedFunscripts().size(); i < size; i += 1) {
        if(LoadedFunscripts()[i].get() == ptr) {
            extensions->ScriptChanged(i);
            break;
        }
    }
    
    Status = Status | OFS_Status::OFS_GradientNeedsUpdate;
}

void OpenFunscripter::ScriptTimelineActionClicked(SDL_Event& ev) noexcept
{
    auto& [btn_ev, action] = *((ScriptTimelineEvents::ActionClickedEventArgs*)ev.user.data1);
    auto& button = btn_ev.button; // turns out I don't need this...

    if (SDL_GetModState() & KMOD_CTRL) {
        ActiveFunscript()->SelectAction(action);
    }
    else {
        player->SetPositionExact(action.atS);
    }
}

void OpenFunscripter::DragNDrop(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);

    std::string dragNDropFile = ev.drop.file;
    closeWithoutSavingDialog([this, dragNDropFile](){
        openFile(dragNDropFile);
    });

    SDL_free(ev.drop.file);
}

void OpenFunscripter::VideoLoaded(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    LoadedProject->Metadata.duration = player->Duration();
    player->SetPositionExact(LoadedProject->Settings.lastPlayerPosition);

    Status |= OFS_Status::OFS_GradientNeedsUpdate;
    
    FUN_ASSERT(ev.user.data1 != nullptr, "data is null");
    const std::string& VideoName = *(std::string*)ev.user.data1;
    if(!VideoName.empty()) {
        scriptTimeline.ClearAudioWaveform();
    }

    tcode->reset();
    {
        std::vector<std::shared_ptr<const Funscript>> scripts;
        scripts.assign(LoadedFunscripts().begin(), LoadedFunscripts().end());
        tcode->setScripts(std::move(scripts));
    }
}

void OpenFunscripter::PlayPauseChange(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if ((intptr_t)ev.user.data1) // true == paused
    {
        tcode->stop();
    }
    else
    {
        std::vector<std::shared_ptr<const Funscript>> scripts;
        scripts.assign(LoadedFunscripts().begin(), LoadedFunscripts().end());
        tcode->play(player->CurrentTime(), std::move(scripts));
    }
}

void OpenFunscripter::update() noexcept {
    OFS_PROFILE(__FUNCTION__);
    const float delta = ImGui::GetIO().DeltaTime;
    extensions->Update(delta);
    player->Update(delta);
    ActiveFunscript()->update();
    ControllerInput::UpdateControllers(settings->data().buttonRepeatIntervalMs);
    scripting->Update();
    scriptTimeline.Update();
    
    if(LoadedProject->Valid) {
        LoadedProject->Update(delta, IdleMode);
    }

    if (Status & OFS_Status::OFS_AutoBackup) {
        autoBackup();
    }

    tcode->sync(player->CurrentTime(), player->CurrentSpeed());
}

void OpenFunscripter::autoBackup() noexcept
{
    if (!LoadedProject->Loaded) { return; }
    std::chrono::duration<float> timeSinceBackup = std::chrono::steady_clock::now() - lastBackup;
    if (timeSinceBackup.count() < AutoBackupIntervalSeconds) { return; }
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
    for (auto it = std::filesystem::begin(iterator); it != std::filesystem::end(iterator); it++) {
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
    if(force) {
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
    if(idle == IdleMode) return;
    if(idle && !player->IsPaused()) return; // can't idle while player is playing
    IdleMode = idle;
}

void OpenFunscripter::Step() noexcept {
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
            
            #ifdef WIN32
            if(OFS_DownloadFfmpeg::FfmpegMissing) {
                ImGui::OpenPopup(OFS_DownloadFfmpeg::ModalId);
                OFS_DownloadFfmpeg::DownloadFfmpegModal();
            }
            #endif

            sim3D->ShowWindow(&settings->data().show_simulator_3d, player->CurrentTime(), BaseOverlay::SplineMode, LoadedProject->Funscripts);
            ShowAboutWindow(&ShowAbout);
            specialFunctions->ShowFunctionsWindow(&settings->data().show_special_functions);
            undoSystem->ShowUndoRedoHistory(&settings->data().show_history);
            simulator.ShowSimulator(&settings->data().show_simulator);
            if (ShowMetadataEditorWindow(&ShowMetadataEditor)) { 
                LoadedProject->Save(true);
            }
            scripting->DrawScriptingMode(NULL);
            LoadedProject->ShowProjectWindow(&ShowProjectEditor);

            extensions->ShowExtensions();
            tcode->DrawWindow(&settings->data().show_tcode, player->CurrentTime());

            OFS_FileLogger::DrawLogWindow(&settings->data().show_debug_log);

            if (keybinds.ShowBindingWindow()) {
                keybinds.save();
            }

            if (settings->ShowPreferenceWindow()) {
                settings->saveSettings();
            }

            playerControls.DrawControls(NULL);

            if (Status & OFS_GradientNeedsUpdate) {
                Status &= ~(OFS_GradientNeedsUpdate);
                playerControls.UpdateHeatmap(player->Duration(), ActiveFunscript()->Actions());
            }

            auto drawBookmarks = [&](ImDrawList* draw_list, const ImRect& frame_bb, bool item_hovered) noexcept
            {
                OFS_PROFILE("DrawBookmarks");

                auto& style = ImGui::GetStyle();
                bool show_text = item_hovered || settings->data().always_show_bookmark_labels;

                // bookmarks
                auto& scriptSettings = LoadedProject->Settings;
                for (int i = 0; i < scriptSettings.Bookmarks.size(); i++) {
                    auto& bookmark = scriptSettings.Bookmarks[i];
                    auto nextBookmarkPtr = i + 1 < scriptSettings.Bookmarks.size() ? &scriptSettings.Bookmarks[i + 1] : nullptr;

                    constexpr float rectWidth = 7.f;
                    const float fontSize = ImGui::GetFontSize();
                    const uint32_t textColor = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]);

                    // if an end_marker appears before a start marker we render it as if was a regular bookmark
                    if (bookmark.type == OFS_ScriptSettings::Bookmark::BookmarkType::START_MARKER) {
                        if (i + 1 < scriptSettings.Bookmarks.size()
                            && nextBookmarkPtr != nullptr && nextBookmarkPtr->type == OFS_ScriptSettings::Bookmark::BookmarkType::END_MARKER) {
                            ImVec2 p1((frame_bb.Min.x + (frame_bb.GetWidth() * (bookmark.atS / player->Duration()))) - (rectWidth / 2.f), frame_bb.Min.y);
                            ImVec2 p2(p1.x + rectWidth, frame_bb.Min.y + frame_bb.GetHeight() + (style.ItemSpacing.y * 3.0f));

                            ImVec2 next_p1((frame_bb.Min.x + (frame_bb.GetWidth() * (nextBookmarkPtr->atS / player->Duration()))) - (rectWidth / 2.f), frame_bb.Min.y);
                            ImVec2 next_p2(next_p1.x + rectWidth, frame_bb.Min.y + frame_bb.GetHeight() + (style.ItemSpacing.y * 3.0f));

                            if (show_text) {
                                draw_list->AddRectFilled(
                                    p1 + ImVec2(rectWidth / 2.f, 0),
                                    next_p2 - ImVec2(rectWidth / 2.f, -fontSize),
                                    IM_COL32(255, 0, 0, 100),
                                    8.f);
                            }

                            draw_list->AddRectFilled(p1, p2, textColor, 8.f);
                            draw_list->AddRectFilled(next_p1, next_p2, textColor, 8.f);

                            if (show_text) {
                                auto size = ImGui::CalcTextSize(bookmark.name.c_str());
                                size.x /= 2.f;
                                size.y += 4.f;
                                float offset = (next_p2.x - p1.x) / 2.f;
                                draw_list->AddText(next_p2 - ImVec2(offset, -fontSize) - size, textColor, bookmark.name.c_str());
                            }

                            i += 1; // skip end marker
                            continue;
                        }
                    }

                    ImVec2 p1((frame_bb.Min.x + (frame_bb.GetWidth() * (bookmark.atS / player->Duration()))) - (rectWidth / 2.f), frame_bb.Min.y);
                    ImVec2 p2(p1.x + rectWidth, frame_bb.Min.y + frame_bb.GetHeight() + (style.ItemSpacing.y * 3.0f));

                    draw_list->AddRectFilled(p1, p2, ImColor(style.Colors[ImGuiCol_Text]), 8.f);

                    if (show_text) {
                        auto size = ImGui::CalcTextSize(bookmark.name.c_str());
                        size.x /= 2.f;
                        size.y /= 8.f;
                        draw_list->AddText(p2 - size, textColor, bookmark.name.c_str());
                    }
                }
            };

            playerControls.DrawTimeline(NULL, drawBookmarks);
            
            scriptTimeline.ShowScriptPositions(player.get(),
                scripting->Overlay().get(),
                &LoadedFunscripts(),
                ActiveFunscriptIdx);

            ShowStatisticsWindow(&settings->data().show_statistics);

            if (settings->data().show_action_editor) {
                ImGui::Begin(TR_ID(ActionEditorWindowId, Tr::ACTION_EDITOR), &settings->data().show_action_editor);
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

            playerWindow->DrawVideoPlayer(NULL, &settings->data().draw_video);
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
        
        float frameLimit = IdleMode ? 10.f : (float)settings->data().framerateLimit;
        const float minFrameTime = (float)PerfFreq / frameLimit;

        int32_t sleepMs = ((minFrameTime - (float)(FrameEnd - FrameStart)) / minFrameTime) * (1000.f / frameLimit);
        if(!IdleMode) sleepMs -= 1;
        if (sleepMs > 0) SDL_Delay(sleepMs); 

        if (!settings->data().vsync) {
            FrameEnd = SDL_GetPerformanceCounter();
            while ((FrameEnd - FrameStart) < minFrameTime) {
                OFS_PAUSE_INTRIN();
                FrameEnd = SDL_GetPerformanceCounter();
            }
        }

        if(SDL_GetTicks() - IdleTimer > 3000) {
            setIdle(true);
        }
    }
	return 0;
}

void OpenFunscripter::Shutdown() noexcept
{
    OFS_DynFontAtlas::Shutdown();
    OFS_Translator::Shutdown();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);   
    SDL_DestroyWindow(window);
    SDL_Quit();

    // these players need to be freed before unloading mpv
    player.reset();
    playerControls.videoPreview.reset();
    OFS_MpvLoader::Unload();
    OFS_FileLogger::Shutdown();
}

void OpenFunscripter::SetCursorType(ImGuiMouseCursor id) noexcept
{
    ImGui::SetMouseCursor(id);
}

void OpenFunscripter::Undo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if(undoSystem->Undo()) scripting->Undo();
}

void OpenFunscripter::Redo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if(undoSystem->Redo()) scripting->Redo();
}

bool OpenFunscripter::openFile(const std::string& file) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!Util::FileExists(file)) {
        Util::MessageBoxAlert(TR(FILE_NOT_FOUND), std::string(TR(COULDNT_FIND_FILE)) + "\n" + file);
        return false;
    }

    auto filePath = Util::PathFromString(file);
    if (filePath.extension().u8string() == OFS_Project::Extension) {
        return openProject(filePath.u8string());
    }
    else {
        return importFile(filePath.u8string());
    }
}

bool OpenFunscripter::importFile(const std::string& file) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!closeProject(false) || !LoadedProject->Import(file))
    {
        auto msg = TR(OFS_FAILED_TO_IMPORT);
        Util::MessageBoxAlert(TR(OFS_FAILED_TO_IMPORT_MSG), msg);
        closeProject(false);
        return false;
    }
    initProject();
    return true;
}

bool OpenFunscripter::openProject(const std::string& file) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!Util::FileExists(file)) {
        Util::MessageBoxAlert(TR(FILE_NOT_FOUND), std::string(TR(COULDNT_FIND_FILE)) + "\n" + file);
        return false;
    }

    if ((!closeProject(false) || !LoadedProject->Load(file))) {
        Util::MessageBoxAlert(TR(FAILED_TO_LOAD),
            FMT("%s\n%s", TR(FAILED_TO_LOAD_MSG), LoadedProject->LoadingError.c_str()));
        closeProject(false);
        return false;
    }
    initProject();
    auto recentFile = OFS_Settings::RecentFile{ LoadedProject->Metadata.title, LoadedProject->LastPath };
    settings->addRecentFile(recentFile);
    return true;
}

void OpenFunscripter::initProject() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (LoadedProject->Loaded) {
        if (LoadedProject->ProjectSettings.NudgeMetadata) {
            ShowMetadataEditor = settings->data().show_meta_on_new;
            LoadedProject->ProjectSettings.NudgeMetadata = false;
        }

        if (Util::FileExists(LoadedProject->MediaPath)) {
            player->OpenVideo(LoadedProject->MediaPath);
        }
        else {
            auto mediaName = Util::PathFromString(LoadedProject->MediaPath).filename();
            auto projectDir = Util::PathFromString(LoadedProject->LastPath).parent_path();
            auto testPath = (projectDir / mediaName).u8string();
            if (Util::FileExists(testPath)) {
                LoadedProject->MediaPath = testPath;
                LoadedProject->Save(true);
                player->OpenVideo(testPath);
            }
            else {
                Util::MessageBoxAlert(TR(FAILED_TO_FIND_VIDEO),
                    TR(FAILED_TO_FIND_VIDEO_MSG));
                pickDifferentMedia();
            }
        }
    }
    updateTitle();

    auto lastPath = Util::PathFromString(LoadedProject->LastPath);
    lastPath.replace_filename("");
    lastPath /= "";
    settings->data().last_path = lastPath.u8string();
    settings->saveSettings();

    lastBackup = std::chrono::steady_clock::now();
}

void OpenFunscripter::UpdateNewActiveScript(int32_t activeIndex) noexcept
{
    ActiveFunscriptIdx = activeIndex;
    updateTitle();
    Status = Status | OFS_Status::OFS_GradientNeedsUpdate;
}

void OpenFunscripter::updateTitle() noexcept
{
    const char* title = "OFS";
    if (LoadedProject->Loaded) {
        title = Util::Format("OpenFunscripter %s@%s - \"%s\"", 
            OFS_LATEST_GIT_TAG, 
            OFS_LATEST_GIT_HASH, 
            LoadedProject->LastPath.c_str()
        );
    }
    else {
        title = Util::Format("OpenFunscripter %s@%s", 
            OFS_LATEST_GIT_TAG, 
            OFS_LATEST_GIT_HASH
        );
    }
    SDL_SetWindowTitle(window, title);
}

void OpenFunscripter::saveProject() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    LoadedProject->Save(true);
    auto recentFile = OFS_Settings::RecentFile{ LoadedProject->Metadata.title, LoadedProject->LastPath };
    settings->addRecentFile(recentFile);
}

void OpenFunscripter::quickExport() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    LoadedProject->Save(true);
    LoadedProject->ExportFunscripts();
}

void OpenFunscripter::exportClips() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    LoadedProject->Save(true);
    Util::OpenDirectoryDialog(TR(CHOOSE_OUTPUT_DIR), settings->data().last_path,
        [&](auto& result) {
            if (result.files.size() > 0) {
                LoadedProject->ExportClips(result.files[0]);
            }
        });
}

bool OpenFunscripter::closeProject(bool closeWithUnsavedChanges) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!closeWithUnsavedChanges && LoadedProject->HasUnsavedEdits()) {
        FUN_ASSERT(false, "this branch should ideally never be taken");
        return false;
    }
    else {
        ActiveFunscriptIdx = 0;
        LoadedProject->Clear();
        player->CloseVideo();
        playerControls.videoPreview->closeVideo();
        updateTitle();
    }
    return true;
}

void OpenFunscripter::pickDifferentMedia() noexcept
{
    Util::OpenFileDialog(TR(PICK_DIFFERENT_MEDIA), LoadedProject->MediaPath,
        [&](auto& result) {
            if (!result.files.empty() && Util::FileExists(result.files[0])) {
                LoadedProject->MediaPath = result.files[0];
                LoadedProject->Save(true);
                player->OpenVideo(LoadedProject->MediaPath);
            }
        }, false);
}

void OpenFunscripter::saveHeatmap(const char* path, int width, int height)
{
    OFS_PROFILE(__FUNCTION__);
    SDL_Surface* surface;
    Uint32 rmask, gmask, bmask, amask;

    // same order as ImGui U32 colors
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;

    surface = SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask);
    if (surface == NULL) {
        LOGF_ERROR("SDL_CreateRGBSurface() failed: %s", SDL_GetError());
        return;
    }

    // not sure if this is always false, on every platform
    const bool mustLock = SDL_MUSTLOCK(surface); 
    if (mustLock) { SDL_LockSurface(surface); }

    SDL_Rect rect = {0};
    rect.h = height;

    const float relStep = 1.f / width;
    rect.w = 1;
    float relPos = 0.f;
    
    ImColor color;
    color.Value.w = 1.f;
    const float shadowStep = 1.f / height;
    ImColor black = IM_COL32_BLACK;

    for (int x = 0; x < width; x++) {
        rect.x = std::round(relPos * width);
        playerControls.Heatmap.Gradient.computeColorAt(relPos, &color.Value.x);
        black.Value.w = 0.f;
        for (int y = 0; y < height; y++) {
            uint32_t* target_pixel = (uint32_t*)((uint8_t*)surface->pixels + y * surface->pitch + x * sizeof(uint32_t));
            ImColor mix = color;
            mix.Value.x = mix.Value.x * (1.f - black.Value.w) + black.Value.x * black.Value.w;
            mix.Value.y = mix.Value.y * (1.f - black.Value.w) + black.Value.y * black.Value.w;
            mix.Value.z = mix.Value.z * (1.f - black.Value.w) + black.Value.z * black.Value.w;
            
            *target_pixel = ImGui::ColorConvertFloat4ToU32(mix);
            black.Value.w += shadowStep;
        }
        relPos += relStep;
    }

    Util::SavePNG(path, surface->pixels, surface->w, surface->h, 4, true);

    if (mustLock) { SDL_UnlockSurface(surface); }
    SDL_FreeSurface(surface);
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
    if (settings->data().mirror_mode && !ActiveFunscript()->HasSelection()) {
        undoSystem->Snapshot(StateType::REMOVE_ACTION);
        for (auto&& script : LoadedFunscripts()) {
            auto action = script->GetClosestAction(player->CurrentTime());
            if (action != nullptr) {
                script->RemoveAction(*action);
            }
        }
    }
    else {
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
}

void OpenFunscripter::addEditAction(int pos) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (settings->data().mirror_mode) {
        int32_t currentActiveScriptIdx = ActiveFunscriptIndex();
        undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS);
        for (int i = 0; i < LoadedFunscripts().size(); i++) {
            UpdateNewActiveScript(i);
            scripting->AddEditAction(FunscriptAction(player->CurrentTime(), pos));
        }
        UpdateNewActiveScript(currentActiveScriptIdx);
    }
    else {
        undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS, ActiveFunscript());
        scripting->AddEditAction(FunscriptAction(player->CurrentTime(), pos));
    }
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
         currentTime + (CopiedSelection.back().atS - CopiedSelection.front().atS + 0.0005f)
        );

    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->AddAction(FunscriptAction(action.atS + offsetTime, action.pos));
    }
    float newPosTime = (CopiedSelection.end() - 1)->atS + offsetTime;
    player->SetPositionExact(newPosTime);
}

void OpenFunscripter::pasteSelectionExact() noexcept {
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

void OpenFunscripter::equalizeSelection() noexcept {
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
    else if(ActiveFunscript()->Selection().size() >= 3) {
        undoSystem->Snapshot(StateType::EQUALIZE_ACTIONS, ActiveFunscript());
        ActiveFunscript()->EqualizeSelection();
    }
}

void OpenFunscripter::invertSelection() noexcept {
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

void OpenFunscripter::isolateAction() noexcept {
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
        else if (prev != nullptr) { ActiveFunscript()->RemoveAction(*prev); }
        else if (next != nullptr) { ActiveFunscript()->RemoveAction(*next); }

    }
}

void OpenFunscripter::repeatLastStroke() noexcept {
    OFS_PROFILE(__FUNCTION__);
    auto stroke = ActiveFunscript()->GetLastStroke(player->CurrentTime());
    if (stroke.size() > 1) {
        auto offsetTime = player->CurrentTime() - stroke.back().atS;
        undoSystem->Snapshot(StateType::REPEAT_STROKE, ActiveFunscript());
        auto action = ActiveFunscript()->GetActionAtTime(player->CurrentTime(), scripting->LogicalFrameTime());
        // if we are on top of an action we ignore the first action of the last stroke
        if (action != nullptr) {
            for(int i=stroke.size()-2; i >= 0; i--) {
                auto action = stroke[i];
                action.atS += offsetTime;
                ActiveFunscript()->AddAction(action);
            }
        }
        else {
            for (int i = stroke.size()-1; i >= 0; i--) {
                auto action = stroke[i];
                action.atS += offsetTime;
                ActiveFunscript()->AddAction(action);
            }
        }
        player->SetPositionExact(stroke.front().atS + offsetTime);
    }
}

void OpenFunscripter::saveActiveScriptAs() {
    std::filesystem::path path = Util::PathFromString(ActiveFunscript()->Path());
    path.make_preferred();
    Util::SaveFileDialog(TR(SAVE), path.u8string(),
        [&](auto& result) {
            if (result.files.size() > 0) {
                LoadedProject->ExportFunscript(result.files[0], ActiveFunscriptIdx);
                std::filesystem::path dir = Util::PathFromString(result.files[0]);
                dir.remove_filename();
                settings->data().last_path = dir.u8string();
            }
        }, {"Funscript", "*.funscript"});
}

void OpenFunscripter::ShowMainMenuBar() noexcept
{
#define BINDING_STRING(binding) keybinds.getBindingString(binding) 
    OFS_PROFILE(__FUNCTION__);
    ImColor alertCol(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
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
        ImVec2 region = ImGui::GetContentRegionAvail();

        if (ImGui::BeginMenu(TR_ID("FILE", Tr::FILE))) {
            if (ImGui::MenuItem(FMT(ICON_FOLDER_OPEN " %s", TR(OPEN_PROJECT)))) {
                auto openProjectDialog = [&]() {
                    Util::OpenFileDialog(TR(OPEN_PROJECT), settings->data().last_path,
                        [&](auto& result) {
                            if (result.files.size() > 0) {
                                auto& file = result.files[0];
                                auto path = Util::PathFromString(file);
                                if (path.extension().u8string() != OFS_Project::Extension) {
                                    Util::MessageBoxAlert(TR(WRONG_FILE), TR(WRONG_FILE_MSG));
                                    return;
                                }
                                else if (Util::FileExists(file)) {
                                    openProject(file);
                                }
                            }
                        }, false, { "*" OFS_PROJECT_EXT }, "Project (" OFS_PROJECT_EXT ")");
                };
                closeWithoutSavingDialog(std::move(openProjectDialog));
            }
            if (LoadedProject->Loaded && ImGui::MenuItem(TR(CLOSE_PROJECT), NULL, false, LoadedProject->Loaded)) {
                closeWithoutSavingDialog([](){});
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR(IMPORT_VIDEO_SCRIPT), 0, false)) {
                auto importNewItemDialog = [&]() {
                    Util::OpenFileDialog(TR(IMPORT_VIDEO_SCRIPT), settings->data().last_path,
                        [&](auto& result) {
                            if (result.files.size() > 0) {
                                auto& file = result.files[0];
                                if (Util::FileExists(file)) {
                                    importFile(file);
                                }
                            }
                        }, false);
                };
                closeWithoutSavingDialog(std::move(importNewItemDialog));
            }
            if (ImGui::BeginMenu(TR_ID("RECENT_FILES", Tr::RECENT_FILES))) {
                if (settings->data().recentFiles.size() == 0) {
                    ImGui::TextDisabled("%s", TR(NO_RECENT_FILES));
                }
                auto& recentFiles = settings->data().recentFiles;
                for (auto it = recentFiles.rbegin(); it != recentFiles.rend(); it++) {
                    auto& recent = *it;
                    if (ImGui::MenuItem(recent.name.c_str())) {
                        if (!recent.projectPath.empty()) {
                            closeWithoutSavingDialog([this, clickedFile = recent.projectPath]() 
                            {
                                openFile(clickedFile);
                            });
                            break;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (ImGui::MenuItem(TR(SAVE_PROJECT), BINDING_STRING("save_project"), false, LoadedProject->Loaded)) {
                saveProject();
            }
            if (ImGui::BeginMenu(TR_ID("EXPORT_MENU", Tr::EXPORT_MENU), LoadedProject->Loaded))
            {
                if (ImGui::MenuItem(FMT(ICON_SHARE " %s", TR(QUICK_EXPORT)), BINDING_STRING("quick_export"))) {
                    quickExport();
                }
                OFS::Tooltip(TR(QUICK_EXPORT_TOOLTIP));
                if (ImGui::MenuItem(FMT(ICON_SHARE " %s", TR(EXPORT_ACTIVE_SCRIPT)))) {
                    saveActiveScriptAs();
                }
                if (ImGui::MenuItem(FMT(ICON_SHARE " %s", TR(EXPORT_ALL)))) {
                    if (LoadedFunscripts().size() == 1) {
                        auto savePath = Util::PathFromString(settings->data().last_path) / (ActiveFunscript()->Title + ".funscript");
                        Util::SaveFileDialog(TR(EXPORT_MENU), savePath.u8string(),
                            [&](auto& result) {
                                if (result.files.size() > 0) {
                                    LoadedProject->ExportFunscript(result.files[0], ActiveFunscriptIdx);
                                    std::filesystem::path dir = Util::PathFromString(result.files[0]);
                                    dir.remove_filename();
                                    settings->data().last_path = dir.u8string();
                                }
                            }, { "Funscript", "*.funscript" });
                    }
                    else if(LoadedFunscripts().size() > 1)
                    {
                        Util::OpenDirectoryDialog(TR(EXPORT_MENU), settings->data().last_path,
                            [&](auto& result) {
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
            if (ImGui::MenuItem(autoBackupTmp && LoadedProject->Loaded ?
                FMT(TR(AUTO_BACKUP_TIMER_FMT), AutoBackupIntervalSeconds - std::chrono::duration_cast<std::chrono::seconds>((std::chrono::steady_clock::now() - lastBackup)).count())
                : TR(AUTO_BACKUP), NULL, &autoBackupTmp)) {
                Status = autoBackupTmp 
                    ? Status | OFS_Status::OFS_AutoBackup 
                    : Status ^ OFS_Status::OFS_AutoBackup;
            }
            if (ImGui::MenuItem(TR(OPEN_BACKUP_DIR))) {
                Util::OpenFileExplorer(Util::Prefpath("backup").c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("PROJECT", Tr::PROJECT), LoadedProject->Loaded))
        {
            if(ImGui::MenuItem(TR(CONFIGURE), NULL, &ShowProjectEditor)) {}
            ImGui::Separator();
            if (ImGui::MenuItem(TR(PICK_DIFFERENT_MEDIA))) {
                pickDifferentMedia();
            }
            if (ImGui::BeginMenu(TR(ADD_MENU), LoadedProject->Loaded)) {
                auto fileAlreadyLoaded = [](const std::string& path) noexcept -> bool {
                    auto app = OpenFunscripter::ptr;
                    auto it = std::find_if(app->LoadedFunscripts().begin(), app->LoadedFunscripts().end(),
                        [file = Util::PathFromString(path)](auto& script) {
                        return Util::PathFromString(script->Path()) == file;
                    }
                    );
                    return it != app->LoadedFunscripts().end();
                };
                auto addNewShortcut = [this, fileAlreadyLoaded](const char* axisExt) noexcept
                {
                    if (ImGui::MenuItem(axisExt))
                    {
                        std::string newScriptPath;
                        {
                            auto root = Util::PathFromString(RootFunscript()->Path());
                            root.replace_extension(Util::Format(".%s.funscript", axisExt));
                            newScriptPath = root.u8string();
                        }

                        if (!fileAlreadyLoaded(newScriptPath)) {
                            LoadedProject->AddFunscript(newScriptPath);
                        }
                    }
                };
                if (ImGui::BeginMenu(TR(ADD_SHORTCUTS))) {
                    for (int i = 1; i < TCodeChannels::Aliases.size() - 1; i++) {
                        addNewShortcut(TCodeChannels::Aliases[i][2]);
                    }
                    addNewShortcut("raw");
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(TR(ADD_NEW))) {
                    Util::SaveFileDialog(TR(ADD_NEW_FUNSCRIPT), settings->data().last_path,
                        [fileAlreadyLoaded](auto& result) noexcept {
                            if (result.files.size() > 0) {
                                auto app = OpenFunscripter::ptr;
                                if (!fileAlreadyLoaded(result.files[0])) {
                                    app->LoadedProject->AddFunscript(result.files[0]);
                                }
                            }
                        }, { "Funscript", "*.funscript" });
                }
                if (ImGui::MenuItem(TR(ADD_EXISTING))) {
                    Util::OpenFileDialog(TR(ADD_EXISTING_FUNSCRIPTS), settings->data().last_path,
                        [fileAlreadyLoaded](auto& result) noexcept {
                            if (result.files.size() > 0) {
                                for (auto&& scriptPath : result.files) {
                                    auto app = OpenFunscripter::ptr;
                                    if (!fileAlreadyLoaded(scriptPath)) {
                                        app->LoadedProject->AddFunscript(scriptPath);
                                    }
                                }
                            }
                        }, true, { "*.funscript" }, "Funscript");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(TR(REMOVE), LoadedFunscripts().size() > 1)) {
                int unloadIndex = -1;
                for (int i = 0; i < LoadedFunscripts().size(); i++) {
                    if (ImGui::MenuItem(LoadedFunscripts()[i]->Title.c_str())) {
                        unloadIndex = i;
                    }
                }
                if (unloadIndex >= 0) {
                    Util::YesNoCancelDialog(TR(REMOVE_SCRIPT),
                        TR(REMOVE_SCRIPT_CONFIRM_MSG), 
                        [this, unloadIndex](Util::YesNoCancel result)
                        {
                            if (result == Util::YesNoCancel::Yes)
                            {
                                LoadedProject->RemoveFunscript(unloadIndex);
                                if (ActiveFunscriptIdx > 0) { ActiveFunscriptIdx--; }
                                UpdateNewActiveScript(ActiveFunscriptIdx);
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
            ImGui::InputInt("##width", &settings->data().heatmapSettings.defaultWidth); ImGui::SameLine();
            ImGui::TextUnformatted("x"); ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##height", &settings->data().heatmapSettings.defaultHeight);
            if (ImGui::MenuItem(TR(SAVE_HEATMAP))) { 
                std::string filename = ActiveFunscript()->Title + "_Heatmap.png";
                auto defaultPath = Util::PathFromString(settings->data().heatmapSettings.defaultPath);
                Util::ConcatPathSafe(defaultPath, filename);
                Util::SaveFileDialog(TR(SAVE_HEATMAP), defaultPath.u8string(),
                    [this](auto& result) {
                        if (result.files.size() > 0) {
                            auto savePath = Util::PathFromString(result.files.front());
                            if (savePath.has_filename()) {
                                saveHeatmap(result.files.front().c_str(), settings->data().heatmapSettings.defaultWidth, settings->data().heatmapSettings.defaultHeight);
                                savePath.replace_filename("");
                                settings->data().heatmapSettings.defaultPath = savePath.u8string();
                            }

                        }
                    }, {"*.png"}, "PNG");
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
        if (ImGui::BeginMenu(TR(BOOKMARKS))) {
            auto& scriptSettings = LoadedProject->Settings;
            if (ImGui::MenuItem(TR(EXPORT_CLIPS), NULL, false, !scriptSettings.Bookmarks.empty())) {
                exportClips();
            }
            OFS::Tooltip(TR(EXPORT_CLIPS_TOOLTIP));
            ImGui::Separator();
            static std::string bookmarkName;
            float currentTime = player->CurrentTime();
            auto editBookmark = std::find_if(scriptSettings.Bookmarks.begin(), scriptSettings.Bookmarks.end(),
                [=](auto& mark) {
                    constexpr float thresholdTime = 1.f;
                    return std::abs(mark.atS - currentTime) <= thresholdTime;
                });
            if (editBookmark != scriptSettings.Bookmarks.end()) {
                int bookmarkIdx = std::distance(scriptSettings.Bookmarks.begin(), editBookmark);
                ImGui::PushID(bookmarkIdx);
                if (ImGui::InputText(TR(NAME), &(*editBookmark).name)) {
                    editBookmark->UpdateType();
                }
                if (ImGui::MenuItem(TR(REMOVE))) {
                    scriptSettings.Bookmarks.erase(editBookmark);
                }
                ImGui::PopID();
            }
            else {
                if (ImGui::InputText(TR(NAME), &bookmarkName, ImGuiInputTextFlags_EnterReturnsTrue) 
                    || ImGui::MenuItem(TR(ADD_BOOKMARK))) {
                    if (bookmarkName.empty()) {
                        bookmarkName = Util::Format("%d#", scriptSettings.Bookmarks.size()+1);
                    }

                    OFS_ScriptSettings::Bookmark bookmark(std::move(bookmarkName), currentTime);
                    scriptSettings.AddBookmark(std::move(bookmark));
                }

                auto it = std::find_if(scriptSettings.Bookmarks.rbegin(), scriptSettings.Bookmarks.rend(),
                    [&](auto& mark) {
                        return mark.atS < player->CurrentTime();
                    });
                if (it != scriptSettings.Bookmarks.rend() && it->type != OFS_ScriptSettings::Bookmark::BookmarkType::END_MARKER) {
                    const char* item = Util::Format(TR(CREATE_INTERVAL_FOR_FMT), it->name.c_str());
                    if (ImGui::MenuItem(item)) {
                        OFS_ScriptSettings::Bookmark bookmark(it->name + "_end", currentTime);
                        scriptSettings.AddBookmark(std::move(bookmark));
                    }
                }
            }

            static float LastPositionTime = -1.f;
            if (ImGui::BeginMenu(TR(GO_TO_MENU))) {
                if (scriptSettings.Bookmarks.size() == 0) {
                    ImGui::TextDisabled(TR(NO_BOOKMARKS));
                }
                else {
                    for (auto& mark : scriptSettings.Bookmarks) {
                        if (ImGui::MenuItem(mark.name.c_str())) {
                            player->SetPositionExact(mark.atS);
                            LastPositionTime = -1.f;
                        }
                        if (ImGui::IsItemHovered()) {
                            if (LastPositionTime < 0.f) LastPositionTime = currentTime;
                            player->SetPositionExact(mark.atS);
                        }
                    }
                }
                ImGui::EndMenu();
            }
            else if (LastPositionTime > 0.f) {
                player->SetPositionExact(LastPositionTime);
                LastPositionTime = -1.f;
            }

            if (ImGui::Checkbox(TR(ALWAYS_SHOW_LABELS), &settings->data().always_show_bookmark_labels)) {
                settings->saveSettings();
            }

            if (ImGui::MenuItem(TR(DELETE_ALL_BOOKMARKS))) {
                scriptSettings.Bookmarks.clear();
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("VIEW_MENU", Tr::VIEW_MENU))) {
#ifndef NDEBUG
            // this breaks the layout after restarting for some reason
            if (ImGui::MenuItem("Reset layout")) { setupDefaultLayout(true); }
            ImGui::Separator();
#endif
            if (ImGui::MenuItem(TR(STATISTICS), NULL, &settings->data().show_statistics)) {}
            if (ImGui::MenuItem(TR(UNDO_REDO_HISTORY), NULL, &settings->data().show_history)) {}
            if (ImGui::MenuItem(TR(SIMULATOR), NULL, &settings->data().show_simulator)) { settings->saveSettings(); }
            if (ImGui::MenuItem(TR(SIMULATOR_3D), NULL, &settings->data().show_simulator_3d)) { settings->saveSettings(); }
            if (ImGui::MenuItem(TR(METADATA), NULL, &ShowMetadataEditor)) {}
            if (ImGui::MenuItem(TR(ACTION_EDITOR), NULL, &settings->data().show_action_editor)) {}
            if (ImGui::MenuItem(TR(SPECIAL_FUNCTIONS), NULL, &settings->data().show_special_functions)) {}
            if (ImGui::MenuItem(TR(T_CODE), NULL, &settings->data().show_tcode)) {}

            ImGui::Separator();

            if (ImGui::MenuItem(TR(DRAW_VIDEO), NULL, &settings->data().draw_video)) { settings->saveSettings(); }
            if (ImGui::MenuItem(TR(RESET_VIDEO_POS), NULL)) { playerWindow->ResetTranslationAndZoom(); }

            auto videoModeToString = [](VideoMode mode) -> const char*
            {
                switch (mode)
                {
                    case VideoMode::FULL: return TR(VIDEO_MODE_FULL);
                    case VideoMode::LEFT_PANE: return TR(VIDEO_MODE_LEFT_PANE);
                    case VideoMode::RIGHT_PANE: return TR(VIDEO_MODE_RIGHT_PANE);
                    case VideoMode::TOP_PANE: return TR(VIDEO_MODE_TOP_PANE);
                    case VideoMode::BOTTOM_PANE: return TR(VIDEO_MODE_BOTTOM_PANE);
                    case VideoMode::VR_MODE: return TR(VIDEO_MODE_VR);
                }
                return "";
            };

            if(ImGui::BeginCombo(TR(VIDEO_MODE), videoModeToString(playerWindow->settings.activeMode)))
            {
                auto& mode = playerWindow->settings.activeMode;
                if(ImGui::Selectable(TR(VIDEO_MODE_FULL), mode == VideoMode::FULL))
                {
                    mode = VideoMode::FULL;
                }
                if(ImGui::Selectable(TR(VIDEO_MODE_LEFT_PANE), mode == VideoMode::LEFT_PANE))
                {
                    mode = VideoMode::LEFT_PANE;
                }
                if(ImGui::Selectable(TR(VIDEO_MODE_RIGHT_PANE), mode == VideoMode::RIGHT_PANE))
                {
                    mode = VideoMode::RIGHT_PANE;
                }
                if(ImGui::Selectable(TR(VIDEO_MODE_TOP_PANE), mode == VideoMode::TOP_PANE))
                {
                    mode = VideoMode::TOP_PANE;
                }
                if(ImGui::Selectable(TR(VIDEO_MODE_BOTTOM_PANE), mode == VideoMode::BOTTOM_PANE))
                {
                    mode = VideoMode::BOTTOM_PANE;
                }
                if(ImGui::Selectable(TR(VIDEO_MODE_VR), mode == VideoMode::VR_MODE))
                {
                    mode = VideoMode::VR_MODE;
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            if (ImGui::BeginMenu(TR(DEBUG))) {
                if (ImGui::MenuItem(TR(METRICS), NULL, &DebugMetrics)) {}
                if (ImGui::MenuItem(TR(LOG_OUTPUT), NULL, &settings->data().show_debug_log)) {}
#ifndef NDEBUG
                if (ImGui::MenuItem("ImGui Demo", NULL, &DebugDemo)) {}
#endif
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR(OPTIONS))) {
            if (ImGui::MenuItem(TR(KEYS))) {
                keybinds.ShowWindow = true;
            }
            bool fullscreenTmp = Status & OFS_Status::OFS_Fullscreen;
            if (ImGui::MenuItem(TR(FULLSCREEN), BINDING_STRING("fullscreen_toggle"), &fullscreenTmp)) {
                SetFullscreen(fullscreenTmp);
                Status = fullscreenTmp 
                    ? Status | OFS_Status::OFS_Fullscreen 
                    : Status ^ OFS_Status::OFS_Fullscreen;
            }
            if (ImGui::MenuItem(TR(PREFERENCES))) {
                settings->ShowWindow = true;
            }
            if (ControllerInput::AnythingConnected()) {
                if (ImGui::BeginMenu(TR(CONTROLLER))) {
                    ImGui::TextColored(ImColor(IM_COL32(0, 255, 0, 255)), "%s", TR(CONTROLLER_CONNECTED));
                    ImGui::DragInt(TR(REPEAT_RATE), &settings->data().buttonRepeatIntervalMs, 1, 25, 500, "%d", ImGuiSliderFlags_AlwaysClamp);
                    static int32_t selectedController = 0;
                    std::vector<const char*> padStrings;
                    for (int i = 0; i < ControllerInput::Controllers.size(); i++) {
                        auto& controller = ControllerInput::Controllers[i];
                        if (controller.connected()) {
                            padStrings.push_back(controller.GetName());
                        }
                        //else {
                        //    padStrings.push_back("--");
                        //}
                    }
                    ImGui::Combo("##ActiveControllers", &selectedController, padStrings.data(), (int32_t)padStrings.size());
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR_ID("EXTENSIONS", Tr::EXTENSIONS_MENU))) {
            if (ImGui::IsWindowAppearing()) {
                extensions->UpdateExtensionList();
            }
            if(ImGui::MenuItem(TR(DEV_MODE), NULL, &OFS_LuaExtensions::DevMode)) {}
            OFS::Tooltip(TR(DEV_MODE_TOOLTIP));
            if(ImGui::MenuItem(TR(SHOW_LOGS), NULL, &OFS_LuaExtensions::ShowLogs)) {}
            if (ImGui::MenuItem(TR(EXTENSION_DIR))) { 
                Util::OpenFileExplorer(Util::Prefpath(OFS_LuaExtensions::ExtensionDir)); 
            }
            ImGui::Separator();
            for (auto& ext : extensions->Extensions) {
                if(ImGui::BeginMenu(ext.NameId.c_str())) {
                    bool isActive = ext.IsActive();
                    if(ImGui::MenuItem(TR(ENABLED), NULL, &isActive)) {
                        ext.Toggle();
                        if(ext.HasError()) {
                            Util::MessageBoxAlert(TR(UNKNOWN_ERROR), ext.Error);
                        }
                    }
                    if(ImGui::MenuItem(Util::Format(TR(SHOW_WINDOW), ext.NameId.c_str()), NULL, &ext.WindowOpen, ext.IsActive())) {}
                    if(ImGui::MenuItem(Util::Format(TR(OPEN_DIRECTORY), ext.NameId.c_str()), NULL)) {
                        Util::OpenFileExplorer(ext.Directory);            
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("?##About")) {
            ImGui::CloseCurrentPopup();
            ImGui::EndMenu();
        }
        if(ImGui::IsItemClicked()) ShowAbout = true;

        ImGui::Separator();
        ImGui::Spacing();
        if (ControllerInput::AnythingConnected()) {
            bool navmodeActive = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad;
            ImGui::Text(ICON_GAMEPAD " " ICON_LONG_ARROW_RIGHT " %s", (navmodeActive) ? TR(NAVIGATION) : TR(SCRIPTING));
        }
        ImGui::Spacing();
        if(IdleMode) {
            ImGui::TextUnformatted(ICON_LEAF);
        }
        if (player->VideoLoaded() && unsavedEdits) {
            const float timeUnit = saveDuration.count() / 60.f;
            ImGui::SameLine(region.x - ImGui::GetFontSize()*13.5f);
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_Text], TR(UNSAVED_CHANGES_FMT), (int)(timeUnit));
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(1);
#undef BINDING_STRING
}

bool OpenFunscripter::ShowMetadataEditorWindow(bool* open) noexcept
{
    if (!*open) return false;
    else {
        ImGui::OpenPopup(TR_ID("METADATA_EDITOR", Tr::METADATA_EDITOR));
    }

    OFS_PROFILE(__FUNCTION__);
    bool save = false;
    auto& metadata = LoadedProject->Metadata;
    
    if (ImGui::BeginPopupModal(TR_ID("METADATA_EDITOR", Tr::METADATA_EDITOR), open, ImGuiWindowFlags_NoDocking)) {
        ImGui::InputText(TR(TITLE), &metadata.title);
        metadata.duration = (int64_t)player->Duration();
        Util::FormatTime(tmpBuf[0], sizeof(tmpBuf[0]), (float)metadata.duration, false);
        ImGui::LabelText(TR(DURATION), "%s", tmpBuf[0]);

        ImGui::InputText(TR(CREATOR), &metadata.creator);
        ImGui::InputText(TR(URL), &metadata.script_url);
        ImGui::InputText(TR(VIDEO_URL), &metadata.video_url);
        ImGui::InputTextMultiline(TR(DESCRIPTION), &metadata.description, ImVec2(0.f, ImGui::GetFontSize()*3.f));
        ImGui::InputTextMultiline(TR(NOTES), &metadata.notes, ImVec2(0.f, ImGui::GetFontSize() * 3.f));

        {
            enum class LicenseType : int32_t {
                None,
                Free,
                Paid
            };
            static LicenseType currentLicense = LicenseType::None;

            auto licenseTypeToString = [](LicenseType type) -> const char*
            {
                switch (type)
                {
                    case LicenseType::None: return TR(NONE);
                    case LicenseType::Free: return TR(FREE);
                    case LicenseType::Paid: return TR(PAID);
                }
                return "";
            };

            if(ImGui::BeginCombo(TR(LICENSE), licenseTypeToString(currentLicense)))
            {
                if(ImGui::Selectable(TR(NONE), currentLicense == LicenseType::None))
                {
                    metadata.license = "";
                    currentLicense = LicenseType::None;
                }
                if(ImGui::Selectable(TR(FREE), currentLicense == LicenseType::Free))
                {
                    metadata.license = "Free";
                    currentLicense = LicenseType::Free;
                }
                if(ImGui::Selectable(TR(PAID), currentLicense == LicenseType::Paid))
                {
                    metadata.license = "Paid";
                    currentLicense = LicenseType::Paid;
                }
                ImGui::EndCombo();
            }

            if (!metadata.license.empty()) {
                ImGui::SameLine(); ImGui::Text("-> \"%s\"", metadata.license.c_str());
            }
        }
    
        auto renderTagButtons = [](std::vector<std::string>& tags) {
            auto availableWidth = ImGui::GetContentRegionAvail().x;
            int removeIndex = -1;
            for (int i = 0; i < tags.size(); i++) {
                ImGui::PushID(i);
                auto& tag = tags[i];

                if (ImGui::Button(tag.c_str())) {
                    removeIndex = i;
                }
                auto nextLineCursor = ImGui::GetCursorPos();
                ImGui::SameLine();
                if (ImGui::GetCursorPosX() + ImGui::GetItemRectSize().x >= availableWidth) {
                    ImGui::SetCursorPos(nextLineCursor);
                }

                ImGui::PopID();
            }
            if (removeIndex != -1) {
                tags.erase(tags.begin() + removeIndex);
            }
        };

        constexpr const char* tagIdString = "##Tag";
        ImGui::TextUnformatted(TR(TAGS));
        static std::string newTag;
        auto addTag = [&metadata, tagIdString](std::string& newTag) {
            Util::trim(newTag);
            if (!newTag.empty()) {
                metadata.tags.emplace_back(newTag); newTag.clear();
            }
            ImGui::ActivateItem(ImGui::GetID(tagIdString));
        };

        if (ImGui::InputText(tagIdString, &newTag, ImGuiInputTextFlags_EnterReturnsTrue)) {
            addTag(newTag);
        };
        ImGui::SameLine();
        if (ImGui::Button(TR(ADD), ImVec2(-1.f, 0.f))) { 
            addTag(newTag);
        }
    
        auto& style = ImGui::GetStyle();

        renderTagButtons(metadata.tags);
        ImGui::NewLine();

        constexpr const char* performerIdString = "##Performer";
        ImGui::TextUnformatted(TR(PERFORMERS));
        static std::string newPerformer;
        auto addPerformer = [&metadata, performerIdString](std::string& newPerformer) {
            Util::trim(newPerformer);
            if (!newPerformer.empty()) {
                metadata.performers.emplace_back(newPerformer); newPerformer.clear(); 
            }
            auto performerID = ImGui::GetID(performerIdString);
            ImGui::ActivateItem(performerID);
        };
        if (ImGui::InputText(performerIdString, &newPerformer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            addPerformer(newPerformer);
        }
        ImGui::SameLine();
        if (ImGui::Button(TR_ID("ADD_PERFORMER", Tr::ADD), ImVec2(-1.f, 0.f))) {
            addPerformer(newPerformer);
        }


        renderTagButtons(metadata.performers);
        ImGui::NewLine();
        ImGui::Separator();
        float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x;
        availWidth /= 2.f;
        if (ImGui::Button(TR(SAVE), ImVec2(availWidth, 0.f))) { save = true; }
        ImGui::SameLine();
        if (ImGui::Button(FMT("%s " ICON_COPY, TR(SAVE_TEMPLATE)), ImVec2(availWidth, 0.f))) {
            settings->data().defaultMetadata = LoadedProject->Metadata;
        }
        OFS::Tooltip(TR(SAVE_TEMPLATE_TOOLTIP));
        Util::ForceMinumumWindowSize(ImGui::GetCurrentWindow());
        ImGui::EndPopup();
    }
    return save;
}

void OpenFunscripter::SetFullscreen(bool fullscreen) {
    static SDL_Rect restoreRect = {0, 0, 1280, 720};
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
        SDL_SetWindowSize(window,  bounds.w, bounds.h+1);
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
    if constexpr (opt_fullscreen)
    {
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
    ImGui::Begin(TR(ABOUT), open, ImGuiWindowFlags_None 
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoCollapse
    );
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
        FUN_ASSERT(((double)currentTime - behind->atS)*1000.0 > 0.001, "This maybe a bug");
        
        ImGui::Text("%s: %.2lf ms", TR(INTERVAL), ((double)currentTime - behind->atS)*1000.0);
        if (front != nullptr) {
            auto duration = front->atS - behind->atS;
            int32_t length = front->pos - behind->pos;
            ImGui::Text("%s: %.02lf units/s", TR(SPEED), std::abs(length) / duration);
            ImGui::Text("%s: %.2lf ms", TR(DURATION), (double)duration * 1000.0);
            if (length > 0) {
                ImGui::Text("%3d " ICON_LONG_ARROW_RIGHT " %3d" " = %3d " ICON_LONG_ARROW_UP, behind->pos, front->pos, length);
            }                                          
            else {                                     
                ImGui::Text("%3d " ICON_LONG_ARROW_RIGHT " %3d" " = %3d " ICON_LONG_ARROW_DOWN, behind->pos, front->pos, -length);
            }
        }
    }

    ImGui::End();

}

void OpenFunscripter::ControllerAxisPlaybackSpeed(SDL_Event& ev) noexcept
{
    static Uint8 lastAxis = 0;
    OFS_PROFILE(__FUNCTION__);
    auto& caxis = ev.caxis;
    if ((Status & OFS_Status::OFS_GamepadSetPlaybackSpeed) && caxis.axis == lastAxis && caxis.value <= 0) {
        Status &= ~(OFS_Status::OFS_GamepadSetPlaybackSpeed);
        return;
    }

    if (caxis.value < 0) { return; }
    if (Status & OFS_Status::OFS_GamepadSetPlaybackSpeed) { return; }
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

void OpenFunscripter::ScriptTimelineDoubleClick(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    float seekToTime = *((float*)&ev.user.data1);
    player->SetPositionExact(seekToTime);
}

void OpenFunscripter::ScriptTimelineSelectTime(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& time =*(ScriptTimelineEvents::SelectTime*)ev.user.data1;
    switch (time.mode)
    {
    default:        
    case ScriptTimelineEvents::Mode::All:
        ActiveFunscript()->SelectTime(time.startTime, time.endTime, time.clear);
        break;
    //case ScriptTimelineEvents::Mode::Top:
    //    undoSystem->Snapshot(StateType::TOP_POINTS_ONLY, ActiveFunscript());
    //    ActiveFunscript()->SelectTopActions(time.start_ms, time.end_ms, time.clear);
    //    break;
    //case ScriptTimelineEvents::Mode::Bottom:
    //    undoSystem->Snapshot(StateType::BOTTOM_POINTS_ONLY, ActiveFunscript());
    //    ActiveFunscript()->SelectBottomActions(time.start_ms, time.end_ms, time.clear);
    //    break;
    //case ScriptTimelineEvents::Mode::Middle:
    //    undoSystem->Snapshot(StateType::MID_POINTS_ONLY, ActiveFunscript());
    //    ActiveFunscript()->SelectMidActions(time.start_ms, time.end_ms, time.clear);
    //    break;
    }
}

void OpenFunscripter::ScriptTimelineActiveScriptChanged(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    UpdateNewActiveScript((intptr_t)ev.user.data1);
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
