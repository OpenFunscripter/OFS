#include "OFS_ScriptPositionsOverlays.h"
#include "OpenFunscripter.h"

void FrameOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto frameTime = enableFramerateOverride ? (1.f / framerateOverride) : app->player->getFrameTime();

    float visibleFrames = ctx.visibleTime / frameTime;
    constexpr float maxVisibleFrames = 400.f;
   
    if (visibleFrames <= (maxVisibleFrames * 0.75f)) {
        //render frame dividers
        float offset = -std::fmod(ctx.offsetTime, frameTime);
        const int lineCount = visibleFrames + 2;
        int alpha = 255 * (1.f - (visibleFrames / maxVisibleFrames));
        for (int i = 0; i < lineCount; i++) {
            ctx.draw_list->AddLine(
                ctx.canvas_pos + ImVec2(((offset + (i * frameTime)) / ctx.visibleTime) * ctx.canvas_size.x, 0.f),
                ctx.canvas_pos + ImVec2(((offset + (i * frameTime)) / ctx.visibleTime) * ctx.canvas_size.x, ctx.canvas_size.y),
                IM_COL32(80, 80, 80, alpha),
                1.f
            );
        }
    }

    // time dividers
    constexpr float maxVisibleTimeDividers = 150.f;
    const float timeIntervalMs = std::round(app->player->getFps() * 0.1f) * frameTime;
    const float visibleTimeIntervals = ctx.visibleTime / timeIntervalMs;
    if (visibleTimeIntervals <= (maxVisibleTimeDividers * 0.8f)) {
        float offset = -std::fmod(ctx.offsetTime, timeIntervalMs);
        const int lineCount = visibleTimeIntervals + 2;
        int alpha = 255 * (1.f - (visibleTimeIntervals / maxVisibleTimeDividers));
        for (int i = 0; i < lineCount; i++) {
            ctx.draw_list->AddLine(
                ctx.canvas_pos + ImVec2(((offset + (i * timeIntervalMs)) / ctx.visibleTime) * ctx.canvas_size.x, 0.f),
                ctx.canvas_pos + ImVec2(((offset + (i * timeIntervalMs)) / ctx.visibleTime) * ctx.canvas_size.x, ctx.canvas_size.y),
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
        float realFrameTime = app->player->getRealCurrentPositionSeconds() - ctx.offsetTime;
        ctx.draw_list->AddLine(
            ctx.canvas_pos + ImVec2((realFrameTime / ctx.visibleTime) * ctx.canvas_size.x, 0.f),
            ctx.canvas_pos + ImVec2((realFrameTime / ctx.visibleTime) * ctx.canvas_size.x, ctx.canvas_size.y),
            IM_COL32(255, 0, 0, 255),
            1.f
        );
    }
}

void FrameOverlay::nextFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    if(enableFramerateOverride)
    {
        app->player->seekRelative(1.f / framerateOverride);
    }
    else
    {
        app->player->nextFrame();
    }    
}

void FrameOverlay::previousFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    if(enableFramerateOverride)
    {
        app->player->seekRelative(-(1.f / framerateOverride));
    }
    else
    {
        app->player->previousFrame();
    }
}

float FrameOverlay::steppingIntervalBackward(float fromTime) noexcept
{
    return enableFramerateOverride ? -(1.f / framerateOverride) : -timeline->frameTime;
}

float FrameOverlay::steppingIntervalForward(float fromTime) noexcept
{
    return enableFramerateOverride ? (1.f / framerateOverride) : timeline->frameTime;
}

void FrameOverlay::DrawSettings() noexcept
{
    if(ImGui::Checkbox(TR_ID("FPS_OVERRIDE_ENABLE", Tr::FPS_OVERRIDE), &enableFramerateOverride))
    {
        framerateOverride = OpenFunscripter::ptr->player->getFps();
    }
    if(enableFramerateOverride)
    {
        if(ImGui::InputFloat(TR_ID("FPS_OVERRIDE", Tr::FPS_OVERRIDE), &framerateOverride, 1.f, 10.f))
        {
            framerateOverride = Util::Clamp(framerateOverride, 1.f, 150.f);
            // snap to new framerate
            auto app = OpenFunscripter::ptr;
            float newPosition = std::round(app->player->getCurrentPositionSeconds() / (1.0 / (double)framerateOverride))
                * (1.0 / (double)framerateOverride);
            app->player->setPositionExact(newPosition, true);
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

        ctx.draw_list->AddLine(
            ctx.canvas_pos + ImVec2(((offset + (i * beatTime)) / ctx.visibleTime) * ctx.canvas_size.x, 0.f),
            ctx.canvas_pos + ImVec2(((offset + (i * beatTime)) / ctx.visibleTime) * ctx.canvas_size.x, ctx.canvas_size.y),
            isWholeMeasure ? beatMultipleColor[tempo.measureIndex] : IM_COL32(255, 255, 255, 153),
            isWholeMeasure ? 5.f : 3.f
        );

        if (isWholeMeasure) {
            stbsp_snprintf(tmp, sizeof(tmp), "%d", thing == 0 ? beatIdx : beatIdx / thing);
            const float textOffsetX = app->settings->data().default_font_size / 2.f;
            ctx.draw_list->AddText(OFS_DynFontAtlas::DefaultFont2, app->settings->data().default_font_size * 2.f,
                ctx.canvas_pos + ImVec2((((offset + (i * beatTime)) / ctx.visibleTime) * ctx.canvas_size.x) + textOffsetX, 0.f),
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

void TempoOverlay::nextFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;

    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    float currentTime = app->player->getCurrentPositionSecondsInterp();
    float newPosition = GetNextPosition(beatTime, currentTime, tempo.beatOffsetSeconds);

    app->player->setPositionExact(newPosition);
}

void TempoOverlay::previousFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;

    float beatTime = (60.f/ tempo.bpm) * beatMultiples[tempo.measureIndex];
    float currentTime = app->player->getCurrentPositionSecondsInterp();
    float newPosition = GetPreviousPosition(beatTime, currentTime, tempo.beatOffsetSeconds);

    app->player->setPositionExact(newPosition);
}

float TempoOverlay::steppingIntervalForward(float fromTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;
    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    return GetNextPosition(beatTime, fromTime, tempo.beatOffsetSeconds) - fromTime;
}

float TempoOverlay::steppingIntervalBackward(float fromTime) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->LoadedProject->Settings.tempoSettings;
    float beatTime = (60.f / tempo.bpm) * beatMultiples[tempo.measureIndex];
    return GetPreviousPosition(beatTime, fromTime, tempo.beatOffsetSeconds) - fromTime;
}