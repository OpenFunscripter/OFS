#include "OpenFunscripter.h"

#include "OpenFunscripterUtil.h"

#include "GradientBar.h"

#include <filesystem>

#include "portable-file-dialogs.h"
#include "stb_sprintf.h"

#include "imgui_stdlib.h"
#include "imgui_internal.h"

// FIX: Add type checking to the deserialization. 
//      I assume it would crash if a field is specified but doesn't have the correct type.

// TODO: use a ringbuffer in the undosystem
// TODO: full controller support
// TODO: make heatmap generation more sophisticated

// the video player supports a lot more than these
// these are the ones looked for when loading funscripts
// also used to generate a filter for the file dialog
constexpr std::array<const char*, 6> SupportedVideoExtensions {
    ".mp4",
    ".mkv",
    ".webm",
    ".wmv",
    ".avi",
    ".m4v",
};

OpenFunscripter* OpenFunscripter::ptr = nullptr;
ImFont* OpenFunscripter::DefaultFont2 = nullptr;

const char* glsl_version = "#version 150";

static SDL_Cursor* SystemCursors[SDL_NUM_SYSTEM_CURSORS];

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
    const char* roboto = "data/fonts/RobotoMono-Regular.ttf";
    const char* fontawesome = "data/fonts/fontawesome-webfont.ttf";

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

    std::error_code ec;
    if (!std::filesystem::exists(roboto, ec) || !std::filesystem::is_regular_file(roboto, ec)) { 
        LOGF_WARN("\"%s\" font is missing.", roboto);
    }
    else {
        font = io.Fonts->AddFontFromFileTTF(roboto, settings->data().default_font_size, &config);
        if (font == nullptr) return false;
        io.FontDefault = font;
    }

    if (!std::filesystem::exists(fontawesome, ec) || !std::filesystem::is_regular_file(fontawesome, ec)) {
        LOGF_WARN("\"%s\" font is missing. No icons.", fontawesome);
    }
    else {
        config.MergeMode = true;
        font = io.Fonts->AddFontFromFileTTF(fontawesome, settings->data().default_font_size, &config, icons_ranges);
        if (font == nullptr) return false;
    }

    config.MergeMode = true;
    font = io.Fonts->AddFontFromFileTTF("data/fonts/NotoSansJP-Regular.otf", settings->data().default_font_size, &config, io.Fonts->GetGlyphRangesJapanese());
    if (font == nullptr) {
        LOG_WARN("Missing japanese glyphs!!!");
    }

    if (!std::filesystem::exists(roboto, ec) || !std::filesystem::is_regular_file(roboto, ec)) {
        LOGF_WARN("\"%s\" font is missing.", roboto);
    } 
    else
    {
        config.MergeMode = false;
        DefaultFont2 = io.Fonts->AddFontFromFileTTF(roboto, settings->data().default_font_size * 2.0f, &config);
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

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
    rawInput.reset();
    events.reset();

    settings->saveSettings();
}

bool OpenFunscripter::setup()
{
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    ptr = this;
    settings = std::make_unique<OpenFunscripterSettings>("data/keybinds.json", "data/config.json");
    
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

    SDL_Surface* surface;
    int image_width = 0;
    int image_height = 0;
    int channels;
    unsigned char* image_data = stbi_load("data/logo64.png", &image_width, &image_height, &channels, 3);
    if (image_data != nullptr) {
        std::array<uint16_t, 64 * 64> pixels;
        for (int i = 0; i < (image_width * image_height * 3); i += 3) {
            pixels[i / 3] = 0;
            pixels[i / 3] |= 0xf << 12;
            pixels[i / 3] |= (image_data[i] / 16) << 8;
            pixels[i / 3] |= (image_data[i + 1] / 16) << 4;
            pixels[i / 3] |= (image_data[i + 2] / 16) << 0;
        }
        surface = SDL_CreateRGBSurfaceFrom(pixels.data(), 64, 64, 64, 16 * 2, 0x0f00, 0x00f0, 0x000f, 0xf000);
        // The icon is attached to the window pointer
        SDL_SetWindowIcon(window, surface);

        // ...and the surface containing the icon pixel data is no longer required.
        SDL_FreeSurface(surface);
        stbi_image_free(image_data);
    }

    // register custom events with sdl
    events = std::make_unique<EventSystem>();
    events->setup();

    keybinds.setup();
    register_bindings(); // needs to happen before setBindings
    keybinds.setBindings(settings->getKeybindings()); // override with user bindings

    scriptPositions.setup();
    LoadedFunscript = std::make_unique<Funscript>();

    scripting = std::make_unique<ScriptingMode>();
    scripting->setup();
    if (!player.setup()) {
        LOG_ERROR("Failed to init video player");
        return false;
    }

    events->Subscribe(EventSystem::FunscriptActionsChangedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptChanged));
    events->Subscribe(EventSystem::FunscriptActionClickedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptActionClicked));
    events->Subscribe(EventSystem::FileDialogOpenEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FileDialogOpenEvent));
    events->Subscribe(EventSystem::FileDialogSaveEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FileDialogSaveEvent));
    events->Subscribe(SDL_DROPFILE, EVENT_SYSTEM_BIND(this, &OpenFunscripter::DragNDrop));
    events->Subscribe(EventSystem::MpvVideoLoaded, EVENT_SYSTEM_BIND(this, &OpenFunscripter::MpvVideoLoaded));
    // cache these here because openFile overrides them
    std::string last_video = settings->data().most_recent_file.video_path;
    std::string last_script = settings->data().most_recent_file.script_path;
    if (!last_script.empty())
        openFile(last_script);
    if (!last_video.empty())
        openFile(last_video);


    rawInput = std::make_unique<ControllerInput>();
    rawInput->setup();
    simulator.setup();
    
    // init cursors
    SDL_FreeCursor(SDL_GetCursor());
    for (int i = 0; i < SDL_NUM_SYSTEM_CURSORS; i++) {
        SystemCursors[i] = SDL_CreateSystemCursor((SDL_SystemCursor)i);
    }
    SetCursorType(SDL_SYSTEM_CURSOR_ARROW);

    SDL_ShowWindow(window);
    return true;
}

void OpenFunscripter::setupDefaultLayout(bool force) noexcept
{
    MainDockspaceID = ImGui::GetID("MainAppDockspace");
    auto imgui_ini = Util::Basepath() / "imgui.ini";
    bool imgui_ini_found = Util::FileExists(imgui_ini.string().c_str());
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

void OpenFunscripter::register_bindings()
{
    {
        KeybindingGroup group;
        group.name = "Actions";
        // DELETE ACTION
        group.bindings.push_back(Keybinding(
            "remove_action",
            "Remove action",
            SDLK_DELETE,
            0,
            true,
            [&](void*) { removeAction(); }
        ));

        //ADD ACTIONS
        group.bindings.push_back(Keybinding(
            "action 0",
            "Action at 0",
            SDLK_KP_0,
            0,
            true,
            [&](void*) { addEditAction(0); }
        ));
        std::stringstream ss;
        for (int i = 1; i < 10; i++) {
            ss.str("");
            ss << "action " << i * 10;
            std::string id = ss.str();
            ss.str("");
            ss << "Action at " << i * 10;

            group.bindings.push_back(Keybinding(
                id,
                ss.str(),
                SDLK_KP_1 + i - 1,
                0,
                true,
                [&, i](void*) { addEditAction(i * 10); }
            ));
        }
        group.bindings.push_back(Keybinding(
            "action 100",
            "Action at 100",
            SDLK_KP_DIVIDE,
            0,
            true,
            [&](void*) { addEditAction(100); }
        ));

        keybinds.registerBinding(group);
    }

    {
        KeybindingGroup group;
        group.name = "Core";

        // SAVE
        group.bindings.push_back(Keybinding(
            "save",
            "Save",
            SDLK_s,
            KMOD_CTRL,

            true,
            [&](void*) { saveScript(); }
        ));


        group.bindings.push_back(Keybinding(
            "sync_timestamps",
            "Sync time with player",
            SDLK_s,
            0,
            true,
            [&](void*) { player.syncWithRealTime(); }
        ));


        keybinds.registerBinding(group);
    }
    {
        KeybindingGroup group;
        group.name = "Navigation";
        // JUMP BETWEEN ACTIONS
        group.bindings.push_back(Keybinding(
            "prev_action",
            "Previous action",
            SDLK_DOWN,
            0,
            false,
            [&](void*) {
                auto action = LoadedFunscript->GetPreviousActionBehind(player.getCurrentPositionMs() - 1.f);
                if (action != nullptr) player.setPosition(action->at);
            }
        ));
        group.bindings.push_back(Keybinding(
            "next_action",
            "Next action",
            SDLK_UP,
            0,
            false,
            [&](void*) {
                auto action = LoadedFunscript->GetNextActionAhead(player.getCurrentPositionMs() + 1.f);
                if (action != nullptr) player.setPosition(action->at);
            }
        ));

        // FRAME CONTROL
        group.bindings.push_back(Keybinding(
            "prev_frame",
            "Previous frame",
            SDLK_LEFT,
            0,
            false,
            [&](void*) { player.previousFrame(); }
        ));

        group.bindings.push_back(Keybinding(
            "next_frame",
            "Next frame",
            SDLK_RIGHT,
            0,
            false,
            [&](void*) { player.nextFrame(); }
        ));

        group.bindings.push_back(Keybinding(
            "fast_step",
            "Fast step",
            SDLK_RIGHT,
            KMOD_CTRL,
            false,
            [&](void*) {
                int32_t frameStep = settings->data().fast_step_amount;
                player.relativeFrameSeek(frameStep);
            }
        ));
        group.bindings.push_back(Keybinding(
            "fast_backstep",
            "Fast backstep",
            SDLK_LEFT,
            KMOD_CTRL,
            false,
            [&](void*) {
                int32_t frameStep = settings->data().fast_step_amount;
                player.relativeFrameSeek(-frameStep);
            }
        ));
        keybinds.registerBinding(group);
    }
    
    {
        KeybindingGroup group;
        group.name = "Utility";
        // UNDO / REDO
        group.bindings.push_back(Keybinding(
            "undo",
            "Undo",
            SDLK_z,
            KMOD_CTRL,
            false,
            [&](void*) { undoRedoSystem.Undo(); }
        ));
        group.bindings.push_back(Keybinding(
            "redo",
            "Redo",
            SDLK_y,
            KMOD_CTRL,
            false,
            [&](void*) { undoRedoSystem.Redo(); }
        ));

        // COPY / PASTE
        group.bindings.push_back(Keybinding(
            "copy",
            "Copy",
            SDLK_c,
            KMOD_CTRL,
            true,
            [&](void*) { copySelection(); }
        ));
        group.bindings.push_back(Keybinding(
            "paste",
            "Paste",
            SDLK_v,
            KMOD_CTRL,
            true,
            [&](void*) { pasteSelection(); }
        ));
        group.bindings.push_back(Keybinding(
            "cut",
            "Cut",
            SDLK_x,
            KMOD_CTRL,
            true,
            [&](void*) { cutSelection(); }
        ));
        group.bindings.push_back(Keybinding(
            "select_all",
            "Select all",
            SDLK_a,
            KMOD_CTRL,
            true,
            [&](void*) { LoadedFunscript->SelectAll(); }
        ));
        group.bindings.push_back(Keybinding(
            "deselect_all",
            "Deselect all",
            SDLK_d,
            KMOD_CTRL,
            true,
            [&](void*) { LoadedFunscript->ClearSelection(); }
        ));

        // SCREENSHOT VIDEO
        group.bindings.push_back(Keybinding(
            "save_frame_as_image",
            "Save frame as image",
            SDLK_F2,
            0,
            true,
            [&](void*) { 
                auto screenshot_dir = Util::Basepath() / "screenshot";
                player.saveFrameToImage(screenshot_dir.string()); 
            }
        ));

        // CHANGE SUBTITLES
        group.bindings.push_back(Keybinding(
            "cycle_subtitles",
            "Cycle subtitles",
            SDLK_j,
            0,
            true,
            [&](void*) { player.cycleSubtitles(); }
        ));

        // FULLSCREEN
        group.bindings.push_back(Keybinding(
            "fullscreen_toggle",
            "Toggle fullscreen",
            SDLK_F10,
            0,
            true,
            [&](void*) { Fullscreen = !Fullscreen; SetFullscreen(Fullscreen); }
        ));
        keybinds.registerBinding(group);
    }

    // MOVE LEFT/RIGHT
    auto move_actions_horizontal = [](int32_t time_ms) {
        auto ptr = OpenFunscripter::ptr;
        if (ptr->LoadedFunscript->HasSelection()) {
            ptr->undoRedoSystem.Snapshot("Actions moved");
            ptr->LoadedFunscript->MoveSelectionTime(time_ms);
        }
        else {
            auto closest = ptr->LoadedFunscript->GetClosestAction(ptr->player.getCurrentPositionMs());
            if (closest != nullptr) {
                ptr->undoRedoSystem.Snapshot("Actions moved");
                FunscriptAction moved(closest->at + time_ms, closest->pos);
                ptr->LoadedFunscript->EditAction(*closest, moved);
            }
        }
    };
    auto move_actions_horizontal_with_video = [](int32_t time_ms) {
        auto ptr = OpenFunscripter::ptr;
        if (ptr->LoadedFunscript->HasSelection()) {
            ptr->undoRedoSystem.Snapshot("Actions moved");
            ptr->LoadedFunscript->MoveSelectionTime(time_ms);
            auto closest = ptr->LoadedFunscript->GetClosestActionSelection(ptr->player.getCurrentPositionMs());
            if (closest != nullptr) { ptr->player.setPosition(closest->at); }
            else { ptr->player.setPosition(ptr->LoadedFunscript->Selection().front().at); }
        }
        else {
            auto closest = ptr->LoadedFunscript->GetClosestAction(ptr->player.getCurrentPositionMs());
            if (closest != nullptr) {
                ptr->undoRedoSystem.Snapshot("Actions moved");
                FunscriptAction moved(closest->at + time_ms, closest->pos);
                ptr->LoadedFunscript->EditAction(*closest, moved);
                ptr->player.setPosition(moved.at);
            }
        }
    };
    {
        KeybindingGroup group;
        group.name = "Moving";
        group.bindings.push_back(Keybinding(
            "move_actions_left_snapped",
            "Move actions left with snapping",
            SDLK_LEFT,
            KMOD_CTRL | KMOD_SHIFT,
            false,
            [&](void*) {
                move_actions_horizontal_with_video(-player.getFrameTimeMs());
            }
        ));
        group.bindings.push_back(Keybinding(
            "move_actions_right_snapped",
            "Move actions right with snapping",
            SDLK_RIGHT,
            KMOD_CTRL | KMOD_SHIFT,
            false,
            [&](void*) {
                move_actions_horizontal_with_video(player.getFrameTimeMs());
            }
        ));

        group.bindings.push_back(Keybinding(
            "move_actions_left",
            "Move actions left",
            SDLK_LEFT,
            KMOD_SHIFT,
            false,
            [&](void*) {
                move_actions_horizontal(-player.getFrameTimeMs());
            }
        ));

        group.bindings.push_back(Keybinding(
            "move_actions_right",
            "Move actions right",
            SDLK_RIGHT,
            KMOD_SHIFT,
            false,
            [&](void*) {
                move_actions_horizontal(player.getFrameTimeMs());
            }
        ));

        // MOVE SELECTION UP/DOWN
        group.bindings.push_back(Keybinding(
            "move_actions_up",
            "Move actions up",
            SDLK_UP,
            KMOD_SHIFT,
            false,
            [&](void*) {
                if (LoadedFunscript->HasSelection()) {
                    undoRedoSystem.Snapshot("Actions moved");
                    LoadedFunscript->MoveSelectionPosition(1);
                }
                else {
                    auto closest = LoadedFunscript->GetClosestAction(player.getCurrentPositionMs());
                    if (closest != nullptr) {
                        FunscriptAction moved(closest->at, closest->pos + 1);
                        if (moved.pos <= 100 && moved.pos >= 0) {
                            undoRedoSystem.Snapshot("Actions moved");
                            LoadedFunscript->EditAction(*closest, moved);
                        }
                    }
                }
            }
        ));
        group.bindings.push_back(Keybinding(
            "move_actions_down",
            "Move actions down",
            SDLK_DOWN,
            KMOD_SHIFT,
            false,
            [&](void*) {
                undoRedoSystem.Snapshot("Actions moved");
                if (LoadedFunscript->HasSelection()) {
                    LoadedFunscript->MoveSelectionPosition(-1);
                }
                else {
                    auto closest = LoadedFunscript->GetClosestAction(player.getCurrentPositionMs());
                    if (closest != nullptr) {
                        FunscriptAction moved(closest->at, closest->pos - 1);
                        if (moved.pos <= 100 && moved.pos >= 0) {
                            undoRedoSystem.Snapshot("Actions moved");
                            LoadedFunscript->EditAction(*closest, moved);
                        }
                    }
                }
            }
        ));
        keybinds.registerBinding(group);
    }
    // FUNCTIONS
    {
        KeybindingGroup group;
        group.name = "Special";
        group.bindings.push_back(Keybinding(
            "equalize_actions",
            "Equalize actions",
            SDLK_e,
            0,
            true,
            [&](void*) { equalizeSelection(); }
        ));
        group.bindings.push_back(Keybinding(
            "invert_actions",
            "Invert actions",
            SDLK_i,
            0,
            true,
            [&](void*) { invertSelection(); }
        ));
        group.bindings.push_back(Keybinding(
            "isolate_action",
            "Isolate action",
            SDLK_r,
            0,
            true,
            [&](void*) { isolateAction(); }
        ));
        keybinds.registerBinding(group);
    }

    // VIDEO CONTROL
    {
        KeybindingGroup group;
        group.name = "Video player";
        // PLAY / PAUSE
        group.bindings.push_back(Keybinding(
            "toggle_play",
            "Play / Pause",
            SDLK_SPACE,
            0,
            true,
            [&](void*) { player.togglePlay(); }
        ));

        // PLAYBACK SPEED
        group.bindings.push_back(Keybinding(
            "decrement_speed",
            "Playbackspeed -10%",
            SDLK_KP_MINUS,
            0,
            true,
            [&](void*) { player.addSpeed(-0.10); }
        ));
        group.bindings.push_back(Keybinding(
            "increment_speed",
            "Playbackspeed +10%",
            SDLK_KP_PLUS,
            0,
            true,
            [&](void*) { player.addSpeed(0.10); }
        ));
        keybinds.registerBinding(group);
    }

}


void OpenFunscripter::new_frame() noexcept
{
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
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
        LoadedFunscript->SelectAction(action);
    }
    else {
        player.setPosition(action.at);
    }
}

void OpenFunscripter::FileDialogOpenEvent(SDL_Event& ev) noexcept
{
    auto& result = *static_cast<std::vector<std::string>*>(ev.user.data1);
    if (result.size() > 0) {
        auto file = result[0];
        if (Util::FileExists(file))
        {
            openFile(file);
        }
    }
}

void OpenFunscripter::FileDialogSaveEvent(SDL_Event& ev) noexcept {
    auto& result = *static_cast<std::string*>(ev.user.data1);
    if (!result.empty())
    {
        saveScript(result.c_str());
        std::filesystem::path dir(result);
        dir.remove_filename();
        settings->data().last_path = dir.string();
    }
}

void OpenFunscripter::DragNDrop(SDL_Event& ev) noexcept
{
    openFile(ev.drop.file);
    SDL_free(ev.drop.file);
}

void OpenFunscripter::MpvVideoLoaded(SDL_Event& ev) noexcept
{
    LoadedFunscript->metadata.duration = player.getDuration();
    LoadedFunscript->reserveActionMemory(player.getTotalNumFrames());
    player.setPosition(LoadedFunscript->scriptSettings.last_pos_ms);

    auto name = Util::Filename(player.getVideoPath());
    auto recentFile = OpenFunscripterSettings::RecentFile{ name, std::string(player.getVideoPath()), LoadedFunscript->current_path };
    settings->addRecentFile(recentFile);
    scriptPositions.ClearAudioWaveform();
}

void OpenFunscripter::update() noexcept {
    OpenFunscripter::SetCursorType(SDL_SYSTEM_CURSOR_ARROW);
    LoadedFunscript->update();
    rawInput->update();
    scripting->update();

    if (RollingBackup) {
        rollingBackup();
    }
}

void OpenFunscripter::rollingBackup() noexcept
{
    if (LoadedFunscript->current_path.empty()) { return; }
    std::chrono::duration<float> timeSinceBackup = std::chrono::system_clock::now() - last_backup;
    if (timeSinceBackup.count() < 61.f) {
        return;
    }
    last_backup = std::chrono::system_clock::now();

    auto backupDir = Util::Basepath();
    backupDir /= "backup";
    auto name = Util::Filename(player.getVideoPath());
    backupDir /= name;
    std::error_code ec;
    std::filesystem::create_directories(backupDir, ec);
    char path_buf[1024];

    stbsp_snprintf(path_buf, sizeof(path_buf), "Backup_%d.funscript", SDL_GetTicks());
    auto savePath = (backupDir / path_buf);
    LOGF_INFO("Backup at \"%s\"", savePath.string().c_str());
    saveScript(savePath.string().c_str() , false);

    auto count_files = [](const std::filesystem::path& path) -> std::size_t
    {
        return (std::size_t)std::distance(std::filesystem::directory_iterator{ path }, std::filesystem::directory_iterator{});
    };
    if (count_files(backupDir) > 5) {
        auto iterator = std::filesystem::directory_iterator{ backupDir };
        auto oldest_backup = std::min_element(std::filesystem::begin(iterator), std::filesystem::end(iterator),
            [](auto& file1, auto& file2) {
                std::error_code ec;
                std::filesystem::file_time_type last_write1 = std::filesystem::last_write_time(file1, ec);
                std::filesystem::file_time_type last_write2 = std::filesystem::last_write_time(file2, ec);
                return last_write1.time_since_epoch() > last_write2.time_since_epoch();
        });

        std::error_code ec;
        if ((*oldest_backup).path().extension() == ".funscript") {
            LOGF_INFO("Removing old backup: \"%s\"", (*oldest_backup).path().string().c_str());
            std::filesystem::remove(*oldest_backup, ec);
            if (ec) {
                LOGF_INFO("Failed to remove old backup\n%s", ec.message().c_str());
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
            undoRedoSystem.ShowUndoRedoHistory(&ShowHistory);
            simulator.ShowSimulator(&settings->data().show_simulator);
            ShowStatisticsWindow(&ShowStatistics);
            if (ShowMetadataEditorWindow(&ShowMetadataEditor)) { saveScript(); }

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
                    if (ImGui::Button("<", ImVec2(-1, 0))) {
                        player.previousFrame();
                    }
                    ImGui::NextColumn();
                    if (ImGui::Button("<<", ImVec2(-1, 0))) {
                        int seek_to = player.getCurrentPositionMs() - seek_ms;
                        if (seek_to < 0)
                            seek_to = 0;
                        player.setPosition(seek_to);
                    }
                    ImGui::NextColumn();

                    if (ImGui::Button((player.isPaused()) ? ICON_PLAY : ICON_PAUSE, ImVec2(-1, 0))) {
                        player.togglePlay();
                    }
                    ImGui::NextColumn();

                    if (ImGui::Button(">>", ImVec2(-1, 0))) {
                        int seek_to = player.getCurrentPositionMs() + seek_ms;
                        if (seek_to > player.getDuration() * 1000.0)
                            seek_to = player.getDuration() * 1000.0;
                        player.setPosition(seek_to);
                    }
                    ImGui::NextColumn();

                    if (ImGui::Button(">", ImVec2(-1, 0))) {
                        player.nextFrame();
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
                        if (player.settings.playback_speed != player.getSpeed()) {
                            player.setSpeed(player.settings.playback_speed);
                        }
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
                {
                    ImGui::Begin(ActionEditorId);
                    if (player.isPaused()) {
                        auto scriptAction = LoadedFunscript->GetActionAtTime(player.getCurrentPositionMs(), player.getFrameTimeMs());

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

void OpenFunscripter::SetCursorType(SDL_SystemCursor id) noexcept
{
    SDL_SetCursor(SystemCursors[id]);
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
        for (auto extension : SupportedVideoExtensions) {
            videoPath = base_path.string() + extension;
            if (Util::FileExists(videoPath)) {
                video_path = videoPath;
                break;
            }
        }
    }
    else {
        video_path = file;
        if (!Util::FileNamesMatch(video_path, LoadedFunscript->current_path)) {
            funscript_path = base_path.string() + ".funscript";
        }
        else {
            funscript_path = LoadedFunscript->current_path;
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
        LoadedFunscript = std::make_unique<Funscript>();
        undoRedoSystem.ClearHistory();
        if (!Util::FileExists(file)) {
            return false;
        }
        return LoadedFunscript->open(file);
    };

    // try load funscript
    bool result = openFunscript(funscript_path);
    if (!result) {
        LOGF_WARN("Couldn't find funscript. \"%s\"", funscript_path.c_str());
    }
    LoadedFunscript->current_path = funscript_path;

    updateTitle();

    auto last_path = std::filesystem::path(file);
    last_path.replace_filename("");
    last_path /= "";
    settings->data().last_path = last_path.string();
    settings->data().most_recent_file.video_path = video_path;
    settings->data().most_recent_file.script_path = funscript_path;
    settings->saveSettings();

    last_save_time = std::chrono::system_clock::now();
    last_backup = std::chrono::system_clock::now();

    return result;
}

void OpenFunscripter::updateTitle()
{
    std::stringstream ss;
    ss.str(std::string());
    
    ss << "OpenFunscripter " FUN_LATEST_GIT_TAG "@" FUN_LATEST_GIT_HASH " - \"" << LoadedFunscript->current_path << "\"";
    SDL_SetWindowTitle(window, ss.str().c_str());
}

void OpenFunscripter::saveScript(const char* path, bool override_location)
{
    LoadedFunscript->metadata.title = std::filesystem::path(LoadedFunscript->current_path)
        .replace_extension("")
        .filename()
        .string();
    LoadedFunscript->metadata.duration = player.getDuration();
    LoadedFunscript->scriptSettings.last_pos_ms = player.getCurrentPositionMs();
    if (path == nullptr) {
        LoadedFunscript->save();
    }
    else {
        LoadedFunscript->save(path, override_location);
        updateTitle();
    }
    if (override_location) {
        last_save_time = std::chrono::system_clock::now();
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
    SDL_SaveBMP(surface, path);

    if (mustLock) { SDL_UnlockSurface(surface); }

    SDL_FreeSurface(surface);
}

void OpenFunscripter::removeAction(FunscriptAction action) noexcept
{
    undoRedoSystem.Snapshot("Remove action");
    LoadedFunscript->RemoveAction(action);
}

void OpenFunscripter::removeAction() noexcept
{
    if (LoadedFunscript->HasSelection()) {
        undoRedoSystem.Snapshot("Removed selection");
        LoadedFunscript->RemoveSelectedActions();
    }
    else {
        auto action = LoadedFunscript->GetClosestAction(player.getCurrentPositionMs());
        if (action != nullptr) {
            removeAction(*action); // snapshoted in here
        }
    }
}

void OpenFunscripter::addEditAction(int pos) noexcept
{
    undoRedoSystem.Snapshot("Add/Edit Action");
    scripting->addEditAction(FunscriptAction(player.getCurrentPositionMs(), pos));
}

void OpenFunscripter::cutSelection() noexcept
{
    if (LoadedFunscript->HasSelection()) {
        copySelection();
        undoRedoSystem.Snapshot("Cut selection");
        LoadedFunscript->RemoveSelectedActions();
    }
}

void OpenFunscripter::copySelection() noexcept
{
    if (LoadedFunscript->HasSelection()) {
        CopiedSelection.clear();
        for (auto action : LoadedFunscript->Selection()) {
            CopiedSelection.emplace_back(action);
        }
    }
}

void OpenFunscripter::pasteSelection() noexcept
{
    if (CopiedSelection.size() == 0) return;
    undoRedoSystem.Snapshot("Paste copied actions");
    // paste CopiedSelection relatively to position
    // NOTE: assumes CopiedSelection is ordered by time
    int offset_ms = std::round(player.getCurrentPositionMs()) - CopiedSelection.begin()->at;

    for (auto& action : CopiedSelection) {
        LoadedFunscript->PasteAction(FunscriptAction(action.at + offset_ms, action.pos), player.getFrameTimeMs());
    }
    player.setPosition((CopiedSelection.end() - 1)->at + offset_ms);
}

void OpenFunscripter::equalizeSelection() noexcept
{
    if (!LoadedFunscript->HasSelection()) {
        undoRedoSystem.Snapshot("Equalize actions");
        // this is a small hack
        auto closest = LoadedFunscript->GetClosestAction(player.getCurrentPositionMs());
        if (closest != nullptr) {
            auto behind = LoadedFunscript->GetPreviousActionBehind(closest->at);
            if (behind != nullptr) {
                auto front = LoadedFunscript->GetNextActionAhead(closest->at);
                if (front != nullptr) {
                    LoadedFunscript->SelectAction(*behind);
                    LoadedFunscript->SelectAction(*closest);
                    LoadedFunscript->SelectAction(*front);
                    LoadedFunscript->EqualizeSelection();
                    LoadedFunscript->ClearSelection();
                }
            }
        }
    }
    else if(LoadedFunscript->Selection().size() >= 3) {
        undoRedoSystem.Snapshot("Equalize actions");
        LoadedFunscript->EqualizeSelection();
    }
}

void OpenFunscripter::invertSelection() noexcept
{
    if (!LoadedFunscript->HasSelection()) {
        undoRedoSystem.Snapshot("Invert actions");
        // same hack as above 
        auto closest = LoadedFunscript->GetClosestAction(player.getCurrentPositionMs());
        LoadedFunscript->SelectAction(*closest);
        LoadedFunscript->InvertSelection();
        LoadedFunscript->ClearSelection();
    }
    else if (LoadedFunscript->Selection().size() >= 3) {
        undoRedoSystem.Snapshot("Invert actions");
        LoadedFunscript->InvertSelection();
    }
}

void OpenFunscripter::isolateAction() noexcept
{
    auto closest = LoadedFunscript->GetClosestAction(player.getCurrentPositionMsInterp());
    if (closest != nullptr) {
        undoRedoSystem.Snapshot("Isolate action");
        auto prev = LoadedFunscript->GetPreviousActionBehind(closest->at - 1);
        auto next = LoadedFunscript->GetNextActionAhead(closest->at + 1);
        if (prev != nullptr && next != nullptr) {
            auto tmp = *next; // removing prev will invalidate the pointer
            LoadedFunscript->RemoveAction(*prev);
            LoadedFunscript->RemoveAction(tmp);
        }
        else if (prev != nullptr) { LoadedFunscript->RemoveAction(*prev); }
        else if (next != nullptr) { LoadedFunscript->RemoveAction(*next); }

    }
}

void OpenFunscripter::showOpenFileDialog()
{
    // we run this in a seperate thread so we don't block the main thread
    // the result gets passed to the main thread via an event
    auto thread = [](void* ctx) {
        auto app = (OpenFunscripter*)ctx;
        auto& path = app->settings->data().last_path;
        std::vector<std::string> filters { "All Files", "*" };
        std::stringstream ss;
        for (auto& ext : SupportedVideoExtensions)
            ss << '*' << ext << ';';
        filters.emplace_back(std::string("Videos ( ") + ss.str() + " )");
        filters.emplace_back(ss.str());
        filters.emplace_back("Funscript ( .funscript )");
        filters.emplace_back("*.funscript");
 
        
        if (!std::filesystem::exists(path)) {
            path = "";
        }
        pfd::open_file fileDialog("Choose a file", path, filters, pfd::opt::none);
        
        static std::vector<std::string> result;
        result = fileDialog.result();
        SDL_Event ev{ 0 };
        ev.type = EventSystem::FileDialogOpenEvent;
        ev.user.data1 = &result;
        SDL_PushEvent(&ev);
        return 0;
    };
    auto handle = SDL_CreateThread(thread, "OpenFunscripterFileDialog", this);
    SDL_DetachThread(handle);
}

void OpenFunscripter::showSaveFileDialog()
{
    // we run this in a seperate thread so we don't block the main thread
    // the result gets passed to the main thread via an event
    auto thread = [](void* ctx) {
        auto app = (OpenFunscripter*)ctx;
        auto path = std::filesystem::path(app->settings->data().most_recent_file.script_path);
        path.replace_extension(".funscript");

        pfd::save_file saveDialog("Save", path.string(), { "Funscript", "*.funscript" });
        static std::string result;
        result = saveDialog.result();
        SDL_Event ev{ 0 };
        ev.type = EventSystem::FileDialogSaveEvent;
        ev.user.data1 = &result;
        SDL_PushEvent(&ev);
        return 0;
    };
    auto handle = SDL_CreateThread(thread, "OpenFunscripterSaveFileDialog", this);
    SDL_DetachThread(handle);
}


void OpenFunscripter::ShowMainMenuBar() noexcept
{
#define BINDING_STRING(binding) keybinds.getBindingString(binding).c_str()
    // TODO: either remove the shortcuts or dynamically retrieve them
    if (ImGui::BeginMenuBar())
    {
        ImVec2 region = ImGui::GetContentRegionAvail();

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(ICON_FOLDER_OPEN" Open video / script")) {
                showOpenFileDialog();
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
                saveScript();
            }
            if (ImGui::MenuItem("Save as...")) {
                showSaveFileDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Enable rolling backup", NULL, &RollingBackup)) {}
            if (ImGui::MenuItem("Open backup directory")) {
                Util::OpenFileExplorer("backup");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Save frame as image", BINDING_STRING("save_frame_as_image")))
            { 
                auto screenshot_dir = Util::Basepath() / "screenshot";
                player.saveFrameToImage(screenshot_dir.string().c_str());
            }
            // this is awkward
            if (ImGui::MenuItem("Open screenshot directory")) {
                std::error_code ec;
                auto screenshot_dir = Util::Basepath() / "screenshot";
                std::filesystem::create_directories(screenshot_dir, ec);
                Util::OpenFileExplorer(screenshot_dir.string().c_str());
            }

            ImGui::Separator();
            static int heatmapWidth = 2000;
            static int heatmapHeight = 200;
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##width", &heatmapWidth); ImGui::SameLine();
            ImGui::Text("%s", "x"); ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            ImGui::InputInt("##heiht", &heatmapHeight);
            if (ImGui::MenuItem("Save heatmap")) { 
                char buf[1024];
                stbsp_snprintf(buf, sizeof(buf), "%s_Heatmap.bmp", LoadedFunscript->metadata.title.c_str());
                auto heatmapPath = Util::Basepath() / "screenshot";
                std::error_code ec;
                std::filesystem::create_directories(heatmapPath, ec);
                heatmapPath /= buf;
                saveHeatmap(heatmapPath.string().c_str(), heatmapWidth, heatmapHeight);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Undo", BINDING_STRING("undo"), false, !undoRedoSystem.UndoEmpty())) {
                undoRedoSystem.Undo();
            }
            if (ImGui::MenuItem("Redo", BINDING_STRING("redo"), false, !undoRedoSystem.RedoEmpty())) {
                undoRedoSystem.Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", BINDING_STRING("cut"), false, LoadedFunscript->HasSelection())) {
                cutSelection();
            }
            if (ImGui::MenuItem("Copy", BINDING_STRING("copy"), false, LoadedFunscript->HasSelection()))
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
                LoadedFunscript->SelectAll();
            }
            if (ImGui::MenuItem("Deselect all", BINDING_STRING("deselect_all"), false)) {
                LoadedFunscript->ClearSelection();
            }

            if (ImGui::BeginMenu("Special")) {
                if (ImGui::MenuItem("Select all left")) {
                    LoadedFunscript->SelectTime(0, player.getCurrentPositionMs());
                }
                if (ImGui::MenuItem("Select all right")) {
                    LoadedFunscript->SelectTime(player.getCurrentPositionMs(), player.getDuration()*1000.f);
                }
                ImGui::Separator();
                static int32_t selectionPoint = -1;
                if (ImGui::MenuItem("Set selection start")) {
                    if (selectionPoint == -1) {
                        selectionPoint = player.getCurrentPositionMs();
                    }
                    else {
                        LoadedFunscript->SelectTime(player.getCurrentPositionMs(), selectionPoint);
                        selectionPoint = -1;
                    }
                }
                if (ImGui::MenuItem("Set selection end")) {
                    if (selectionPoint == -1) {
                        selectionPoint = player.getCurrentPositionMs();
                    }
                    else {
                        LoadedFunscript->SelectTime(selectionPoint, player.getCurrentPositionMs());
                        selectionPoint = -1;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Top points only", NULL, false)) {
                if (LoadedFunscript->HasSelection()) {
                    undoRedoSystem.Snapshot("Top points only");
                    LoadedFunscript->SelectTopActions();
                }
            }
            if (ImGui::MenuItem("Mid points only", NULL, false)) {
                if (LoadedFunscript->HasSelection()) {
                    undoRedoSystem.Snapshot("Mid points only");
                    LoadedFunscript->SelectMidActions();
                }
            }
            if (ImGui::MenuItem("Bottom points only", NULL, false)) {
                if (LoadedFunscript->HasSelection()) {
                    undoRedoSystem.Snapshot("Bottom points only");
                    LoadedFunscript->SelectBottomActions();
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
                undoRedoSystem.Snapshot("Frame align");
                LoadedFunscript->AlignWithFrameTimeSelection(player.getFrameTimeMs());
            }
            Util::Tooltip("Don't use on already aligned actions.");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Bookmarks")) {
            static std::string bookmarkName;
            auto& bookmarks = LoadedFunscript->scriptSettings.Bookmarks;
            auto editBookmark = std::find_if(bookmarks.begin(), bookmarks.end(),
                [&](auto& mark) {
                    constexpr int threshold = 15000;
                    int32_t current = player.getCurrentPositionMs();
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
                    bookmark.at = player.getCurrentPositionMs();
                    if (bookmarkName.empty())
                    {
                        std::stringstream ss;
                        ss << LoadedFunscript->Bookmarks().size() + 1 << '#';
                        bookmarkName = ss.str();
                    }
                    bookmark.name = bookmarkName;
                    bookmarkName = "";
                    LoadedFunscript->AddBookmark(bookmark);
                }
            }

            if (ImGui::BeginMenu("Go to...")) {
                if (LoadedFunscript->Bookmarks().size() == 0) {
                    ImGui::TextDisabled("No bookmarks");
                }
                else {
                    for (auto& mark : LoadedFunscript->Bookmarks()) {
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
            if (ImGui::MenuItem(StatisticsId, NULL, &ShowStatistics)) {}
            if (ImGui::MenuItem(UndoSystem::UndoHistoryId, NULL, &ShowHistory)) {}
            if (ImGui::MenuItem(ScriptSimulator::SimulatorId, NULL, &settings->data().show_simulator)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Metadata", NULL, &ShowMetadataEditor)) {}
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
        if(ImGui::MenuItem("About", NULL, &ShowAbout)) {}
        if (player.isLoaded()) {
            ImGui::SameLine(region.x - ImGui::GetFontSize()*12);
            std::chrono::duration<float> duration = std::chrono::system_clock::now() - last_save_time;
            ImColor col(ImGui::GetStyle().Colors[ImGuiCol_Text]);
            if (duration.count() > (60.f*5.f)) {
                col = IM_COL32(255, 0, 0, 255);
            }
            ImGui::TextColored(col,"last saved %d minutes ago", (int)(duration.count() / 60.f));
        }
        ImGui::EndMenuBar();
    }
#undef BINDING_STRING
}

bool OpenFunscripter::ShowMetadataEditorWindow(bool* open) noexcept
{
    if (!*open) return false;
    bool save = false;
    auto& metadata = LoadedFunscript->metadata;
    ImGui::Begin("Metadata Editor", open, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
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
    }
    
    ImGui::Text("%s", "Tags");
    static std::string newTag;
    ImGui::InputText("##Tag", &newTag); ImGui::SameLine(); 
    if (ImGui::Button("Add", ImVec2(-1.f, 0.f))) { 
        Util::trim(newTag);
        if (!newTag.empty()) {
            metadata.tags.emplace_back(newTag); newTag.clear();
        }
    }
    
    auto& style = ImGui::GetStyle();

    auto availableWidth = ImGui::GetContentRegionAvail().x;

    int removeIndex = -1;
    for (int i = 0; i < metadata.tags.size(); i++) {
        ImGui::PushID(i);
        auto& tag = metadata.tags[i];
        
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
    ImGui::NewLine();
    if (removeIndex != -1) {
        metadata.tags.erase(metadata.tags.begin() + removeIndex);
        removeIndex = -1;
    }

    ImGui::Text("%s", "Performers");
    static std::string newPerformer;
    ImGui::InputText("##Performer", &newPerformer); ImGui::SameLine();
    if (ImGui::Button("Add##Performer", ImVec2(-1.f, 0.f))) {
        Util::trim(newPerformer);
        if (!newPerformer.empty()) {
            metadata.performers.emplace_back(newPerformer); newPerformer.clear(); 
        }
    }

    availableWidth = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < metadata.performers.size(); i++) {
        ImGui::PushID(i);
        auto& performer = metadata.performers[i];

        if (ImGui::Button(performer.c_str())) {
            removeIndex = i;
        }

        auto nextLineCursor = ImGui::GetCursorPos();
        ImGui::SameLine();
        if (ImGui::GetCursorPosX() + ImGui::GetItemRectSize().x >= availableWidth) {
            ImGui::SetCursorPos(nextLineCursor);
        }
        ImGui::PopID();
    }
    ImGui::NewLine();
    if (removeIndex != -1) {
        metadata.performers.erase(metadata.performers.begin() + removeIndex);
        removeIndex = -1;
    }
    
    if (ImGui::Button("Save", ImVec2(-1.f, 0.f))) { save = true; }
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
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
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
    if (ImGui::Button("Latest release", ImVec2(-1.f, 0.f))) {
        Util::OpenUrl("https://github.com/gagax1234/OpenFunscripter/releases/latest");
    }
    ImGui::End();
}

void OpenFunscripter::ShowStatisticsWindow(bool* open) noexcept
{
    if (!*open) return;
    ImGui::Begin(StatisticsId, open, ImGuiWindowFlags_None);
    const FunscriptAction* behind = LoadedFunscript->GetActionAtTime(player.getCurrentPositionMs(), 0);
    const FunscriptAction* front = nullptr;
    if (behind != nullptr) {
        front = LoadedFunscript->GetNextActionAhead(player.getCurrentPositionMs() + 1);
    }
    else {
        behind = LoadedFunscript->GetPreviousActionBehind(player.getCurrentPositionMs());
        front = LoadedFunscript->GetNextActionAhead(player.getCurrentPositionMs());
    }

    if (behind != nullptr) {
        ImGui::Text("Interval: %d ms", (int32_t)player.getCurrentPositionMs() - behind->at);
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

    // bookmarks
    for (auto& bookmark : LoadedFunscript->scriptSettings.Bookmarks) {
        const float rectWidth = 7.f;

        ImVec2 p1((frame_bb.Min.x + (frame_bb.GetWidth() * (bookmark.at / (player.getDuration() * 1000.0)))) - (rectWidth/2.f), frame_bb.Min.y);       
        ImVec2 p2(p1.x + rectWidth, frame_bb.Min.y + frame_bb.GetHeight() + (style.ItemSpacing.y * 3.0f));

        //ImRect rect(p1, p2);
        //ImGui::ItemSize(rect);
        //auto bookmarkId = ImGui::GetID(bookmark.name.c_str());
        //ImGui::ItemAdd(rect, bookmarkId);

        draw_list->AddRectFilled(p1, p2, ImColor(style.Colors[ImGuiCol_PlotHistogram]), 8.f);

        if (item_hovered || settings->data().always_show_bookmark_labels) {
            auto size = ImGui::CalcTextSize(bookmark.name.c_str());
            size.x /= 2.f;
            size.y /= 8.f;
            draw_list->AddText(p2 - size, ImColor(style.Colors[ImGuiCol_Text]), bookmark.name.c_str());
        }
    }

    ImGradient::DrawGradientBar(&TimelineGradient, frame_bb.Min, frame_bb.GetWidth(), frame_bb.GetHeight());

    const float timeline_pos_cursor_w = 5.f;
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

    const float current_pos_x = frame_bb.Min.x + frame_bb.GetWidth() * (*position);
    draw_list->AddLine(ImVec2(current_pos_x, frame_bb.Min.y), ImVec2(current_pos_x, frame_bb.Max.y), timeline_cursor_back, timeline_pos_cursor_w);
    draw_list->AddLine(ImVec2(current_pos_x, frame_bb.Min.y), ImVec2(current_pos_x, frame_bb.Max.y), timeline_cursor_front, timeline_pos_cursor_w / 2.f);

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

    if (LoadedFunscript->Actions().size() == 0) {
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

    auto segments = getSegments(LoadedFunscript->Actions(), 10000);
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
            HeatMap.computeColorAt(actionsRelToMax, (float*)&color.Value);
            float markPos = kernel_offset  / totalDurationMs;
            grad.addMark(markPos, color);
        } while (kernel_offset < (segment.front().at + durationMs));
        grad.addMark((kernel_offset + 1.f) / totalDurationMs, IM_COL32(0, 0, 0, 255));
    }
    grad.refreshCache();
}


