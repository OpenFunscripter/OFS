#include "OFS_ScriptPositionsOverlays.h"
#include "OpenFunscripter.h"

void FrameOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto frameTime = app->player.getFrameTimeMs();

    float visibleFrames = ctx.visibleSizeMs / frameTime;
    constexpr float maxVisibleFrames = 400.f;
    if (visibleFrames <= (maxVisibleFrames * 0.75f)) {
        //render frame dividers
        float offset = -std::fmod(ctx.offset_ms, frameTime);
        const int lineCount = visibleFrames + 2;
        int alpha = 255 * (1.f - (visibleFrames / maxVisibleFrames));
        for (int i = 0; i < lineCount; i++) {
            ctx.draw_list->AddLine(
                ctx.canvas_pos + ImVec2(((offset + (i * frameTime)) / ctx.visibleSizeMs) * ctx.canvas_size.x, 0.f),
                ctx.canvas_pos + ImVec2(((offset + (i * frameTime)) / ctx.visibleSizeMs) * ctx.canvas_size.x, ctx.canvas_size.y),
                IM_COL32(80, 80, 80, alpha),
                1.f
            );
        }

        // out of sync line
        if (app->player.isPaused() || app->player.getSpeed() <= 0.1) {
            float realFrameTime = app->player.getRealCurrentPositionMs() - ctx.offset_ms;
            ctx.draw_list->AddLine(
                ctx.canvas_pos + ImVec2((realFrameTime / ctx.visibleSizeMs) * ctx.canvas_size.x, 0.f),
                ctx.canvas_pos + ImVec2((realFrameTime / ctx.visibleSizeMs) * ctx.canvas_size.x, ctx.canvas_size.y),
                IM_COL32(255, 0, 0, alpha),
                1.f
            );
        }
    }

    // time dividers
    constexpr float maxVisibleTimeDividers = 150.f;
    const float timeIntervalMs = std::round(app->player.getFps() * 0.1f) * app->player.getFrameTimeMs();
    const float visibleTimeIntervals = ctx.visibleSizeMs / timeIntervalMs;
    if (visibleTimeIntervals <= (maxVisibleTimeDividers * 0.8f)) {
        float offset = -std::fmod(ctx.offset_ms, timeIntervalMs);
        const int lineCount = visibleTimeIntervals + 2;
        int alpha = 255 * (1.f - (visibleTimeIntervals / maxVisibleTimeDividers));
        for (int i = 0; i < lineCount; i++) {
            ctx.draw_list->AddLine(
                ctx.canvas_pos + ImVec2(((offset + (i * timeIntervalMs)) / ctx.visibleSizeMs) * ctx.canvas_size.x, 0.f),
                ctx.canvas_pos + ImVec2(((offset + (i * timeIntervalMs)) / ctx.visibleSizeMs) * ctx.canvas_size.x, ctx.canvas_size.y),
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
}

void FrameOverlay::nextFrame() noexcept
{
    OpenFunscripter::ptr->player.nextFrame();
}

void FrameOverlay::previousFrame() noexcept
{
    OpenFunscripter::ptr->player.previousFrame();
}

void TempoOverlay::DrawSettings() noexcept
{
    BaseOverlay::DrawSettings();
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->Userdata<OFS_ScriptSettings>().tempoSettings;
    if (ImGui::InputInt("BPM", &tempo.bpm, 1, 100)) {
        tempo.bpm = std::max(1, tempo.bpm);
    }

    ImGui::DragFloat("Offset", &tempo.beat_offset_seconds, 0.001f, -10.f, 10.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    if (ImGui::BeginCombo("Snap", beatMultiplesStrings[tempo.multiIDX], ImGuiComboFlags_PopupAlignLeft)) {
        for (int i = 0; i < beatMultiples.size(); i++) {
            if (ImGui::Selectable(beatMultiplesStrings[i])) {
                tempo.multiIDX = i;
            }
            else if (ImGui::IsItemHovered()) {
                tempo.multiIDX = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Text("Interval: %dms", static_cast<int32_t>(((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX]));
}

void TempoOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->Userdata<OFS_ScriptSettings>().tempoSettings;
    BaseOverlay::DrawHeightLines(ctx);
    timeline->DrawAudioWaveform(ctx);
    BaseOverlay::DrawActionLines(ctx);
    BaseOverlay::DrawSecondsLabel(ctx);
    BaseOverlay::DrawScriptLabel(ctx);

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    int32_t visibleBeats = ctx.visibleSizeMs / beatTimeMs;
    int32_t invisiblePreviousBeats = ctx.offset_ms / beatTimeMs;

#ifndef NDEBUG
    static int32_t prevInvisiblePreviousBeats = 0;
    if (prevInvisiblePreviousBeats != invisiblePreviousBeats) {
        LOGF_INFO("%d", invisiblePreviousBeats);
    }
    prevInvisiblePreviousBeats = invisiblePreviousBeats;
#endif

    static bool playedSound = false;
    static float oldOffset = 0.f;
    float offset = -std::fmod(ctx.offset_ms, beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);
    if (std::abs(std::abs(offset) - beatTimeMs) <= 0.1f) {
        // this prevents a bug where the measures get offset by one "unit"
        offset = 0.f;
    }

    const int lineCount = visibleBeats + 2;
    auto& style = ImGui::GetStyle();
    char tmp[32];

    int32_t lineOffset = (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
    for (int i = -lineOffset; i < lineCount - lineOffset; i++) {
        int32_t beatIdx = invisiblePreviousBeats + i;
        const int32_t thing = (int32_t)(1.f / ((beatMultiples[tempo.multiIDX] / 4.f)));
        const bool isWholeMeasure = beatIdx % thing == 0;

        ctx.draw_list->AddLine(
            ctx.canvas_pos + ImVec2(((offset + (i * beatTimeMs)) / ctx.visibleSizeMs) * ctx.canvas_size.x, 0.f),
            ctx.canvas_pos + ImVec2(((offset + (i * beatTimeMs)) / ctx.visibleSizeMs) * ctx.canvas_size.x, ctx.canvas_size.y),
            isWholeMeasure ? beatMultipleColor[tempo.multiIDX] : IM_COL32(255, 255, 255, 180),
            isWholeMeasure ? 7.f : 3.f
        );

        if (isWholeMeasure) {
            stbsp_snprintf(tmp, sizeof(tmp), "%d", thing == 0 ? beatIdx : beatIdx / thing);
            const float textOffsetX = app->settings->data().default_font_size / 2.f;
            ctx.draw_list->AddText(OpenFunscripter::DefaultFont2, app->settings->data().default_font_size * 2.f,
                ctx.canvas_pos + ImVec2((((offset + (i * beatTimeMs)) / ctx.visibleSizeMs) * ctx.canvas_size.x) + textOffsetX, 0.f),
                ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
                tmp
            );
        }
    }
}

void TempoOverlay::nextFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->Userdata<OFS_ScriptSettings>().tempoSettings;

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    float currentMs = app->player.getCurrentPositionMsInterp();
    int32_t beatIdx = std::ceil(currentMs / beatTimeMs);
    if (std::abs(tempo.beat_offset_seconds) >= 0.001f) {
        beatIdx -= (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
    }
    beatIdx += 1;
    int32_t newPositionMs = (beatIdx * beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);

    app->player.setPositionExact(newPositionMs);
}

void TempoOverlay::previousFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->Userdata<OFS_ScriptSettings>().tempoSettings;

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    float currentMs = app->player.getCurrentPositionMsInterp();
    int32_t beatIdx = std::ceil(currentMs / beatTimeMs);
    if (std::abs(tempo.beat_offset_seconds) >= 0.001f) {
        beatIdx -= (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
    }
    beatIdx -= 1;
    int32_t newPositionMs = (beatIdx * beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);

    app->player.setPositionExact(newPositionMs);
}