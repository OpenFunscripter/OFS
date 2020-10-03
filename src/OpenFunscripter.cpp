#include "OpenFunscripter.h"

#include "OpenFunscripterUtil.h"

#include "GradientBar.h"

#include <filesystem>

#include "portable-file-dialogs.h"
#include "stb_sprintf.h"

#include "imgui_stdlib.h"
#include "imgui_internal.h"

// TODO: Undo Window make snapshots clickable to return to a state immediately
// TODO: Rolling backup
// TODO: QoL make keybindings groupable just a visual improvement

// TODO: consider bundling/replacing of snapshots for certain actions like moving which generates a lot of snapshots
//		 everytime the action is snapshoted matches the previous one we don't snapshot anything 
//		 this way undoing the moving would be a single click this may not always be desired though

// TODO: Buffer snapshots to disk to save memory after a certain threshold (500+)

// TODO: [MAJOR FEATURE] working with raw actions and controller input


OpenFunscripter* OpenFunscripter::ptr = nullptr;
ImFont* OpenFunscripter::DefaultFont2 = nullptr;

const char* glsl_version = "#version 150";

bool OpenFunscripter::imgui_setup()
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
    if (!std::filesystem::exists(roboto) || !std::filesystem::is_regular_file(roboto)) { 
        LOGF_WARN("\"%s\" font is missing.", roboto);
    }
    else {
        font = io.Fonts->AddFontFromFileTTF(roboto, settings->data().default_font_size, &config);
        if (font == nullptr) return false;
        io.FontDefault = font;
    }

    if (!std::filesystem::exists(fontawesome) || !std::filesystem::is_regular_file(fontawesome)) {
        LOGF_WARN("\"%s\" font is missing. No icons.", fontawesome);
    }
    else {
        config.MergeMode = true;
        font = io.Fonts->AddFontFromFileTTF(fontawesome, settings->data().default_font_size, &config, icons_ranges);
        if (font == nullptr) return false;
    }

    config.MergeMode = false;
    DefaultFont2 = io.Fonts->AddFontFromFileTTF(roboto, settings->data().default_font_size * 2.0f, &config);
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
    settings->saveSettings();
}

bool OpenFunscripter::setup()
{
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    ptr = this;
    
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


    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    window = SDL_CreateWindow(
        "OpenFunscripter " FUN_LATEST_GIT_TAG,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    
    if (gladLoadGL() == 0) {
        LOG_ERROR("Failed to load glad.");
        return false;
    }

    
    settings = std::make_unique<OpenFunscripterSettings>("data/keybinds.json", "data/config.json");
    if (!imgui_setup()) {
        LOG_ERROR("Failed to setup ImGui");
        return false;
    }

    // register custom events with sdl
    events.setup();

    keybinds.setup();
    register_bindings(); // needs to happen before setBindings
    keybinds.setBindings(settings->getKeybindings()); // override with user bindings

    scriptPositions.setup();
    LoadedFunscript = std::make_unique<Funscript>();

    scripting.setup();
    if (!player.setup()) {
        LOG_ERROR("Failed to init video player");
        return false;
    }

    events.Subscribe(EventSystem::FunscriptActionsChangedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptChanged));
    events.Subscribe(EventSystem::FunscriptActionClickedEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FunscriptActionClicked));
    events.Subscribe(EventSystem::FileDialogOpenEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FileDialogOpenEvent));
    events.Subscribe(EventSystem::FileDialogSaveEvent, EVENT_SYSTEM_BIND(this, &OpenFunscripter::FileDialogSaveEvent));
    events.Subscribe(SDL_DROPFILE, EVENT_SYSTEM_BIND(this, &OpenFunscripter::DragNDrop));
    events.Subscribe(EventSystem::MpvVideoLoaded, EVENT_SYSTEM_BIND(this, &OpenFunscripter::MpvVideoLoaded));
    // cache these here because openFile overrides them
    std::string last_video = settings->data().last_opened_video;
    std::string last_script = settings->data().last_opened_script;  
    if (!last_script.empty())
        openFile(last_script);
    if (!last_video.empty())
        openFile(last_video);


    rawInput.setup();
    simulator.setup();
    return true;
}

void OpenFunscripter::register_bindings()
{
    // UNDO / REDO
    keybinds.registerBinding(Keybinding(
        "undo",
        "Undo",
        SDLK_z,
        KMOD_CTRL,
        false,
        [&](void*) { undoRedoSystem.Undo(); }
    ));
    keybinds.registerBinding(Keybinding(
        "redo",
        "Redo",
        SDLK_y,
        KMOD_CTRL,
        false,
        [&](void*) { undoRedoSystem.Redo(); }
    ));

    // COPY / PASTE
    keybinds.registerBinding(Keybinding(
        "copy",
        "Copy",
        SDLK_c,
        KMOD_CTRL,
        true,
        [&](void*) { copySelection(); }
    ));
    keybinds.registerBinding(Keybinding(
        "paste",
        "Paste",
        SDLK_v,
        KMOD_CTRL,
        true,
        [&](void*) { pasteSelection(); }
    ));
    keybinds.registerBinding(Keybinding(
        "cut",
        "Cut",
        SDLK_x,
        KMOD_CTRL,
        true,
        [&](void*) { cutSelection(); }
    ));
    keybinds.registerBinding(Keybinding(
        "select_all",
        "Select all",
        SDLK_a,
        KMOD_CTRL,
        true,
        [&](void*) { LoadedFunscript->SelectAll(); }
    ));

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
    keybinds.registerBinding(Keybinding(
        "move_actions_left_snapped",
        "Move actions left with snapping",
        SDLK_LEFT,
        KMOD_CTRL | KMOD_SHIFT,
        false,
        [&](void*) {
            move_actions_horizontal_with_video(-player.getFrameTimeMs());
        }
    ));
    keybinds.registerBinding(Keybinding(
        "move_actions_right_snapped",
        "Move actions right with snapping",
        SDLK_RIGHT,
        KMOD_CTRL | KMOD_SHIFT,
        false,
        [&](void*) {
            move_actions_horizontal_with_video(player.getFrameTimeMs());
        }
    ));

    keybinds.registerBinding(Keybinding(
        "move_actions_left",
        "Move actions left",
        SDLK_LEFT,
        KMOD_SHIFT,
        false,
        [&](void*) {
            move_actions_horizontal(-player.getFrameTimeMs());
        }
    ));

    keybinds.registerBinding(Keybinding(
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
    keybinds.registerBinding(Keybinding(
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
                    undoRedoSystem.Snapshot("Actions moved");
                    FunscriptAction moved(closest->at, closest->pos + 1);
                    LoadedFunscript->EditAction(*closest, moved);
                }
            }
        }
    ));
    keybinds.registerBinding(Keybinding(
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
                    undoRedoSystem.Snapshot("Actions moved");
                    FunscriptAction moved(closest->at, closest->pos - 1);
                    LoadedFunscript->EditAction(*closest, moved);
                }
            }
        }
    ));

    // FUNCTIONS
    keybinds.registerBinding(Keybinding(
        "equalize_actions",
        "Equalize actions",
        SDLK_UNKNOWN,
        0,
        true,
        [&](void*) {
            equalizeSelection();
        }
    ));
    keybinds.registerBinding(Keybinding(
        "invert_actions",
        "Invert actions",
        SDLK_UNKNOWN,
        0,
        true,
        [&](void*) {
            invertSelection();
        }
    ));

    // SAVE
    keybinds.registerBinding(Keybinding(
        "save",
        "Save",
        SDLK_s,
        KMOD_CTRL,
        true,
        [&](void*) { saveScript(); }
    ));

    // FRAME CONTROL
    keybinds.registerBinding(Keybinding(
        "prev_frame",
        "Previous frame",
        SDLK_LEFT,
        0,
        false,
        [&](void*) { player.previousFrame(); }
    ));
    keybinds.registerBinding(Keybinding(
        "next_frame",
        "Next frame",
        SDLK_RIGHT,
        0,
        false,
        [&](void*) { player.nextFrame(); }
    ));

    // JUMP BETWEEN ACTIONS
    keybinds.registerBinding(Keybinding(
        "prev_action",
        "Previous action",
        SDLK_DOWN,
        0,
        false,
        [&](void*) {
            auto action = LoadedFunscript->GetPreviousActionBehind(player.getCurrentPositionMs() - player.getFrameTimeMs());
            if (action != nullptr) player.setPosition(action->at);
        }
    ));
    keybinds.registerBinding(Keybinding(
        "next_action",
        "Next action",
        SDLK_UP,
        0,
        false,
        [&](void*) {
            auto action = LoadedFunscript->GetNextActionAhead(player.getCurrentPositionMs() + player.getFrameTimeMs());
            if (action != nullptr) player.setPosition(action->at);
        }
    ));

    // PLAY / PAUSE
    keybinds.registerBinding(Keybinding(
        "toggle_play",
        "Play / Pause",
        SDLK_SPACE,
        0,
        true,
        [&](void*) { player.togglePlay(); }
    ));
    // PLAYBACK SPEED
    keybinds.registerBinding(Keybinding(
        "decrement_speed",
        "Playbackspeed -25%",
        SDLK_KP_MINUS,
        0,
        true,
        [&](void*) { player.addSpeed(-0.25); }
    ));
    keybinds.registerBinding(Keybinding(
        "increment_speed",
        "Playbackspeed +25%",
        SDLK_KP_PLUS,
        0,
        true,
        [&](void*) { player.addSpeed(0.25); }
    ));


    // DELETE ACTION
    keybinds.registerBinding(Keybinding(
        "remove_action",
        "Remove action",
        SDLK_DELETE,
        0,
        true,
        [&](void*) { removeAction(); }
    ));

    //ADD ACTIONS
    keybinds.registerBinding(Keybinding(
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

        keybinds.registerBinding(Keybinding(
            id,
            ss.str(),
            SDLK_KP_1 + i - 1,
            0,
            true,
            [&, i](void*) { addEditAction(i * 10); }
        ));
    }
    keybinds.registerBinding(Keybinding(
        "action 100",
        "Action at 100",
        SDLK_KP_DIVIDE,
        0,
        true,
        [&](void*) { addEditAction(100); }
    ));

    // FULLSCREEN
    keybinds.registerBinding(Keybinding(
        "fullscreen_toggle",
        "Toggle fullscreen",
        SDLK_F10,
        0,
        true,
        [&](void*) { Fullscreen = !Fullscreen; SetFullscreen(Fullscreen); }
    ));

    // SCREENSHOT VIDEO
    keybinds.registerBinding(Keybinding(
        "save_frame_as_image",
        "Save frame as image",
        SDLK_F2,
        0,
        true,
        [&](void*) { player.saveFrameToImage(settings->data().screenshot_dir); }
    ));
}



void OpenFunscripter::new_frame()
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

void OpenFunscripter::render()
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

void OpenFunscripter::process_events()
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
        events.PushEvent(event);
    }
}

void OpenFunscripter::FunscriptChanged(SDL_Event& ev)
{
    updateTimelineGradient = true;
}

void OpenFunscripter::FunscriptActionClicked(SDL_Event& ev)
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

void OpenFunscripter::FileDialogOpenEvent(SDL_Event& ev)
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

void OpenFunscripter::FileDialogSaveEvent(SDL_Event& ev) {
    auto& result = *static_cast<std::string*>(ev.user.data1);
    if (!result.empty())
    {
        saveScript(result.c_str());
        std::filesystem::path dir(result);
        dir.remove_filename();
        settings->data().last_path = dir.string();
    }
}

void OpenFunscripter::DragNDrop(SDL_Event& ev)
{
    openFile(ev.drop.file);
    SDL_free(ev.drop.file);
}

void OpenFunscripter::MpvVideoLoaded(SDL_Event& ev)
{
    LoadedFunscript->metadata.original_total_duration_ms = player.getDuration() * 1000.0;
}

void OpenFunscripter::update() {
    LoadedFunscript->update();
    rawInput.update();
}

int OpenFunscripter::run()
{
    while (!exit_app) {
        update();
        process_events();
        new_frame();
        {
            // IMGUI HERE
            CreateDockspace();
            ShowUndoRedoHistory(&ShowHistory);
            simulator.ShowSimulator(&settings->data().show_simulator);
            ShowStatisticsWindow(&ShowStatistics);
            if (ShowMetadataEditorWindow(&ShowMetadataEditor)) { saveScript(); }
            player.DrawVideoPlayer(NULL);
            scripting.DrawScriptingMode(NULL);

            if (keybinds.ShowBindingWindow()) {
                settings->saveKeybinds(keybinds.getBindings());
            }

            if (settings->ShowPreferenceWindow()) {
                settings->saveSettings();
            }

            if (player.isLoaded()) {
                ImGui::Begin("Video Controls");
                {
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
                }

                static bool mute = false;
                ImGui::Columns(2, 0, false);
                if (ImGui::Checkbox(mute ? ICON_VOLUME_OFF : ICON_VOLUME_UP, &mute)) {
                    if (mute)
                        player.setVolume(0.0f);
                    else
                        player.setVolume(player.volume);
                }
                ImGui::SetColumnWidth(0, ImGui::GetItemRectSize().x + 10);
                ImGui::NextColumn();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##Volume", &player.volume, 0.0f, 1.0f)) {
                    player.setVolume(player.volume);
                    if (player.volume > 0.0f)
                        mute = false;
                }
                ImGui::NextColumn();




                ImGui::End();
                ImGui::Begin("Time");

                static float actualPlaybackSpeed = 1.0f;
                {
                    const double speedCalcUpdateFrequency = 1.0;
                    static uint32_t start_time = SDL_GetTicks();
                    static float lastPlayerPosition = 0.0f;
                    if (!player.isPaused()) {
                        if ((SDL_GetTicks() - start_time)/1000.0f >= speedCalcUpdateFrequency) {
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

                ImGui::Columns(6, 0, false);
                {               
                    // format total duration
                    // this doesn't need to be done every frame
                    Util::FormatTime(tmp_buf[1], sizeof(tmp_buf[1]), player.getDuration(), true);

                    double time_seconds = player.getCurrentPositionSeconds();
                    Util::FormatTime(tmp_buf[0], sizeof(tmp_buf[0]), time_seconds, true);
                    ImGui::Text(" %s / %s (x%.03f)", tmp_buf[0], tmp_buf[1], actualPlaybackSpeed); 
                    ImGui::NextColumn();
                }

                auto& style = ImGui::GetStyle();
                ImGui::SetColumnWidth(0, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                
                ImGui::Checkbox("Smooth", &player.smooth_scrolling);
                Util::Tooltip("Smooths out the scrolling of the script timeline.\nEspecially at low playback speeds or higher refresh rates than the video.");
                
                ImGui::SetColumnWidth(1, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                ImGui::NextColumn();

                if (ImGui::Button("1x", ImVec2(0, 0))) {
                    player.setSpeed(1.f);
                }
                ImGui::SetColumnWidth(2, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                ImGui::NextColumn();

                if (ImGui::Button("-25%", ImVec2(0, 0))) {
                    player.addSpeed(-0.25);
                }
                ImGui::SetColumnWidth(3, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                ImGui::NextColumn();

                if (ImGui::Button("+25%", ImVec2(0, 0))) {
                    player.addSpeed(0.25);
                }
                ImGui::SetColumnWidth(4, ImGui::GetItemRectSize().x + style.ItemSpacing.x);
                ImGui::NextColumn();

                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::SliderFloat("##Speed", &player.playbackSpeed, player.minPlaybackSpeed, player.maxPlaybackSpeed)) {
                    if (player.playbackSpeed != player.getSpeed()) {
                        player.setSpeed(player.playbackSpeed);
                    }
                }
                Util::Tooltip("Speed");

                ImGui::Columns(1, 0, false);

                float position = player.getPosition();
                if (DrawTimelineWidget("Timeline", &position)) {
                    player.setPosition(position);
                }

                scriptPositions.ShowScriptPositions(NULL, player.getCurrentPositionMs());
                ImGui::End();


                ImGui::Begin("Action Editor");
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

            if (DebugDemo) {
                ImGui::ShowDemoWindow(&DebugDemo);
            }

            if (DebugMetrics) {
                ImGui::ShowMetricsWindow(&DebugMetrics);
            }

        }
        render();
        SDL_GL_SwapWindow(window);
    }
	return 0;
}

void OpenFunscripter::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);   
    SDL_DestroyWindow(window);
    SDL_Quit();
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
        bool succ = player.openVideo(video_path);
        if (!succ) { LOGF_ERROR("Failed to open video: \"%s\"", video_path.c_str()); }
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
    settings->data().last_opened_video = video_path;
    settings->data().last_opened_script = funscript_path;
    settings->saveSettings();

    last_save_time = std::chrono::system_clock::now();

    return result;
}

void OpenFunscripter::updateTitle()
{
    std::stringstream ss;
    ss.str(std::string());
    
    ss << "OpenFunscripter " FUN_LATEST_GIT_TAG " - \"" << LoadedFunscript->current_path << "\"";
    SDL_SetWindowTitle(window, ss.str().c_str());
}

void OpenFunscripter::fireAlert(const std::string& msg)
{
    pfd::notify alert("OpenFunscripter", msg, pfd::icon::info);
    alert.ready(20);
}

void OpenFunscripter::saveScript(const char* path)
{
    LoadedFunscript->metadata.original_name = std::filesystem::path(LoadedFunscript->current_path)
        .replace_extension("")
        .filename()
        .string();
    LoadedFunscript->metadata.original_total_duration_ms = player.getDuration() * 1000.0;
    if (path == nullptr) {
        LoadedFunscript->save();
    }
    else {
        LoadedFunscript->save(path);
        updateTitle();
    }
    fireAlert("Script saved!");
    last_save_time = std::chrono::system_clock::now();
}

void OpenFunscripter::saveHeatmap(const char* path, int width, int height)
{
    SDL_Surface* surface;
    Uint32 rmask, gmask, bmask, amask;

    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;

    surface = SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask);
    if (surface == NULL) {
        LOGF_ERROR("SDL_CreateRGBSurface() failed: %s", SDL_GetError());
        return;
    }

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
    SDL_FreeSurface(surface);
}

void OpenFunscripter::removeAction(const FunscriptAction& action)
{
    undoRedoSystem.Snapshot("Remove action");
    LoadedFunscript->RemoveAction(action);
}

void OpenFunscripter::removeAction()
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

void OpenFunscripter::addEditAction(int pos)
{
    undoRedoSystem.Snapshot("Add/Edit Action");
    scripting.addEditAction(FunscriptAction(player.getCurrentPositionMs(), pos));
}

void OpenFunscripter::cutSelection()
{
    if (LoadedFunscript->HasSelection()) {
        copySelection();
        undoRedoSystem.Snapshot("Cut selection");
        LoadedFunscript->RemoveSelectedActions();
    }
}

void OpenFunscripter::copySelection()
{
    if (LoadedFunscript->HasSelection()) {
        CopiedSelection.clear();
        for (auto action : LoadedFunscript->Selection()) {
            CopiedSelection.emplace_back(action);
        }
    }
}

void OpenFunscripter::pasteSelection()
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

void OpenFunscripter::equalizeSelection()
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

void OpenFunscripter::invertSelection()
{
    if (!LoadedFunscript->HasSelection()) {
        undoRedoSystem.Snapshot("Invert actions");
        // same hack as above 
        auto closest = LoadedFunscript->GetClosestAction(player.getCurrentPositionMs());
        if (closest != nullptr) {
            auto behind = LoadedFunscript->GetPreviousActionBehind(closest->at);
            if (behind != nullptr) {
                auto front = LoadedFunscript->GetNextActionAhead(closest->at);
                if (front != nullptr) {
                    LoadedFunscript->SelectAction(*behind);
                    LoadedFunscript->SelectAction(*closest);
                    LoadedFunscript->SelectAction(*front);
                    LoadedFunscript->InvertSelection();
                    LoadedFunscript->ClearSelection();
                }
            }
        }
    }
    else if (LoadedFunscript->Selection().size() >= 3) {
        undoRedoSystem.Snapshot("Invert actions");
        LoadedFunscript->InvertSelection();
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
        for (auto& ext : app->SupportedVideoExtensions)
            ss << '*' << ext << ';';
        filters.emplace_back(std::string("Videos ( ") + ss.str() + " )");
        filters.emplace_back(ss.str());
        filters.emplace_back("Funscript ( .funscript )");
        filters.emplace_back("*.funscript");
 
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
        auto path = std::filesystem::path(app->settings->data().last_opened_script);
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


void OpenFunscripter::ShowMainMenuBar()
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

            if (ImGui::MenuItem("Save", BINDING_STRING("save"))) {
                saveScript();
            }
            if (ImGui::MenuItem("Save as...")) {
                showSaveFileDialog();
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Automatic rolling backup", NULL, &RollingBackup, false)) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Save frame as image", BINDING_STRING("save_frame_as_image")))
            { 
                player.saveFrameToImage(settings->data().screenshot_dir);
            }
            // this is awkward
            if (ImGui::MenuItem("Open screenshot directory", NULL, false,
#ifdef WIN32
                true
#else 
                false
#endif 
                )) {
                std::filesystem::path dir(settings->data().screenshot_dir);
                dir = std::filesystem::absolute(dir);
                char tmp_buf[1024];
                stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "explorer %s", dir.string().c_str());
                std::system(tmp_buf);
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
                stbsp_snprintf(buf, sizeof(buf), "%s_Heatmap.bmp", LoadedFunscript->metadata.original_name.c_str());
                std::filesystem::path heatmapPath(settings->data().screenshot_dir);
                std::filesystem::create_directories(heatmapPath);
                heatmapPath /= buf;
                saveHeatmap(heatmapPath.string().c_str(), heatmapWidth, heatmapHeight); 
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Undo", BINDING_STRING("undo"), false, !undoRedoSystem.UndoStack.empty())) {
                undoRedoSystem.Undo();
            }
            if (ImGui::MenuItem("Redo", BINDING_STRING("redo"), false, !undoRedoSystem.RedoStack.empty())) {
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
                            float newPos = Util::Clamp<float>(mark.at / (player.getDuration() * 1000.0), 0.0f, 1.0f);
                            player.setPosition(newPos);
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
            if (ImGui::MenuItem("Statistics", NULL, &ShowStatistics)) {}
            if (ImGui::MenuItem("Undo/Redo History", NULL, &ShowHistory)) {}
            if (ImGui::MenuItem("Simulator", NULL, &settings->data().show_simulator)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Metadata", NULL, &ShowMetadataEditor)) {}
            ImGui::Separator();

            if (ImGui::MenuItem("Draw Video", NULL, &settings->data().draw_video)) { settings->saveSettings(); }
            if (ImGui::MenuItem("Reset video position", NULL)) { player.resetTranslationAndZoom(); }
            ImGui::Combo("Video Mode", (int*)&player.activeMode, 
                "Full Video\0"
                "Left Pane\0"
                "Right Pane\0"
                "Top Pane\0"
                "Bottom Pane\0"
                "VR Mode\0"
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
            if (ImGui::MenuItem("Keybindings")) {
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

bool OpenFunscripter::ShowMetadataEditorWindow(bool* open)
{
    if (!*open) return false;
    bool save = false;
    auto& metadata = LoadedFunscript->metadata;
    ImGui::Begin("Metadata Editor", open, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
    ImGui::LabelText("Original name", "%s", metadata.original_name.c_str());
    Util::FormatTime(tmp_buf[0], sizeof(tmp_buf), metadata.original_total_duration_ms / 1000.f, true);
    ImGui::LabelText("Original duration", "%s", tmp_buf[0]);

    ImGui::InputText("Creator", &metadata.creator);
    ImGui::InputText("Url", &metadata.url);
    ImGui::InputText("Video url", &metadata.url_video);
    ImGui::InputTextMultiline("Comment", &metadata.comment);
    ImGui::Checkbox("Paid", &metadata.paid);
    
    static std::string newTag;
    ImGui::InputText("Tag", &newTag); ImGui::SameLine(); 
    if (ImGui::Button("Add", ImVec2(-1.f, 0.f))) { 
        Util::trim(newTag);
        if (!newTag.empty()) {
            metadata.tags.emplace_back(newTag); newTag.clear();
        }
    }
    
    auto& style = ImGui::GetStyle();

    auto availableWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Text("%s", "Tags"); ImGui::SameLine();

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

    static std::string newPerformer;
    ImGui::InputText("Performer", &newPerformer); ImGui::SameLine();
    if (ImGui::Button("Add##Performer", ImVec2(-1.f, 0.f))) {
        Util::trim(newPerformer);
        if (!newPerformer.empty()) {
            metadata.performers.emplace_back(newPerformer); newPerformer.clear(); 
        }
    }

    availableWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Text("%s", "Performers"); ImGui::SameLine();

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
    if (fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetWindowBordered(window, SDL_FALSE);
    }
    else {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowBordered(window, SDL_TRUE);
    }
}

void OpenFunscripter::CreateDockspace()
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
        ImGuiID dockspace_id = ImGui::GetID("MainAppDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    ShowMainMenuBar();

    ImGui::End();
}

void OpenFunscripter::ShowStatisticsWindow(bool* open)
{
    if (!*open) return;
    ImGui::Begin("Statistics", open, ImGuiWindowFlags_None);
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

void OpenFunscripter::ShowUndoRedoHistory(bool* open)
{
    if (*open) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(200, 200));
        ImGui::Begin("Undo/Redo History", open, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
        //for(auto it = undoRedoSystem.RedoStack.rbegin(); it != undoRedoSystem.RedoStack.rend(); it++) {
        ImGui::TextDisabled("Redo stack");
        // TODO: get rid of the string comparison but keep the counting
        for (auto it = undoRedoSystem.RedoStack.begin(); it != undoRedoSystem.RedoStack.end(); it++) {
            int count = 1;
            auto copy_it = it;
            while (++copy_it != undoRedoSystem.RedoStack.end() && copy_it->Message == it->Message) {
                count++;
            }
            it = copy_it - 1;

            ImGui::BulletText("%s (%d)", (*it).Message.c_str(), count);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Undo stack");
        for (auto it = undoRedoSystem.UndoStack.rbegin(); it != undoRedoSystem.UndoStack.rend(); it++) {
            int count = 1;
            auto copy_it = it;
            while (++copy_it != undoRedoSystem.UndoStack.rend() && copy_it->Message == it->Message) {
                count++;
            }
            it = copy_it - 1;

            ImGui::BulletText("%s (%d)", (*it).Message.c_str(), count);

        }
        ImGui::End();
    }
}

bool OpenFunscripter::DrawTimelineWidget(const char* label, float* position)
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
            double time_delta = time_seconds - player.getCurrentPositionSeconds();
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

    // this comes fairly close to what ScriptPlayer's heatmap looks like
    const float durationMs = player.getDuration() * 1000.0;
    const float kernel_size_ms = 5000.f;
    const float max_actions_in_kernel = 24.5f;

    float kernel_offset = 0.f;
    ImColor color(0.f, 0.f, 0.f, 1.f);
    do {
        int actions_in_kernel = 0;
        float kernel_start = kernel_offset;
        float kernel_end = kernel_offset + kernel_size_ms;

        if (kernel_offset < (LoadedFunscript->Actions().end() - 1)->at)
        {
            for (int i = 0; i < LoadedFunscript->Actions().size(); i++) {
                auto& action = LoadedFunscript->Actions()[i];
                if (action.at >= kernel_start && action.at <= kernel_end)
                    actions_in_kernel++;
                else if (action.at > kernel_end)
                    break;
            }
        }
        kernel_offset += kernel_size_ms;

        float actionsRelToMax = Util::Clamp((float)actions_in_kernel / max_actions_in_kernel, 0.0f, 1.0f);

        HeatMap.computeColorAt(actionsRelToMax, (float*)&color.Value);

        float markPos = (kernel_offset + (kernel_size_ms / 2.f) - kernel_size_ms) / durationMs;
        grad.addMark(markPos, color);
    } while (kernel_offset < durationMs);
    grad.refreshCache();
}
