#include "ScriptingMode.h"

#include "OpenFunscripter.h"

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
    ctx = OpenFunscripter::ptr;
    setMode(DEFAULT_MODE);
}

void ScriptingMode::DrawScriptingMode(bool* open)
{
	ImGui::Begin("Scripting Mode", open);
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
    ImGui::PopItemWidth();
	ImGui::End();
}

void ScriptingMode::setMode(ScriptingModeEnum mode)
{
    active_mode = mode;
    switch (mode) {
    case ALTERNATING:
    {
        impl = std::make_unique<AlternatingImpl>();
        break;
    }
    case DYNAMIC_INJECTION:
    {
        impl = std::make_unique<DynamicInjectionImpl>();
        break;
    }
    case RECORDING:
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

void ScriptingMode::addEditAction(const FunscriptAction& action)
{
    auto ptr = ctx->LoadedFunscript->GetActionAtTime(action.at, ctx->player.getFrameTimeMs());
    if (ptr != nullptr) {
        ctx->LoadedFunscript->EditAction(*ptr, FunscriptAction(ptr->at, action.pos));
    }
    else {
	    impl->addAction(action);
    }
}

// dynamic top injection
void DynamicInjectionImpl::DrawModeSettings()
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

}

// dynamic injection
void DynamicInjectionImpl::addAction(const FunscriptAction& action)
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
void AlternatingImpl::DrawModeSettings()
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

void AlternatingImpl::addAction(const FunscriptAction& action)
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
    app->events.Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &RecordingImpl::ControllerAxisMotion));
    app->events.Subscribe(SDL_CONTROLLERBUTTONUP, EVENT_SYSTEM_BIND(this, &RecordingImpl::ControllerButtonUp));
    app->events.Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &RecordingImpl::ControllerButtonDown));
    app->events.Subscribe(SDL_MOUSEMOTION, EVENT_SYSTEM_BIND(this, &RecordingImpl::MouseMovement));
}

RecordingImpl::~RecordingImpl()
{
    auto app = OpenFunscripter::ptr;
    app->events.Unsubscribe(SDL_CONTROLLERAXISMOTION, this);
    app->events.Unsubscribe(SDL_CONTROLLERBUTTONUP, this);
    app->events.Unsubscribe(SDL_CONTROLLERBUTTONDOWN, this);
    app->events.Unsubscribe(SDL_MOUSEMOTION, this);
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

void RecordingImpl::ControllerButtonUp(SDL_Event& ev)
{
}

void RecordingImpl::ControllerButtonDown(SDL_Event& ev)
{
}

void RecordingImpl::MouseMovement(SDL_Event& ev)
{
    if (activeMode != RecordingMode::Mouse) return; 
    SDL_MouseMotionEvent& motion = ev.motion;
    auto app = OpenFunscripter::ptr;
    // there's alot of indirection here
    auto simP1 = app->settings->data().simulator->P1;
    auto simP2 = app->settings->data().simulator->P2;

    auto [top_y, bottom_y] = std::minmax(simP1.y, simP2.y);


    value = motion.y - bottom_y;
    value /= (top_y - bottom_y);
    value = Util::Clamp(value, 0.f, 1.f);

    value = ((value - 0.f) / (1.f - 0.f)) * (1.f - -1.f) + -1.f;
}

void RecordingImpl::DrawModeSettings()
{

    static float deadzone = (float)ControllerDeadzone / std::numeric_limits<int16_t>::max();
    ImGui::Combo("Mode", (int*)&activeMode,
        "Mouse\0"
        "Controller\0"
        "\0");

    if (activeMode == RecordingMode::Controller) {
        ImGui::Text("%s", "Controller deadzone");
        ImGui::SliderFloat("Deadzone", &deadzone, 0.f, 0.5f);
        ControllerDeadzone = std::numeric_limits<int16_t>::max() * deadzone;
    }

    ImGui::Text("%s", "Position"); 
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::SliderInt("##Pos", &currentPos, 0, 100);
    ImGui::PopItemFlag();

    ImGui::Checkbox("Invert", &inverted);
    
    currentPos = Util::Clamp<int32_t>(50.f + (50.f * value), 0, 100);
    if (inverted) { currentPos = std::abs(currentPos - 100); }

    ImGui::Spacing();
    auto app = OpenFunscripter::ptr;
    bool playing = !app->player.isPaused();
    if (playing && recordingActive != playing) {
        app->undoRedoSystem.Snapshot("Recording");
    }
    recordingActive = playing;
    if (recordingActive) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
        ImGui::Text("%s", "Recording active");
        ImGui::PopStyleColor();
        
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        ImGui::Text("%s", "Recording paused");
        ImGui::PopStyleColor();
    }
}

void RecordingImpl::addAction(const FunscriptAction& action)
{
    // same as default
    ctx().AddAction(action);
}

void RecordingImpl::update() noexcept
{
    auto app = OpenFunscripter::ptr;
    if (recordingActive) {
        FunscriptAction act(app->player.getCurrentPositionMs(), currentPos);
        ctx().AddActionRaw(act);
    }
}