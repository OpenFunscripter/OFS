#include "OpenFunscripter.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_ImGui.h"
#include "OFS_Simulator3D.h"
#include "GradientBar.h"
#include "FunscriptHeatmap.h"
#include "OFS_Shader.h"

#include <filesystem>

#include "stb_sprintf.h"

#include "imgui_stdlib.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "ImGuizmo.h"

#include "asap.h"

#include "glad/gl.h"

// FIX: Add type checking to the deserialization. It does crash if a field type doesn't match.

// TODO: improve shift click add action with simulator
//       it bugs out if the simulator is on the same height as the script timeline

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
    ".flac",
    ".wmv",
    ".ogg"
};

OpenFunscripter* OpenFunscripter::ptr = nullptr;
ImFont* OpenFunscripter::DefaultFont2 = nullptr;

constexpr const char* glsl_version = "#version 150";

static ImGuiID MainDockspaceID;
constexpr const char* StatisticsId = "Statistics";
constexpr const char* ActionEditorId = "Action editor";

constexpr int DefaultWidth = 1920;
constexpr int DefaultHeight= 1080;

constexpr int AutoBackupIntervalSeconds = 60;

bool OpenFunscripter::loadFonts(const char* font_override) noexcept
{
    auto& io = ImGui::GetIO();

    // LOAD FONTS
    auto roboto = font_override ? font_override : Util::Resource("fonts/RobotoMono-Regular.ttf");    
    auto fontawesome = Util::Resource("fonts/fontawesome-webfont.ttf");
    auto noto_jp = Util::Resource("fonts/NotoSansJP-Regular.otf");

    unsigned char* pixels;
    int width, height;

    // Add character ranges and merge into the previous font
    // The ranges array is not copied by the AddFont* functions and is used lazily
    // so ensure it is available for duration of font usage
    static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 }; // will not be copied by AddFont* so keep in scope.
    ImFontConfig config;

    ImFont* font = nullptr;

    GLuint font_tex = (GLuint)(intptr_t)io.Fonts->TexID;
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();

    if (!Util::FileExists(roboto)) { 
        LOGF_WARN("\"%s\" font is missing.", roboto.c_str());
        roboto = Util::Resource("fonts/RobotoMono-Regular.ttf");
    }

    if (!Util::FileExists(roboto)) {
        LOGF_WARN("\"%s\" font is missing.", roboto.c_str());
    }
    else {
        font = io.Fonts->AddFontFromFileTTF(roboto.c_str(), settings->data().default_font_size, &config);
        if (font == nullptr) return false;
        io.FontDefault = font;
    }

    if (!Util::FileExists(fontawesome)) {
        LOGF_WARN("\"%s\" font is missing. No icons.", fontawesome.c_str());
    }
    else {
        config.MergeMode = true;
        font = io.Fonts->AddFontFromFileTTF(fontawesome.c_str(), settings->data().default_font_size, &config, icons_ranges);
        if (font == nullptr) return false;
    }

    if (!Util::FileExists(noto_jp)) {
        LOGF_WARN("\"%s\" font is missing. No japanese glyphs.", noto_jp.c_str());
    }
    else {
        config.MergeMode = true;
        font = io.Fonts->AddFontFromFileTTF(noto_jp.c_str(), settings->data().default_font_size, &config, io.Fonts->GetGlyphRangesJapanese());
        if (font == nullptr) {
            LOG_WARN("Missing japanese glyphs!!!");
        }
    }

    if (!Util::FileExists(roboto)) {
        LOGF_WARN("\"%s\" font is missing.", roboto.c_str());
    } 
    else
    {
        config.MergeMode = false;
        DefaultFont2 = io.Fonts->AddFontFromFileTTF(roboto.c_str(), settings->data().default_font_size * 2.0f, &config);
    }
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    
    // Upload texture to graphics system
    if (!font_tex) {
        glGenTextures(1, &font_tex);
    }
    glBindTexture(GL_TEXTURE_2D, font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    io.Fonts->TexID = (void*)(intptr_t)font_tex;
    return true;
}

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
    LOGF_DEBUG("init imgui with glsl: %s", glsl_version);
    ImGui_ImplOpenGL3_Init(glsl_version);

    loadFonts(settings->data().font_override.empty() ? nullptr : settings->data().font_override.c_str());
  
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
    player.reset();
    events.reset();
}

bool OpenFunscripter::setup(int argc, char* argv[])
{
#ifndef NDEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
#endif
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    ptr = this;
    auto prefPath = Util::Prefpath("");
    Util::CreateDirectories(prefPath);

    settings = std::make_unique<OpenFunscripterSettings>(Util::Prefpath("config.json"));
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        LOGF_ERROR("Error: %s\n", SDL_GetError());
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

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
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

    IO = std::make_unique<OFS_AsyncIO>();
    IO->Init();
    LoadedProject = std::make_unique<OFS_Project>();
    player = std::make_unique<VideoplayerWindow>();
    if (!player->setup(settings->data().force_hw_decoding)) {
        LOG_ERROR("Failed to init video player");
        return false;
    }
    OFS_ScriptSettings::player = &player->settings;
    playerControls.setup();
    playerControls.player = player.get();
    closeProject();

    undoSystem = std::make_unique<UndoSystem>(&LoadedProject->Funscripts);

    keybinds.setup(*events);
    registerBindings(); // needs to happen before setBindings
    keybinds.load(Util::Prefpath("keybinds.json"));

    scriptPositions.setup(undoSystem.get());

    scripting = std::make_unique<ScriptingMode>();
    scripting->setup();
    events->Subscribe(FunscriptEvents::FunscriptActionsChangedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptChanged));
    events->Subscribe(SDL_DROPFILE, EVENT_SYSTEM_BIND(this, &OpenFunscripter::DragNDrop));
    events->Subscribe(VideoEvents::MpvVideoLoaded, EVENT_SYSTEM_BIND(this, &OpenFunscripter::MpvVideoLoaded));
    events->Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ControllerAxisPlaybackSpeed));
    events->Subscribe(ScriptTimelineEvents::FunscriptActionClicked, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActionClicked));
    events->Subscribe(ScriptTimelineEvents::ScriptpositionWindowDoubleClick, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineDoubleClick));
    events->Subscribe(ScriptTimelineEvents::FunscriptSelectTime, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineSelectTime));
    events->Subscribe(VideoEvents::PlayPauseChanged, EVENT_SYSTEM_BIND(this, &OpenFunscripter::MpvPlayPauseChange));
    events->Subscribe(ScriptTimelineEvents::ActiveScriptChanged, EVENT_SYSTEM_BIND(this, &OpenFunscripter::ScriptTimelineActiveScriptChanged));

    if (argc > 1) {
        const char* path = argv[1];
        openFile(path);
    }
    else if (!settings->data().recentFiles.empty()) {
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
    player->OnRenderCallback = [](const ImDrawList * parent_list, const ImDrawCmd * cmd) {
        auto app = OpenFunscripter::ptr;
        if (app->settings->data().show_simulator_3d) {
            app->sim3D->renderSim();
        }
    };

    HeatmapGradient::Init();

    tcode = std::make_unique<TCodePlayer>();
    tcode->loadSettings(Util::Prefpath("tcode.json"));
    SDL_ShowWindow(window);
    return true;
}

void OpenFunscripter::setupDefaultLayout(bool force) noexcept
{
    MainDockspaceID = ImGui::GetID("MainAppDockspace");
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

        ImGui::DockBuilderDockWindow(VideoplayerWindow::PlayerId, dock_player_center_id);
        ImGui::DockBuilderDockWindow(OFS_VideoplayerControls::PlayerTimeId, dock_time_bottom_id);
        ImGui::DockBuilderDockWindow(OFS_VideoplayerControls::PlayerControlId, dock_player_control_id);
        ImGui::DockBuilderDockWindow(ScriptTimeline::PositionsId, dock_positions_id);
        ImGui::DockBuilderDockWindow(ScriptingMode::ScriptingModeId, dock_mode_right_id);
        ImGui::DockBuilderDockWindow(ScriptSimulator::SimulatorId, dock_simulator_right_id);
        ImGui::DockBuilderDockWindow(ActionEditorId, dock_action_right_id);
        ImGui::DockBuilderDockWindow(StatisticsId, dock_stats_right_id);
        ImGui::DockBuilderDockWindow(UndoSystem::UndoHistoryId, dock_undo_right_id);
        simulator.CenterSimulator();
        ImGui::DockBuilderFinish(MainDockspaceID);
    }
}

void OpenFunscripter::registerBindings()
{
    {
        KeybindingGroup group;
        group.name = "Actions";
        // DELETE ACTION
        auto& remove_action = group.bindings.emplace_back(
            "remove_action",
            "Remove action",
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
            "Action at 0",
            true,
            [&](void*) { addEditAction(0); }
        );
        action_0.key = Keybinding(
            SDLK_KP_0,
            0
        );

        for (int i = 1; i < 10; i++) {
            std::string id = Util::Format("action %d", i * 10);
            std::string desc = Util::Format("Action at %d", i*10);

            auto& action = group.bindings.emplace_back(
                id,
                desc,
                true,
                [&, i](void*) { addEditAction(i * 10); }
            );
            action.key = Keybinding(
                SDLK_KP_1 + i - 1,
                0
            );
        }
        auto& action_100 = group.bindings.emplace_back(
            "action 100",
            "Action at 100",
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
        KeybindingGroup group;
        group.name = "Core";

        // SAVE
        auto& save_project = group.bindings.emplace_back(
            "save_project",
            "Save project",
            true,
            [&](void*) { saveProject(); }
        );
        save_project.key = Keybinding(
            SDLK_s,
            KMOD_CTRL
        );

        auto& quick_export = group.bindings.emplace_back(
            "quick_export",
            "Quick export",
            "true",
            [&](void*) { quickExport(); }
        );
        quick_export.key = Keybinding(
            SDLK_s,
            KMOD_CTRL | KMOD_SHIFT
        );

        auto& sync_timestamp = group.bindings.emplace_back(
            "sync_timestamps",
            "Sync time with player",
            true,
            [&](void*) { player->syncWithRealTime(); }
        );
        sync_timestamp.key = Keybinding(
            SDLK_s,
            0
        );

        auto& cycle_loaded_forward_scripts = group.bindings.emplace_back(
            "cycle_loaded_forward_scripts",
            "Cycle forward loaded scripts",
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
            "Cycle backward loaded scripts",
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

        keybinds.registerBinding(std::move(group));
    }
    {
        KeybindingGroup group;
        group.name = "Navigation";
        // JUMP BETWEEN ACTIONS
        auto& prev_action = group.bindings.emplace_back(
            "prev_action",
            "Previous action",
            false,
            [&](void*) {
                auto action = ActiveFunscript()->GetPreviousActionBehind(player->getCurrentPositionSecondsInterp() - 0.001f);
                if (action != nullptr) player->setPositionExact(action->atS);
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
            "Next action",
            false,
            [&](void*) {
                auto action = ActiveFunscript()->GetNextActionAhead(player->getCurrentPositionSecondsInterp() + 0.001f);
                if (action != nullptr) player->setPositionExact(action->atS);
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
            "Previous action (multi)",
            false,
            [&](void*) {
                bool foundAction = false;
                float closestTime = std::numeric_limits<float>::max();
                float currentTime = player->getCurrentPositionSecondsInterp();

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
                    player->setPositionExact(closestTime);
                }
            }
        );
        prev_action_multi.key = Keybinding(
            SDLK_DOWN,
            KMOD_CTRL
        );

        auto& next_action_multi = group.bindings.emplace_back(
            "next_action_multi",
            "Next action (multi)",
            false,
            [&](void*) {
                bool foundAction = false;
                float closestTime = std::numeric_limits<float>::max();
                float currentTime = player->getCurrentPositionSecondsInterp();
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
                    player->setPositionExact(closestTime);
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
            "Previous frame",
            false,
            [&](void*) { 
                if (player->isPaused()) {
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
            "Next frame",
            false,
            [&](void*) { 
                if (player->isPaused()) {
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
            "Fast step",
            false,
            [&](void*) {
                int32_t frameStep = settings->data().fast_step_amount;
                player->relativeFrameSeek(frameStep);
            }
        );
        fast_step.key = Keybinding(
            SDLK_RIGHT,
            KMOD_CTRL
        );

        auto& fast_backstep = group.bindings.emplace_back(
            "fast_backstep",
            "Fast backstep",
            false,
            [&](void*) {
                int32_t frameStep = settings->data().fast_step_amount;
                player->relativeFrameSeek(-frameStep);
            }
        );
        fast_backstep.key = Keybinding(
            SDLK_LEFT,
            KMOD_CTRL
        );
        keybinds.registerBinding(std::move(group));
    }
    
    {
        KeybindingGroup group;
        group.name = "Utility";
        // UNDO / REDO
        auto& undo = group.bindings.emplace_back(
            "undo",
            "Undo",
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
            "Redo",
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
            "Copy",
            true,
            [&](void*) { copySelection(); }
        );
        copy.key = Keybinding(
            SDLK_c,
            KMOD_CTRL
        );

        auto& paste = group.bindings.emplace_back(
            "paste",
            "Paste",
            true,
            [&](void*) { pasteSelection(); }
        );
        paste.key = Keybinding(
            SDLK_v,
            KMOD_CTRL
        );

        auto& paste_exact = group.bindings.emplace_back(
            "paste_exact",
            "Paste exact",
            true,
            [&](void*) {pasteSelectionExact(); }
        );
        paste_exact.key = Keybinding(
            SDLK_v,
            KMOD_CTRL | KMOD_SHIFT
        );

        auto& cut = group.bindings.emplace_back(
            "cut",
            "Cut",
            true,
            [&](void*) { cutSelection(); }
        );
        cut.key = Keybinding(
            SDLK_x,
            KMOD_CTRL
        );

        auto& select_all = group.bindings.emplace_back(
            "select_all",
            "Select all",
            true,
            [&](void*) { ActiveFunscript()->SelectAll(); }
        );
        select_all.key = Keybinding(
            SDLK_a,
            KMOD_CTRL
        );

        auto& deselect_all = group.bindings.emplace_back(
            "deselect_all",
            "Deselect all",
            true,
            [&](void*) { ActiveFunscript()->ClearSelection(); }
        );
        deselect_all.key = Keybinding(
            SDLK_d,
            KMOD_CTRL
        );

        auto& select_all_left = group.bindings.emplace_back(
            "select_all_left",
            "Select all left",
            true,
            [&](void*) { ActiveFunscript()->SelectTime(0, player->getCurrentPositionSecondsInterp()); }
        );
        select_all_left.key = Keybinding(
            SDLK_LEFT,
            KMOD_CTRL | KMOD_ALT
        );

        auto& select_all_right = group.bindings.emplace_back(
            "select_all_right",
            "Select all right",
            true,
            [&](void*) { ActiveFunscript()->SelectTime(player->getCurrentPositionSecondsInterp(), player->getDuration()); }
        );
        select_all_right.key = Keybinding(
            SDLK_RIGHT,
            KMOD_CTRL | KMOD_ALT
        );

        auto& select_top_points = group.bindings.emplace_back(
            "select_top_points",
            "Select top points",
            true,
            [&](void*) { selectTopPoints(); }
        );
        
        auto& select_middle_points = group.bindings.emplace_back(
            "select_middle_points",
            "Select middle points",
            true,
            [&](void*) { selectMiddlePoints(); }
        );
        
        auto& select_bottom_points = group.bindings.emplace_back(
            "select_bottom_points",
            "Select bottom points",
            true,
            [&](void*) { selectBottomPoints(); }
        );

        auto& toggle_mirror_mode = group.bindings.emplace_back(
            "toggle_mirror_mode",
            "Toggle mirror mode",
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
            "Save frame as image",
            true,
            [&](void*) { 
                auto screenshot_dir = Util::Prefpath("screenshot");
                player->saveFrameToImage(screenshot_dir);
            }
        );
        save_frame_as_image.key = Keybinding(
            SDLK_F2,
            0
        );

        // CHANGE SUBTITLES
        auto& cycle_subtitles = group.bindings.emplace_back(
            "cycle_subtitles",
            "Cycle subtitles",
            true,
            [&](void*) { player->cycleSubtitles(); }
        );
        cycle_subtitles.key = Keybinding(
            SDLK_j,
            0
        );

        // FULLSCREEN
        auto& fullscreen_toggle = group.bindings.emplace_back(
            "fullscreen_toggle",
            "Toggle fullscreen",
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
                ? app->scriptPositions.overlay->steppingIntervalForward(app->ActiveFunscript()->Selection().front().atS)
                : app->scriptPositions.overlay->steppingIntervalBackward(app->ActiveFunscript()->Selection().front().atS);

            app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
            app->ActiveFunscript()->MoveSelectionTime(time, app->player->getFrameTime());
        }
        else {
            auto closest = ptr->ActiveFunscript()->GetClosestAction(app->player->getCurrentPositionSecondsInterp());
            if (closest != nullptr) {
                auto time = forward
                    ? app->scriptPositions.overlay->steppingIntervalForward(closest->atS)
                    : app->scriptPositions.overlay->steppingIntervalBackward(closest->atS);

                FunscriptAction moved(closest->atS + time, closest->pos);
                auto closestInMoveRange = app->ActiveFunscript()->GetActionAtTime(moved.atS, app->player->getFrameTime());
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
                ? app->scriptPositions.overlay->steppingIntervalForward(app->ActiveFunscript()->Selection().front().atS)
                : app->scriptPositions.overlay->steppingIntervalBackward(app->ActiveFunscript()->Selection().front().atS);

            app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
            app->ActiveFunscript()->MoveSelectionTime(time, app->player->getFrameTime());
            auto closest = ptr->ActiveFunscript()->GetClosestActionSelection(app->player->getCurrentPositionSecondsInterp());
            if (closest != nullptr) { app->player->setPositionExact(closest->atS); }
            else { app->player->setPositionExact(app->ActiveFunscript()->Selection().front().atS); }
        }
        else {
            auto closest = app->ActiveFunscript()->GetClosestAction(ptr->player->getCurrentPositionSecondsInterp());
            if (closest != nullptr) {
                auto time = forward
                    ? app->scriptPositions.overlay->steppingIntervalForward(closest->atS)
                    : app->scriptPositions.overlay->steppingIntervalBackward(closest->atS);

                FunscriptAction moved(closest->atS + time, closest->pos);
                auto closestInMoveRange = app->ActiveFunscript()->GetActionAtTime(moved.atS, app->player->getFrameTime());

                if (closestInMoveRange == nullptr 
                    || (forward && closestInMoveRange->atS < moved.atS) 
                    || (!forward && closestInMoveRange->atS > moved.atS)) {
                    app->undoSystem->Snapshot(StateType::ACTIONS_MOVED, app->ActiveFunscript());
                    app->ActiveFunscript()->EditAction(*closest, moved);
                    app->player->setPositionExact(moved.atS);
                }
            }
        }
    };
    {
        KeybindingGroup group;
        group.name = "Moving";
        auto& move_actions_up_ten = group.bindings.emplace_back(
            "move_actions_up_ten",
            "Move actions +10 up",
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(10);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos + 10, 0, 100)));
                    }
                }
            }
        );

        auto& move_actions_down_ten = group.bindings.emplace_back(
            "move_actions_down_ten",
            "Move actions -10 down",
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(-10);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos - 10, 0, 100)));
                    }
                }
            }
        );


        auto& move_actions_up_five = group.bindings.emplace_back(
            "move_actions_up_five",
            "Move actions +5 up",
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(5);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos + 5, 0, 100)));
                    }
                }
            }
        );

        auto& move_actions_down_five = group.bindings.emplace_back(
            "move_actions_down_five",
            "Move actions -5 down",
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection())
                {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(-5);
                }
                else
                {
                    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
                    if (closest != nullptr) {
                        undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                        ActiveFunscript()->EditAction(*closest, FunscriptAction(closest->atS, Util::Clamp<int32_t>(closest->pos - 5, 0, 100)));
                    }
                }
            }
        );


        auto& move_actions_left_snapped = group.bindings.emplace_back(
            "move_actions_left_snapped",
            "Move actions left with snapping",
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
            "Move actions right with snapping",
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
            "Move actions left",
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
            "Move actions right",
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
            "Move actions up",
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection()) {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(1);
                }
                else {
                    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
            "Move actions down",
            false,
            [&](void*) {
                if (ActiveFunscript()->HasSelection()) {
                    undoSystem->Snapshot(StateType::ACTIONS_MOVED, ActiveFunscript());
                    ActiveFunscript()->MoveSelectionPosition(-1);
                }
                else {
                    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
            "Move to current position",
            true,
            [&](void*) {
                auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
                if (closest != nullptr) {
                    undoSystem->Snapshot(StateType::MOVE_ACTION_TO_CURRENT_POS, ActiveFunscript());
                    ActiveFunscript()->EditAction(*closest, FunscriptAction(player->getCurrentPositionSecondsInterp(), closest->pos));
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
        KeybindingGroup group;
        group.name = "Special";
        auto& equalize = group.bindings.emplace_back(
            "equalize_actions",
            "Equalize actions",
            true,
            [&](void*) { equalizeSelection(); }
        );
        equalize.key = Keybinding(
            SDLK_e,
            0
        );

        auto& invert = group.bindings.emplace_back(
            "invert_actions",
            "Invert actions",
            true,
            [&](void*) { invertSelection(); }
        );
        invert.key = Keybinding(
            SDLK_i,
            0
        );
        auto& isolate = group.bindings.emplace_back(
            "isolate_action",
            "Isolate action",
            true,
            [&](void*) { isolateAction(); }
        );
        isolate.key = Keybinding(
            SDLK_r,
            0
        );

        auto& repeat_stroke = group.bindings.emplace_back(
            "repeat_stroke",
            "Repeat stroke",
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
        KeybindingGroup group;
        group.name = "Video player";
        // PLAY / PAUSE
        auto& toggle_play = group.bindings.emplace_back(
            "toggle_play",
            "Play / Pause",
            true,
            [&](void*) { player->togglePlay(); }
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
            "Playback speed -10%",
            true,
            [&](void*) { player->addSpeed(-0.10); }
        );
        decrement_speed.key = Keybinding(
            SDLK_KP_MINUS,
            0
        );
        auto& increment_speed = group.bindings.emplace_back(
            "increment_speed",
            "Playback speed +10%",
            true,
            [&](void*) { player->addSpeed(0.10); }
        );
        increment_speed.key = Keybinding(
            SDLK_KP_PLUS,
            0
        );

        keybinds.registerBinding(std::move(group));
    }

    {
        KeybindingGroup group;
        group.name = "Controller";
        auto& toggle_nav_mode = group.bindings.emplace_back(
            "toggle_controller_navmode",
            "Toggle controller navigation",
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
            "Forward 1 second",
            false,
            [&](void*) { player->seekRelative(1000); }
        );
        seek_forward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
            false
        );

        auto& seek_backward_second = group.bindings.emplace_back(
            "seek_backward_second",
            "Backward 1 second",
            false,
            [&](void*) { player->seekRelative(-1000); }
        );
        seek_backward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            false
        );

        auto& add_action_controller = group.bindings.emplace_back(
            "add_action_controller",
            "Add action",
            true,
            [&](void*) { addEditAction(100); }
        );
        add_action_controller.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_A,
            false
        );

        auto& toggle_recording_mode = group.bindings.emplace_back(
            "toggle_recording_mode",
            "Toggle recording mode",
            true,
            [&](void*) {
                static ScriptingModeEnum prevMode = ScriptingModeEnum::RECORDING;
                if (scripting->mode() != ScriptingModeEnum::RECORDING) {
                    prevMode = scripting->mode();
                    scripting->setMode(ScriptingModeEnum::RECORDING);
                    ((RecordingImpl&)scripting->Impl()).setRecordingMode(RecordingImpl::RecordingMode::Controller);
                }
                else {
                    scripting->setMode(prevMode);
                }
            }
        );
        toggle_recording_mode.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_BACK,
            false
        );

        auto& controller_select = group.bindings.emplace_back(
            "set_selection_controller",
            "Controller select",
            true,
            [&](void*) {
                if (scriptPositions.selectionStart() < 0) {
                    scriptPositions.setStartSelection(player->getCurrentPositionSecondsInterp());
                }
                else {
                    auto tmp = player->getCurrentPositionSecondsInterp();
                    auto [min, max] = std::minmax<float>(scriptPositions.selectionStart(), tmp);
                    ActiveFunscript()->SelectTime(min, max);
                    scriptPositions.setStartSelection(-1);
                }
            }
        );
        controller_select.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSTICK,
            false
        );

        auto& set_playbackspeed_controller = group.bindings.emplace_back(
            "set_current_playbackspeed_controller",
            "Set current playback speed",
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
        PassiveBindingGroup group;
        group.name = "Point timeline";

        auto& move_or_add_point_modifier = group.bindings.emplace_back(
            "move_or_add_point_modifier",
            "Click drag/add point in the timeline"
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
        PassiveBindingGroup group;
        group.name = "Simulator";
        auto& click_add_point_simulator = group.bindings.emplace_back(
            "click_add_point_simulator",
            "Click simulator to add a point"
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
    ImGui_ImplSDL2_NewFrame(window);
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
}

void OpenFunscripter::processEvents() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    SDL_Event event;
    bool IsExiting = false;
    while (SDL_PollEvent(&event))
    {
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
#ifndef NDEBUG
        case SDL_KEYDOWN:
        {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                exitApp(true);
            }
            break;
        }
#endif
        }
        events->Propagate(event);
    }
}

void OpenFunscripter::FunscriptChanged(SDL_Event& ev) noexcept
{
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
        player->setPositionExact(action.atS);
    }
}

void OpenFunscripter::DragNDrop(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (closeProject()) {
        openFile(ev.drop.file);
    }
    SDL_free(ev.drop.file);
}

void OpenFunscripter::MpvVideoLoaded(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    LoadedProject->Metadata.duration = player->getDuration();
    player->setPositionExact(LoadedProject->Settings.lastPlayerPosition);
    ActiveFunscript()->NotifyActionsChanged(false);

    Status |= OFS_Status::OFS_GradientNeedsUpdate;
    const char* VideoName = (const char*)ev.user.data1;
    if (VideoName)
    {
        auto recentFile = OpenFunscripterSettings::RecentFile{ LoadedProject->Metadata.title, LoadedProject->LastPath };
        settings->addRecentFile(recentFile);
        scriptPositions.ClearAudioWaveform();
    }

    tcode->reset();
    {
        std::vector<std::shared_ptr<const Funscript>> scripts;
        scripts.assign(LoadedFunscripts().begin(), LoadedFunscripts().end());
        tcode->setScripts(std::move(scripts));
    }
}

void OpenFunscripter::MpvPlayPauseChange(SDL_Event& ev) noexcept
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
        tcode->play(player->getCurrentPositionSecondsInterp(), std::move(scripts));
    }
}

void OpenFunscripter::update() noexcept {
    OFS_PROFILE(__FUNCTION__);
    float& delta = ImGui::GetIO().DeltaTime;
    player->update(delta);
    ActiveFunscript()->update();
    ControllerInput::UpdateControllers(settings->data().buttonRepeatIntervalMs);
    scripting->update();

    if (Status & OFS_Status::OFS_AutoBackup) {
        autoBackup();
    }

    tcode->sync(player->getCurrentPositionSecondsInterp(), player->getSpeed());
}

void OpenFunscripter::autoBackup() noexcept
{
    if (!LoadedProject->Loaded) { return; }
    std::chrono::duration<float> timeSinceBackup = std::chrono::steady_clock::now() - lastBackup;
    if (timeSinceBackup.count() < AutoBackupIntervalSeconds) { return; }
    OFS_PROFILE(__FUNCTION__);
    lastBackup = std::chrono::steady_clock::now();

    auto backupDir = std::filesystem::path(Util::Prefpath("backup"));
    auto name = Util::Filename(player->getVideoPath());
    name = Util::trim(name); // this needs to be trimmed because trailing spaces
    
    static auto BackupStartPoint = asap::now();
    name = Util::Format("%s_%02d%02d%02d_%02d%02d%02d", 
        name.c_str(), BackupStartPoint.year(), 
        BackupStartPoint.month(), 
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
    auto savePath = backupDir
        / Util::Format("%s_%02d-%02d-%02d" OFS_PROJECT_EXT ".backup", name.c_str(), time.hour(), time.minute(), time.second());
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
        Util::YesNoCancelDialog("Unsaved changes", "Do you want to save and exit?",
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

void OpenFunscripter::step() noexcept {
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
            sim3D->ShowWindow(&settings->data().show_simulator_3d, player->getCurrentPositionSecondsInterp(), BaseOverlay::SplineMode, LoadedProject->Funscripts);
            ShowAboutWindow(&ShowAbout);
            specialFunctions->ShowFunctionsWindow(&settings->data().show_special_functions);
            undoSystem->ShowUndoRedoHistory(&settings->data().show_history);
            simulator.ShowSimulator(&settings->data().show_simulator);
            ShowStatisticsWindow(&settings->data().show_statistics);
            if (ShowMetadataEditorWindow(&ShowMetadataEditor)) { 
                LoadedProject->Save(true);
            }
            scripting->DrawScriptingMode(NULL);
            LoadedProject->ShowProjectWindow(&ShowProjectEditor);


            tcode->DrawWindow(&settings->data().show_tcode, player->getCurrentPositionSecondsInterp());

            if (keybinds.ShowBindingWindow()) {
                keybinds.save();
            }

            if (settings->ShowPreferenceWindow()) {
                settings->saveSettings();
            }

            playerControls.DrawControls(NULL);

            if (Status & OFS_GradientNeedsUpdate) {
                Status &= ~(OFS_GradientNeedsUpdate);
                playerControls.UpdateHeatmap(player->getDuration(), ActiveFunscript()->Actions());
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
                            ImVec2 p1((frame_bb.Min.x + (frame_bb.GetWidth() * (bookmark.atS / player->getDuration()))) - (rectWidth / 2.f), frame_bb.Min.y);
                            ImVec2 p2(p1.x + rectWidth, frame_bb.Min.y + frame_bb.GetHeight() + (style.ItemSpacing.y * 3.0f));

                            ImVec2 next_p1((frame_bb.Min.x + (frame_bb.GetWidth() * (nextBookmarkPtr->atS / player->getDuration()))) - (rectWidth / 2.f), frame_bb.Min.y);
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

                    ImVec2 p1((frame_bb.Min.x + (frame_bb.GetWidth() * (bookmark.atS / player->getDuration()))) - (rectWidth / 2.f), frame_bb.Min.y);
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
            
            // this is an easter egg / gimmick
            if (scriptPositions.WaveformPartyMode) {
                scriptPositions.WaveShader->use();
                scriptPositions.WaveShader->ScriptPos(ActiveFunscript()->SplineClamped(player->getCurrentPositionSecondsInterp()));
            }
            scriptPositions.ShowScriptPositions(NULL, player->getCurrentPositionSecondsInterp(), player->getDuration(), player->getFrameTime(), &LoadedFunscripts(), ActiveFunscriptIdx);

            if (settings->data().show_action_editor) {
                ImGui::Begin(ActionEditorId, &settings->data().show_action_editor);
                OFS_PROFILE(ActionEditorId);

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

                if (player->isPaused()) {
                    ImGui::Spacing();
                    auto scriptAction = ActiveFunscript()->GetActionAtTime(player->getCurrentPositionSecondsInterp(), player->getFrameTime());
                    if (!scriptAction) {
                        // create action
                        static int newActionPosition = 0;
                        ImGui::SetNextItemWidth(-1.f);
                        ImGui::SliderInt("##Position", &newActionPosition, 0, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
                        if (ImGui::Button("Add action", ImVec2(-1.f, 0.f))) {
                            addEditAction(newActionPosition);
                        }
                    }
                }
                ImGui::End();
            }

            if (DebugDemo) {
                ImGui::ShowDemoWindow(&DebugDemo);
            }

            if (DebugMetrics) {
                ImGui::ShowMetricsWindow(&DebugMetrics);
            }

            player->DrawVideoPlayer(NULL, &settings->data().draw_video);
        }

        render();
    }
    OFS_ENDPROFILING();
    SDL_GL_SwapWindow(window);
}

int OpenFunscripter::run() noexcept
{
    newFrame();
    setupDefaultLayout(false);
    render();
    const uint64_t PerfFreq = SDL_GetPerformanceFrequency();
    while (!(Status & OFS_Status::OFS_ShouldExit)) {
        const uint64_t minFrameTime = (float)PerfFreq / (float)settings->data().framerateLimit;
        uint64_t FrameStart = SDL_GetPerformanceCounter();
        step();
        uint64_t FrameEnd = SDL_GetPerformanceCounter();
        
        if (!settings->data().vsync) {
            int32_t sleepMs = ((float)(minFrameTime - (FrameEnd - FrameStart)) / (float)minFrameTime) * (1000.f / (float)settings->data().framerateLimit);
            sleepMs -= 1;
            if (sleepMs > 0 && sleepMs < 32) { SDL_Delay(sleepMs); }
            FrameEnd = SDL_GetPerformanceCounter();
            while ((FrameEnd - FrameStart) < minFrameTime) {
                OFS_PAUSE_INTRIN();
                FrameEnd = SDL_GetPerformanceCounter();
            }
        }
    }
	return 0;
}

void OpenFunscripter::shutdown() noexcept
{
    IO->Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);   
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void OpenFunscripter::SetCursorType(ImGuiMouseCursor id) noexcept
{
    ImGui::SetMouseCursor(id);
}

void OpenFunscripter::Undo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if(undoSystem->Undo()) scripting->undo();
}

void OpenFunscripter::Redo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if(undoSystem->Redo()) scripting->redo();
}

bool OpenFunscripter::openFile(const std::string& file) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!Util::FileExists(file)) return false;
    std::filesystem::path filePath = Util::PathFromString(file);
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
    if (!closeProject() || !LoadedProject->Import(file))
    {
        auto msg = "OpenFunscripter failed to import.\n"
            "Does a project with the same name already exist?\n"
            "Try opening that instead.";
        Util::MessageBoxAlert("Failed to import", msg);
        closeProject();
        return false;
    }
    initProject();
    return true;
}

bool OpenFunscripter::openProject(const std::string& file) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!Util::FileExists(file)) {
        Util::MessageBoxAlert("File not found", "Couldn't find file:\n" + file);
        return false;
    }

    if (!closeProject() || !LoadedProject->Load(file)) {
        Util::MessageBoxAlert("Failed to load", 
            Util::Format("The project failed to load.\n%s", LoadedProject->LoadingError.c_str()));
        closeProject();
        return false;
    }
    initProject();
    return true;
}

void OpenFunscripter::initProject() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (LoadedProject->Loaded) {
        if (LoadedProject->ProjectSettings.NudgeMetadata) {
            ShowMetadataEditor = true;
            LoadedProject->ProjectSettings.NudgeMetadata = false;
            LoadedProject->Save(true);
        }

        if (Util::FileExists(LoadedProject->MediaPath)) {
            player->openVideo(LoadedProject->MediaPath);
        }
        else {
            pickDifferentMedia();
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
    ActiveFunscript()->NotifyActionsChanged(false);
}

void OpenFunscripter::updateTitle() noexcept
{
    const char* title = "OFS";
    if (LoadedProject->Loaded) {
        title = Util::Format("OpenFunscripter %s@%s - \"%s\"", 
            OFS_LATEST_GIT_TAG, 
            OFS_LATEST_GIT_HASH, 
            LoadedProject->LastPath.c_str());
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
    LoadedProject->Save(true);
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
    Util::OpenDirectoryDialog("Choose output directory.", settings->data().last_path,
        [&](auto& result) {
            if (result.files.size() > 0) {
                LoadedProject->ExportClips(result.files[0]);
            }
        });
}

bool OpenFunscripter::closeProject() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (LoadedProject->HasUnsavedEdits()) {
        FUN_ASSERT(false, "this branch should ideally never be taken");
        return false;
    }
    else {
        ActiveFunscriptIdx = 0;
        LoadedProject->Clear();
        player->closeVideo();
        playerControls.videoPreview->closeVideo();
        updateTitle();
    }
    return true;
}

void OpenFunscripter::pickDifferentMedia() noexcept
{
    Util::OpenFileDialog("Pick different media", LoadedProject->MediaPath,
        [&](auto& result)
        {
            if (!result.files.empty() && Util::FileExists(result.files[0])) {
                LoadedProject->MediaPath = result.files[0];
                LoadedProject->Save(true);
                player->openVideo(LoadedProject->MediaPath);
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

    SDL_Rect rect{ 0 };
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
            auto action = script->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
            auto action = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
            scripting->addEditAction(FunscriptAction(player->getCurrentPositionSecondsInterp(), pos));
        }
        UpdateNewActiveScript(currentActiveScriptIdx);
    }
    else {
        undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS, ActiveFunscript());
        scripting->addEditAction(FunscriptAction(player->getCurrentPositionSecondsInterp(), pos));
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
            CopiedSelection.emplace_back(action);
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
    float currentTime = player->getCurrentPositionSecondsInterp();
    float offsetTime = currentTime - CopiedSelection.begin()->atS;

    if (CopiedSelection.size() >= 2)
    {
        FUN_ASSERT(CopiedSelection.front().atS < CopiedSelection.back().atS, "order is messed up");
        ActiveFunscript()->RemoveActionsInInterval(currentTime, currentTime + (CopiedSelection.back().atS - CopiedSelection.front().atS));
    }

    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->AddAction(FunscriptAction(action.atS + offsetTime, action.pos));
    }
    float newPosTime = (CopiedSelection.end() - 1)->atS + offsetTime;
    player->setPositionExact(newPosTime);
}

void OpenFunscripter::pasteSelectionExact() noexcept {
    OFS_PROFILE(__FUNCTION__);
    if (CopiedSelection.size() == 0) return;
    
    if (CopiedSelection.size() >= 2)
    {
        FUN_ASSERT(CopiedSelection.front().atS < CopiedSelection.back().atS, "order is messed up");
        ActiveFunscript()->RemoveActionsInInterval(CopiedSelection.front().atS, CopiedSelection.back().atS);
    }

    // paste without altering timestamps
    undoSystem->Snapshot(StateType::PASTE_COPIED_ACTIONS, ActiveFunscript());
    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->AddAction(action);
    }
}

void OpenFunscripter::equalizeSelection() noexcept {
    OFS_PROFILE(__FUNCTION__);
    if (!ActiveFunscript()->HasSelection()) {
        undoSystem->Snapshot(StateType::EQUALIZE_ACTIONS, ActiveFunscript());
        // this is a small hack
        auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
        auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
    auto closest = ActiveFunscript()->GetClosestAction(player->getCurrentPositionSecondsInterp());
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
    auto stroke = ActiveFunscript()->GetLastStroke(player->getCurrentPositionSecondsInterp());
    if (stroke.size() > 1) {
        auto offsetTime = player->getCurrentPositionSecondsInterp() - stroke.back().atS;
        undoSystem->Snapshot(StateType::REPEAT_STROKE, ActiveFunscript());
        auto action = ActiveFunscript()->GetActionAtTime(player->getCurrentPositionSecondsInterp(), player->getFrameTime());
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
        player->setPositionExact(stroke.front().atS + offsetTime);
    }
}

void OpenFunscripter::saveActiveScriptAs() {
    std::filesystem::path path = Util::PathFromString(ActiveFunscript()->Path());
    path.make_preferred();
    Util::SaveFileDialog("Save", path.u8string(),
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
    if (player->isLoaded() && unsavedEdits) {
        saveDuration = std::chrono::system_clock::now() - ActiveFunscript()->EditTime();
        const float timeUnit = saveDuration.count() / 60.f;
        if (timeUnit >= 5.f) {
            alertCol = ImLerp(alertCol.Value, ImColor(IM_COL32(184, 33, 22, 255)).Value, std::max(std::sin(saveDuration.count()), 0.f));
        }
    }

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, alertCol.Value);
    if (ImGui::BeginMainMenuBar())
    {
        ImVec2 region = ImGui::GetContentRegionAvail();

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(ICON_FOLDER_OPEN" Open project")) {
                Util::OpenFileDialog("Open project", settings->data().last_path,
                    [&](auto& result) {
                        if (result.files.size() > 0) {
                            auto& file = result.files[0];
                            auto path = Util::PathFromString(file);
                            if (path.extension().u8string() != OFS_Project::Extension) {
                                Util::MessageBoxAlert("Wrong file", "That's not a project file.");
                                return;
                            }
                            else if (Util::FileExists(file)) {
                                openProject(file);
                            }
                        }
                    }, false, {"*" OFS_PROJECT_EXT}, "Project (" OFS_PROJECT_EXT ")");
            }
            if (ImGui::MenuItem("Import video/script", 0, false, !LoadedProject->Loaded))
            {
                Util::OpenFileDialog("Import video/script", settings->data().last_path,
                    [&](auto& result) {
                        if (result.files.size() > 0) {
                            auto& file = result.files[0];
                            if (Util::FileExists(file)) {
                                importFile(file);
                            }
                        }
                    }, false);
            }
            OFS::Tooltip(LoadedProject->Loaded ? "Close current project first." : "Videos & scripts get imported into a new project.");
            if (ImGui::BeginMenu("Recent files")) {
                if (settings->data().recentFiles.size() == 0) {
                    ImGui::TextDisabled("%s", "No recent files");
                }
                auto& recentFiles = settings->data().recentFiles;
                for (auto it = recentFiles.rbegin(); it != recentFiles.rend(); it++) {
                    auto& recent = *it;
                    if (ImGui::MenuItem(recent.name.c_str())) {
                        if (!recent.projectPath.empty())
                            openFile(recent.projectPath);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Save project", BINDING_STRING("save_project"))) {
                saveProject();
            }
            if (ImGui::BeginMenu("Export...", LoadedProject->Loaded))
            {
                if (ImGui::MenuItem(ICON_SHARE " Quick export", BINDING_STRING("quick_export"))) {
                    quickExport();
                }
                OFS::Tooltip("Exports all scripts as .funscript in their default paths.");
                if (ImGui::MenuItem(ICON_SHARE " Export active script")) {
                    saveActiveScriptAs();
                }
                if (ImGui::MenuItem(ICON_SHARE " Export all")) {
                    if (LoadedFunscripts().size() == 1) {
                        auto savePath = Util::PathFromString(settings->data().last_path) / (ActiveFunscript()->Title + "_share.funscript");
                        Util::SaveFileDialog("Share funscript", savePath.u8string(),
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
                        Util::OpenDirectoryDialog("Choose output directory.\nAll scripts will get saved with an _share appended", settings->data().last_path,
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
            if (ImGui::MenuItem(autoBackupTmp ?
                Util::Format("Auto Backup in %ld seconds", AutoBackupIntervalSeconds - std::chrono::duration_cast<std::chrono::seconds>((std::chrono::steady_clock::now() - lastBackup)).count())
                : "Auto Backup", NULL, &autoBackupTmp)) {
                Status = autoBackupTmp 
                    ? Status | OFS_Status::OFS_AutoBackup 
                    : Status ^ OFS_Status::OFS_AutoBackup;
            }
            if (ImGui::MenuItem("Open backup directory")) {
                Util::OpenFileExplorer(Util::Prefpath("backup").c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Project", LoadedProject->Loaded))
        {
            if(ImGui::MenuItem("Configure", NULL, &ShowProjectEditor)) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Pick different media")) {
                pickDifferentMedia();
            }
            if (ImGui::BeginMenu("Add...", LoadedProject->Loaded)) {
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
                if (ImGui::BeginMenu("Shortcuts")) {
                    for (int i = 1; i < TCodeChannels::Aliases.size() - 1; i++) {
                        addNewShortcut(TCodeChannels::Aliases[i][2]);
                    }
                    addNewShortcut("raw");
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Add new")) {
                    Util::SaveFileDialog("Add new funscript", settings->data().last_path,
                        [fileAlreadyLoaded](auto& result) noexcept {
                            if (result.files.size() > 0) {
                                auto app = OpenFunscripter::ptr;
                                if (!fileAlreadyLoaded(result.files[0])) {
                                    app->LoadedProject->AddFunscript(result.files[0]);
                                }
                            }
                        }, { "Funscript", "*.funscript" });
                }
                if (ImGui::MenuItem("Add existing")) {
                    Util::OpenFileDialog("Add existing funscripts", settings->data().last_path,
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
            if (ImGui::BeginMenu("Remove", LoadedFunscripts().size() > 1)) {
                int unloadIndex = -1;
                for (int i = 0; i < LoadedFunscripts().size(); i++) {
                    if (ImGui::MenuItem(LoadedFunscripts()[i]->Title.c_str())) {
                        unloadIndex = i;
                    }
                }
                if (unloadIndex >= 0) {
                    Util::YesNoCancelDialog("Remove script",
                        "If the script has not been exported this can not be reverted.\n"
                        "Continue?", 
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
            ImGui::Separator();
            if (LoadedProject->Loaded && ImGui::MenuItem("Save and close project", NULL, false, LoadedProject->Loaded)) {
                LoadedProject->Save(true);
                closeProject();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Save frame as image", BINDING_STRING("save_frame_as_image")))
            { 
                auto screenshot_dir = Util::Prefpath("screenshot");
                player->saveFrameToImage(screenshot_dir);
            }
            if (ImGui::MenuItem("Open screenshot directory")) {
                auto screenshot_dir = Util::Prefpath("screenshot");
                Util::CreateDirectories(screenshot_dir);
                Util::OpenFileExplorer(screenshot_dir.c_str());
            }

            ImGui::Separator();

            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##width", &settings->data().heatmapSettings.defaultWidth); ImGui::SameLine();
            ImGui::TextUnformatted("x"); ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##height", &settings->data().heatmapSettings.defaultHeight);
            if (ImGui::MenuItem("Save heatmap")) { 
                std::string filename = ActiveFunscript()->Title + "_Heatmap.png";
                auto defaultPath = Util::PathFromString(settings->data().heatmapSettings.defaultPath);
                Util::ConcatPathSafe(defaultPath, filename);
                Util::SaveFileDialog("Save heatmap", defaultPath.u8string(),
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
            if (ImGui::MenuItem("Undo", BINDING_STRING("undo"), false, !undoSystem->UndoEmpty())) {
                this->Undo();
            }
            if (ImGui::MenuItem("Redo", BINDING_STRING("redo"), false, !undoSystem->RedoEmpty())) {
                this->Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", BINDING_STRING("cut"), false, ActiveFunscript()->HasSelection())) {
                cutSelection();
            }
            if (ImGui::MenuItem("Copy", BINDING_STRING("copy"), false, ActiveFunscript()->HasSelection()))
            {
                copySelection();
            }
            if (ImGui::MenuItem("Paste", BINDING_STRING("paste"), false, CopiedSelection.size() > 0))
            {
                pasteSelection();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Select")) {
            if (ImGui::MenuItem("Select all", BINDING_STRING("select_all"), false)) {
                ActiveFunscript()->SelectAll();
            }
            if (ImGui::MenuItem("Deselect all", BINDING_STRING("deselect_all"), false)) {
                ActiveFunscript()->ClearSelection();
            }

            if (ImGui::BeginMenu("Special")) {
                if (ImGui::MenuItem("Select all left", BINDING_STRING("select_all_left"), false)) {
                    ActiveFunscript()->SelectTime(0, player->getCurrentPositionSecondsInterp());
                }
                if (ImGui::MenuItem("Select all right", BINDING_STRING("select_all_right"), false)) {
                    ActiveFunscript()->SelectTime(player->getCurrentPositionSecondsInterp(), player->getDuration());
                }
                ImGui::Separator();
                static int32_t selectionPoint = -1;
                if (ImGui::MenuItem("Set selection start")) {
                    if (selectionPoint == -1) {
                        selectionPoint = player->getCurrentPositionSecondsInterp();
                    }
                    else {
                        ActiveFunscript()->SelectTime(player->getCurrentPositionSecondsInterp(), selectionPoint);
                        selectionPoint = -1;
                    }
                }
                if (ImGui::MenuItem("Set selection end")) {
                    if (selectionPoint == -1) {
                        selectionPoint = player->getCurrentPositionSecondsInterp();
                    }
                    else {
                        ActiveFunscript()->SelectTime(selectionPoint, player->getCurrentPositionSecondsInterp());
                        selectionPoint = -1;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Top points only", BINDING_STRING("select_top_points"), false)) {
                if (ActiveFunscript()->HasSelection()) {
                    selectTopPoints();
                }
            }
            if (ImGui::MenuItem("Mid points only", BINDING_STRING("select_middle_points"), false)) {
                if (ActiveFunscript()->HasSelection()) {
                    selectMiddlePoints();
                }
            }
            if (ImGui::MenuItem("Bottom points only", BINDING_STRING("select_bottom_points"), false)) {
                if (ActiveFunscript()->HasSelection()) {
                    selectBottomPoints();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Equalize", BINDING_STRING("equalize_actions"), false)) {
                equalizeSelection();
            }
            if (ImGui::MenuItem("Invert", BINDING_STRING("invert_actions"), false)) {
                invertSelection();
            }
            if (ImGui::MenuItem("Isolate", BINDING_STRING("isolate_action"))) {
                isolateAction();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Bookmarks")) {
            auto& scriptSettings = LoadedProject->Settings;
            if (ImGui::MenuItem("Export Clips", NULL, false, !scriptSettings.Bookmarks.empty())) {
                exportClips();
            }
            ImGui::Separator();
            static std::string bookmarkName;
            float currentTime = player->getCurrentPositionSecondsInterp();
            auto editBookmark = std::find_if(scriptSettings.Bookmarks.begin(), scriptSettings.Bookmarks.end(),
                [=](auto& mark) {
                    constexpr float thresholdTime = 1.f;
                    return std::abs(mark.atS - currentTime) <= thresholdTime;
                });
            if (editBookmark != scriptSettings.Bookmarks.end()) {
                if (ImGui::InputText("Name", &(*editBookmark).name)) {
                    editBookmark->UpdateType();
                }
                if (ImGui::MenuItem("Delete")) {
                    scriptSettings.Bookmarks.erase(editBookmark);
                }
            }
            else {
                if (ImGui::InputText("Name", &bookmarkName, ImGuiInputTextFlags_EnterReturnsTrue) 
                    || ImGui::MenuItem("Add Bookmark")) {
                    if (bookmarkName.empty()) {
                        bookmarkName = Util::Format("%d#", scriptSettings.Bookmarks.size()+1);
                    }

                    OFS_ScriptSettings::Bookmark bookmark(std::move(bookmarkName), player->getCurrentPositionSecondsInterp());
                    scriptSettings.AddBookmark(std::move(bookmark));
                }

                auto it = std::find_if(scriptSettings.Bookmarks.rbegin(), scriptSettings.Bookmarks.rend(),
                    [&](auto& mark) {
                        return mark.atS < player->getCurrentPositionSecondsInterp();
                    });
                if (it != scriptSettings.Bookmarks.rend() && it->type != OFS_ScriptSettings::Bookmark::BookmarkType::END_MARKER) {
                    char tmp[512];
                    stbsp_snprintf(tmp, sizeof(tmp), "Create interval for \"%s\"", it->name.c_str());
                    if (ImGui::MenuItem(tmp)) {
                        OFS_ScriptSettings::Bookmark bookmark(it->name + "_end", player->getCurrentPositionSecondsInterp());
                        scriptSettings.AddBookmark(std::move(bookmark));
                    }
                }
            }

            if (ImGui::BeginMenu("Go to...")) {
                if (scriptSettings.Bookmarks.size() == 0) {
                    ImGui::TextDisabled("No bookmarks");
                }
                else {
                    for (auto& mark : scriptSettings.Bookmarks) {
                        if (ImGui::MenuItem(mark.name.c_str())) {
                            player->setPositionExact(mark.atS);
                        }
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::Checkbox("Always show labels", &settings->data().always_show_bookmark_labels)) {
                settings->saveSettings();
            }

            if (ImGui::MenuItem("Delete all bookmarks"))
            {
                scriptSettings.Bookmarks.clear();
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
#ifndef NDEBUG
            // this breaks the layout after restarting for some reason
            if (ImGui::MenuItem("Reset layout")) { setupDefaultLayout(true); }
            ImGui::Separator();
#endif
            if (ImGui::MenuItem(StatisticsId, NULL, &settings->data().show_statistics)) {}
            if (ImGui::MenuItem(UndoSystem::UndoHistoryId, NULL, &settings->data().show_history)) {}
            if (ImGui::MenuItem(ScriptSimulator::SimulatorId, NULL, &settings->data().show_simulator)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Simulator 3D", NULL, &settings->data().show_simulator_3d)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Metadata", NULL, &ShowMetadataEditor)) {}
            if (ImGui::MenuItem("Action editor", NULL, &settings->data().show_action_editor)) {}
            if (ImGui::MenuItem(SpecialFunctionsWindow::SpecialFunctionsId, NULL, &settings->data().show_special_functions)) {}
            if (ImGui::MenuItem("T-Code", NULL, &settings->data().show_tcode)) {}

            ImGui::Separator();

            if (ImGui::MenuItem("Draw video", NULL, &settings->data().draw_video)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Reset video position", NULL)) { player->resetTranslationAndZoom(); }
            ImGui::Combo("Video Mode", (int32_t*)&player->settings.activeMode,
                "Full Video\0"
                "Left Pane\0"
                "Right Pane\0"
                "Top Pane\0"
                "Bottom Pane\0"
                "VR\0"
                "\0");
            ImGui::Separator();
            if (ImGui::BeginMenu("DEBUG ONLY")) {
                if (ImGui::MenuItem("ImGui", NULL, &DebugMetrics)) {}
                if (ImGui::MenuItem("ImGui Demo", NULL, &DebugDemo)) {}
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("Keys")) {
                keybinds.ShowWindow = true;
            }
            bool fullscreenTmp = Status & OFS_Status::OFS_Fullscreen;
            if (ImGui::MenuItem("Fullscreen", BINDING_STRING("fullscreen_toggle"), &fullscreenTmp)) {
                SetFullscreen(fullscreenTmp);
                Status = fullscreenTmp 
                    ? Status | OFS_Status::OFS_Fullscreen 
                    : Status ^ OFS_Status::OFS_Fullscreen;
            }
            if (ImGui::MenuItem("Preferences")) {
                settings->ShowWindow = true;
            }
            ImGui::EndMenu();
        }
        if (ControllerInput::AnythingConnected()) {
            if (ImGui::BeginMenu("Controller")) {
                ImGui::TextColored(ImColor(IM_COL32(0, 255, 0, 255)), "%s", "Controller connected!");
                ImGui::DragInt("Repeat rate", &settings->data().buttonRepeatIntervalMs, 1, 25, 500, "%d", ImGuiSliderFlags_AlwaysClamp);
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
                OFS::Tooltip("Selecting doesn't do anything right now.");

                ImGui::EndMenu();
            }
        }
        if(ImGui::MenuItem("About", NULL, &ShowAbout)) {}
        ImGui::Separator();
        ImGui::Spacing();
        if (ControllerInput::AnythingConnected()) {
            bool navmodeActive = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad;
            ImGui::Text(ICON_GAMEPAD " " ICON_LONG_ARROW_RIGHT " %s", (navmodeActive) ? "Navigation" : "Scripting");
        }
        if (player->isLoaded() && unsavedEdits) {
            const float timeUnit = saveDuration.count() / 60.f;
            ImGui::SameLine(region.x - ImGui::GetFontSize()*13.5f);
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_Text], "unsaved changes %d minutes ago", (int)(timeUnit));
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
        ImGui::OpenPopup("Metadata Editor");
    }

    OFS_PROFILE(__FUNCTION__);
    bool save = false;
    auto& metadata = LoadedProject->Metadata;
    
    if (ImGui::BeginPopupModal("Metadata Editor", open, ImGuiWindowFlags_NoDocking))
    {
        ImGui::InputText("Title", &metadata.title);
        metadata.duration = (int64_t)player->getDuration();
        Util::FormatTime(tmpBuf[0], sizeof(tmpBuf[0]), (float)metadata.duration, false);
        ImGui::LabelText("Duration", "%s", tmpBuf[0]);

        ImGui::InputText("Creator", &metadata.creator);
        ImGui::InputText("Url", &metadata.script_url);
        ImGui::InputText("Video url", &metadata.video_url);
        ImGui::InputTextMultiline("Description", &metadata.description, ImVec2(0.f, ImGui::GetFontSize()*3.f));
        ImGui::InputTextMultiline("Notes", &metadata.notes, ImVec2(0.f, ImGui::GetFontSize() * 3.f));

        {
            enum class LicenseType : int32_t {
                None,
                Free,
                Paid
            };
            static LicenseType currentLicense = LicenseType::None;

            if (ImGui::Combo("License", (int32_t*)&currentLicense, " \0Free\0Paid\0")) {
                switch (currentLicense) {
                case  LicenseType::None:
                    metadata.license = "";
                    break;
                case LicenseType::Free:
                    metadata.license = "Free";
                    break;
                case LicenseType::Paid:
                    metadata.license = "Paid";
                    break;
                }
            }
            if (!metadata.license.empty()) {
                ImGui::SameLine(); ImGui::Text("-> \"%s\"", metadata.license.c_str());
            }
        }
    
        auto renderTagButtons = [](std::vector<std::string>& tags)
        {
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
        ImGui::TextUnformatted("Tags");
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
        if (ImGui::Button("Add", ImVec2(-1.f, 0.f))) { 
            addTag(newTag);
        }
    
        auto& style = ImGui::GetStyle();

        renderTagButtons(metadata.tags);
        ImGui::NewLine();

        constexpr const char* performerIdString = "##Performer";
        ImGui::TextUnformatted("Performers");
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
        if (ImGui::Button("Add##Performer", ImVec2(-1.f, 0.f))) {
            addPerformer(newPerformer);
        }


        renderTagButtons(metadata.performers);
        ImGui::NewLine();
        ImGui::Separator();
        float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x;
        availWidth /= 2.f;
        if (ImGui::Button("Save", ImVec2(availWidth, 0.f))) { save = true; }
        ImGui::SameLine();
        if (ImGui::Button("Save template " ICON_COPY, ImVec2(availWidth, 0.f))) {
            settings->data().defaultMetadata = LoadedProject->Metadata;
        }
        OFS::Tooltip("Saves all current values as defaults for later.\n"
        "Don't worry about title and duration.");
        Util::ForceMinumumWindowSize(ImGui::GetCurrentWindow());
        ImGui::EndPopup();
    }
    return save;
}

void OpenFunscripter::SetFullscreen(bool fullscreen) {
    static SDL_Rect restoreRect{ 0,0, 1280,720 };
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
    ImGui::Begin("About", open, ImGuiWindowFlags_None 
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoCollapse
    );
    ImGui::TextUnformatted("OpenFunscripter " OFS_LATEST_GIT_TAG);
    ImGui::Text("Commit: %s", OFS_LATEST_GIT_HASH);
    if (ImGui::Button("Latest release " ICON_GITHUB, ImVec2(-1.f, 0.f))) {
        Util::OpenUrl("https://github.com/gagax1234/OpenFunscripter/releases/latest");
    }
    ImGui::End();
}

void OpenFunscripter::ShowStatisticsWindow(bool* open) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);
    ImGui::Begin(StatisticsId, open, ImGuiWindowFlags_None);
    const float currentTime = player->getCurrentPositionSecondsInterp();
    const FunscriptAction* front = ActiveFunscript()->GetActionAtTime(currentTime, 0.f);
    const FunscriptAction* behind = nullptr;
    if (front != nullptr) {
        behind = ActiveFunscript()->GetPreviousActionBehind(front->atS);
    }
    else {
        behind = ActiveFunscript()->GetPreviousActionBehind(currentTime);
        front = ActiveFunscript()->GetNextActionAhead(currentTime);
    }

    if (behind != nullptr) {
        ImGui::Text("Interval: %.2lf ms", ((double)currentTime - behind->atS)*1000.0);
        if (front != nullptr) {
            auto duration = front->atS - behind->atS;
            int32_t length = front->pos - behind->pos;
            ImGui::Text("Speed: %.02lf units/s", std::abs(length) / duration);
            ImGui::Text("Duration: %.2lf ms", (double)duration * 1000.0);
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
        app->player->setSpeed(speed);
        lastAxis = caxis.axis;
    }
    else if (caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        float speed = 1.f + (caxis.value / (float)std::numeric_limits<int16_t>::max());
        app->player->setSpeed(speed);
        lastAxis = caxis.axis;
    }
}

void OpenFunscripter::ScriptTimelineDoubleClick(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    float seekToTime = *((float*)&ev.user.data1);
    player->setPositionExact(seekToTime);
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
