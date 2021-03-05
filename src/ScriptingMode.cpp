#include "ScriptingMode.h"

#include "OpenFunscripter.h"
#include "OFS_Util.h"

#include "imgui.h"
#include "imgui_internal.h"

ScripingModeBaseImpl::ScripingModeBaseImpl()
{
}

inline Funscript& ScripingModeBaseImpl::ctx() {
    return OpenFunscripter::script();
}

void ScriptingMode::setup()
{
    setMode(ScriptingModeEnum::DEFAULT_MODE);
    setOverlay(ScriptingOverlayModes::FRAME);
}

void ScriptingMode::DrawScriptingMode(bool* open) noexcept
{
    OFS_PROFILE(__FUNCTION__);
	ImGui::Begin(ScriptingModeId, open);
    ImGui::PushItemWidth(-1);
    // ATTENTION: order needs to be the same as the enum
    if (ImGui::Combo("##Mode", (int*)&active_mode, 
        "Default\0"
        "Alternating\0"
        "Dynamic injection\0"
        "Recording\0"
        "\0")) {
        setMode(active_mode);
    }
    Util::Tooltip("Scripting mode");
    impl->DrawModeSettings();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();

    if (ImGui::Combo("##OverlayMode", (int*)&active_overlay,
        "Frame\0"
        "Tempo\0"
        "None\0"
        "\0")) {
        setOverlay(active_overlay);
    }
    Util::Tooltip("Scripting overlay");
    OpenFunscripter::ptr->scriptPositions.overlay->DrawSettings();
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();
    auto app = OpenFunscripter::ptr;
    ImGui::DragInt("Offset (ms)", &app->settings->data().action_insert_delay_ms);
    Util::Tooltip("Applies an offset to actions inserted while the video is playing.\n- : inserts earlier\n+ : inserts later");
    if (app->LoadedFunscripts.size() > 1) {
        ImGui::Checkbox("Mirror mode", &OpenFunscripter::ptr->settings->data().mirror_mode);
        Util::Tooltip("Mirrors add/edit/remove action across all loaded scripts.");
    }
    else {
        app->settings->data().mirror_mode = false;
    }
	ImGui::End();
}

void ScriptingMode::setMode(ScriptingModeEnum mode) noexcept
{
    active_mode = mode;
    switch (mode) {
    case ScriptingModeEnum::ALTERNATING:
    {
        impl = std::make_unique<AlternatingImpl>();
        break;
    }
    case ScriptingModeEnum::DYNAMIC_INJECTION:
    {
        impl = std::make_unique<DynamicInjectionImpl>();
        break;
    }
    case ScriptingModeEnum::RECORDING:
    {
        impl = std::make_unique<RecordingImpl>();
        break;
    }
    default:
    {
        impl = std::make_unique<DefaultModeImpl>();
        break;
    }
    }
}

void ScriptingMode::setOverlay(ScriptingOverlayModes mode) noexcept
{
    active_overlay = mode;
    auto timeline = &OpenFunscripter::ptr->scriptPositions;
    switch (mode)
    {
    case FRAME:
        OpenFunscripter::ptr->scriptPositions.overlay = std::make_unique<FrameOverlay>(timeline);
        break;
    case TEMPO:
        OpenFunscripter::ptr->scriptPositions.overlay = std::make_unique<TempoOverlay>(timeline);
        break;
    case EMPTY:
        OpenFunscripter::ptr->scriptPositions.overlay = std::make_unique<EmptyOverlay>(timeline);
        break;
    default:
        break;
    }
}

void ScriptingMode::addEditAction(FunscriptAction action) noexcept
{
    auto app = OpenFunscripter::ptr;
    if (!app->player->isPaused()) {
        // apply offset
        action.at += app->settings->data().action_insert_delay_ms;
    }
    auto ptr = OpenFunscripter::script().GetActionAtTime(action.at, app->player->getFrameTimeMs());
    if (ptr != nullptr) {
        app->ActiveFunscript()->EditAction(*ptr, FunscriptAction(ptr->at, action.pos));
    }
    else {
	    impl->addAction(action);
    }
}

void ScriptingMode::NextFrame() noexcept
{
    OpenFunscripter::ptr->scriptPositions.overlay->nextFrame();
}

void ScriptingMode::PreviousFrame() noexcept
{
    OpenFunscripter::ptr->scriptPositions.overlay->previousFrame();
}

void ScriptingMode::update() noexcept
{
    impl->update();
    OpenFunscripter::ptr->scriptPositions.overlay->update();
}

// dynamic top injection
void DynamicInjectionImpl::DrawModeSettings() noexcept
{
    ImGui::SliderFloat("##Target speed (units/s)", &target_speed, min_speed, max_speed);
    Util::Tooltip("Target speed (units/s)");
    target_speed = std::round(Util::Clamp(target_speed, min_speed, max_speed));

    ImGui::SliderFloat("##Up/Down speed bias", &direction_bias, -0.50f, 0.50f);
    Util::Tooltip("Up/Down speed bias");

    ImGui::Columns(2, 0, false);
    if (ImGui::RadioButton("Top", top_bottom_direction == 1)) {
        top_bottom_direction = 1;
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton("Bottom", top_bottom_direction == -1)) {
        top_bottom_direction = -1;
    }
    ImGui::NextColumn();
    ImGui::Columns(1);
}

// dynamic injection
void DynamicInjectionImpl::addAction(FunscriptAction action) noexcept
{
    auto previous = ctx().GetPreviousActionBehind(action.at);
    if (previous != nullptr) {
        int32_t inject_at = previous->at + ((action.at - previous->at) / 2) + (((action.at - previous->at) / 2) * direction_bias);

        int32_t inject_duration = inject_at - previous->at;
        int32_t inject_pos = Util::Clamp<int32_t>(previous->pos + (top_bottom_direction * (inject_duration / 1000.0) * target_speed), 0.0, 100.0);
        ctx().AddAction(FunscriptAction(inject_at, inject_pos));
    }
    ctx().AddAction(action);
}

// alternating
void AlternatingImpl::DrawModeSettings() noexcept
{
    ImGui::Checkbox("Fixed range", &fixed_range_enabled);
    if (fixed_range_enabled) {
        bool input_active = false;
        
        ImGui::LabelText("BottomLabel", "%s", "Fixed bottom");
        ImGui::InputInt("Fixed bottom", &fixed_bottom, 1, 100);
        input_active = input_active || ImGui::IsItemActive();
        
        ImGui::LabelText("TopLabel", "%s", "Fixed top");
        ImGui::InputInt("Fixed top", &fixed_top);
        input_active = input_active || ImGui::IsItemActive();

        fixed_bottom = Util::Clamp<int>(fixed_bottom, 0, 100);
        fixed_top = Util::Clamp<int>(fixed_top, 0, 100);
        
        if (fixed_bottom > fixed_top && !input_active)
        {
            // correct user error :^)
            auto tmp = fixed_bottom;
            fixed_bottom = fixed_top;
            fixed_top = tmp;
        }
    }
}

void AlternatingImpl::addAction(FunscriptAction action) noexcept
{
    auto previous = ctx().GetPreviousActionBehind(action.at);
    if (fixed_range_enabled) {
        if (previous != nullptr) {
            if (previous->pos >= fixed_top)
            {
                ctx().AddAction(FunscriptAction(action.at, fixed_bottom));
            }
            else {
                ctx().AddAction(FunscriptAction(action.at, fixed_top));
            }
        }
        else {
            if (std::abs(action.pos - fixed_bottom) > std::abs(action.pos - fixed_top))
                ctx().AddAction(FunscriptAction(action.at, fixed_top));
            else
                ctx().AddAction(FunscriptAction(action.at, fixed_bottom));
        }
        return;
    }

    if (previous != nullptr) {

        if (action.pos >= 50) {
            if (previous->pos - action.pos >= 0) {
                ctx().AddAction(FunscriptAction(action.at, 100 - action.pos));
            }
            else {
                ctx().AddAction(action);
            }
        }
        else {
            if (action.pos - previous->pos >= 0) {
                ctx().AddAction(FunscriptAction(action.at, 100 - action.pos));
            }
            else {
                ctx().AddAction(action);
            }
        }
        return;
    }
    ctx().AddAction(action);
}

// recording
RecordingImpl::RecordingImpl()
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &RecordingImpl::ControllerAxisMotion)); 
}

RecordingImpl::~RecordingImpl()
{
    auto app = OpenFunscripter::ptr;
    app->events->Unsubscribe(SDL_CONTROLLERAXISMOTION, this);
}

void RecordingImpl::ControllerAxisMotion(SDL_Event& ev)
{
    if (activeMode != RecordingMode::Controller) return;
    auto& axis = ev.caxis;
    const float range = (float)std::numeric_limits<int16_t>::max() - ControllerDeadzone;

    if (axis.value >= 0 && axis.value < ControllerDeadzone)
        axis.value = 0;
    else if (axis.value < 0 && axis.value > -ControllerDeadzone)
        axis.value = 0;
    else if (axis.value >= ControllerDeadzone)
        axis.value -= ControllerDeadzone;
    else if (axis.value <= ControllerDeadzone)
        axis.value += ControllerDeadzone;


    switch (axis.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        left_x = Util::Clamp(axis.value / range, -1.f, 1.f);
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        left_y = Util::Clamp(axis.value / range, -1.f, 1.f);
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        right_x = Util::Clamp(axis.value / range, -1.f, 1.f);
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        right_y = Util::Clamp(axis.value / range, -1.f, 1.f);
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        left_trigger = Util::Clamp(axis.value / range, -1.f, 1.f);
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        right_trigger = Util::Clamp(axis.value / range, -1.f, 1.f);
        break;
    }

    //float value = std::max(right_len, left_len);
    //value = std::max(value, left_trigger);
    //value = std::max(value, right_trigger);

    if (std::abs(right_y) > std::abs(left_y)) {
        float right_len = std::sqrt(/*(right_x * right_x) +*/ (right_y * right_y));
        if (right_y < 0.f) {
            // up
            value = right_len;
        }
        else {
            // down
            value = -right_len;
        }
    }
    else {
        float left_len = std::sqrt(/*(left_x * left_x) +*/ (left_y * left_y));
        if (left_y < 0.f) {
            // up
            value = left_len;
        }
        else {
            // down
            value = -left_len;
        }
    }
}

void RecordingImpl::DrawModeSettings() noexcept
{

    ImGui::Combo("Mode", (int*)&activeMode,
        "Mouse\0"
        "Controller\0"
        "\0");

    switch (activeMode) {
    case RecordingMode::Controller:
    {
        ImGui::TextUnformatted("Controller deadzone");
        ImGui::SliderInt("Deadzone", &ControllerDeadzone, 0, std::numeric_limits<int16_t>::max());
        ImGui::Checkbox("Center", &controllerCenter);
        if (controllerCenter) {
            currentPos = Util::Clamp<int32_t>(50.f + (50.f * value), 0, 100);
        }
        else {
            currentPos = Util::Clamp<int32_t>(100.f * std::abs(value), 0, 100);
        }
        break;
    }
    case RecordingMode::Mouse:
    {
        value = OpenFunscripter::ptr->simulator.getMouseValue();
        currentPos = Util::Clamp<int32_t>(50.f + (50.f * value), 0, 100);
        break;
    }
    }

    ImGui::Checkbox("Invert", &inverted); ImGui::SameLine(); ImGui::Checkbox("Record on play", &automaticRecording);
    if (inverted) { 
        currentPos = std::abs(currentPos - 100); 
    }
    ImGui::TextUnformatted("Position"); 
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::SliderInt("##Pos", &currentPos, 0, 100);
    ImGui::PopItemFlag();


    ImGui::Spacing();
    auto app = OpenFunscripter::ptr;
    bool playing = !app->player->isPaused();
    if (automaticRecording && playing && recordingActive != playing) {
        autoBackupTmp = app->AutoBackup;
        app->AutoBackup = false;
        recordingJustStarted = true;
    }
    else if (!playing && recordingActive) {
        recordingActive = false;
        app->AutoBackup = autoBackupTmp;
        recordingJustStopped = true;
    }

    if (recordingActive && playing) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
        ImGui::TextUnformatted("Recording active");
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        ImGui::TextUnformatted("Recording paused");
        ImGui::PopStyleColor();
    }   
}

void RecordingImpl::addAction(FunscriptAction action) noexcept
{
    // same as default
    ctx().AddAction(action);
}

void RecordingImpl::update() noexcept
{
    auto app = OpenFunscripter::ptr;
    if (recordingActive) {
        uint32_t frameEstimate = app->player->getCurrentFrameEstimate();
        app->scriptPositions.RecordingBuffer[frameEstimate] = std::move(FunscriptAction(app->player->getCurrentPositionMs(), currentPos));
        app->simulator.positionOverride = currentPos;
    }
    else if (recordingJustStarted) {
        recordingJustStarted = false;
        recordingActive = true;
        app->scriptPositions.RecordingBuffer.clear();
        app->scriptPositions.RecordingBuffer.resize(app->player->getTotalNumFrames());
    }
    else if (recordingJustStopped) {
        recordingJustStopped = false;

        int32_t offsetMs = app->settings->data().action_insert_delay_ms;
        if (app->settings->data().mirror_mode) {
            app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, true, app->ActiveFunscript().get());
            for (auto&& script : app->LoadedFunscripts) {
                for (auto&& action : app->scriptPositions.RecordingBuffer) {
                    if (action.at >= 0) 
                    {
                        action.at += offsetMs;
                        script->AddActionSafe(action);
                    }
                }
            }
        }
        else {
            app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, false, app->ActiveFunscript().get());
            for (auto&& action : app->scriptPositions.RecordingBuffer) {
                if (action.at >= 0) {
                    action.at += offsetMs;
                    ctx().AddActionSafe(action);
                }
            }
        }
        app->scriptPositions.RecordingBuffer.clear();
    }
}
