#include "ScriptingMode.h"

#include "OpenFunscripter.h"
#include "OFS_Util.h"

#include "imgui.h"
#include "imgui_internal.h"

ScripingModeBaseImpl::ScripingModeBaseImpl()
{
}

void ScripingModeBaseImpl::addEditAction(FunscriptAction action) noexcept
{
    ctx().AddEditAction(action, OpenFunscripter::ptr->player->getFrameTimeMs());
}

inline Funscript& ScripingModeBaseImpl::ctx() {
    return OpenFunscripter::script();
}

void ScriptingMode::setup()
{
    modes[ScriptingModeEnum::DEFAULT_MODE] = std::make_unique<DefaultModeImpl>();
    modes[ScriptingModeEnum::ALTERNATING] = std::make_unique<AlternatingImpl>();
    modes[ScriptingModeEnum::RECORDING] = std::make_unique<RecordingImpl>();
    modes[ScriptingModeEnum::DYNAMIC_INJECTION] = std::make_unique<DynamicInjectionImpl>();

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
    if (impl) { impl->finish(); }
    if (mode >= ScriptingModeEnum::DEFAULT_MODE && mode < ScriptingModeEnum::COUNT) {
        active_mode = mode;
    }
    else {
        active_mode = ScriptingModeEnum::DEFAULT_MODE;
    }
    impl = modes[active_mode].get();
}

void ScriptingMode::setOverlay(ScriptingOverlayModes mode) noexcept
{
    active_overlay = mode;
    auto timeline = &OpenFunscripter::ptr->scriptPositions;
    switch (mode)
    {
    case ScriptingOverlayModes::FRAME:
        OpenFunscripter::ptr->scriptPositions.overlay = std::make_unique<FrameOverlay>(timeline);
        break;
    case ScriptingOverlayModes::TEMPO:
        OpenFunscripter::ptr->scriptPositions.overlay = std::make_unique<TempoOverlay>(timeline);
        break;
    case ScriptingOverlayModes::EMPTY:
        OpenFunscripter::ptr->scriptPositions.overlay = std::make_unique<EmptyOverlay>(timeline);
        break;
    default:
        break;
    }
}

void ScriptingMode::undo() noexcept
{
    impl->undo();
}

void ScriptingMode::redo() noexcept
{
    impl->redo();
}

void ScriptingMode::addEditAction(FunscriptAction action) noexcept
{
    auto app = OpenFunscripter::ptr;
    if (!app->player->isPaused()) {
        // apply offset
        action.at += app->settings->data().action_insert_delay_ms;
    }
	impl->addEditAction(action);
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
void DynamicInjectionImpl::addEditAction(FunscriptAction action) noexcept
{
    auto previous = ctx().GetPreviousActionBehind(action.at);
    if (previous != nullptr) {
        int32_t inject_at = previous->at + ((action.at - previous->at) / 2) + (((action.at - previous->at) / 2) * direction_bias);

        int32_t inject_duration = inject_at - previous->at;
        int32_t inject_pos = Util::Clamp<int32_t>(previous->pos + (top_bottom_direction * (inject_duration / 1000.0) * target_speed), 0.0, 100.0);
        ScripingModeBaseImpl::addEditAction(FunscriptAction(inject_at, inject_pos));
    }
    ScripingModeBaseImpl::addEditAction(action);
}

// alternating
void AlternatingImpl::DrawModeSettings() noexcept
{
    if (fixedRangeEnabled) {
        ImGui::TextDisabled("Next point is at %d.", nextPosition ? fixedBottom : fixedTop);
    }
    else {
        ImGui::TextDisabled("Next point is %s.", nextPosition ? "inverted" : "not inverted");
    }
    ImGui::Checkbox("Fixed range", &fixedRangeEnabled);
    if (fixedRangeEnabled) {
        bool inputActive = false;
        auto& style = ImGui::GetStyle();
        float availdWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x;

        ImGui::SetNextItemWidth(availdWidth/2.f);
        ImGui::InputInt("##Fixed bottom", &fixedBottom, 1, 100);
        inputActive = inputActive || ImGui::IsItemActive();
        
        ImGui::SameLine();

        ImGui::SetNextItemWidth(availdWidth / 2.f);
        ImGui::InputInt("##Fixed top", &fixedTop);
        inputActive = inputActive || ImGui::IsItemActive();

        fixedBottom = Util::Clamp<int>(fixedBottom, 0, 100);
        fixedTop = Util::Clamp<int>(fixedTop, 0, 100);
        
        if (fixedBottom > fixedTop && !inputActive) {
            // correct user error :^)
            auto tmp = fixedBottom;
            fixedBottom = fixedTop;
            fixedTop = tmp;
        }
    }
}

void AlternatingImpl::addEditAction(FunscriptAction action) noexcept
{
    if (fixedRangeEnabled) {
        action.pos = nextPosition ? fixedBottom : fixedTop;
    }
    else {
        action.pos = nextPosition ? 100 - action.pos : action.pos;
    }
    ScripingModeBaseImpl::addEditAction(action);
    nextPosition = !nextPosition;
}

void AlternatingImpl::undo() noexcept
{
    nextPosition = !nextPosition;
}

void AlternatingImpl::redo() noexcept
{
    nextPosition = !nextPosition;
}

inline void RecordingImpl::singleAxisRecording() noexcept
{
    auto app = OpenFunscripter::ptr;
    uint32_t frameEstimate = app->player->getCurrentFrameEstimate();
    app->scriptPositions.RecordingBuffer[frameEstimate]
        = std::move(std::make_pair(FunscriptAction(app->player->getCurrentPositionMs(), currentPosY), FunscriptAction()));
    app->simulator.positionOverride = currentPosY;
}

inline void RecordingImpl::twoAxisRecording() noexcept
{
    auto app = OpenFunscripter::ptr;
    uint32_t frameEstimate = app->player->getCurrentFrameEstimate();
    int32_t at = app->player->getCurrentPositionMs();
    app->scriptPositions.RecordingBuffer[frameEstimate]
        = std::move(std::make_pair(FunscriptAction(at, currentPosX), FunscriptAction(at, 100 - currentPosY)));
    app->sim3D->RollOverride = currentPosX;
    app->sim3D->PitchOverride = 100 - currentPosY;
}

inline void RecordingImpl::finishSingleAxisRecording() noexcept
{
    auto app = OpenFunscripter::ptr;
    int32_t offsetMs = app->settings->data().action_insert_delay_ms;
    if (app->settings->data().mirror_mode) {
        app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, true, app->ActiveFunscript().get());
        for (auto&& script : app->LoadedFunscripts) {
            for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
                auto& action = actionP.first;
                if (action.at >= 0) {
                    action.at += offsetMs;
                    script->AddActionSafe(action);
                }
            }
        }
    }
    else {
        app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, false, app->ActiveFunscript().get());
        for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
            auto& action = actionP.first;
            if (action.at >= 0) {
                action.at += offsetMs;
                ctx().AddActionSafe(action);
            }
        }
    }
    app->scriptPositions.RecordingBuffer.clear();
}

inline void RecordingImpl::finishTwoAxisRecording() noexcept
{
    auto app = OpenFunscripter::ptr;
    int32_t offsetMs = app->settings->data().action_insert_delay_ms;
    app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, true, nullptr);
    int32_t rollIdx = app->sim3D->rollIndex;
    int32_t pitchIdx = app->sim3D->pitchIndex;
    if (rollIdx > 0 && rollIdx < app->LoadedFunscripts.size()) {
        auto& script = app->LoadedFunscripts[rollIdx];
        for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
            auto& actionX = actionP.first;
            if (actionX.at >= 0) {
                actionX.at += offsetMs;
                script->AddActionSafe(actionX);
            }
        }
    }
    if (pitchIdx > 0 && pitchIdx < app->LoadedFunscripts.size()) {
        auto& script = app->LoadedFunscripts[pitchIdx];
        for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
            auto& actionY = actionP.second;
            if (actionY.at >= 0) {
                actionY.at += offsetMs;
                script->AddActionSafe(actionY);
            }
        }
    }
    app->scriptPositions.RecordingBuffer.clear();
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


    if (std::abs(right_x) > std::abs(left_x)) {
        valueX = right_x;
    }
    else {
        valueX = left_x;
    }

    if (std::abs(right_y) > std::abs(left_y)) {
        valueY = -right_y;
    }
    else {
        valueY = -left_y;
    }
}

void RecordingImpl::DrawModeSettings() noexcept
{
    auto app = OpenFunscripter::ptr;

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
            currentPosX = Util::Clamp<int32_t>(50.f + (50.f * valueX), 0, 100);
            currentPosY = Util::Clamp<int32_t>(50.f + (50.f * valueY), 0, 100);
        }
        else {
            currentPosX = Util::Clamp<int32_t>(100.f * std::abs(valueX), 0, 100);
            currentPosY = Util::Clamp<int32_t>(100.f * std::abs(valueY), 0, 100);
        }
        if (!recordingActive) {
            ImGui::SameLine();
            ImGui::Checkbox("Two axes", &twoAxesMode);
            Util::Tooltip("Recording pitch & roll at once.\nUsing Simulator 3D settings.\nOnly works with a controller.");
        }
        break;
    }
    case RecordingMode::Mouse:
    {
        twoAxesMode = false;
        valueY = app->simulator.getMouseValue();
        currentPosY = Util::Clamp<int32_t>(50.f + (50.f * valueY), 0, 100);
        break;
    }
    }

    ImGui::Checkbox("Invert", &inverted); ImGui::SameLine(); ImGui::Checkbox("Record on play", &automaticRecording);
    if (inverted) { 
        currentPosX = 100 - currentPosX;
        currentPosY = 100 - currentPosY; 
    }
    if (twoAxesMode) {
        ImGui::TextUnformatted("X / Y");
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::SliderInt("##PosX", &currentPosX, 0, 100);
        ImGui::SliderInt("##PosY", &currentPosY, 0, 100);
        ImGui::PopItemFlag();
    }
    else
    {
        ImGui::TextUnformatted("Position"); 
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::SliderInt("##Pos", &currentPosY, 0, 100);
        ImGui::PopItemFlag();
    }


    ImGui::Spacing();
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

void RecordingImpl::update() noexcept
{
    auto app = OpenFunscripter::ptr;
    if (recordingActive) {
        if (twoAxesMode) { twoAxisRecording(); }
        else { singleAxisRecording(); }
    }
    else if (recordingJustStarted) {
        recordingJustStarted = false;
        recordingActive = true;
        app->scriptPositions.RecordingBuffer.clear();
        app->scriptPositions.RecordingBuffer.resize(app->player->getTotalNumFrames());
    }
    else if (recordingJustStopped) {
        recordingJustStopped = false;
        if (twoAxesMode) { finishTwoAxisRecording(); }
        else { finishSingleAxisRecording(); }
        automaticRecording = false;
    }
}

void RecordingImpl::finish() noexcept
{
    // this fixes a bug when the mode gets changed during a recording
    if (recordingActive) {
        recordingActive = false;
        recordingJustStopped = true;
        update();
    }
}
