#include "OpenFunscripter.h"

#include "OpenFunscripterUtil.h"

#include "GradientBar.h"

#include <filesystem>

#include "stb_sprintf.h"

#include "ImGuizmo.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"

// TODO: reduce memory usage when generating waveform data

// FIX: Add type checking to the deserialization. 
//      I assume it would crash if a field is specified but doesn't have the correct type.

// TODO: use a ringbuffer in the undosystem
// TODO: make heatmap generation more sophisticated

// TODO: improve shift click add action with simulator
//       it bugs out if the simulator is on the same height as the script timeline

// BUG: loading script without video breaks everything
// TODO: extend "range extender" functionality ( only extend bottom/top, range reducer )
// TODO: render simulator relative to video position & zoom


// TODO: Change how twist is implemented in the 3D simulator

// BUG: Simulator 3D move widget doesn't show when settings window is in a separate platform window/viewport

// BUG: scripts not getting unloaded when loading new video?

// the video player supports a lot more than these
// these are the ones looked for when loading funscripts
constexpr std::array<const char*, 6> SupportedVideoExtensions {
    ".mp4",
    ".mkv",
    ".webm",
    ".wmv",
    ".avi",
    ".m4v",
};

constexpr std::array<const char*, 4> SupportedAudioExtensions{
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
constexpr const char* PlayerTimeId = "Time";
constexpr const char* PlayerControlId = "Controls";
constexpr const char* ActionEditorId = "Action editor";

constexpr int DefaultWidth = 1920;
constexpr int DefaultHeight= 1080;

bool OpenFunscripter::imgui_setup() noexcept
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigViewportsNoDecoration = false;
    io.ConfigViewportsNoAutoMerge = false;
    io.ConfigViewportsNoTaskBarIcon = false;
    
    static auto imguiIniPath = Util::Prefpath("imgui.ini");
    io.IniFilename = imguiIniPath.c_str();

    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // LOAD FONTS
    auto roboto = Util::Resource("fonts/RobotoMono-Regular.ttf");
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
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();

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
    GLuint font_tex;
    glGenTextures(1, &font_tex);
    glBindTexture(GL_TEXTURE_2D, font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    io.Fonts->TexID = (void*)(intptr_t)font_tex;
    return true;
}

OpenFunscripter::~OpenFunscripter()
{
    // needs a certain destruction order
    scripting.reset();
    controllerInput.reset();
    specialFunctions.reset();
    for (auto&& script : LoadedFunscripts) { script.reset(); }
    events.reset();

    settings->saveSettings();
}

bool OpenFunscripter::setup()
{
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    ptr = this;
    auto prefPath = Util::Prefpath("");
    Util::CreateDirectories(prefPath);

    settings = std::make_unique<OpenFunscripterSettings>(Util::Prefpath("keybinds.json"), Util::Prefpath("config.json"));
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        LOGF_ERROR("Error: %s\n", SDL_GetError());
        return false;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

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
        "OpenFunscripter " FUN_LATEST_GIT_TAG "@" FUN_LATEST_GIT_HASH,
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
    
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    
    if (gladLoadGL() == 0) {
        LOG_ERROR("Failed to load glad.");
        return false;
    }
    
    if (!imgui_setup()) {
        LOG_ERROR("Failed to setup ImGui");
        return false;
    }

    //SDL_Surface* surface;
    //int image_width = 0;
    //int image_height = 0;
    //int channels;
    //unsigned char* image_data = stbi_load(Util::Prefpath("logo64.png").c_str(), &image_width, &image_height, &channels, 3);
    //if (image_data != nullptr) {
    //    std::array<uint16_t, 64 * 64> pixels;
    //    for (int i = 0; i < (image_width * image_height * 3); i += 3) {
    //        pixels[i / 3] = 0;
    //        pixels[i / 3] |= 0xf << 12;
    //        pixels[i / 3] |= (image_data[i] / 16) << 8;
    //        pixels[i / 3] |= (image_data[i + 1] / 16) << 4;
    //        pixels[i / 3] |= (image_data[i + 2] / 16) << 0;
    //    }
    //    surface = SDL_CreateRGBSurfaceFrom(pixels.data(), 64, 64, 64, 16 * 2, 0x0f00, 0x00f0, 0x000f, 0xf000);
    //    // The icon is attached to the window pointer
    //    SDL_SetWindowIcon(window, surface);

    //    // ...and the surface containing the icon pixel data is no longer required.
    //    SDL_FreeSurface(surface);
    //    stbi_image_free(image_data);
    //}

    // register custom events with sdl
    events = std::make_unique<EventSystem>();
    events->setup();

    keybinds.setup();
    register_bindings(); // needs to happen before setBindings
    keybinds.setBindings(settings->getKeybindings()); // override with user bindings

    scriptPositions.setup();
    clearLoadedScripts(); // initialized std::vector with one Funscript

    scripting = std::make_unique<ScriptingMode>();
    scripting->setup();
    if (!player.setup()) {
        LOG_ERROR("Failed to init video player");
        return false;
    }

    events->Subscribe(EventSystem::FunscriptActionsChangedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptChanged));
    events->Subscribe(EventSystem::FunscriptActionClickedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptActionClicked));
    events->Subscribe(SDL_DROPFILE, EVENT_SYSTEM_BIND(this, &OpenFunscripter::DragNDrop));
    events->Subscribe(EventSystem::MpvVideoLoaded, EVENT_SYSTEM_BIND(this, &OpenFunscripter::MpvVideoLoaded));
    
    if (!settings->data().recentFiles.empty()) {
        // cache these here because openFile overrides them
        std::string last_video = settings->data().recentFiles.back().video_path;
        std::string last_script = settings->data().recentFiles.back().script_path;
        if (!last_script.empty())
            openFile(last_script);
        if (!last_video.empty() && player.isLoaded())
            openFile(last_video);
    }

    specialFunctions = std::make_unique<SpecialFunctionsWindow>();
    controllerInput = std::make_unique<ControllerInput>();
    controllerInput->setup();
    simulator.setup();
    sim3D = std::make_unique<Simulator3D>();
    sim3D->setup();

    SDL_ShowWindow(window);

#ifndef NDEBUG
    scripting->setMode(ScriptingModeEnum::RECORDING);
#endif

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
        ImGui::DockBuilderDockWindow(PlayerTimeId, dock_time_bottom_id);
        ImGui::DockBuilderDockWindow(PlayerControlId, dock_player_control_id);
        ImGui::DockBuilderDockWindow(ScriptPositionsWindow::PositionsId, dock_positions_id);
        ImGui::DockBuilderDockWindow(ScriptingMode::ScriptingModeId, dock_mode_right_id);
        ImGui::DockBuilderDockWindow(ScriptSimulator::SimulatorId, dock_simulator_right_id);
        ImGui::DockBuilderDockWindow(ActionEditorId, dock_action_right_id);
        ImGui::DockBuilderDockWindow(StatisticsId, dock_stats_right_id);
        ImGui::DockBuilderDockWindow(UndoSystem::UndoHistoryId, dock_undo_right_id);
        simulator.CenterSimulator();
        ImGui::DockBuilderFinish(MainDockspaceID);
    }
}

void OpenFunscripter::clearLoadedScripts() noexcept
{
    LoadedFunscripts.clear();
    LoadedFunscripts.emplace_back(std::move(std::make_unique<Funscript>()));
    ActiveFunscriptIdx = 0;
}

void OpenFunscripter::register_bindings()
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

        std::stringstream ss;
        for (int i = 1; i < 10; i++) {
            ss.str("");
            ss << "action " << i * 10;
            std::string id = ss.str();
            ss.str("");
            ss << "Action at " << i * 10;

            auto& action = group.bindings.emplace_back(
                id,
                ss.str(),
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

        keybinds.registerBinding(group);
    }

    {
        KeybindingGroup group;
        group.name = "Core";

        // SAVE
        auto& save = group.bindings.emplace_back(
            "save",
            "Save",
            true,
            [&](void*) { saveScripts(); }
        );
        save.key = Keybinding(
            SDLK_s,
            KMOD_CTRL
        );

        auto& sync_timestamp = group.bindings.emplace_back(
            "sync_timestamps",
            "Sync time with player",
            true,
            [&](void*) { player.syncWithRealTime(); }
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
                    ActiveFunscriptIdx %= LoadedFunscripts.size();
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
                    ActiveFunscriptIdx %= LoadedFunscripts.size();
                } while (!ActiveFunscript()->Enabled);
                UpdateNewActiveScript(ActiveFunscriptIdx);
            }
        );
        cycle_loaded_backward_scripts.key = Keybinding(
            SDLK_PAGEUP,
            0
        );


        keybinds.registerBinding(group);
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
                auto action = ActiveFunscript()->GetPreviousActionBehind(player.getCurrentPositionMsInterp() - 1.f);
                if (action != nullptr) player.setPosition(action->at);
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
                auto action = ActiveFunscript()->GetNextActionAhead(player.getCurrentPositionMsInterp() + 1.f);
                if (action != nullptr) player.setPosition(action->at);
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
                int32_t closestMs = std::numeric_limits<int32_t>::max();
                int32_t currentMs = std::round(player.getCurrentPositionMsInterp());

                for(int i=0; i < LoadedFunscripts.size(); i++) {
                    auto& script = LoadedFunscripts[i];
                    auto action = script->GetPreviousActionBehind(currentMs - 1);
                    if (action != nullptr) {
                        if (std::abs(currentMs - action->at) < std::abs(currentMs - closestMs)) {
                            foundAction = true;
                            closestMs = action->at;
                        }
                    }
                }
                if (foundAction) {
                    player.setPosition(closestMs);
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
                int32_t closestMs = std::numeric_limits<int32_t>::max();
                int32_t currentMs = std::round(player.getCurrentPositionMsInterp());
                for (int i = 0; i < LoadedFunscripts.size(); i++) {
                    auto& script = LoadedFunscripts[i];
                    auto action = script->GetNextActionAhead(currentMs + 1);
                    if (action != nullptr) {
                        if (std::abs(currentMs - action->at) < std::abs(currentMs - closestMs)) {
                            foundAction = true;
                            closestMs = action->at;
                        }
                    }
                }
                if (foundAction) {
                    player.setPosition(closestMs);
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
                if (player.isPaused()) {
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
                if (player.isPaused()) {
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
                player.relativeFrameSeek(frameStep);
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
                player.relativeFrameSeek(-frameStep);
            }
        );
        fast_backstep.key = Keybinding(
            SDLK_LEFT,
            KMOD_CTRL
        );
        keybinds.registerBinding(group);
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
                if (settings->data().mirror_mode) {
                    for (auto&& script : LoadedFunscripts) {
                        script->undoSystem->Undo();
                    }
                }
                else {
                    ActiveFunscript()->undoSystem->Undo(); 
                }
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
                if (settings->data().mirror_mode) {
                    for (auto&& script : LoadedFunscripts) {
                        script->undoSystem->Redo();
                    }
                }
                else {
                    ActiveFunscript()->undoSystem->Redo(); 
                }
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

        auto& toggle_mirror_mode = group.bindings.emplace_back(
            "toggle_mirror_mode",
            "Toggle mirror mode",
            true,
            [&](void*) { if (LoadedFunscripts.size() > 0) { settings->data().mirror_mode = !settings->data().mirror_mode; }}
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
                player.saveFrameToImage(screenshot_dir); 
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
            [&](void*) { player.cycleSubtitles(); }
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
            [&](void*) { Fullscreen = !Fullscreen; SetFullscreen(Fullscreen); }
        );
        fullscreen_toggle.key = Keybinding(
            SDLK_F10,
            0
        );
        keybinds.registerBinding(group);
    }

    // MOVE LEFT/RIGHT
    auto move_actions_horizontal = [](int32_t time_ms) {
        auto ptr = OpenFunscripter::ptr;
        auto undoSystem = ptr->script().undoSystem.get();
        if (ptr->ActiveFunscript()->HasSelection()) {
            undoSystem->Snapshot(StateType::ACTIONS_MOVED);
            ptr->ActiveFunscript()->MoveSelectionTime(time_ms);
        }
        else {
            auto closest = ptr->ActiveFunscript()->GetClosestAction(ptr->player.getCurrentPositionMsInterp());
            if (closest != nullptr) {
                undoSystem->Snapshot(StateType::ACTIONS_MOVED);
                FunscriptAction moved(closest->at + time_ms, closest->pos);
                ptr->ActiveFunscript()->EditAction(*closest, moved);
            }
        }
    };
    auto move_actions_horizontal_with_video = [](int32_t time_ms) {
        auto ptr = OpenFunscripter::ptr;
        auto undoSystem = ptr->script().undoSystem.get();
        if (ptr->ActiveFunscript()->HasSelection()) {
            undoSystem->Snapshot(StateType::ACTIONS_MOVED);
            ptr->ActiveFunscript()->MoveSelectionTime(time_ms);
            auto closest = ptr->ActiveFunscript()->GetClosestActionSelection(ptr->player.getCurrentPositionMsInterp());
            if (closest != nullptr) { ptr->player.setPosition(closest->at); }
            else { ptr->player.setPosition(ptr->ActiveFunscript()->Selection().front().at); }
        }
        else {
            auto closest = ptr->ActiveFunscript()->GetClosestAction(ptr->player.getCurrentPositionMsInterp());
            if (closest != nullptr) {
                undoSystem->Snapshot(StateType::ACTIONS_MOVED);
                FunscriptAction moved(closest->at + time_ms, closest->pos);
                ptr->ActiveFunscript()->EditAction(*closest, moved);
                ptr->player.setPosition(moved.at);
            }
        }
    };
    {
        KeybindingGroup group;
        group.name = "Moving";
        auto& move_actions_left_snapped = group.bindings.emplace_back(
            "move_actions_left_snapped",
            "Move actions left with snapping",
            false,
            [&](void*) {
                move_actions_horizontal_with_video(-player.getFrameTimeMs());
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
                move_actions_horizontal_with_video(player.getFrameTimeMs());
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
                move_actions_horizontal(-player.getFrameTimeMs());
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
                move_actions_horizontal(player.getFrameTimeMs());
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
                    ActiveFunscript()->undoSystem->Snapshot(StateType::ACTIONS_MOVED);
                    ActiveFunscript()->MoveSelectionPosition(1);
                }
                else {
                    auto closest = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
                    if (closest != nullptr) {
                        FunscriptAction moved(closest->at, closest->pos + 1);
                        if (moved.pos <= 100 && moved.pos >= 0) {
                            ActiveFunscript()->undoSystem->Snapshot(StateType::ACTIONS_MOVED);
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
                ActiveFunscript()->undoSystem->Snapshot(StateType::ACTIONS_MOVED);
                if (ActiveFunscript()->HasSelection()) {
                    ActiveFunscript()->MoveSelectionPosition(-1);
                }
                else {
                    auto closest = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
                    if (closest != nullptr) {
                        FunscriptAction moved(closest->at, closest->pos - 1);
                        if (moved.pos <= 100 && moved.pos >= 0) {
                            ActiveFunscript()->undoSystem->Snapshot(StateType::ACTIONS_MOVED);
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
                auto closest = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
                if (closest != nullptr) {
                    ActiveFunscript()->undoSystem->Snapshot(StateType::MOVE_ACTION_TO_CURRENT_POS);
                    ActiveFunscript()->EditAction(*closest, FunscriptAction(player.getCurrentPositionMsInterp(), closest->pos));
                }
            }
        );
        move_action_to_current_pos.key = Keybinding(
            SDLK_END,
            0
        );

        keybinds.registerBinding(group);
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

        keybinds.registerBinding(group);
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
            [&](void*) { player.togglePlay(); }
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
            [&](void*) { player.addSpeed(-0.10); }
        );
        decrement_speed.key = Keybinding(
            SDLK_KP_MINUS,
            0
        );
        auto& increment_speed = group.bindings.emplace_back(
            "increment_speed",
            "Playback speed +10%",
            true,
            [&](void*) { player.addSpeed(0.10); }
        );
        increment_speed.key = Keybinding(
            SDLK_KP_PLUS,
            0
        );

        keybinds.registerBinding(group);
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
            [&](void*) { seekByTime(1000); }
        );
        seek_forward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
            false
        );

        auto& seek_backward_second = group.bindings.emplace_back(
            "seek_backward_second",
            "Backward 1 second",
            false,
            [&](void*) { seekByTime(-1000); }
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
                static ScriptingModeEnum prevMode;
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
                    scriptPositions.setStartSelection(player.getCurrentPositionMsInterp());
                }
                else {
                    int32_t tmp = player.getCurrentPositionMsInterp();
                    auto [min, max] = std::minmax(scriptPositions.selectionStart(), tmp);
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
                keybinds.SetControllerPlaybackSpeed();
            }
        );
        set_playbackspeed_controller.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_X,
            false
        );
        keybinds.registerBinding(group);
    }
}


void OpenFunscripter::new_frame() noexcept
{
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

void OpenFunscripter::process_events() noexcept
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
        {
            exit_app = true;
        }
            break;
        case SDL_WINDOWEVENT:
        {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                exit_app = true;
            }
            break;
        }
        }
        events->PushEvent(event);
    }
}

void OpenFunscripter::FunscriptChanged(SDL_Event& ev) noexcept
{
    updateTimelineGradient = true;
}

void OpenFunscripter::FunscriptActionClicked(SDL_Event& ev) noexcept
{
    auto& [btn_ev, action] = *((ActionClickedEventArgs*)ev.user.data1);
    auto& button = btn_ev.button; // turns out I don't need this...

    if (SDL_GetModState() & KMOD_CTRL) {
        ActiveFunscript()->SelectAction(action);
    }
    else {
        player.setPosition(action.at);
    }
}

void OpenFunscripter::DragNDrop(SDL_Event& ev) noexcept
{
    openFile(ev.drop.file);
    SDL_free(ev.drop.file);
}

void OpenFunscripter::MpvVideoLoaded(SDL_Event& ev) noexcept
{
    ActiveFunscript()->metadata.duration = player.getDuration();
    ActiveFunscript()->reserveActionMemory(player.getTotalNumFrames());
    player.setPosition(ActiveFunscript()->scriptSettings.last_pos_ms);
    if (ActiveFunscript()->scriptSettings.player != nullptr) {
        player.setSpeed(ActiveFunscript()->scriptSettings.player->playback_speed);
        player.setVolume(ActiveFunscript()->scriptSettings.player->volume);
    }
    ActiveFunscript()->NotifyActionsChanged(false);

    auto name = Util::Filename(player.getVideoPath());
    ActiveFunscript()->metadata.title = name;
    auto recentFile = OpenFunscripterSettings::RecentFile{ name, std::string(player.getVideoPath()), ActiveFunscript()->current_path };
    settings->addRecentFile(recentFile);
    scriptPositions.ClearAudioWaveform();
}

void OpenFunscripter::update() noexcept {
    ActiveFunscript()->update();
    ControllerInput::UpdateControllers();
    scripting->update();

    if (RollingBackup) {
        rollingBackup();
    }
}

void OpenFunscripter::rollingBackup() noexcept
{
    if (ActiveFunscript()->current_path.empty()) { return; }
    std::chrono::duration<float> timeSinceBackup = std::chrono::system_clock::now() - last_backup;
    if (timeSinceBackup.count() < 61.f) {
        return;
    }
    last_backup = std::chrono::system_clock::now();

    auto backupDir = std::filesystem::path(Util::Prefpath("backup"));
    auto name = Util::Filename(player.getVideoPath());
    name = Util::trim(name); // this needs to be trimmed because trailing spaces
#ifdef WIN32
    backupDir /= Util::Utf8ToUtf16(name);
#else
    backupDir /= name;
#endif
    if (!Util::CreateDirectories(backupDir)) {
        return;
    }
    
    char path_buf[1024];
    for (auto&& script : LoadedFunscripts) {

        auto scriptName = Util::Filename(script->current_path);
        Util::trim(scriptName);
#ifdef WIN32
        auto scriptBackupDir = backupDir / Util::Utf8ToUtf16(scriptName);
#else
        auto scriptBackupDir = backupDir / scriptName;
#endif
        if (!Util::CreateDirectories(scriptBackupDir)) {
            continue;
        }

        stbsp_snprintf(path_buf, sizeof(path_buf), "Backup_%d.funscript", SDL_GetTicks());
        auto savePath = scriptBackupDir / path_buf;
        LOGF_INFO("Backup at \"%s\"", savePath.u8string().c_str());
        script->save(savePath.u8string().c_str(), false);

        // delete oldest backup
        auto count_files = [](const std::filesystem::path& path) -> std::size_t
        {
            std::error_code ec;
            auto safe_iterator = std::filesystem::directory_iterator(path, ec);
            auto count = (std::size_t)std::distance(safe_iterator, std::filesystem::end(safe_iterator));
            if (ec) {
                LOGF_ERROR("Failed to count files %s", ec.message().c_str());
                LOGF_ERROR("Path: \"%s\"", path.string().c_str());
                return 0;
            }
            return count;
        };
        auto get_oldest_file = [](const std::filesystem::path& path) {
            std::error_code ec;
            auto iterator = std::filesystem::directory_iterator(path, ec);
            std::filesystem::directory_iterator oldest = std::filesystem::directory_iterator(path, ec);
            for (auto it = std::filesystem::begin(iterator); it != std::filesystem::end(iterator); it++) {
                auto path1 = it->path();
                auto path2 = oldest->path();
                auto time1 = std::filesystem::last_write_time(path1, ec).time_since_epoch().count();
                auto time2 = std::filesystem::last_write_time(path2, ec).time_since_epoch().count();
                if (time1 < time2) {
                    oldest = it;
                }
            }
            return oldest;
        };
        if (count_files(scriptBackupDir) > 5) {
            auto oldest_backup = get_oldest_file(scriptBackupDir);
            std::error_code ec;
            if ((*oldest_backup).path().extension() == ".funscript") {
                LOGF_INFO("Removing old backup: \"%s\"", (*oldest_backup).path().u8string().c_str());
                std::filesystem::remove(*oldest_backup, ec);
                if (ec) {
                    LOGF_INFO("Failed to remove old backup\n%s", ec.message().c_str());
                }
            }
        }
    }
}

int OpenFunscripter::run() noexcept
{
    new_frame();
    setupDefaultLayout(false);
    render();
    while (!exit_app) {
        process_events();
        update();
        new_frame();
        {
            // IMGUI HERE
            CreateDockspace();
            ShowAboutWindow(&ShowAbout);
            specialFunctions->ShowFunctionsWindow(&settings->data().show_special_functions);
            ActiveFunscript()->undoSystem->ShowUndoRedoHistory(&settings->data().show_history);
            simulator.ShowSimulator(&settings->data().show_simulator);
            ShowStatisticsWindow(&settings->data().show_statistics);
            if (ShowMetadataEditorWindow(&ShowMetadataEditor)) { ActiveFunscript()->save(); }
            sim3D->ShowWindow(&settings->data().show_simulator_3d);
            scripting->DrawScriptingMode(NULL);

            if (keybinds.ShowBindingWindow()) {
                settings->saveKeybinds(keybinds.getBindings());
            }

            if (settings->ShowPreferenceWindow()) {
                settings->saveSettings();
            }

            if (player.isLoaded()) {
                {
                    ImGui::Begin(PlayerControlId);
                
                    const int seek_ms = 3000;
                    // Playback controls
                    ImGui::Columns(5, 0, false);
                    if (ImGui::Button(ICON_STEP_BACKWARD /*"<"*/, ImVec2(-1, 0))) {
                        if (player.isPaused()) {
                            scripting->PreviousFrame();
                        }
                    }
                    ImGui::NextColumn();
                    if (ImGui::Button(ICON_BACKWARD /*"<<"*/, ImVec2(-1, 0))) {
                        seekByTime(-seek_ms);
                    }
                    ImGui::NextColumn();

                    if (ImGui::Button((player.isPaused()) ? ICON_PLAY : ICON_PAUSE, ImVec2(-1, 0))) {
                        player.togglePlay();
                    }
                    ImGui::NextColumn();

                    if (ImGui::Button(ICON_FORWARD /*">>"*/, ImVec2(-1, 0))) {
                        seekByTime(seek_ms);
                    }
                    ImGui::NextColumn();

                    if (ImGui::Button(ICON_STEP_FORWARD /*">"*/, ImVec2(-1, 0))) {
                        if (player.isPaused()) {
                            scripting->NextFrame();
                        }
                    }
                    ImGui::NextColumn();

                static bool mute = false;
                ImGui::Columns(2, 0, false);
                if (ImGui::Checkbox(mute ? ICON_VOLUME_OFF : ICON_VOLUME_UP, &mute)) {
                    if (mute)
                        player.setVolume(0.0f);
                    else
                        player.setVolume(player.settings.volume);
                }
                ImGui::SetColumnWidth(0, ImGui::GetItemRectSize().x + 10);
                ImGui::NextColumn();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##Volume", &player.settings.volume, 0.0f, 1.0f)) {
                    player.settings.volume = Util::Clamp(player.settings.volume, 0.0f, 1.f);
                    player.setVolume(player.settings.volume);
                    if (player.settings.volume > 0.0f)
                        mute = false;
                }
                ImGui::NextColumn();
                ImGui::End();
            }
                {
                    ImGui::Begin(PlayerTimeId);

                    static float actualPlaybackSpeed = 1.0f;
                    {
                        const double speedCalcUpdateFrequency = 1.0;
                        static uint32_t start_time = SDL_GetTicks();
                        static float lastPlayerPosition = 0.0f;
                        if (!player.isPaused()) {
                            if ((SDL_GetTicks() - start_time) / 1000.0f >= speedCalcUpdateFrequency) {
                                double duration = player.getDuration();
                                double position = player.getPosition();
                                double expectedStep = speedCalcUpdateFrequency / duration;
                                double actualStep = std::abs(position - lastPlayerPosition);
                                actualPlaybackSpeed = actualStep / expectedStep;

                                lastPlayerPosition = player.getPosition();
                                start_time = SDL_GetTicks();
                            }
                        }
                        else {
                            lastPlayerPosition = player.getPosition();
                            start_time = SDL_GetTicks();
                        }
                    }

                    ImGui::Columns(5, 0, false);
                    {
                        // format total duration
                        // this doesn't need to be done every frame
                        Util::FormatTime(tmp_buf[1], sizeof(tmp_buf[1]), player.getDuration(), true);

                        double time_seconds = player.getCurrentPositionSecondsInterp();
                        Util::FormatTime(tmp_buf[0], sizeof(tmp_buf[0]), time_seconds, true);
                        ImGui::Text(" %s / %s (x%.03f)", tmp_buf[0], tmp_buf[1], actualPlaybackSpeed);
                        ImGui::NextColumn();
                    }

                    auto& style = ImGui::GetStyle();
                    ImGui::SetColumnWidth(0, ImGui::GetItemRectSize().x + style.ItemSpacing.x);

                    if (ImGui::Button("1x", ImVec2(0, 0))) {
                        player.setSpeed(1.f);
                    }
                    ImGui::SetColumnWidth(1, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                    ImGui::NextColumn();

                    if (ImGui::Button("-10%", ImVec2(0, 0))) {
                        player.addSpeed(-0.10);
                    }
                    ImGui::SetColumnWidth(2, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                    ImGui::NextColumn();

                    if (ImGui::Button("+10%", ImVec2(0, 0))) {
                        player.addSpeed(0.10);
                    }
                    ImGui::SetColumnWidth(3, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                    ImGui::NextColumn();

                    ImGui::SetNextItemWidth(-1.f);
                    if (ImGui::SliderFloat("##Speed", &player.settings.playback_speed, player.minPlaybackSpeed, player.maxPlaybackSpeed)) {
                        player.setSpeed(player.settings.playback_speed);
                    }
                    Util::Tooltip("Speed");

                    ImGui::Columns(1, 0, false);

                    float position = player.getPosition();
                    static bool hasSeeked = false;
                    if (DrawTimelineWidget("Timeline", &position)) {
                        if (!player.isPaused()) {
                            hasSeeked = true;
                        }
                        player.setPosition(position, true);
                    }
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && hasSeeked) {
                        player.setPaused(false);
                        hasSeeked = false;
                    }

                    scriptPositions.ShowScriptPositions(NULL, player.getCurrentPositionMsInterp());
                    ImGui::End();
                }
                if(settings->data().show_action_editor)
                {
                    ImGui::Begin(ActionEditorId, &settings->data().show_action_editor);
                    if (player.isPaused()) {
                        auto scriptAction = ActiveFunscript()->GetActionAtTime(player.getCurrentPositionMsInterp(), player.getFrameTimeMs());

                        if (scriptAction == nullptr)
                        {
                            // create action
                            static int newActionPosition = 0;
                            ImGui::SliderInt("Position", &newActionPosition, 0, 100);
                            if (ImGui::Button("New Action")) {
                                addEditAction(newActionPosition);
                            }
                        }
                    }

                    ImGui::Separator();
                    ImGui::Columns(1, 0, false);
                    if (ImGui::Button("100", ImVec2(-1, 0))) {
                        addEditAction(100);
                    }
                    for (int i = 9; i != 0; i--) {
                        if (i % 3 == 0) {
                            ImGui::Columns(3, 0, false);
                        }
                        sprintf(tmp_buf[0], "%d", i * 10);
                        if (ImGui::Button(tmp_buf[0], ImVec2(-1, 0))) {
                            addEditAction(i * 10);
                        }
                        ImGui::NextColumn();
                    }
                    ImGui::Columns(1, 0, false);
                    if (ImGui::Button("0", ImVec2(-1, 0))) {
                        addEditAction(0);
                    }

                    ImGui::Separator();
                    ImGui::End();
                }
            }

            if (DebugDemo) {
                ImGui::ShowDemoWindow(&DebugDemo);
            }

            if (DebugMetrics) {
                ImGui::ShowMetricsWindow(&DebugMetrics);
            }

            player.DrawVideoPlayer(NULL);
        }

        render();
        SDL_GL_SwapWindow(window);
    }
	return 0;
}

void OpenFunscripter::shutdown() noexcept
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);   
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void OpenFunscripter::SetCursorType(ImGuiMouseCursor id) noexcept
{
    //SDL_SetCursor(SystemCursors[id]);
    ImGui::SetMouseCursor(id);
}


bool OpenFunscripter::openFile(const std::string& file)
{
    if (!Util::FileExists(file)) return false;
    
    std::filesystem::path file_path(file);
    std::filesystem::path base_path = file_path;
    base_path.replace_extension("");
    std::string video_path;
    std::string funscript_path;

    if (file_path.extension() == ".funscript")
    {
        funscript_path = file;
        // try find video
        std::string videoPath;
        for (auto&& extension : SupportedVideoExtensions) {
            videoPath = base_path.string() + extension;
            if (Util::FileExists(videoPath)) {
                video_path = videoPath;
                break;
            }
        }

        if (video_path.empty()) {
            // try find audio
            for (auto&& extension : SupportedAudioExtensions) {
                videoPath = base_path.string() + extension;
                if (Util::FileExists(videoPath)) {
                    video_path = videoPath;
                    break;
                }
            }
        }
    }
    else {
        video_path = file;
        if (ScriptLoaded() && !Util::FileNamesMatch(video_path, RootFunscript()->current_path)) {
            funscript_path = base_path.string() + ".funscript";
        }
        else {
            funscript_path = RootFunscript()->current_path;
        }
    }

    if (video_path.empty()) {
        if (!Util::FileNamesMatch(player.getVideoPath(), funscript_path)) {
            LOG_WARN("No video found.\nLoading scripts without a video is not supported.");
            player.closeVideo();
        }
    }
    else {
        player.openVideo(video_path);
    }

    auto openFunscript = [this](const std::string& file) -> bool {
        RootFunscript() = std::make_unique<Funscript>();
        RootFunscript()->undoSystem->ClearHistory();
        if (!Util::FileExists(file)) {
            return false;
        }
        return RootFunscript()->open(file);
    };

    // try load funscript
    bool result = openFunscript(funscript_path);
    if (!result) {
        LOGF_WARN("Couldn't find funscript. \"%s\"", funscript_path.c_str());
        // do not return false here future me
    }
    for (auto&& associated : RootFunscript()->scriptSettings.associatedScripts) {
        auto associated_script = std::make_unique<Funscript>();
        if (associated_script->open(associated)) {
            LoadedFunscripts.emplace_back(std::move(associated_script));
        }
    }

    RootFunscript()->current_path = funscript_path;

    updateTitle();

    auto last_path = std::filesystem::path(file);
    last_path.replace_filename("");
    last_path /= "";
    settings->data().last_path = last_path.string();
    settings->saveSettings();

    last_backup = std::chrono::system_clock::now();

    return result;
}

void OpenFunscripter::UpdateNewActiveScript(int32_t activeIndex) noexcept
{
    ActiveFunscriptIdx = activeIndex;
    updateTitle();
    ActiveFunscript()->NotifyActionsChanged(false);
}

void OpenFunscripter::updateTitle() noexcept
{
    std::stringstream ss;
    ss.str(std::string());
    
    ss << "OpenFunscripter " FUN_LATEST_GIT_TAG "@" FUN_LATEST_GIT_HASH " - \"" << ActiveFunscript()->current_path << "\"";
    SDL_SetWindowTitle(window, ss.str().c_str());
}

void OpenFunscripter::saveScripts() noexcept
{
    for (auto&& script : LoadedFunscripts) {
        script->metadata.title = std::filesystem::path(script->current_path)
            .replace_extension("")
            .filename()
            .string();
        script->metadata.duration = player.getDuration();
        script->scriptSettings.last_pos_ms = player.getCurrentPositionMs();
        script->save();
    }
}

void OpenFunscripter::saveHeatmap(const char* path, int width, int height)
{
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
    while (relPos <= 1.f) {
        rect.x = std::round(relPos * width);
        TimelineGradient.computeColorAt(relPos, &color.Value.x);
        SDL_FillRect(surface, &rect, ImGui::ColorConvertFloat4ToU32(color));
        relPos += relStep;
    }
    Util::SavePNG(path, surface->pixels, surface->w, surface->h, 4, true);

    if (mustLock) { SDL_UnlockSurface(surface); }
    SDL_FreeSurface(surface);
}

void OpenFunscripter::removeAction(FunscriptAction action) noexcept
{
    ActiveFunscript()->undoSystem->Snapshot(StateType::REMOVE_ACTION);
    ActiveFunscript()->RemoveAction(action);
}

void OpenFunscripter::removeAction() noexcept
{
    if (settings->data().mirror_mode) {
        for (auto&& script : LoadedFunscripts) {
            auto action = script->GetClosestAction(player.getCurrentPositionMsInterp());
            if (action != nullptr) {
                script->undoSystem->Snapshot(StateType::REMOVE_ACTION);
                script->RemoveAction(*action);
            }
        }
    }
    else {
        if (ActiveFunscript()->HasSelection()) {
            ActiveFunscript()->undoSystem->Snapshot(StateType::REMOVE_SELECTION);
            ActiveFunscript()->RemoveSelectedActions();
        }
        else {
            auto action = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
            if (action != nullptr) {
                removeAction(*action); // snapshoted in here
            }
        }
    }
}

void OpenFunscripter::addEditAction(int pos) noexcept
{
    if (settings->data().mirror_mode) {
        int32_t currentActiveScriptIdx = ActiveFunscriptIndex();
        for (int i = 0; i < LoadedFunscripts.size(); i++) {
            UpdateNewActiveScript(i);
            ActiveFunscript()->undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS);
            scripting->addEditAction(FunscriptAction(std::round(player.getCurrentPositionMsInterp()), pos));
        }
        UpdateNewActiveScript(currentActiveScriptIdx);
    }
    else {
        ActiveFunscript()->undoSystem->Snapshot(StateType::ADD_EDIT_ACTIONS);
        scripting->addEditAction(FunscriptAction(std::round(player.getCurrentPositionMsInterp()), pos));
    }
}

void OpenFunscripter::seekByTime(int32_t ms) noexcept
{
    int32_t seek_to = player.getCurrentPositionMs() + ms;
    seek_to = std::max(seek_to, 0);
    player.setPosition(seek_to);
}

void OpenFunscripter::cutSelection() noexcept
{
    if (ActiveFunscript()->HasSelection()) {
        copySelection();
        ActiveFunscript()->undoSystem->Snapshot(StateType::CUT_SELECTION);
        ActiveFunscript()->RemoveSelectedActions();
    }
}

void OpenFunscripter::copySelection() noexcept
{
    if (ActiveFunscript()->HasSelection()) {
        CopiedSelection.clear();
        for (auto action : ActiveFunscript()->Selection()) {
            CopiedSelection.emplace_back(action);
        }
    }
}

void OpenFunscripter::pasteSelection() noexcept
{
    if (CopiedSelection.size() == 0) return;
    ActiveFunscript()->undoSystem->Snapshot(StateType::PASTE_COPIED_ACTIONS);
    // paste CopiedSelection relatively to position
    // NOTE: assumes CopiedSelection is ordered by time
    int offset_ms = std::round(player.getCurrentPositionMsInterp()) - CopiedSelection.begin()->at;

    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->PasteAction(FunscriptAction(action.at + offset_ms, action.pos), 1);
    }
    player.setPosition((CopiedSelection.end() - 1)->at + offset_ms);
}

void OpenFunscripter::pasteSelectionExact() noexcept {
    if (CopiedSelection.size() == 0) return;
    // paste without altering timestamps
    ActiveFunscript()->undoSystem->Snapshot(StateType::PASTE_COPIED_ACTIONS);
    for (auto&& action : CopiedSelection) {
        ActiveFunscript()->PasteAction(action, 1);
    }
}

void OpenFunscripter::equalizeSelection() noexcept
{
    if (!ActiveFunscript()->HasSelection()) {
        ActiveFunscript()->undoSystem->Snapshot(StateType::EQUALIZE_ACTIONS);
        // this is a small hack
        auto closest = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
        if (closest != nullptr) {
            auto behind = ActiveFunscript()->GetPreviousActionBehind(closest->at);
            if (behind != nullptr) {
                auto front = ActiveFunscript()->GetNextActionAhead(closest->at);
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
        ActiveFunscript()->undoSystem->Snapshot(StateType::EQUALIZE_ACTIONS);
        ActiveFunscript()->EqualizeSelection();
    }
}

void OpenFunscripter::invertSelection() noexcept
{
    if (!ActiveFunscript()->HasSelection()) {
        // same hack as above 
        auto closest = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
        if (closest != nullptr) {
            ActiveFunscript()->undoSystem->Snapshot(StateType::INVERT_ACTIONS);
            ActiveFunscript()->SelectAction(*closest);
            ActiveFunscript()->InvertSelection();
            ActiveFunscript()->ClearSelection();
        }
    }
    else if (ActiveFunscript()->Selection().size() >= 3) {
        ActiveFunscript()->undoSystem->Snapshot(StateType::INVERT_ACTIONS);
        ActiveFunscript()->InvertSelection();
    }
}

void OpenFunscripter::isolateAction() noexcept
{
    auto closest = ActiveFunscript()->GetClosestAction(player.getCurrentPositionMsInterp());
    if (closest != nullptr) {
        ActiveFunscript()->undoSystem->Snapshot(StateType::ISOLATE_ACTION);
        auto prev = ActiveFunscript()->GetPreviousActionBehind(closest->at - 1);
        auto next = ActiveFunscript()->GetNextActionAhead(closest->at + 1);
        if (prev != nullptr && next != nullptr) {
            auto tmp = *next; // removing prev will invalidate the pointer
            ActiveFunscript()->RemoveAction(*prev);
            ActiveFunscript()->RemoveAction(tmp);
        }
        else if (prev != nullptr) { ActiveFunscript()->RemoveAction(*prev); }
        else if (next != nullptr) { ActiveFunscript()->RemoveAction(*next); }

    }
}

void OpenFunscripter::repeatLastStroke() noexcept
{
    auto stroke = ActiveFunscript()->GetLastStroke(player.getCurrentPositionMsInterp());
    if (stroke.size() > 1) {
        int32_t offset_ms = player.getCurrentPositionMsInterp() - stroke.back().at;
        ActiveFunscript()->undoSystem->Snapshot(StateType::REPEAT_STROKE);
        auto action = ActiveFunscript()->GetActionAtTime(player.getCurrentPositionMsInterp(), player.getFrameTimeMs());
        // if we are on top of an action we ignore the first action of the last stroke
        if (action != nullptr) {
            for(int i=stroke.size()-2; i >= 0; i--) {
                auto action = stroke[i];
                action.at += offset_ms;
                ActiveFunscript()->PasteAction(action, player.getFrameTimeMs());
            }
        }
        else {
            for (int i = stroke.size()-1; i >= 0; i--) {
                auto action = stroke[i];
                action.at += offset_ms;
                ActiveFunscript()->PasteAction(action, player.getFrameTimeMs());
            }
        }
        player.setPosition(stroke.front().at + offset_ms);
    }
}

void OpenFunscripter::showOpenFileDialog()
{
    Util::OpenFileDialog("Choose a file", settings->data().last_path,
        [&](auto& result) {
            if (result.files.size() > 0) {
                auto file = result.files[0];
                if (Util::FileExists(file))
                {
                    clearLoadedScripts();
                    openFile(file);
                }
            }
        }, false);
}

void OpenFunscripter::saveActiveScriptAs()
{
    std::filesystem::path path = ActiveFunscript()->current_path;
    path.make_preferred();
    Util::SaveFileDialog("Save", path.string(),
        [&](auto& result) {
            if (result.files.size() > 0) {
                ActiveFunscript()->save(result.files[0], true);
                std::filesystem::path dir(result.files[0]);
                dir.remove_filename();
                settings->data().last_path = dir.string();
            }
        }, {"Funscript", "*.funscript"});
}

void OpenFunscripter::ShowMainMenuBar() noexcept
{
#define BINDING_STRING(binding) keybinds.getBindingString(binding).c_str()  
    
    ImColor alertCol(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
    std::chrono::duration<float> saveDuration;
    if (player.isLoaded() && ActiveFunscript()->HasUnsavedEdits()) {
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
            if (ImGui::MenuItem(ICON_FOLDER_OPEN" Open video / script")) {
                showOpenFileDialog();
            }
            if (ImGui::BeginMenu("Open...", player.isLoaded())) {
                auto fileAlreadyLoaded = [](const std::string& path) -> bool {
                    auto app = OpenFunscripter::ptr;
                    auto it = std::find_if(app->LoadedFunscripts.begin(), app->LoadedFunscripts.end(),
                        [file = std::filesystem::path(path)](auto& script) {
                            return std::filesystem::path(script->current_path) == file;
                        }
                    );
                    return it != app->LoadedFunscripts.end();
                };
                if (ImGui::MenuItem("Add new")) {
                    Util::SaveFileDialog("Add new funscript", settings->data().last_path,
                        [fileAlreadyLoaded](auto& result) {
                            if (result.files.size() > 0) {
                                auto app = OpenFunscripter::ptr;
                                if (!fileAlreadyLoaded(result.files[0])) {
                                    auto newScript = std::make_unique<Funscript>();
                                    newScript->current_path = result.files[0];
                                    newScript->metadata.title = Util::Filename(result.files[0]);
                                    app->LoadedFunscripts.emplace_back(std::move(newScript));
                                }
                            }
                        }, { "Funscript", "*.funscript" });
                }
                if (ImGui::MenuItem("Add existing")) {
                    Util::OpenFileDialog("Add existing funscripts", settings->data().last_path,
                        [fileAlreadyLoaded](auto& result) {
                            if (result.files.size() > 0) {
                                for (auto&& scriptPath : result.files) {
                                    auto newScript = std::make_unique<Funscript>();
                                    if (newScript->open(scriptPath)) {
                                        auto app = OpenFunscripter::ptr;
                                        if (!fileAlreadyLoaded(scriptPath)) {
                                            newScript->current_path = scriptPath;
                                            newScript->metadata.title = Util::Filename(scriptPath);
                                            OpenFunscripter::ptr->LoadedFunscripts.emplace_back(std::move(newScript));
                                        }
                                    }
                                }
                            }
                        }, true, { "*.funscript" }, "Funscript");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Unload", LoadedFunscripts.size() > 1)) {
                int unloadIndex = -1;
                for(int i=0; i < LoadedFunscripts.size(); i++) {
                    if (ImGui::MenuItem(LoadedFunscripts[i]->metadata.title.c_str())) {
                        unloadIndex = i;
                    }
                }
                if (unloadIndex >= 0) {
                    LoadedFunscripts.erase(LoadedFunscripts.begin() + unloadIndex);
                    if (ActiveFunscriptIdx > 0) { ActiveFunscriptIdx--; }
                    UpdateNewActiveScript(ActiveFunscriptIdx);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Recent files")) {
                if (settings->data().recentFiles.size() == 0) {
                    ImGui::TextDisabled("%s", "No recent files");
                }
                auto& recentFiles = settings->data().recentFiles;
                for (auto it = recentFiles.rbegin(); it != recentFiles.rend(); it++) {
                    auto& recent = *it;
                    if (ImGui::MenuItem(recent.name.c_str())) {
                        if (!recent.script_path.empty())
                            openFile(recent.script_path);
                        if (!recent.video_path.empty())
                            openFile(recent.video_path);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Save", BINDING_STRING("save"))) {
                saveScripts();
            }
            if (ImGui::MenuItem("Save as...")) {
                saveActiveScriptAs();
            }
            if (ImGui::MenuItem(ICON_SHARE" Share...")) {
                auto savePath = std::filesystem::path(settings->data().last_path) / (Util::Filename(ActiveFunscript()->current_path) + "_share.funscript");
                Util::SaveFileDialog("Share funscript", savePath.string(),
                    [&](auto& result) {
                        if (result.files.size() > 0) {
                            ActiveFunscript()->saveMinium(result.files[0]);
                        }
                    }, { "Funscript", "*.funscript" });
            }
            Util::Tooltip("Saves the bare minium.");
            ImGui::Separator();
            if (ImGui::MenuItem("Enable rolling backup", NULL, &RollingBackup)) {}
            if (ImGui::MenuItem("Open backup directory")) {
                Util::OpenFileExplorer(Util::Prefpath("backup").c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Save frame as image", BINDING_STRING("save_frame_as_image")))
            { 
                auto screenshot_dir = Util::Prefpath("screenshot");
                player.saveFrameToImage(screenshot_dir);
            }
            if (ImGui::MenuItem("Open screenshot directory")) {
                auto screenshot_dir = Util::Prefpath("screenshot");
                Util::CreateDirectories(screenshot_dir);
                Util::OpenFileExplorer(screenshot_dir.c_str());
            }

            ImGui::Separator();

            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##width", &settings->data().heatmapSettings.defaultWidth); ImGui::SameLine();
            ImGui::Text("%s", "x"); ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##height", &settings->data().heatmapSettings.defaultHeight);
            if (ImGui::MenuItem("Save heatmap")) { 
                std::string filename = ActiveFunscript()->metadata.title + "_Heatmap.png";
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
            if (ImGui::MenuItem("Undo", BINDING_STRING("undo"), false, !ActiveFunscript()->undoSystem->UndoEmpty())) {
                if (settings->data().mirror_mode) {
                    for (auto&& script : LoadedFunscripts) {
                        script->undoSystem->Undo();
                    }
                }
                else {
                    ActiveFunscript()->undoSystem->Undo();
                }
            }
            if (ImGui::MenuItem("Redo", BINDING_STRING("redo"), false, !ActiveFunscript()->undoSystem->RedoEmpty())) {
                if (settings->data().mirror_mode) {
                    for (auto&& script : LoadedFunscripts) {
                        script->undoSystem->Redo();
                    }
                }
                else {
                    ActiveFunscript()->undoSystem->Redo();
                }
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
                if (ImGui::MenuItem("Select all left")) {
                    ActiveFunscript()->SelectTime(0, player.getCurrentPositionMsInterp());
                }
                if (ImGui::MenuItem("Select all right")) {
                    ActiveFunscript()->SelectTime(player.getCurrentPositionMsInterp(), player.getDuration()*1000.f);
                }
                ImGui::Separator();
                static int32_t selectionPoint = -1;
                if (ImGui::MenuItem("Set selection start")) {
                    if (selectionPoint == -1) {
                        selectionPoint = player.getCurrentPositionMsInterp();
                    }
                    else {
                        ActiveFunscript()->SelectTime(player.getCurrentPositionMsInterp(), selectionPoint);
                        selectionPoint = -1;
                    }
                }
                if (ImGui::MenuItem("Set selection end")) {
                    if (selectionPoint == -1) {
                        selectionPoint = player.getCurrentPositionMsInterp();
                    }
                    else {
                        ActiveFunscript()->SelectTime(selectionPoint, player.getCurrentPositionMsInterp());
                        selectionPoint = -1;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Top points only", NULL, false)) {
                if (ActiveFunscript()->HasSelection()) {
                    ActiveFunscript()->undoSystem->Snapshot(StateType::TOP_POINTS_ONLY);
                    ActiveFunscript()->SelectTopActions();
                }
            }
            if (ImGui::MenuItem("Mid points only", NULL, false)) {
                if (ActiveFunscript()->HasSelection()) {
                    ActiveFunscript()->undoSystem->Snapshot(StateType::MID_POINTS_ONLY);
                    ActiveFunscript()->SelectMidActions();
                }
            }
            if (ImGui::MenuItem("Bottom points only", NULL, false)) {
                if (ActiveFunscript()->HasSelection()) {
                    ActiveFunscript()->undoSystem->Snapshot(StateType::BOTTOM_POINTS_ONLY);
                    ActiveFunscript()->SelectBottomActions();
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
            ImGui::Separator();
            if (ImGui::MenuItem("Frame align selection", NULL)) {
                ActiveFunscript()->undoSystem->Snapshot(StateType::FRAME_ALIGN);
                ActiveFunscript()->AlignWithFrameTimeSelection(player.getFrameTimeMs());
            }
            Util::Tooltip("Don't use on already aligned actions.");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Bookmarks")) {
            static std::string bookmarkName;
            auto& bookmarks = ActiveFunscript()->scriptSettings.Bookmarks;
            auto editBookmark = std::find_if(bookmarks.begin(), bookmarks.end(),
                [&](auto& mark) {
                    constexpr int threshold = 15000;
                    int32_t current = player.getCurrentPositionMsInterp();
                    return std::abs(mark.at - current) <= threshold;
                });
            if (editBookmark != bookmarks.end()) {
                ImGui::InputText("Name", &(*editBookmark).name);
                if (ImGui::MenuItem("Delete")) {
                    bookmarks.erase(editBookmark);
                }
            }
            else {
                ImGui::InputText("Name", &bookmarkName);
                if (ImGui::MenuItem("Add Bookmark")) {
                    Funscript::Bookmark bookmark;
                    bookmark.at = player.getCurrentPositionMsInterp();
                    if (bookmarkName.empty())
                    {
                        std::stringstream ss;
                        ss << ActiveFunscript()->Bookmarks().size() + 1 << '#';
                        bookmarkName = ss.str();
                    }
                    bookmark.name = bookmarkName;
                    bookmarkName = "";
                    ActiveFunscript()->AddBookmark(bookmark);
                }
            }

            if (ImGui::BeginMenu("Go to...")) {
                if (ActiveFunscript()->Bookmarks().size() == 0) {
                    ImGui::TextDisabled("No bookmarks");
                }
                else {
                    for (auto& mark : ActiveFunscript()->Bookmarks()) {
                        if (ImGui::MenuItem(mark.name.c_str())) {
                            player.setPosition(mark.at);
                        }
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::Checkbox("Always show labels", &settings->data().always_show_bookmark_labels)) {
                settings->saveSettings();
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
            ImGui::Separator();

            if (ImGui::MenuItem("Draw video", NULL, &settings->data().draw_video)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Reset video position", NULL)) { player.resetTranslationAndZoom(); }
            ImGui::Combo("Video Mode", (int32_t*)&player.settings.activeMode,
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
            if (ImGui::MenuItem("Fullscreen", BINDING_STRING("fullscreen_toggle"), &Fullscreen)) {
                SetFullscreen(Fullscreen);
            }
            if (ImGui::MenuItem("Preferences")) {
                settings->ShowWindow = true;
            }
            ImGui::EndMenu();
        }
        if (ControllerInput::AnythingConnected()) {
            if (ImGui::BeginMenu("Controller")) {
                ImGui::TextColored(ImColor(IM_COL32(0, 255, 0, 255)), "%s", "Controller connected!");
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
                ImGui::Combo("##ActiveControllers", &selectedController, padStrings.data(), padStrings.size());
                Util::Tooltip("Selecting doesn't do anything right now.");

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
        if (player.isLoaded() && ActiveFunscript()->HasUnsavedEdits()) {
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
    bool save = false;
    auto& metadata = ActiveFunscript()->metadata;
    ImGui::Begin("Metadata Editor", open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking);
    ImGui::LabelText("Title", "%s", metadata.title.c_str());
    Util::FormatTime(tmp_buf[0], sizeof(tmp_buf), metadata.duration, false);
    ImGui::LabelText("Duration", "%s", tmp_buf[0]);

    ImGui::InputText("Creator", &metadata.creator);
    ImGui::InputText("Url", &metadata.script_url);
    ImGui::InputText("Video url", &metadata.video_url);
    ImGui::InputTextMultiline("Description", &metadata.description, ImVec2(0.f, ImGui::GetFontSize()*3.f));
    ImGui::InputTextMultiline("Notes", &metadata.notes, ImVec2(0.f, ImGui::GetFontSize() * 3.f));

    {
        enum LicenseType : int32_t {
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

    ImGui::TextUnformatted("Tags");
    static std::string newTag;
    auto addTag = [&metadata](std::string& newTag) {
        Util::trim(newTag);
        if (!newTag.empty()) {
            metadata.tags.emplace_back(newTag); newTag.clear();
        }
        ImGui::ActivateItem(ImGui::GetID("##Tag"));
    };

    if (ImGui::InputText("##Tag", &newTag, ImGuiInputTextFlags_EnterReturnsTrue)) {
        addTag(newTag);
    };
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(-1.f, 0.f))) { 
        addTag(newTag);
    }
    
    auto& style = ImGui::GetStyle();

    renderTagButtons(metadata.tags);
    ImGui::NewLine();

    ImGui::TextUnformatted("Performers");
    static std::string newPerformer;
    auto addPerformer = [&metadata](std::string& newPerformer) {
        Util::trim(newPerformer);
        if (!newPerformer.empty()) {
            metadata.performers.emplace_back(newPerformer); newPerformer.clear(); 
        }
        ImGui::ActivateItem(ImGui::GetID("##Performer"));
    };
    if (ImGui::InputText("##Performer", &newPerformer, ImGuiInputTextFlags_EnterReturnsTrue)) {
        addPerformer(newPerformer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##Performer", ImVec2(-1.f, 0.f))) {
        addPerformer(newPerformer);
    }


    renderTagButtons(metadata.performers);
    ImGui::NewLine();
    
    if (ImGui::Button("Save", ImVec2(-1.f, 0.f))) { save = true; }
    Util::ForceMinumumWindowSize(ImGui::GetCurrentWindow());
    ImGui::End();
    return save;
}

void OpenFunscripter::SetFullscreen(bool fullscreen) {
    static SDL_Rect restoreRect{ 0,0, 1280,720 };
    if (fullscreen) {
        //SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowPosition(window, &restoreRect.x, &restoreRect.y);
        SDL_GetWindowSize(window, &restoreRect.w, &restoreRect.h);

        SDL_SetWindowResizable(window, SDL_FALSE);
        SDL_SetWindowBordered(window, SDL_FALSE);
        SDL_SetWindowPosition(window, 0, 0);
        int display = SDL_GetWindowDisplayIndex(window);
        SDL_Rect bounds;
        SDL_GetDisplayBounds(display, &bounds);
        SDL_SetWindowSize(window,  bounds.w, bounds.h);
    }
    else {
        //SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowResizable(window, SDL_TRUE);
        SDL_SetWindowBordered(window, SDL_TRUE);
        SDL_SetWindowPosition(window, restoreRect.x, restoreRect.y);
        SDL_SetWindowSize(window, restoreRect.w, restoreRect.h);
    }
}

void OpenFunscripter::CreateDockspace() noexcept
{
    const bool opt_fullscreen_persistant = true;
    const bool opt_fullscreen = opt_fullscreen_persistant;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;

    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = /*ImGuiWindowFlags_MenuBar |*/ ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("MainDockSpace", 0, window_flags);
    ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGui::DockSpace(MainDockspaceID, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    ShowMainMenuBar();

    ImGui::End();
}

void OpenFunscripter::ShowAboutWindow(bool* open) noexcept
{
    if (!*open) return;
    ImGui::Begin("About", open, ImGuiWindowFlags_None 
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoCollapse
    );
    ImGui::Text("%s", "OpenFunscripter " FUN_LATEST_GIT_TAG);
    ImGui::Text("Commit: %s", FUN_LATEST_GIT_HASH);
    if (ImGui::Button("Latest release " ICON_GITHUB, ImVec2(-1.f, 0.f))) {
        Util::OpenUrl("https://github.com/gagax1234/OpenFunscripter/releases/latest");
    }
    ImGui::End();
}

void OpenFunscripter::ShowStatisticsWindow(bool* open) noexcept
{
    if (!*open) return;
    ImGui::Begin(StatisticsId, open, ImGuiWindowFlags_None);
    const int32_t currentMs = std::round(player.getCurrentPositionMs());
    const FunscriptAction* front = ActiveFunscript()->GetActionAtTime(currentMs, 0);
    const FunscriptAction* behind = nullptr;
    if (front != nullptr) {
        behind = ActiveFunscript()->GetPreviousActionBehind(front->at);
    }
    else {
        behind = ActiveFunscript()->GetPreviousActionBehind(currentMs);
        front = ActiveFunscript()->GetNextActionAhead(currentMs);
    }

    if (behind != nullptr) {
        ImGui::Text("Interval: %d ms", currentMs - behind->at);
        if (front != nullptr) {
            int32_t duration = front->at - behind->at;
            int32_t length = front->pos - behind->pos;
            ImGui::Text("Speed: %.02lf units/s", std::abs(length) / (duration/1000.0));
            ImGui::Text("Duration: %d ms", duration);
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

bool OpenFunscripter::DrawTimelineWidget(const char* label, float* position) noexcept
{
    bool change = false;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    auto draw_list = window->DrawList;

    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ImGui::GetFontSize() * 1.5f;

    const ImRect frame_bb(window->DC.CursorPos + style.FramePadding, window->DC.CursorPos + ImVec2(w, h) - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max);

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb))
        return false;

    if (updateTimelineGradient) {
        updateTimelineGradient = false;
        UpdateTimelineGradient(TimelineGradient);
    }

    bool item_hovered = ImGui::IsItemHovered();



    const float current_pos_x = frame_bb.Min.x + frame_bb.GetWidth() * (*position);
    const float offset_progress_h = h / 5.f;
    const float offset_progress_w = current_pos_x - frame_bb.Min.x;
    draw_list->AddRectFilled(frame_bb.Min + ImVec2(-1.f, offset_progress_h), frame_bb.Min + ImVec2(offset_progress_w, frame_bb.GetHeight()) + ImVec2(0.f, offset_progress_h), ImColor(style.Colors[ImGuiCol_PlotLinesHovered]));
    draw_list->AddRectFilled(frame_bb.Min + ImVec2(offset_progress_w, offset_progress_h), frame_bb.Max + ImVec2(1.f, offset_progress_h), IM_COL32(150, 150, 150, 255));

    // bookmarks
    for (auto& bookmark : ActiveFunscript()->scriptSettings.Bookmarks) {
        const float rectWidth = 7.f;

        ImVec2 p1((frame_bb.Min.x + (frame_bb.GetWidth() * (bookmark.at / (player.getDuration() * 1000.0)))) - (rectWidth/2.f), frame_bb.Min.y);       
        ImVec2 p2(p1.x + rectWidth, frame_bb.Min.y + frame_bb.GetHeight() + (style.ItemSpacing.y * 3.0f));

        //ImRect rect(p1, p2);
        //ImGui::ItemSize(rect);
        //auto bookmarkId = ImGui::GetID(bookmark.name.c_str());
        //ImGui::ItemAdd(rect, bookmarkId);

        draw_list->AddRectFilled(p1, p2, ImColor(style.Colors[ImGuiCol_Text]), 8.f);

        if (item_hovered || settings->data().always_show_bookmark_labels) {
            auto size = ImGui::CalcTextSize(bookmark.name.c_str());
            size.x /= 2.f;
            size.y /= 8.f;
            draw_list->AddText(p2 - size, ImColor(style.Colors[ImGuiCol_Text]), bookmark.name.c_str());
        }
    }

    // position highlight
    ImVec2 p1(current_pos_x, frame_bb.Min.y);
    ImVec2 p2(current_pos_x, frame_bb.Max.y);
    constexpr float timeline_pos_cursor_w = 5.f;
    draw_list->AddLine(p1+ImVec2(0.f, h/3.f), p2+ImVec2(0.f, h/3.f), IM_COL32(255, 0, 0, 255), timeline_pos_cursor_w/2.f);
    

    ImGradient::DrawGradientBar(&TimelineGradient, frame_bb.Min, frame_bb.GetWidth(), frame_bb.GetHeight());



    const ImColor timeline_cursor_back = IM_COL32(255, 255, 255, 255);
    const ImColor timeline_cursor_front = IM_COL32(0, 0, 0, 255);
    static bool dragging = false;
    auto mouse = ImGui::GetMousePos();
    float rel_timeline_pos = ((mouse.x - frame_bb.Min.x) / frame_bb.GetWidth());

    if (item_hovered) {
        draw_list->AddLine(ImVec2(mouse.x, frame_bb.Min.y), ImVec2(mouse.x, frame_bb.Max.y), timeline_cursor_back, timeline_pos_cursor_w);
        draw_list->AddLine(ImVec2(mouse.x, frame_bb.Min.y), ImVec2(mouse.x, frame_bb.Max.y), timeline_cursor_front, timeline_pos_cursor_w / 2.f);

        ImGui::BeginTooltip();
        {
            double time_seconds = player.getDuration() * rel_timeline_pos;
            double time_delta = time_seconds - player.getCurrentPositionSecondsInterp();
            Util::FormatTime(tmp_buf[0], sizeof(tmp_buf[0]), time_seconds, false);
            Util::FormatTime(tmp_buf[1], sizeof(tmp_buf[1]), (time_delta > 0) ? time_delta : -time_delta, false);
            if (time_delta > 0)
                ImGui::Text("%s (+%s)", tmp_buf[0], tmp_buf[1]);
            else                                            
                ImGui::Text("%s (-%s)", tmp_buf[0], tmp_buf[1]);
        }
        ImGui::EndTooltip();

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            *position = rel_timeline_pos;
            change = true;
            dragging = true;
        }
    }

    if (dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        *position = rel_timeline_pos;
        change = true;
    }
    else {
        dragging = false;
    }

    draw_list->AddLine(p1, p2, timeline_cursor_back, timeline_pos_cursor_w);
    draw_list->AddLine(p1, p2, timeline_cursor_front, timeline_pos_cursor_w / 2.f);

    
    const float min_val = 0.f;
    const float max_val = 1.f;
    if (change) { *position = Util::Clamp(*position, min_val, max_val); }


    return change;
}

void OpenFunscripter::UpdateTimelineGradient(ImGradient& grad)
{
    grad.clear();
    grad.addMark(0.f, IM_COL32(0, 0, 0, 255));
    grad.addMark(1.f, IM_COL32(0, 0, 0, 255));

    if (ActiveFunscript()->Actions().size() == 0) {
        return;
    }

    std::array<ImColor, 6> heatColor{
        IM_COL32(0x00, 0x00, 0x00, 0xFF),
        IM_COL32(0x1E, 0x90, 0xFF, 0xFF),
        IM_COL32(0x00, 0xFF, 0xFF, 0xFF),
        IM_COL32(0x00, 0xFF, 0x00, 0xFF),
        IM_COL32(0xFF, 0xFF, 0x00, 0xFF),
        IM_COL32(0xFF, 0x00, 0x00, 0xFF),
    };
    ImGradient HeatMap;
    float pos = 0.0f;
    for (auto& col : heatColor) {
        HeatMap.addMark(pos, col);
        pos += (1.f / (heatColor.size() - 1));
    }
    HeatMap.refreshCache();

    auto getSegments = [](const std::vector<FunscriptAction>& actions, int32_t gapDurationMs) -> std::vector<std::vector<FunscriptAction>> {
        std::vector<std::vector<FunscriptAction>> segments;
        {
            FunscriptAction previous(0, -1);

            for (auto& action : actions)
            {
                if (previous.pos == action.pos) { 
                    continue;
                }

                if (action.at - previous.at >= gapDurationMs) {
                    segments.emplace_back();
                }
                if (segments.size() == 0) { segments.emplace_back(); }
                segments.back().emplace_back(action);

                previous = action;
            }

            return segments;
        }
    };

    // this comes fairly close to what ScriptPlayer's heatmap looks like
    const float totalDurationMs = player.getDuration() * 1000.0;
    const float kernel_size_ms = 5000.f;
    const float max_actions_in_kernel = 24.5f;

    ImColor color(0.f, 0.f, 0.f, 1.f);

    auto segments = getSegments(ActiveFunscript()->Actions(), 10000);
    for (auto& segment : segments) {
        const float durationMs = segment.back().at - segment.front().at;
        float kernel_offset = segment.front().at;
        grad.addMark(kernel_offset / totalDurationMs, IM_COL32(0, 0, 0, 255));
        do {
            int actions_in_kernel = 0;
            float kernel_start = kernel_offset;
            float kernel_end = kernel_offset + kernel_size_ms;

            if (kernel_offset < segment.back().at)
            {
                for (int i = 0; i < segment.size(); i++) {
                    auto& action = segment[i];
                    if (action.at >= kernel_start && action.at <= kernel_end)
                        actions_in_kernel++;
                    else if (action.at > kernel_end)
                        break;
                }
            }
            kernel_offset += kernel_size_ms;


            float actionsRelToMax = Util::Clamp((float)actions_in_kernel / max_actions_in_kernel, 0.0f, 1.0f);
            HeatMap.getColorAt(actionsRelToMax, (float*)&color.Value);
            float markPos = kernel_offset  / totalDurationMs;
            grad.addMark(markPos, color);
        } while (kernel_offset < (segment.front().at + durationMs));
        grad.addMark((kernel_offset + 1.f) / totalDurationMs, IM_COL32(0, 0, 0, 255));
    }
    grad.refreshCache();
}


