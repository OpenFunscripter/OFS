#include "ScriptPositionsOverlayMode.h"
#include "OpenFunscripter.h"

void BaseOverlay::DrawScriptPositionContent(ImDrawList* draw_list, float visibleSizeMs, float offset_ms, ImVec2 canvas_pos, ImVec2 canvas_size) noexcept
{
    // height indicators
    for (int i = 0; i < 9; i++) {
        auto color = (i == 4) ? IM_COL32(150, 150, 150, 255) : IM_COL32(80, 80, 80, 255);
        auto thickness = (i == 4) ? 2.f : 1.0f;
        draw_list->AddLine(
            canvas_pos + ImVec2(0.0, (canvas_size.y / 10.f) * (i + 1)),
            canvas_pos + ImVec2(canvas_size.x, (canvas_size.y / 10.f) * (i + 1)),
            color,
            thickness
        );
    }
}

void BaseOverlay::nextFrame() noexcept
{
	OpenFunscripter::ptr->player.nextFrame();
}

void BaseOverlay::previousFrame() noexcept
{
	OpenFunscripter::ptr->player.previousFrame();
}

void TempoOverlay::DrawSettings() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->scriptSettings.tempoSettings;
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

void TempoOverlay::DrawScriptPositionContent(ImDrawList* draw_list, float visibleSizeMs, float offset_ms, ImVec2 canvas_pos, ImVec2 canvas_size) noexcept
{
    BaseOverlay::DrawScriptPositionContent(draw_list, visibleSizeMs, offset_ms, canvas_pos, canvas_size);
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->scriptSettings.tempoSettings;

    app->scriptPositions.DrawAudioWaveform(draw_list, canvas_pos, canvas_size);

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    int32_t visibleBeats = visibleSizeMs / beatTimeMs;
    int32_t invisiblePreviousBeats = offset_ms / beatTimeMs;

#ifndef NDEBUG
    static int32_t prevInvisiblePreviousBeats = 0;
    if (prevInvisiblePreviousBeats != invisiblePreviousBeats) {
        LOGF_INFO("%d", invisiblePreviousBeats);
    }
    prevInvisiblePreviousBeats = invisiblePreviousBeats;
#endif

    static bool playedSound = false;
    static float oldOffset = 0.f;
    float offset = -std::fmod(offset_ms, beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);
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

        draw_list->AddLine(
            canvas_pos + ImVec2(((offset + (i * beatTimeMs)) / visibleSizeMs) * canvas_size.x, 0.f),
            canvas_pos + ImVec2(((offset + (i * beatTimeMs)) / visibleSizeMs) * canvas_size.x, canvas_size.y),
            isWholeMeasure ? beatMultipleColor[tempo.multiIDX] : IM_COL32(255, 255, 255, 180),
            isWholeMeasure ? 7.f : 3.f
        );

        if (isWholeMeasure) {
            stbsp_snprintf(tmp, sizeof(tmp), "%d", thing == 0 ? beatIdx : beatIdx / thing);
            const float textOffsetX = app->settings->data().default_font_size / 2.f;
            draw_list->AddText(OpenFunscripter::DefaultFont2, app->settings->data().default_font_size * 2.f,
                canvas_pos + ImVec2((((offset + (i * beatTimeMs)) / visibleSizeMs) * canvas_size.x) + textOffsetX, 0.f),
                ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
                tmp
            );
        }
    }
}

void TempoOverlay::nextFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->scriptSettings.tempoSettings;

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    float currentMs = app->player.getCurrentPositionMsInterp();
    int32_t beatIdx = currentMs / beatTimeMs;
    if (std::abs(tempo.beat_offset_seconds) >= 0.001f) {
        beatIdx -= (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
        beatIdx += 1;
    }
    beatIdx += 1;
    int32_t newPositionMs = (beatIdx * beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);

    app->player.setPosition(newPositionMs);
}

void TempoOverlay::previousFrame() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->scriptSettings.tempoSettings;

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    float currentMs = app->player.getCurrentPositionMsInterp();
    int32_t beatIdx = currentMs / beatTimeMs;
    if (std::abs(tempo.beat_offset_seconds) >= 0.001f) {
        beatIdx -= (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
    }
    else {
        beatIdx -= 1;
    }
    int32_t newPositionMs = (beatIdx * beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);

    app->player.setPosition(newPositionMs);
}

void FrameOverlay::DrawScriptPositionContent(ImDrawList* draw_list, float visibleSizeMs, float offset_ms, ImVec2 canvas_pos, ImVec2 canvas_size) noexcept
{
    BaseOverlay::DrawScriptPositionContent(draw_list, visibleSizeMs, offset_ms, canvas_pos, canvas_size);
    auto app = OpenFunscripter::ptr;
    auto frameTime = app->player.getFrameTimeMs();

    float visibleFrames = visibleSizeMs / frameTime;
    constexpr float maxVisibleFrames = 400.f;
    if (visibleFrames <= (maxVisibleFrames * 0.75f)) {
        //render frame dividers
        float offset = -std::fmod(offset_ms, frameTime);
        const int lineCount = visibleFrames + 2;
        int alpha = 255 * (1.f - (visibleFrames / maxVisibleFrames));
        for (int i = 0; i < lineCount; i++) {
            draw_list->AddLine(
                canvas_pos + ImVec2(((offset + (i * frameTime)) / visibleSizeMs) * canvas_size.x, 0.f),
                canvas_pos + ImVec2(((offset + (i * frameTime)) / visibleSizeMs) * canvas_size.x, canvas_size.y),
                IM_COL32(80, 80, 80, alpha),
                1.f
            );
        }

        // out of sync line
        if (app->player.isPaused() || app->player.getSpeed() <= 0.1) {
            float realFrameTime = app->player.getRealCurrentPositionMs() - offset_ms;
            draw_list->AddLine(
                canvas_pos + ImVec2((realFrameTime / visibleSizeMs) * canvas_size.x, 0.f),
                canvas_pos + ImVec2((realFrameTime / visibleSizeMs) * canvas_size.x, canvas_size.y),
                IM_COL32(255, 0, 0, alpha),
                1.f
            );
        }
    }

    // time dividers
    constexpr float maxVisibleTimeDividers = 150.f;
    const float timeIntervalMs = std::round(app->player.getFps() * 0.1f) * app->player.getFrameTimeMs();
    const float visibleTimeIntervals = visibleSizeMs / timeIntervalMs;
    if (visibleTimeIntervals <= (maxVisibleTimeDividers * 0.8f)) {
        float offset = -std::fmod(offset_ms, timeIntervalMs);
        const int lineCount = visibleTimeIntervals + 2;
        int alpha = 255 * (1.f - (visibleTimeIntervals / maxVisibleTimeDividers));
        for (int i = 0; i < lineCount; i++) {
            draw_list->AddLine(
                canvas_pos + ImVec2(((offset + (i * timeIntervalMs)) / visibleSizeMs) * canvas_size.x, 0.f),
                canvas_pos + ImVec2(((offset + (i * timeIntervalMs)) / visibleSizeMs) * canvas_size.x, canvas_size.y),
                IM_COL32(80, 80, 80, alpha),
                3.f
            );
        }
    }
    app->scriptPositions.DrawAudioWaveform(draw_list, canvas_pos, canvas_size);
}
