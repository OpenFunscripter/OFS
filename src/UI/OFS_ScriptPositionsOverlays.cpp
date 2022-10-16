#include "OFS_ScriptPositionsOverlays.h"
#include "OpenFunscripter.h"

void FrameOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    auto app = OpenFunscripter::ptr;
    float fps = enableFpsOverride ? fpsOverride : app->player->Fps();
    float frameTime = enableFpsOverride ? (1.f / fpsOverride) : app->scripting->LogicalFrameTime();
    float visibleFrames = ctx.visibleTime / frameTime;
    constexpr float maxVisibleFrames = 400.f;
   
    if (visibleFrames <= (maxVisibleFrames * 0.75f)) {
        //render frame dividers
        float offset = -std::fmod(ctx.offsetTime, frameTime);
        const int lineCount = visibleFrames + 2;
        int alpha = 255 * (1.f - (visibleFrames / maxVisibleFrames));
        for (int i = 0; i < lineCount; i++) {
            ctx.drawList->AddLine(
                ctx.canvasPos + ImVec2(((offset + (i * frameTime)) / ctx.visibleTime) * ctx.canvasSize.x, 0.f),
                ctx.canvasPos + ImVec2(((offset + (i * frameTime)) / ctx.visibleTime) * ctx.canvasSize.x, ctx.canvasSize.y),
                IM_COL32(80, 80, 80, alpha),
                1.f
            );
        }
    }

    // time dividers
    constexpr float maxVisibleTimeDividers = 150.f;
    const float timeIntervalMs = std::round(fps * 0.1f) * frameTime;
    const float visibleTimeIntervals = ctx.visibleTime / timeIntervalMs;
    if (visibleTimeIntervals <= (maxVisibleTimeDividers * 0.8f)) {
        float offset = -std::fmod(ctx.offsetTime, timeIntervalMs);
        const int lineCount = visibleTimeIntervals + 2;
        int alpha = 255 * (1.f - (visibleTimeIntervals / maxVisibleTimeDividers));
        for (int i = 0; i < lineCount; i++) {
            ctx.drawList->AddLine(
                ctx.canvasPos + ImVec2(((offset + (i * timeIntervalMs)) / ctx.visibleTime) * ctx.canvasSize.x, 0.f),
                ctx.canvasPos + ImVec2(((offset + (i * timeIntervalMs)) / ctx.visibleTime) * ctx.canvasSize.x, ctx.canvasSize.y),
                IM_COL32(80, 80, 80, alpha),
                3.f
            );
        }
    }
    BaseOverlay::DrawHeightLines(ctx);
    timeline->DrawAudioWaveform(ctx);
    BaseOverlay::DrawActionLines(ctx);
    BaseOverlay::DrawSecondsLabel(ctx);
    BaseOverlay::DrawScriptLabel(ctx);
 
    // out of sync line
    if (BaseOverlay::SyncLineEnable) {
        float realFrameTime = app->player->CurrentPlayerTime() - ctx.offsetTime;
        ctx.drawList->AddLine(
            ctx.canvasPos + ImVec2((realFrameTime / ctx.visibleTime) * ctx.canvasSize.x, 0.f),
            ctx.canvasPos + ImVec2((realFrameTime / ctx.visibleTime) * ctx.canvasSize.x, ctx.canvasSize.y),
            IM_COL32(255, 0, 0, 255),
            1.f
        );
    }
}

void FrameOverlay::nextFrame(float realFrameTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    if(enableFpsOverride) {
        app->player->SeekRelative(1.f / fpsOverride);
    }
    else {
        app->player->NextFrame();
    }    
}

void FrameOverlay::previousFrame(float realFrameTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    if(enableFpsOverride) {
        app->player->SeekRelative(-(1.f / fpsOverride));
    }
    else {
        app->player->PreviousFrame();
    }
}

float FrameOverlay::steppingIntervalBackward(float realFrameTime, float fromTime) noexcept
{
    return enableFpsOverride ? -(1.f / fpsOverride) : -realFrameTime;
}

float FrameOverlay::steppingIntervalForward(float realFrameTime, float fromTime) noexcept
{
    return enableFpsOverride ? (1.f / fpsOverride) : realFrameTime;
}

float FrameOverlay::logicalFrameTime(float realFrameTime) noexcept
{
    return enableFpsOverride ? (1.f / fpsOverride) : realFrameTime;
}

void FrameOverlay::DrawSettings() noexcept
{
    if(ImGui::Checkbox(TR_ID("FPS_OVERRIDE_ENABLE", Tr::FPS_OVERRIDE), &enableFpsOverride))
    {
        fpsOverride = OpenFunscripter::ptr->player->Fps();
    }
    if(enableFpsOverride) {
        if(ImGui::InputFloat(TR_ID("FPS_OVERRIDE", Tr::FPS_OVERRIDE), &fpsOverride, 1.f, 10.f))
        {
            fpsOverride = Util::Clamp(fpsOverride, 1.f, 150.f);
            // snap to new framerate
            auto app = OpenFunscripter::ptr;
            float newPosition = std::round(app->player->CurrentTime() / (1.0 / (double)fpsOverride))
                * (1.0 / (double)fpsOverride);
            app->player->SetPositionExact(newPosition, true);
        }
    }
}

void TempoOverlay::DrawSettings() noexcept
{
    BaseOverlay::DrawSettings();
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;
    if (ImGui::InputFloat(TR(BPM), &tempo.bpm, 1.f, 100.f)) {
        tempo.bpm = std::max(1.f, tempo.bpm);
    }

    ImGui::DragFloat(TR(OFFSET), &tempo.beatOffsetSeconds, 0.001f, -10.f, 10.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    if (ImGui::BeginCombo(TR(SNAP), TRD(beatMultiplesStrings[tempo.measureIndex]), ImGuiComboFlags_PopupAlignLeft)) {
        for (int i = 0; i < beatMultiples.size(); i++) {
            if (ImGui::Selectable(TRD(beatMultiplesStrings[i]))) {
                tempo.measureIndex = i;
            }
            else if (ImGui::IsItemHovered()) {
                tempo.measureIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Text("%s: %.2fms", TR(INTERVAL), static_cast<float>(((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.measureIndex]));
}

void TempoOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;
    BaseOverlay::DrawHeightLines(ctx);
    timeline->DrawAudioWaveform(ctx);
    BaseOverlay::DrawActionLines(ctx);
    BaseOverlay::DrawSecondsLabel(ctx);
    BaseOverlay::DrawScriptLabel(ctx);

    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    int32_t visibleBeats = ctx.visibleTime / beatTime;
    int32_t invisiblePreviousBeats = ctx.offsetTime / beatTime;

#ifndef NDEBUG
    static int32_t prevInvisiblePreviousBeats = 0;
    if (prevInvisiblePreviousBeats != invisiblePreviousBeats) {
        LOGF_INFO("%d", invisiblePreviousBeats);
    }
    prevInvisiblePreviousBeats = invisiblePreviousBeats;
#endif

    float offset = -std::fmod(ctx.offsetTime, beatTime) + tempo.beatOffsetSeconds;

    const int lineCount = visibleBeats + 2;
    auto& style = ImGui::GetStyle();
    char tmp[32];

    int32_t lineOffset = tempo.beatOffsetSeconds / beatTime;
    for (int i = -lineOffset; i < lineCount - lineOffset; i++) {
        int32_t beatIdx = invisiblePreviousBeats + i;
        const int32_t thing = (int32_t)(1.f / ((beatMultiples[tempo.measureIndex] / 4.f)));
        const bool isWholeMeasure = beatIdx % thing == 0;

        ctx.drawList->AddLine(
            ctx.canvasPos + ImVec2(((offset + (i * beatTime)) / ctx.visibleTime) * ctx.canvasSize.x, 0.f),
            ctx.canvasPos + ImVec2(((offset + (i * beatTime)) / ctx.visibleTime) * ctx.canvasSize.x, ctx.canvasSize.y),
            isWholeMeasure ? beatMultipleColor[tempo.measureIndex] : IM_COL32(255, 255, 255, 153),
            isWholeMeasure ? 5.f : 3.f
        );

        if (isWholeMeasure) {
            stbsp_snprintf(tmp, sizeof(tmp), "%d", thing == 0 ? beatIdx : beatIdx / thing);
            const float textOffsetX = app->settings->data().default_font_size / 2.f;
            ctx.drawList->AddText(OFS_DynFontAtlas::DefaultFont2, app->settings->data().default_font_size * 2.f,
                ctx.canvasPos + ImVec2((((offset + (i * beatTime)) / ctx.visibleTime) * ctx.canvasSize.x) + textOffsetX, 0.f),
                ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
                tmp
            );
        }
    }
}

static float GetNextPosition(float beatTime, float currentTime, float beatOffset) noexcept
{
    float beatIdx = ((currentTime - beatOffset) / beatTime);
    beatIdx = std::floor(beatIdx);

    beatIdx += 1.f;

    float newPosition = (beatIdx * beatTime) + beatOffset;

    if (std::abs(newPosition - currentTime) <= 0.001f) {
        // ugh
        newPosition += beatTime;
    }

    return newPosition;
}

static float GetPreviousPosition(float beatTime, float currentTime, float beatOffset) noexcept
{
    float beatIdx = ((currentTime - beatOffset) / beatTime);
    beatIdx = std::ceil(beatIdx);

    beatIdx -= 1.f;
    float newPosition = (beatIdx * beatTime) + beatOffset;

    if(std::abs(newPosition - currentTime) <= 0.001f) {
        // ugh
        newPosition -= beatTime;
    }

    return newPosition;
}

void TempoOverlay::nextFrame(float realFrameTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;

    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    float currentTime = app->player->CurrentTime();
    float newPosition = GetNextPosition(beatTime, currentTime, tempo.beatOffsetSeconds);

    app->player->SetPositionExact(newPosition);
}

void TempoOverlay::previousFrame(float realFrameTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;

    float beatTime = (60.f/ tempo.bpm) * beatMultiples[tempo.measureIndex];
    float currentTime = app->player->CurrentTime();
    float newPosition = GetPreviousPosition(beatTime, currentTime, tempo.beatOffsetSeconds);

    app->player->SetPositionExact(newPosition);
}

float TempoOverlay::steppingIntervalForward(float realFrameTime, float fromTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;
    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    return GetNextPosition(beatTime, fromTime, tempo.beatOffsetSeconds) - fromTime;
}

float TempoOverlay::steppingIntervalBackward(float realFrameTime, float fromTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;
    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    return GetPreviousPosition(beatTime, fromTime, tempo.beatOffsetSeconds) - fromTime;
}