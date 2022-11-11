#include "OpenFunscripter.h"
#include "OFS_ScriptingMode.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_Localization.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "OFS_ImGui.h"
#include "OFS_Simulator3D.h"


#include "state/ScriptModeState.h"


void ScriptingModeBase::AddEditAction(FunscriptAction action) noexcept
{
    auto app = OpenFunscripter::ptr;
    ctx().AddEditAction(action, app->scripting->LogicalFrameTime());
}

inline Funscript& ScriptingModeBase::ctx() noexcept
{
    auto app = OpenFunscripter::ptr;
    return *app->ActiveFunscript().get();
}

void ScriptingMode::Init() noexcept
{
    stateHandle = OFS_AppState<ScriptingModeState>::Register(ScriptingModeState::StateName);

    modes[ScriptingModeEnum::DEFAULT_MODE] = std::make_unique<DefaultMode>();
    modes[ScriptingModeEnum::ALTERNATING] = std::make_unique<AlternatingMode>();
    modes[ScriptingModeEnum::RECORDING] = std::make_unique<RecordingMode>();
    modes[ScriptingModeEnum::DYNAMIC_INJECTION] = std::make_unique<DynamicInjectionMode>();
    SetMode(ScriptingModeEnum::DEFAULT_MODE);
    SetOverlay(ScriptingOverlayModes::FRAME);
}

inline static const char* ScriptingModeToString(ScriptingModeEnum mode) noexcept
{
    switch (mode) {
        case ScriptingModeEnum::DEFAULT_MODE: return TR(DEFAULT_MODE);
        case ScriptingModeEnum::ALTERNATING: return TR(ALTERNATING_MODE);
        case ScriptingModeEnum::DYNAMIC_INJECTION: return TR(DYNAMIC_INJECTION_MODE);
        case ScriptingModeEnum::RECORDING: return TR(RECORDING_MODE);
    }
    return "";
}

inline static const char* OverlayModeToString(ScriptingOverlayModes mode) noexcept
{
    switch (mode) {
        case ScriptingOverlayModes::FRAME: return TR(FRAME_OVERLAY);
        case ScriptingOverlayModes::TEMPO: return TR(TEMPO_OVERLAY);
        case ScriptingOverlayModes::EMPTY: return TR(EMPTY_OVERLAY);
    }
    return "";
}


void ScriptingMode::DrawScriptingMode(bool* open) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& state = ScriptingModeState::State(stateHandle);
    auto app = OpenFunscripter::ptr;
    ImGui::Begin(TR_ID(WindowId, Tr::MODE), open);
    ImGui::PushItemWidth(-1);

    if (ImGui::BeginCombo("##Mode", ScriptingModeToString(activeMode), ImGuiComboFlags_None)) {
        if (ImGui::Selectable(TR_ID("DEFAULT", Tr::DEFAULT_MODE), activeMode == ScriptingModeEnum::DEFAULT_MODE)) {
            SetMode(ScriptingModeEnum::DEFAULT_MODE);
        }
        if (ImGui::Selectable(TR_ID("ALTERNATING", Tr::ALTERNATING_MODE), activeMode == ScriptingModeEnum::ALTERNATING)) {
            SetMode(ScriptingModeEnum::ALTERNATING);
        }
        if (ImGui::Selectable(TR_ID("DYNAMIC_INJECTION", Tr::DYNAMIC_INJECTION_MODE), activeMode == ScriptingModeEnum::DYNAMIC_INJECTION)) {
            SetMode(ScriptingModeEnum::DYNAMIC_INJECTION);
        }
        if (ImGui::Selectable(TR_ID("RECORDING", Tr::RECORDING_MODE), activeMode == ScriptingModeEnum::RECORDING)) {
            SetMode(ScriptingModeEnum::RECORDING);
        }
        ImGui::EndCombo();
    }
    OFS::Tooltip(TR(SCRIPTING_MODE));
    Mode()->DrawModeSettings();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();

    if (ImGui::BeginCombo("##OverlayMode", OverlayModeToString(activeOverlay), ImGuiComboFlags_None)) {
        if (ImGui::Selectable(TR_ID("FRAME_OVERLAY", Tr::FRAME_OVERLAY), activeOverlay == ScriptingOverlayModes::FRAME)) {
            SetOverlay(ScriptingOverlayModes::FRAME);
        }
        if (ImGui::Selectable(TR_ID("TEMPO_OVERLAY", Tr::TEMPO_OVERLAY), activeOverlay == ScriptingOverlayModes::TEMPO)) {
            SetOverlay(ScriptingOverlayModes::TEMPO);
        }
        if (ImGui::Selectable(TR_ID("EMPTY_OVERLAY", Tr::EMPTY_OVERLAY), activeOverlay == ScriptingOverlayModes::EMPTY)) {
            SetOverlay(ScriptingOverlayModes::EMPTY);
        }
        ImGui::EndCombo();
    }

    OFS::Tooltip(TR(SCRIPTING_OVERLAY));
    DrawOverlaySettings();
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::Spacing();
    ImGui::DragInt(TR(OFFSET_MS), &state.actionInsertDelayMs);
    OFS::Tooltip(TR(OFFSET_TOOLTIP));
    ImGui::End();
}

void ScriptingMode::DrawOverlaySettings() noexcept
{
    overlayImpl->DrawSettings();
}

void ScriptingMode::SetMode(ScriptingModeEnum mode) noexcept
{
    Mode()->Finish();
    if (mode >= ScriptingModeEnum::DEFAULT_MODE && mode < ScriptingModeEnum::COUNT) {
        activeMode = mode;
    }
    else {
        activeMode = ScriptingModeEnum::DEFAULT_MODE;
    }
}

void ScriptingMode::SetOverlay(ScriptingOverlayModes mode) noexcept
{
    activeOverlay = mode;
    auto app = OpenFunscripter::ptr;
    auto timeline = &app->scriptTimeline;
    switch (mode) {
        case ScriptingOverlayModes::FRAME:
            overlayImpl = std::make_unique<FrameOverlay>(timeline);
            break;
        case ScriptingOverlayModes::TEMPO:
            overlayImpl = std::make_unique<TempoOverlay>(timeline);
            break;
        case ScriptingOverlayModes::EMPTY:
            overlayImpl = std::make_unique<EmptyOverlay>(timeline);
            break;
        default:
            break;
    }
}

void ScriptingMode::Undo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    Mode()->Undo();
}

void ScriptingMode::Redo() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    Mode()->Redo();
}

void ScriptingMode::AddEditAction(FunscriptAction action) noexcept
{
    auto app = OpenFunscripter::ptr;
    if (!app->player->IsPaused()) {
        // apply offset
        auto& state = ScriptingModeState::State(stateHandle);
        action.atS += state.actionInsertDelayMs / 1000.f;
    }
    Mode()->AddEditAction(action);
}

void ScriptingMode::NextFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    float frameTime = app->player->FrameTime();
    overlayImpl->nextFrame(frameTime);
}

void ScriptingMode::PreviousFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    float frameTime = app->player->FrameTime();
    overlayImpl->previousFrame(frameTime);
}

float ScriptingMode::SteppingIntervalForward(float fromTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    float frameTime = app->player->FrameTime();
    return overlayImpl->steppingIntervalForward(frameTime, fromTime);
}

float ScriptingMode::SteppingIntervalBackward(float fromTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    float frameTime = app->player->FrameTime();
    return overlayImpl->steppingIntervalBackward(frameTime, fromTime);
}

float ScriptingMode::LogicalFrameTime() noexcept
{
    auto app = OpenFunscripter::ptr;
    float realFrameTime = app->player->FrameTime();
    return overlayImpl->logicalFrameTime(realFrameTime);
}

void ScriptingMode::Update() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    Mode()->Update();
    overlayImpl->update();
}

// dynamic top injection
void DynamicInjectionMode::DrawModeSettings() noexcept
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
void DynamicInjectionMode::AddEditAction(FunscriptAction action) noexcept
{
    auto previous = ctx().GetPreviousActionBehind(action.atS);
    if (previous != nullptr) {
        auto injectAt = previous->atS + ((action.atS - previous->atS) / 2) + (((action.atS - previous->atS) / 2) * directionBias);
        auto inject_duration = injectAt - previous->atS;

        int32_t injectPos = Util::Clamp<int32_t>(previous->pos + (topBottomDirection * inject_duration * targetSpeed), 0, 100);
        ScriptingModeBase::AddEditAction(FunscriptAction(injectAt, injectPos));
    }
    ScriptingModeBase::AddEditAction(action);
}

// alternating
void AlternatingMode::DrawModeSettings() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (contextSensitive) {
        auto behind = ctx().GetPreviousActionBehind(std::round(app->player->CurrentTime()) - 0.001f);
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

        ImGui::SetNextItemWidth(availdWidth / 2.f);
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

void AlternatingMode::AddEditAction(FunscriptAction action) noexcept
{
    if (contextSensitive) {
        auto behind = ctx().GetPreviousActionBehind(action.atS - 0.001f);
        if (behind && behind->pos <= 50 && action.pos <= 50) {
            // Top
            action.pos = 100 - action.pos;
        }
        else if (behind && behind->pos > 50 && action.pos > 50) {
            // Bottom
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
    ScriptingModeBase::AddEditAction(action);
    if (!contextSensitive) {
        nextPosition = !nextPosition;
    }
}

void AlternatingMode::Undo() noexcept
{
    nextPosition = !nextPosition;
}

void AlternatingMode::Redo() noexcept
{
    nextPosition = !nextPosition;
}

inline void RecordingMode::singleAxisRecording() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    recordingAxisX->AddAction(FunscriptAction(app->player->CurrentTime(), currentPosY));
    app->simulator.positionOverride = currentPosY;
}

inline void RecordingMode::twoAxisRecording() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;

    float atS = app->player->CurrentTime();
    recordingAxisX->AddAction(FunscriptAction(atS, currentPosX));
    recordingAxisY->AddAction(FunscriptAction(atS, currentPosY));

    app->sim3D->RollOverride = currentPosX;
    app->sim3D->PitchOverride = 100 - currentPosY;
}

// recording
RecordingMode::RecordingMode() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &RecordingMode::ControllerAxisMotion));
}

RecordingMode::~RecordingMode() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Unsubscribe(SDL_CONTROLLERAXISMOTION, this);
}

void RecordingMode::ControllerAxisMotion(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (activeType != RecordingType::Controller) return;
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
            leftX = Util::Clamp(axis.value / range, -1.f, 1.f);
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
            leftY = Util::Clamp(axis.value / range, -1.f, 1.f);
            break;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            rightX = Util::Clamp(axis.value / range, -1.f, 1.f);
            break;
        case SDL_CONTROLLER_AXIS_RIGHTY:
            rightY = Util::Clamp(axis.value / range, -1.f, 1.f);
            break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            leftTrigger = Util::Clamp(axis.value / range, -1.f, 1.f);
            break;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            rightTrigger = Util::Clamp(axis.value / range, -1.f, 1.f);
            break;
    }


    if (std::abs(rightX) > std::abs(leftX)) {
        valueX = rightX;
    }
    else {
        valueX = leftX;
    }

    if (std::abs(rightY) > std::abs(leftY)) {
        valueY = -rightY;
    }
    else {
        valueY = -leftY;
    }
}

inline static const char* RecordingModeToString(RecordingMode::RecordingType mode) noexcept
{
    switch (mode) {
        case RecordingMode::RecordingType::Controller: return TR(CONTROLLER);
        case RecordingMode::RecordingType::Mouse: return TR(MOUSE);
    }
    return "";
}

void RecordingMode::DrawModeSettings() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;

    if (ImGui::BeginCombo(TR_ID("MODE", Tr::MODE), RecordingModeToString(activeType), ImGuiComboFlags_None)) {
        if (ImGui::Selectable(TR(MOUSE), activeType == RecordingType::Mouse)) {
            activeType = RecordingType::Mouse;
        }
        if (ImGui::Selectable(TR(CONTROLLER), activeType == RecordingType::Controller)) {
            activeType = RecordingType::Controller;
        }
        ImGui::EndCombo();
    }

    switch (activeType) {
        case RecordingType::Controller: {
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
        case RecordingType::Mouse: {
            twoAxesMode = false;
            valueY = app->simulator.getMouseValue();
            currentPosY = Util::Clamp<int32_t>(50.f + (50.f * valueY), 0, 100);
            break;
        }
    }

    ImGui::Checkbox(TR(INVERT), &inverted);
    ImGui::SameLine();
    ImGui::Checkbox(TR(RECORD_ON_PLAY), &automaticRecording);
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
    else {
        ImGui::TextUnformatted(TR(POSITION));
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::SliderInt("##Pos", &currentPosY, 0, 100);
        ImGui::PopItemFlag();
    }


    ImGui::Spacing();
    bool playing = !app->player->IsPaused();
    if (automaticRecording && playing && recordingActive != playing) {
        if (!twoAxesMode) {
            recordingAxisX = app->ActiveFunscript();
            app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, recordingAxisX);
        }
        else {
            int32_t rollIdx = app->sim3D->rollIndex;
            int32_t pitchIdx = app->sim3D->pitchIndex;
            recordingAxisX = app->LoadedFunscripts()[rollIdx];
            recordingAxisY = app->LoadedFunscripts()[pitchIdx];
            app->undoSystem->Snapshot(StateType::GENERATE_ACTIONS, { recordingAxisX, recordingAxisY });
        }
        recordingActive = true;
    }
    else if (!playing && recordingActive) {
        recordingAxisX = nullptr;
        recordingAxisY = nullptr;
        recordingActive = false;
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

void RecordingMode::Update() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (recordingActive) {
        if (twoAxesMode) {
            twoAxisRecording();
        }
        else {
            singleAxisRecording();
        }
    }
}

void RecordingMode::Finish() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    // this fixes a bug when the mode gets changed during a recording
    if (recordingActive) {
        recordingAxisX = nullptr;
        recordingAxisY = nullptr;
        recordingActive = false;
    }
}
