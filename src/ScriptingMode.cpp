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
    setMode(ScriptingModeEnum::DEFAULT_MODE);
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

void ScriptingMode::addEditAction(FunscriptAction action)
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
void DynamicInjectionImpl::addAction(FunscriptAction action)
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

void AlternatingImpl::addAction(FunscriptAction action)
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
    app->simulator.SimulateRawActions = false;
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

inline static double PerpendicularDistance(const FunscriptRawAction pt, const FunscriptRawAction lineStart, const FunscriptRawAction lineEnd)
{
    double dx = (double)lineEnd.at - lineStart.at;
    double dy = (double)lineEnd.pos - lineStart.pos;

    //Normalise
    double mag = std::sqrt(dx*dx + dy*dy);
    if (mag > 0.0)
    {
        dx /= mag; dy /= mag;
    }

    double pvx = (double)pt.at - lineStart.at;
    double pvy = (double)pt.pos - lineStart.pos;

    //Get dot product (project pv onto normalized direction)
    double pvdot = dx * pvx + dy * pvy;

    //Scale line direction vector
    double dsx = pvdot * dx;
    double dsy = pvdot * dy;

    //Subtract this from pv
    double ax = pvx - dsx;
    double ay = pvy - dsy;

    return std::sqrt(ax*ax + ay*ay);
}

inline static void RamerDouglasPeucker(const std::vector<FunscriptRawAction>& pointList, double epsilon, std::vector<FunscriptRawAction>& out)
{
    // Find the point with the maximum distance from line between start and end
    double dmax = 0.0;
    size_t index = 0;
    size_t end = pointList.size() - 1;
    for (size_t i = 1; i < end; i++)
    {
        double d = PerpendicularDistance(pointList[i], pointList[0], pointList[end]);
        if (d > dmax)
        {
            index = i;
            dmax = d;
        }
    }

    // If max distance is greater than epsilon, recursively simplify
    if (dmax > epsilon)
    {
        // Recursive call
        std::vector<FunscriptRawAction> recResults1;
        std::vector<FunscriptRawAction> recResults2;
        std::vector<FunscriptRawAction> firstLine(pointList.begin(), pointList.begin() + index + 1);
        std::vector<FunscriptRawAction> lastLine(pointList.begin() + index, pointList.end());
        RamerDouglasPeucker(firstLine, epsilon, recResults1);
        RamerDouglasPeucker(lastLine, epsilon, recResults2);

        // Build the result list
        out.assign(recResults1.begin(), recResults1.end() - 1);
        out.insert(out.end(), recResults2.begin(), recResults2.end());
    }
    else
    {
        //Just return start and end points
        out.clear();
        out.push_back(pointList[0]);
        out.push_back(pointList[end]);
    }
}


void RecordingImpl::DrawModeSettings()
{

    static float deadzone = (float)ControllerDeadzone / std::numeric_limits<int16_t>::max();
    ImGui::Combo("Mode", (int*)&activeMode,
        "Mouse\0"
        "Controller\0"
        "\0");

    switch (activeMode) {
    case RecordingMode::Controller:
    {
        ImGui::Text("%s", "Controller deadzone");
        ImGui::SliderFloat("Deadzone", &deadzone, 0.f, 0.5f);
        ControllerDeadzone = std::numeric_limits<int16_t>::max() * deadzone;
        break;
    }
    case RecordingMode::Mouse:
    {
        value = OpenFunscripter::ptr->simulator.getMouseValue();
        break;
    }
    }

    ImGui::Text("%s", "Position"); 
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::SliderInt("##Pos", &currentPos, 0, 100);
    ImGui::PopItemFlag();

    ImGui::Checkbox("Invert", &inverted); ImGui::SameLine(); ImGui::Checkbox("Record on play", &automaticRecording);
    currentPos = Util::Clamp<int32_t>(50.f + (50.f * value), 0, 100);
    if (inverted) { currentPos = std::abs(currentPos - 100); }

    ImGui::Spacing();
    auto app = OpenFunscripter::ptr;
    bool playing = !app->player.isPaused();
    if (automaticRecording && playing && recordingActive != playing) {
        recordingActive = true;
        app->simulator.SimulateRawActions = true;
        rollingBackupTmp = app->RollingBackup;
        app->RollingBackup = false;
        ctx().Raw().NewRecording(app->player.getTotalNumFrames());
    }
    else if (!playing && recordingActive) {
        recordingActive = false;
        app->RollingBackup = rollingBackupTmp;
        app->simulator.SimulateRawActions = false;
    }

    if (recordingActive && playing) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
        ImGui::Text("%s", "Recording active");
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        ImGui::Text("%s", "Recording paused");
        ImGui::PopStyleColor();
    }
    
    static bool OpenRecordingsWindow = true;
    if (ImGui::Button("Recordings", ImVec2(-1.f, 0.f))) { OpenRecordingsWindow = !OpenRecordingsWindow; }
    
    if (OpenRecordingsWindow) {
        static float epsilon = 0.f;
        static Funscript::FunscriptRawData::Recording GeneratedRecording;
        ImGui::Begin("Recordings", &OpenRecordingsWindow, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
        ImGui::Text("Total recordings: %ld", ctx().Raw().Recordings.size());
       
        if (ctx().Raw().Recordings.size() > 0 && GeneratedRecording.RawActions.size() == 0) {
            ImGui::NewLine();
            if (ImGui::Button("Delete selected (Can't be undone)", ImVec2(-1.f, 0.f))) {
                ctx().Raw().RemoveActiveRecording();
            }
            ImGui::NewLine();
            int count = 0;
            char tmp[32];
            stbsp_snprintf(tmp, sizeof(tmp), "Recording %d#", ctx().Raw().RecordingIdx);
            if (ImGui::BeginCombo("Selected", tmp)) {
                for (auto&& recording : ctx().Raw().Recordings) {
                    stbsp_snprintf(tmp, sizeof(tmp), "Recording %d#", count);
                    const bool is_selected = (ctx().Raw().RecordingIdx == count);

                    if (ImGui::Selectable(tmp, is_selected)) {
                        ctx().Raw().RecordingIdx = count;
                    }

                    if (ImGui::IsItemHovered()) {
                        ctx().Raw().RecordingIdx = count;
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                    count++;
                }
                ImGui::EndCombo();
            }

            ctx().Raw().RecordingIdx = Util::Clamp<int32_t>(ctx().Raw().RecordingIdx, 0, ctx().Raw().Recordings.size()-1);

            if (ImGui::Button("Generate actions from recording", ImVec2(-1.f, 0.f))) {
                app->undoRedoSystem.Snapshot("Generate actions");
                std::vector<FunscriptRawAction> simplified;
                GeneratedRecording.RawActions = ctx().Raw().Active().RawActions;
                RamerDouglasPeucker(GeneratedRecording.RawActions, epsilon, simplified);
                for (auto&& act : simplified) {
                    if (act.at >= 0) {
                        ctx().AddEditAction(FunscriptAction(act.at, act.pos), app->player.getFrameTimeMs()/4.f);
                    }
                }
            }
        }


        if (GeneratedRecording.RawActions.size() > 0) {
            ImGui::NewLine();
            ImGui::Text("%s", "Tweaking");
            ImGui::TextDisabled("%s", "This works by undoing the previous \"Generate actions\"!\nDo not do anything else till you \"Finalize\"");
            if (ImGui::DragFloat("Epsilon", &epsilon, 0.2f, 0.f, 200.f)) {
                epsilon = Util::Clamp<float>(epsilon, 0.f, 200.f);
                app->undoRedoSystem.Undo();
                app->undoRedoSystem.Snapshot("Generate actions");
                std::vector<FunscriptRawAction> simplified;
                RamerDouglasPeucker(GeneratedRecording.RawActions, epsilon, simplified);
                for (auto&& act : simplified) {
                    if (act.at >= 0) {
                        ctx().AddEditAction(FunscriptAction(act.at, act.pos), app->player.getFrameTimeMs()/4.f);
                    }
                }
            }
            if (ImGui::Button("Finalize", ImVec2(-1.f, 0.f))) {
                GeneratedRecording.RawActions.clear();
            }
        }

        ImGui::End();
    }
}

void RecordingImpl::addAction(FunscriptAction action)
{
    // same as default
    ctx().AddAction(action);
}

void RecordingImpl::update() noexcept
{
    auto app = OpenFunscripter::ptr;
    if (recordingActive) {
        ctx().AddActionRaw(app->player.getCurrentFrameEstimate(), app->player.getCurrentPositionMs(), currentPos, app->player.getFrameTimeMs());
    }
}