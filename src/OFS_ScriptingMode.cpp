#include "OpenFunscripter.h"
#include "OFS_ScriptingMode.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_Localization.h"

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

inline static const char* ScriptingModeToString(ScriptingModeEnum mode) noexcept
{
    switch (mode)
    {
        case ScriptingModeEnum::DEFAULT_MODE: return TR(DEFAULT_MODE);
        case ScriptingModeEnum::ALTERNATING: return TR(ALTERNATING_MODE);
        case ScriptingModeEnum::DYNAMIC_INJECTION: return TR(DYNAMIC_INJECTION_MODE);
        case ScriptingModeEnum::RECORDING: return TR(RECORDING_MODE);
    }
    return "";
}

inline static const char* OverlayModeToString(ScriptingOverlayModes mode) noexcept
{
    switch (mode)
    {
        case ScriptingOverlayModes::FRAME: return TR(FRAME_OVERLAY);
        case ScriptingOverlayModes::TEMPO: return TR(TEMPO_OVERLAY);
        case ScriptingOverlayModes::EMPTY: return TR(EMPTY_OVERLAY);
    }
    return "";
}


void ScriptingMode::DrawScriptingMode(bool* open) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
	ImGui::Begin(TR_ID(WindowId, Tr::MODE), open);
    ImGui::PushItemWidth(-1);

    if(ImGui::BeginCombo("##Mode", ScriptingModeToString(activeMode), ImGuiComboFlags_None))
    {
        if(ImGui::Selectable(TR_ID("DEFAULT", Tr::DEFAULT_MODE), activeMode == ScriptingModeEnum::DEFAULT_MODE))
        {
            setMode(ScriptingModeEnum::DEFAULT_MODE);
        }
        if(ImGui::Selectable(TR_ID("ALTERNATING", Tr::ALTERNATING_MODE), activeMode == ScriptingModeEnum::ALTERNATING))
        {
            setMode(ScriptingModeEnum::ALTERNATING);
        }
        if(ImGui::Selectable(TR_ID("DYNAMIC_INJECTION", Tr::DYNAMIC_INJECTION_MODE), activeMode == ScriptingModeEnum::DYNAMIC_INJECTION))
        {
            setMode(ScriptingModeEnum::DYNAMIC_INJECTION);
        }
        if(ImGui::Selectable(TR_ID("RECORDING", Tr::RECORDING_MODE), activeMode == ScriptingModeEnum::RECORDING))
        {
            setMode(ScriptingModeEnum::RECORDING);
        }
        ImGui::EndCombo();
    }
    OFS::Tooltip(TR(SCRIPTING_MODE));
    impl->DrawModeSettings();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();

    if(ImGui::BeginCombo("##OverlayMode", OverlayModeToString(activeOverlay), ImGuiComboFlags_None))
    {
        if(ImGui::Selectable(TR_ID("FRAME_OVERLAY", Tr::FRAME_OVERLAY), activeOverlay == ScriptingOverlayModes::FRAME))
        {
            setOverlay(ScriptingOverlayModes::FRAME);
        }
        if(ImGui::Selectable(TR_ID("TEMPO_OVERLAY", Tr::TEMPO_OVERLAY), activeOverlay == ScriptingOverlayModes::TEMPO))
        {
            setOverlay(ScriptingOverlayModes::TEMPO);
        }
        if(ImGui::Selectable(TR_ID("EMPTY_OVERLAY", Tr::EMPTY_OVERLAY), activeOverlay == ScriptingOverlayModes::EMPTY))
        {
            setOverlay(ScriptingOverlayModes::EMPTY);
        }
        ImGui::EndCombo();
    }

    OFS::Tooltip(TR(SCRIPTING_OVERLAY));
    app->scriptTimeline.overlay->DrawSettings();
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();
    ImGui::DragInt(TR(OFFSET_MS), &app->settings->data().action_insert_delay_ms);
    OFS::Tooltip(TR(OFFSET_TOOLTIP));
    if (app->LoadedFunscripts().size() > 1) {
        ImGui::Checkbox(TR(MIRROR_MODE), &app->settings->data().mirror_mode);
        OFS::Tooltip(TR(MIRROR_MODE_TOOLTIP));
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
    auto timeline = &app->scriptTimeline;
    switch (mode)
    {
    case ScriptingOverlayModes::FRAME:
        app->scriptTimeline.overlay = std::make_unique<FrameOverlay>(timeline);
        break;
    case ScriptingOverlayModes::TEMPO:
        app->scriptTimeline.overlay = std::make_unique<TempoOverlay>(timeline);
        break;
    case ScriptingOverlayModes::EMPTY:
        app->scriptTimeline.overlay = std::make_unique<EmptyOverlay>(timeline);
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
    OpenFunscripter::ptr->scriptTimeline.overlay->nextFrame();
}

void ScriptingMode::PreviousFrame() noexcept
{
    OpenFunscripter::ptr->scriptTimeline.overlay->previousFrame();
}

void ScriptingMode::update() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    impl->update();
    OpenFunscripter::ptr->scriptTimeline.overlay->update();
}

// dynamic top injection
void DynamicInjectionImpl::DrawModeSettings() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImGui::SliderFloat("##Target speed (units/s)", &targetSpeed, MinSpeed, MaxSpeed, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    OFS::Tooltip(TR(DI_TARGET_SPEED));
    targetSpeed = std::round(Util::Clamp(targetSpeed, MinSpeed, MaxSpeed));

    ImGui::SliderFloat("##Up/Down speed bias", &directionBias, -0.9f, 0.9f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    OFS::Tooltip(TR(DI_UP_DOWN_BIAS));

    ImGui::Columns(2, 0, false);
    if (ImGui::RadioButton(TR(TOP), topBottomDirection == 1)) {
        topBottomDirection = 1;
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton(TR(BOTTOM), topBottomDirection == -1)) {
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
            ImGui::TextDisabled("%s: %s", TR(NEXT_POINT), behind->pos <= 50 ? TR(TOP) : TR(BOTTOM));
        }
        else {
            ImGui::TextDisabled("%s: %s", TR(NEXT_POINT), TR(BOTTOM));
        }
    }
    else {
        if (fixedRangeEnabled) {
            ImGui::TextDisabled(TR(NEXT_POINT_AT_FMT), nextPosition ? fixedBottom : fixedTop);
        }
        else {
            ImGui::TextDisabled(TR(NEXT_POINT_IS_FMT), nextPosition ? TR(INVERTED) : TR(NOT_INVERTED));
        }
    }
    ImGui::Checkbox(TR(FIXED_RANGE), &fixedRangeEnabled);
    ImGui::Checkbox(TR(CONTEXT_SENSITIVE), &contextSensitive);
    OFS::Tooltip(TR(CONTEXT_SENSITIVE_TOOLTIP));
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
    app->scriptTimeline.RecordingBuffer[frameEstimate]
        = std::make_pair(FunscriptAction(app->player->getCurrentPositionSecondsInterp(), currentPosY), FunscriptAction());
    app->simulator.positionOverride = currentPosY;
}

inline void RecordingImpl::twoAxisRecording() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    uint32_t frameEstimate = app->player->getCurrentFrameEstimate();
    float atS = app->player->getCurrentPositionSecondsInterp();
    app->scriptTimeline.RecordingBuffer[frameEstimate]
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
            for (auto&& actionP : app->scriptTimeline.RecordingBuffer) {
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
        for (auto&& actionP : app->scriptTimeline.RecordingBuffer) {
            auto& action = actionP.first;
            if (action.pos >= 0) {
                action.atS += offsetTime;
                ctx().AddAction(action);
            }
        }
    }
    app->scriptTimeline.RecordingBuffer.clear();
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
        for (auto&& actionP : app->scriptTimeline.RecordingBuffer) {
            auto& actionX = actionP.first;
            if (actionX.pos >= 0) {
                actionX.atS += offsetTime;
                script->AddAction(actionX);
            }
        }
    }
    if (pitchIdx > 0 && pitchIdx < app->LoadedFunscripts().size()) {
        auto& script = app->LoadedFunscripts()[pitchIdx];
        for (auto&& actionP : app->scriptTimeline.RecordingBuffer) {
            auto& actionY = actionP.second;
            if (actionY.pos >= 0) {
                actionY.atS += offsetTime;
                script->AddAction(actionY);
            }
        }
    }
    app->scriptTimeline.RecordingBuffer.clear();
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

inline static const char* RecordingModeToString(RecordingImpl::RecordingMode mode) noexcept
{
    switch(mode)
    {
        case RecordingImpl::RecordingMode::Controller: return TR(CONTROLLER);
        case RecordingImpl::RecordingMode::Mouse: return TR(MOUSE);
    }
    return "";
}

void RecordingImpl::DrawModeSettings() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;

    if(ImGui::BeginCombo(TR_ID("MODE", Tr::MODE), RecordingModeToString(activeMode), ImGuiComboFlags_None))
    {
        if(ImGui::Selectable(TR(MOUSE), activeMode == RecordingMode::Mouse))
        {
            activeMode = RecordingMode::Mouse;
        }
        if(ImGui::Selectable(TR(CONTROLLER), activeMode == RecordingMode::Controller))
        {
            activeMode = RecordingMode::Controller;
        }
        ImGui::EndCombo();
    }

    switch (activeMode) {
        case RecordingMode::Controller:
        {
            ImGui::TextUnformatted(TR(CONTROLLER_DEADZONE));
            ImGui::SliderInt(TR(DEADZONE), &ControllerDeadzone, 0, std::numeric_limits<int16_t>::max());
            ImGui::Checkbox(TR(CENTER), &controllerCenter);
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
                ImGui::Checkbox(TR(TWO_AXES), &twoAxesMode);
                OFS::Tooltip(TR(TWO_AXES_TOOLTIP));
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

    ImGui::Checkbox(TR(INVERT), &inverted); ImGui::SameLine(); ImGui::Checkbox(TR(RECORD_ON_PLAY), &automaticRecording);
    if (inverted) { 
        currentPosX = 100 - currentPosX;
        currentPosY = 100 - currentPosY; 
    }
    if (twoAxesMode) {
        ImGui::TextUnformatted(TR(TWO_AXES_AXES));
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::SliderInt("##PosX", &currentPosX, 0, 100);
        ImGui::SliderInt("##PosY", &currentPosY, 0, 100);
        ImGui::PopItemFlag();
    }
    else
    {
        ImGui::TextUnformatted(TR(POSITION)); 
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
        ImGui::TextUnformatted(TR(RECORDING_ACTIVE));
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        ImGui::TextUnformatted(TR(RECORDING_PAUSED));
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
        app->scriptTimeline.RecordingBuffer.clear();
        app->scriptTimeline.RecordingBuffer.resize(app->player->getTotalNumFrames(),
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
