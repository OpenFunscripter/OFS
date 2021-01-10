#include "OFP.h"

#include "OFS_Util.h"
#include "FunscriptHeatmap.h"
#include "ScriptPositionsOverlayMode.h"

#include "SDL.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"

#include <filesystem>
#include <sstream>
#include <limits>

constexpr const char* glsl_version = "#version 150";
constexpr int DefaultWidth = 1920;
constexpr int DefaultHeight = 1080;

ImFont* OFP::DefaultFont2 = nullptr;

static ImGuiID MainDockspaceID;

// TODO: fix whirligig implementation maybe use libcluon for the tcp
// TODO: show active loop in the timeline
// TODO: autohome when paused

// TODO: try fix to layout issues with docking

constexpr std::array<const char*, 6> SupportedVideoExtensions{
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

void OFP::set_fullscreen(bool fullscreen) noexcept
{
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
        SDL_SetWindowSize(window, bounds.w, bounds.h);
    }
    else {
        //SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowResizable(window, SDL_TRUE);
        SDL_SetWindowBordered(window, SDL_TRUE);
        SDL_SetWindowPosition(window, restoreRect.x, restoreRect.y);
        SDL_SetWindowSize(window, restoreRect.w, restoreRect.h);
    }
}

void OFP::set_default_layout(bool force) noexcept
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

        auto dock_player_control_id = ImGui::DockBuilderSplitNode(dock_time_bottom_id, ImGuiDir_Left, 0.15f, &dock_time_bottom_id, &dock_time_bottom_id);

        ImGui::DockBuilderGetNode(dock_player_center_id)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDocking;
        ImGui::DockBuilderGetNode(dock_positions_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDocking;
        ImGui::DockBuilderGetNode(dock_time_bottom_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDocking;
        ImGui::DockBuilderGetNode(dock_player_control_id)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDocking;

        ImGui::DockBuilderDockWindow(VideoplayerWindow::PlayerId, dock_player_center_id);
        ImGui::DockBuilderDockWindow(OFP_WrappedVideoplayerControls::PlayerTimeId, dock_time_bottom_id);
        ImGui::DockBuilderDockWindow(OFP_WrappedVideoplayerControls::PlayerControlId, dock_player_control_id);
        ImGui::DockBuilderDockWindow(ScriptTimeline::PositionsId, dock_positions_id);
        ImGui::DockBuilderDockWindow(Videobrowser::VideobrowserId, dock_mode_right_id);

        // filebrowser scene
        ImGui::DockBuilderDockWindow(Videobrowser::VideobrowserSceneId, dock_player_center_id);

        ImGui::DockBuilderFinish(MainDockspaceID);
    }
}

void OFP::process_events() noexcept
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
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            timer.reset();
            break;
        }
        events->PushEvent(event);
    }
}

void OFP::update() noexcept
{
    tcode->sync(player->getCurrentPositionMsInterp(), player->getSpeed());
    ControllerInput::UpdateControllers(100);
    scriptTimeline.overlay->update();

    settings.videoPlayer->current_vr_rotation += rotateVR;
    settings.videoPlayer->vr_zoom *= zoomVR;

    if (!settings.enable_autohide) 
    {
        timer.reset();
    }
    else {
        static ImVec2 prevPos;
        auto mousePos = ImGui::GetMousePos();
        auto viewport = ImGui::GetMainViewport();
        ImRect viewport_bb;
        viewport_bb.Min = viewport->Pos;
        viewport_bb.Max = viewport->Pos + viewport->Size;

        if (viewport_bb.Contains(mousePos)) {
            if (prevPos.x != mousePos.x || prevPos.y != mousePos.y) {
                timer.reset();
            }
        }
        prevPos = mousePos;
    }
    SDL_ShowCursor(!timer.hidden());
}

void OFP::new_frame() noexcept
{
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    
    OFS::Im3d_NewFrame();
}

void OFP::render() noexcept
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

void OFP::ShowMainMenuBar() noexcept
{
#define BINDING_STRING(binding) keybinds.getBindingString(binding).c_str()  
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(ICON_FOLDER_OPEN" Open video / script")) {
                Util::OpenFileDialog("Choose a file", "",
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
            if (ImGui::MenuItem("Videobrowser")) {
                SetActiveScene(OFP_Scene::Filebrowser);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("T-Code", NULL, &settings.show_tcode);
            ImGui::MenuItem("Videobrowser", NULL, &settings.show_browser);
            ImGui::MenuItem("Simulator 3D", NULL, &settings.show_sim3d);
            ImGui::MenuItem("Playback controls", NULL, &settings.show_controls);
            ImGui::MenuItem("Time", NULL, &settings.show_time);
            ImGui::MenuItem("Script Timeline", NULL, &settings.show_timeline);

            ImGui::Separator();
            if (ImGui::MenuItem("Draw video", NULL, &settings.show_video)) { }
            if (ImGui::MenuItem("Reset video position", NULL)) { player->resetTranslationAndZoom(); }
            ImGui::Separator();
            if (ImGui::MenuItem("VR mode", NULL, player->settings->activeMode == VideoMode::VR_MODE)) {
                ToggleVrMode();
            }
            ImGui::Separator();
            ImGui::MenuItem("Auto-hide", NULL, &settings.enable_autohide);
#ifndef NDEBUG
            ImGui::Separator();
            if (ImGui::BeginMenu("DEBUG ONLY")) {
                if (ImGui::MenuItem("ImGui", NULL, &DebugMetrics)) {}
                if (ImGui::MenuItem("ImGui Demo", NULL, &DebugDemo)) {}
                ImGui::EndMenu();
            }
#endif
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            if (ImGui::MenuItem("Keys")) {
                keybinds.ShowWindow = true;
            }
            if (ImGui::MenuItem("Fullscreen", BINDING_STRING("fullscreen_toggle"), &Fullscreen)) {
                set_fullscreen(Fullscreen);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Loop"))
        {
            if (player->LoopActive()) {
                ImGui::Text("%.3lf to %.3lf", player->LoopASeconds(), player->LoopBSeconds());
                if (ImGui::MenuItem("Clear loop")) { player->CycleLoopAB(); }
            }
            else { if (ImGui::MenuItem("Set")) { player->CycleLoopAB(); } }
            ImGui::EndMenu();
        }
#ifdef WIN32
        if (ImGui::MenuItem("Whirligig", NULL, &Whirligig)) {
            player->closeVideo();
            clearLoadedScripts();
            if (Whirligig)
            {
                player = std::make_unique<WhirligigPlayer>(this);
            }
            else
            {
                player = std::make_unique<DefaultPlayer>();
            }
            settings.videoPlayer = player->settings;
            player->setup();
        }
#endif
        ImGui::Separator();
        ImGui::Spacing();
        if (ControllerInput::AnythingConnected()) {
            bool navmodeActive = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad;
            ImGui::Text(ICON_GAMEPAD " " ICON_LONG_ARROW_RIGHT " %s", (navmodeActive) ? "Navigation" : "Player");
        }
        ImGui::EndMainMenuBar();
    }
#undef BINDING_STRING
}

void OFP::CreateDockspace(bool withMenuBar) noexcept
{
    const bool opt_fullscreen_persistant = true;
    const bool opt_fullscreen = opt_fullscreen_persistant;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_HiddenTabBar;

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

    if (withMenuBar) {
        ShowMainMenuBar();
    }

    ImGui::End();
}

void OFP::SetNavigationMode(bool enable) noexcept
{
    auto& io = ImGui::GetIO();
    io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableGamepad);
    if (enable) { io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; }
}

void OFP::SetActiveScene(OFP_Scene scene) noexcept
{
    settings.ActiveScene = scene;
    switch (scene) {
    case OFP_Scene::Player:
        SetNavigationMode(false);
        break;
    case OFP_Scene::Filebrowser:
        SetNavigationMode(true);
        break;
    }
}

void OFP::ToggleVrMode() noexcept
{
    if (player->settings->activeMode == VideoMode::VR_MODE) {
        player->settings->activeMode = VideoMode::FULL;
    }
    else {
        player->settings->activeMode = VideoMode::VR_MODE;
    }
}

OFP::~OFP() noexcept
{
    settings.save();
    player.reset();
    OFS::Im3d_Shutdown();
}

bool OFP::load_fonts(const char* font_override) noexcept
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
        font = io.Fonts->AddFontFromFileTTF(roboto.c_str(), settings.default_font_size, &config);
        if (font == nullptr) return false;
        io.FontDefault = font;
    }

    if (!Util::FileExists(fontawesome)) {
        LOGF_WARN("\"%s\" font is missing. No icons.", fontawesome.c_str());
    }
    else {
        config.MergeMode = true;
        font = io.Fonts->AddFontFromFileTTF(fontawesome.c_str(), settings.default_font_size, &config, icons_ranges);
        if (font == nullptr) return false;
    }

    if (!Util::FileExists(noto_jp)) {
        LOGF_WARN("\"%s\" font is missing. No japanese glyphs.", noto_jp.c_str());
    }
    else {
        config.MergeMode = true;
        font = io.Fonts->AddFontFromFileTTF(noto_jp.c_str(), settings.default_font_size, &config, io.Fonts->GetGlyphRangesJapanese());
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
        DefaultFont2 = io.Fonts->AddFontFromFileTTF(roboto.c_str(), settings.default_font_size * 2.0f, &config);
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

bool OFP::imgui_setup() noexcept
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigViewportsNoDecoration = true;
    io.ConfigViewportsNoAutoMerge = false;
    io.ConfigViewportsNoTaskBarIcon = false;
    
    static auto imguiIniPath = Util::PrefpathOFP("ofp_imgui.ini");
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
    LOGF_DEBUG("init imgui with glsl: %s", glsl_version);
    ImGui_ImplOpenGL3_Init(glsl_version);

    load_fonts(settings.font_override.empty() ? nullptr : settings.font_override.c_str());
    return true;
}

void OFP::register_bindings() noexcept
{
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
            true
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

        auto& toggle_vr_mode = group.bindings.emplace_back(
            "toggle_vr_mode",
            "Toggle VR mode",
            true,
            [&](void*) { ToggleVrMode(); }
        );
        toggle_vr_mode.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_BACK,
            true
        );

        auto& ab_loop = group.bindings.emplace_back(
            "ab_loop",
            "Cycle AB Loop",
            true,
            [&](void*) {player->CycleLoopAB(); }
        );
        ab_loop.key = Keybinding(
            SDLK_l,
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
                auto action = RootFunscript()->GetPreviousActionBehind(player->getCurrentPositionMsInterp() - 1.f);
                if (action != nullptr) player->setPositionExact(action->at);
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
                auto action = RootFunscript()->GetNextActionAhead(player->getCurrentPositionMsInterp() + 1.f);
                if (action != nullptr) player->setPositionExact(action->at);
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

        // FRAME CONTROL
        auto& prev_frame = group.bindings.emplace_back(
            "prev_frame",
            "Previous frame",
            false,
            [&](void*) {
                if (player->isPaused()) {
                    player->previousFrame();
                }
            }
        );
        prev_frame.key = Keybinding(
            SDLK_LEFT,
            0
        );

        auto& next_frame = group.bindings.emplace_back(
            "next_frame",
            "Next frame",
            false,
            [&](void*) {
                if (player->isPaused()) {
                    player->nextFrame();
                }
            }
        );
        next_frame.key = Keybinding(
            SDLK_RIGHT,
            0
        );

        keybinds.registerBinding(group);
    }
    {
        KeybindingGroup group;
        group.name = "Utility";
        
        auto& cycle_scenes = group.bindings.emplace_back(
            "cycle_scenes",
            "Cycle scenes",
            true,
            [&](void*) {
                int32_t activeScene = settings.ActiveScene;
                activeScene++;
                activeScene %= OFP_Scene::TotalScenes;
                SetActiveScene((OFP_Scene)activeScene);
            }
        );
        cycle_scenes.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSTICK,
            true
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
            [&](void*) { Fullscreen = !Fullscreen; set_fullscreen(Fullscreen); }
        );
        fullscreen_toggle.key = Keybinding(
            SDLK_F10,
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
            "Forward 3 second",
            false,
            [&](void*) { player->seekRelative(3000); }
        );
        seek_forward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
            false
        );

        auto& seek_backward_second = group.bindings.emplace_back(
            "seek_backward_second",
            "Backward 3 second",
            false,
            [&](void*) { player->seekRelative(-3000); }
        );
        seek_backward_second.controller = ControllerBinding(
            SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            false
        );

        keybinds.registerBinding(group);
    }
}

bool OFP::setup()
{
#ifndef NDEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
#endif

    LOG_DEBUG("trying to init sdl");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        LOGF_ERROR("Error: %s\n", SDL_GetError());
        return false;
    }
    LOG_DEBUG("SDL init done!");

#if __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac according to imgui example
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0 /*| SDL_GL_CONTEXT_DEBUG_FLAG*/);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    LOG_DEBUG("trying to create window");
    window = SDL_CreateWindow(
        "OFP " OFS_LATEST_GIT_TAG "@" OFS_LATEST_GIT_HASH,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DefaultWidth, DefaultHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN
    );
    LOG_DEBUG("created window");

    SDL_Rect display;
    int windowDisplay = SDL_GetWindowDisplayIndex(window);
    SDL_GetDisplayBounds(windowDisplay, &display);
    if (DefaultWidth >= display.w || DefaultHeight >= display.h) {
        SDL_MaximizeWindow(window);
    }

    LOG_DEBUG("trying to create gl context");
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    LOG_DEBUG("created gl context");

    player = std::make_unique<DefaultPlayer>();
    settings.videoPlayer = player->settings;

    settings.load(Util::PrefpathOFP("settings.json"));
    SDL_GL_SetSwapInterval(settings.vsync);

    if (gladLoadGL() == 0) {
        LOG_ERROR("Failed to load glad.");
        return false;
    }

    if (!imgui_setup()) {
        LOG_ERROR("Failed to setup ImGui");
        return false;
    }
    bool result = true;

    events = std::make_unique<EventSystem>();
    events->setup();
    FunscriptEvents::RegisterEvents();
    VideoEvents::RegisterEvents();
    KeybindingEvents::RegisterEvents();
    VideobrowserEvents::RegisterEvents();
    ScriptTimelineEvents::RegisterEvents();

    keybinds.setup(*events);
    register_bindings();
    keybinds.load(Util::PrefpathOFP("keybinds.json"));


	result &= player->setup();

    events->Subscribe(SDL_DROPFILE, EVENT_SYSTEM_BIND(this, &OFP::DragNDrop));
    events->Subscribe(VideoEvents::MpvVideoLoaded, EVENT_SYSTEM_BIND(this, &OFP::MpvVideoLoaded));
    events->Subscribe(VideoEvents::PlayPauseChanged, EVENT_SYSTEM_BIND(this, &OFP::MpvPlayPauseChange));
    events->Subscribe(VideobrowserEvents::VideobrowserItemClicked, EVENT_SYSTEM_BIND(this, &OFP::VideobrowserItemClicked));
    events->Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &OFP::ControllerAxis));

    controllerInput = std::make_unique<ControllerInput>();
    controllerInput->setup(*events);
    tcode = std::make_unique<TCodePlayer>();
    videobrowser = std::make_unique<Videobrowser>(&settings.videoBrowser);

    clearLoadedScripts();

    scriptTimeline.setup(NULL);
    scriptTimeline.overlay = std::make_unique<EmptyOverlay>(&scriptTimeline);

    if (!settings.last_file.empty()) {
        openFile(settings.last_file);
    }
    
    OFS::Im3d_Init();
    SetActiveScene(settings.ActiveScene);

    sim3d = std::make_unique<Simulator3D>();
    sim3d->setup();

    tcode->loadSettings(Util::PrefpathOFP("tcode.json"));

    SDL_ShowWindow(window);
	return result;
}

int OFP::run() noexcept
{
    new_frame();
    set_default_layout(false);
    render();
    while (!exit_app) {
        step();
    }
	return 0;
}

void OFP::step() noexcept
{
    process_events();
    new_frame();
    update();
    {
        switch (settings.ActiveScene) {
        case OFP_Scene::Player:
            PlayerScene();
            break;
        case OFP_Scene::Filebrowser:
            FilebrowserScene();
            break;
        }
#ifndef NDEBUG
        if (DebugDemo) {
            ImGui::ShowDemoWindow(&DebugDemo);
        }

        if (DebugMetrics) {
            ImGui::ShowMetricsWindow(&DebugMetrics);
        }
#endif
    }
    render();
    if (settings.show_sim3d && settings.ActiveScene == OFP_Scene::Player) {
        sim3d->render();
    }
    OFS::Im3d_EndFrame();
    SDL_GL_SwapWindow(window);
}

void OFP::PlayerScene() noexcept 
{
    CreateDockspace(!timer.hidden());

    bool HideElement = !timer.hidden();

    sim3d->ShowWindow(&settings.show_sim3d, player->getCurrentPositionMsInterp(), BaseOverlay::SplineLines, LoadedFunscripts);

    playerControls.DrawControls(&settings.show_controls, player.get());
    playerControls.DrawTimeline(&settings.show_time, player.get());
    scriptTimeline.ShowScriptPositions(&settings.show_timeline, player->getCurrentPositionMsInterp(), player->getDuration() * 1000.f, player->getFrameTimeMs(), LoadedFunscripts, RootFunscript().get());

    if (keybinds.ShowBindingWindow()) { keybinds.save(); }

    tcode->DrawWindow(&settings.show_tcode);

    videobrowser->ShowBrowser(Videobrowser::VideobrowserId, &settings.show_browser);
    player->DrawVideoPlayer(NULL, &settings.show_video);    
}

void OFP::FilebrowserScene() noexcept 
{
    CreateDockspace(false);
    videobrowser->ShowBrowser(Videobrowser::VideobrowserSceneId, NULL);
    bool NotOpen = false;
    player->DrawVideoPlayer(&NotOpen, &settings.show_video);
    timer.reset();
}

void OFP::shutdown() noexcept
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void OFP::updateTitle() noexcept
{
    std::stringstream ss;
    ss.str(std::string());

    ss << "OFP " OFS_LATEST_GIT_TAG "@" OFS_LATEST_GIT_HASH;
    if (!RootFunscript()->current_path.empty()) {
        ss << " - \"" << RootFunscript()->current_path << "\"";
    }
    SDL_SetWindowTitle(window, ss.str().c_str());
}

bool OFP::openFile(const std::string& file) noexcept
{
    if (!Util::FileExists(file)) return false;

    std::filesystem::path file_path = Util::PathFromString(file);
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
            videoPath = base_path.u8string() + extension;
            if (Util::FileExists(videoPath)) {
                video_path = videoPath;
                break;
            }
        }

        if (video_path.empty()) {
            // try find audio
            for (auto&& extension : SupportedAudioExtensions) {
                videoPath = base_path.u8string() + extension;
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
            funscript_path = base_path.u8string() + ".funscript";
        }
        else {
            funscript_path = RootFunscript()->current_path;
        }
    }
    clearLoadedScripts();
    auto openFunscript = [this](const std::string& file) -> bool {
        RootFunscript() = std::make_shared<Funscript>();
        if (!Util::FileExists(file)) {
            return false;
        }
        return RootFunscript()->open<OFP_ScriptSettings>(file, "OFP");
    };

    // try load funscript
    bool result = openFunscript(funscript_path);
    if (result)
    {
        RootFunscript()->Userdata<OFP_ScriptSettings>().ScriptChannel = TChannel::L0;
    }

    if (!result) {
        LOGF_WARN("Couldn't find funscript. \"%s\"", funscript_path.c_str());
    }
    int channelIdx = 0;
    for (auto& axisAliases : TCodeChannels::Aliases) {
        for (auto alias : axisAliases) {
            auto axisFile = base_path;
            std::stringstream ss;
            ss << axisFile.filename().u8string() << '.' << alias << ".funscript";
            axisFile.replace_filename(ss.str());
            auto axisFileString = axisFile.u8string();
            if (Util::FileExists(axisFileString)) {
                auto& script = LoadedFunscripts.emplace_back();
                script = std::make_shared<Funscript>();
                script->open<OFP_ScriptSettings>(axisFileString, "OFP");
                script->Userdata<OFP_ScriptSettings>().ScriptChannel = static_cast<TChannel>(channelIdx);
            }
        }
        channelIdx++;
    }

    if (video_path.empty()) {
        if (!Util::FileNamesMatch(player->getVideoPath(), funscript_path)) {
            LOG_ERROR("No video found.\nLoading scripts without a video is not supported.");
            player->closeVideo();
        }
    }
    else {
        player->openVideo(video_path);
    }


    updateTitle();

    return result;
}

void OFP::clearLoadedScripts() noexcept
{
    LoadedFunscripts.clear();
    LoadedFunscripts.emplace_back(std::move(std::make_shared<Funscript>()));
    playerControls.TimelineGradient.clear();
    playerControls.TimelineGradient.addMark(0.f, IM_COL32_BLACK);
    playerControls.TimelineGradient.addMark(1.f, IM_COL32_BLACK);
    playerControls.TimelineGradient.refreshCache();
    updateTitle();
}

void OFP::DragNDrop(SDL_Event& ev) noexcept
{
    openFile(ev.drop.file);
    SDL_free(ev.drop.file);
}

void OFP::MpvVideoLoaded(SDL_Event& ev) noexcept
{
    OFS::UpdateHeatmapGradient(player->getDuration() * 1000.f, playerControls.TimelineGradient, RootFunscript()->Actions());
    settings.last_file = player->getVideoPath();
    if(!videobrowser->ClickedFilePath.empty()) player->setPaused(false);
}

void OFP::MpvPlayPauseChange(SDL_Event& ev) noexcept
{
    if (player->isPaused()) {
        tcode->stop();
    }
    else {
        if (RootFunscript()->current_path.empty()) return;
        auto l0_it = std::find_if(LoadedFunscripts.begin(), LoadedFunscripts.end(), [](auto& script) { return script->template Userdata<OFP_ScriptSettings>().ScriptChannel == TChannel::L0; });
        auto r0_it = std::find_if(LoadedFunscripts.begin(), LoadedFunscripts.end(), [](auto& script) { return script->template Userdata<OFP_ScriptSettings>().ScriptChannel == TChannel::R0; });
        auto r1_it = std::find_if(LoadedFunscripts.begin(), LoadedFunscripts.end(), [](auto& script) { return script->template Userdata<OFP_ScriptSettings>().ScriptChannel == TChannel::R1; });
        auto r2_it = std::find_if(LoadedFunscripts.begin(), LoadedFunscripts.end(), [](auto& script) { return script->template Userdata<OFP_ScriptSettings>().ScriptChannel == TChannel::R2; });

        std::weak_ptr<Funscript> L0 = l0_it != LoadedFunscripts.end() ? *l0_it : std::weak_ptr<Funscript>();
        std::weak_ptr<Funscript> R0 = r0_it != LoadedFunscripts.end() ? *r0_it : std::weak_ptr<Funscript>();;
        std::weak_ptr<Funscript> R1 = r1_it != LoadedFunscripts.end() ? *r1_it : std::weak_ptr<Funscript>();;
        std::weak_ptr<Funscript> R2 = r2_it != LoadedFunscripts.end() ? *r2_it : std::weak_ptr<Funscript>();;
        tcode->play(player->getCurrentPositionMsInterp(), std::move(L0), std::move(R0), std::move(R1), std::move(R2));
    }
}

void OFP::VideobrowserItemClicked(SDL_Event& ev) noexcept
{
    SetActiveScene(OFP_Scene::Player);
    openFile(videobrowser->ClickedFilePath);
}

void OFP::ControllerAxis(SDL_Event& ev) noexcept
{
    auto& io = ImGui::GetIO();
    if (!(io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) && settings.videoPlayer->activeMode == VideoMode::VR_MODE)
    {
        auto& caxis = ev.caxis;
        constexpr int deadzone = 2000;
        if (std::abs(caxis.value) < deadzone) {
            switch (caxis.axis) {
            case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX:
                rotateVR.x = 0;
                break;
            case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY:
                rotateVR.y = 0;
                break;

            case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                zoomVR = 1.f;
                break;
            }
            return;
        }

        float relativeValue = (float)(caxis.value - deadzone) / (std::numeric_limits<int16_t>::max() - deadzone);
        relativeValue *= 0.007f;

        switch (caxis.axis) {
        case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX:
            rotateVR.x = -relativeValue;
            break;
        case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY:
            rotateVR.y = relativeValue;
            break;
        case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            zoomVR = 1.000000000001f * (1.f+relativeValue);
            break;
        case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            zoomVR = 0.999999000999f * (1.f-relativeValue);
            break;
        }

    }
}
