#include "ScriptingMode.h"

#include "OpenFunscripter.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "OFS_ImGui.h"
#include "OFS_Simulator3D.h"

ScripingModeBaseImpl::ScripingModeBaseImpl()
{
}

void ScripingModeBaseImpl::addEditAction(FunscriptAction action) noexcept
{
    auto app = OpenFunscripter::ptr;
    ctx().AddEditAction(action, app->player->getFrameTime());
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
    auto app = OpenFunscripter::ptr;
	ImGui::Begin(ScriptingModeId, open);
    ImGui::PushItemWidth(-1);
    // ATTENTION: order needs to be the same as the enum
    if (ImGui::Combo("##Mode", (int*)&activeMode, 
        "Default\0"
        "Alternating\0"
        "Dynamic injection\0"
        "Recording\0"
        "\0")) {
        setMode(activeMode);
    }
    OFS::Tooltip("Scripting mode");
    impl->DrawModeSettings();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();

    if (ImGui::Combo("##OverlayMode", (int*)&activeOverlay,
        "Frame\0"
        "Tempo\0"
        "None\0"
        "\0")) {
        setOverlay(activeOverlay);
    }
    OFS::Tooltip("Scripting overlay");
    app->scriptPositions.overlay->DrawSettings();
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();
    ImGui::DragInt("Offset (ms)", &app->settings->data().action_insert_delay_ms);
    OFS::Tooltip("Applies an offset to actions inserted while the video is playing.\n- : inserts earlier\n+ : inserts later");
    if (app->LoadedFunscripts().size() > 1) {
        ImGui::Checkbox("Mirror mode", &app->settings->data().mirror_mode);
        OFS::Tooltip("Mirrors add/edit/remove action across all loaded scripts.");
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
        activeMode = mode;
    }
    else {
        activeMode = ScriptingModeEnum::DEFAULT_MODE;
    }
    impl = modes[activeMode].get();
}

void ScriptingMode::setOverlay(ScriptingOverlayModes mode) noexcept
{
    activeOverlay = mode;
    auto app = OpenFunscripter::ptr;
    auto timeline = &app->scriptPositions;
    switch (mode)
    {
    case ScriptingOverlayModes::FRAME:
        app->scriptPositions.overlay = std::make_unique<FrameOverlay>(timeline);
        break;
    case ScriptingOverlayModes::TEMPO:
        app->scriptPositions.overlay = std::make_unique<TempoOverlay>(timeline);
        break;
    case ScriptingOverlayModes::EMPTY:
        app->scriptPositions.overlay = std::make_unique<EmptyOverlay>(timeline);
        break;
    default:
        break;
    }
}

void ScriptingMode::undo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    impl->undo();
}

void ScriptingMode::redo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    impl->redo();
}

void ScriptingMode::addEditAction(FunscriptAction action) noexcept
{
    auto app = OpenFunscripter::ptr;
    if (!app->player->isPaused()) {
        // apply offset
        action.atS += app->settings->data().action_insert_delay_ms / 1000.f;
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
    OFS_PROFILE(__FUNCTION__);
    impl->update();
    OpenFunscripter::ptr->scriptPositions.overlay->update();
}

// dynamic top injection
void DynamicInjectionImpl::DrawModeSettings() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImGui::SliderFloat("##Target speed (units/s)", &targetSpeed, MinSpeed, MaxSpeed, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    OFS::Tooltip("Target speed (units/s)");
    targetSpeed = std::round(Util::Clamp(targetSpeed, MinSpeed, MaxSpeed));

    ImGui::SliderFloat("##Up/Down speed bias", &directionBias, -0.9f, 0.9f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    OFS::Tooltip("Up/Down speed bias");

    ImGui::Columns(2, 0, false);
    if (ImGui::RadioButton("Top", topBottomDirection == 1)) {
        topBottomDirection = 1;
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton("Bottom", topBottomDirection == -1)) {
        topBottomDirection = -1;
    }
    ImGui::NextColumn();
    ImGui::Columns(1);
}

// dynamic injection
void DynamicInjectionImpl::addEditAction(FunscriptAction action) noexcept
{
    auto previous = ctx().GetPreviousActionBehind(action.atS);
    if (previous != nullptr) {
        auto injectAt = previous->atS + ((action.atS - previous->atS) / 2) + (((action.atS - previous->atS) / 2) * directionBias);
        auto inject_duration = injectAt - previous->atS;

        int32_t injectPos = Util::Clamp<int32_t>(previous->pos + (topBottomDirection * inject_duration * targetSpeed), 0, 100);
        ScripingModeBaseImpl::addEditAction(FunscriptAction(injectAt, injectPos));
    }
    ScripingModeBaseImpl::addEditAction(action);
}

// alternating
void AlternatingImpl::DrawModeSettings() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (contextSensitive) {
        auto behind = ctx().GetPreviousActionBehind(std::round(app->player->getCurrentPositionSecondsInterp()) - 0.001f);
        if (behind) {
            ImGui::TextDisabled("Next point: %s", behind->pos <= 50 ? "Top" : "Bottom");
        }
        else {
            ImGui::TextDisabled("Next point: Bottom");
        }
    }
    else {
        if (fixedRangeEnabled) {
            ImGui::TextDisabled("Next point is at %d.", nextPosition ? fixedBottom : fixedTop);
        }
        else {
            ImGui::TextDisabled("Next point is %s.", nextPosition ? "inverted" : "not inverted");
        }
    }
    ImGui::Checkbox("Fixed range", &fixedRangeEnabled);
    ImGui::Checkbox("Context sensitive", &contextSensitive);
    OFS::Tooltip("Alternates based on the previous action.");
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
    if (contextSensitive) {
        auto behind = ctx().GetPreviousActionBehind(action.atS - 0.001f);
        if (behind && behind->pos <= 50 && action.pos <= 50) {
            //Top
            action.pos = 100 - action.pos;
        }
        else if(behind && behind->pos > 50 && action.pos > 50) {
            //Bottom
            action.pos = 100 - action.pos;
        }
    }
    else {
        if (fixedRangeEnabled) {
            action.pos = nextPosition ? fixedBottom : fixedTop;
        }
        else {
            action.pos = nextPosition ? 100 - action.pos : action.pos;
        }
    }
    ScripingModeBaseImpl::addEditAction(action);
    if (!contextSensitive) {
        nextPosition = !nextPosition;
    }
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
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    uint32_t frameEstimate = app->player->getCurrentFrameEstimate();
    app->scriptPositions.RecordingBuffer[frameEstimate]
        = std::make_pair(FunscriptAction(app->player->getCurrentPositionSecondsInterp(), currentPosY), FunscriptAction());
    app->simulator.positionOverride = currentPosY;
}

inline void RecordingImpl::twoAxisRecording() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    uint32_t frameEstimate = app->player->getCurrentFrameEstimate();
    float atS = app->player->getCurrentPositionSecondsInterp();
    app->scriptPositions.RecordingBuffer[frameEstimate]
        = std::make_pair(FunscriptAction(atS, currentPosX), FunscriptAction(atS, 100 - currentPosY));
    app->sim3D->RollOverride = currentPosX;
    app->sim3D->PitchOverride = 100 - currentPosY;
}

inline void RecordingImpl::finishSingleAxisRecording() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    float offsetTime = app->settings->data().action_insert_delay_ms / 1000.f;
    if (app->settings->data().mirror_mode) {
        app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS);
        for (auto&& script : app->LoadedFunscripts()) {
            for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
                auto& action = actionP.first;
                if (action.pos >= 0) {
                    action.atS += offsetTime;
                    script->AddAction(action);
                }
            }
        }
    }
    else {
        app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, app->ActiveFunscript());
        for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
            auto& action = actionP.first;
            if (action.pos >= 0) {
                action.atS += offsetTime;
                ctx().AddAction(action);
            }
        }
    }
    app->scriptPositions.RecordingBuffer.clear();
}

inline void RecordingImpl::finishTwoAxisRecording() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    float offsetTime = app->settings->data().action_insert_delay_ms / 1000.f;
    app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS);
    int32_t rollIdx = app->sim3D->rollIndex;
    int32_t pitchIdx = app->sim3D->pitchIndex;
    if (rollIdx > 0 && rollIdx < app->LoadedFunscripts().size()) {
        auto& script = app->LoadedFunscripts()[rollIdx];
        for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
            auto& actionX = actionP.first;
            if (actionX.pos >= 0) {
                actionX.atS += offsetTime;
                script->AddAction(actionX);
            }
        }
    }
    if (pitchIdx > 0 && pitchIdx < app->LoadedFunscripts().size()) {
        auto& script = app->LoadedFunscripts()[pitchIdx];
        for (auto&& actionP : app->scriptPositions.RecordingBuffer) {
            auto& actionY = actionP.second;
            if (actionY.pos >= 0) {
                actionY.atS += offsetTime;
                script->AddAction(actionY);
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
    OFS_PROFILE(__FUNCTION__);
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
    OFS_PROFILE(__FUNCTION__);
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
                OFS::Tooltip("Recording pitch & roll at once.\nUsing Simulator 3D settings.\nOnly works with a controller.");
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
        autoBackupTmp = app->Status & OFS_Status::OFS_AutoBackup;
        app->Status &= ~(OFS_Status::OFS_AutoBackup);
        recordingJustStarted = true;
    }
    else if (!playing && recordingActive) {
        recordingActive = false;
        
        if (autoBackupTmp) {
            app->Status |= OFS_Status::OFS_AutoBackup;
        }
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
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (recordingActive) {
        if (twoAxesMode) { twoAxisRecording(); }
        else { singleAxisRecording(); }
    }
    else if (recordingJustStarted) {
        recordingJustStarted = false;
        recordingActive = true;
        app->scriptPositions.RecordingBuffer.clear();
        app->scriptPositions.RecordingBuffer.resize(app->player->getTotalNumFrames(),
            std::make_pair(FunscriptAction(), FunscriptAction()));
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
    OFS_PROFILE(__FUNCTION__);
    // this fixes a bug when the mode gets changed during a recording
    if (recordingActive) {
        recordingActive = false;
        recordingJustStopped = true;
        update();
    }
}
