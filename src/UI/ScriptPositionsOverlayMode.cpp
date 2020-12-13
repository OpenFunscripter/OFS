#include "ScriptPositionsOverlayMode.h"
#include "ScriptPositionsOverlayMode.h"
#include "ScriptPositionsOverlayMode.h"
#include "ScriptPositionsOverlayMode.h"
#include "OpenFunscripter.h"


ImGradient BaseOverlay::speedGradient;
std::vector<BaseOverlay::ColoredLine> BaseOverlay::ColoredLines;
std::vector<ImVec2> BaseOverlay::SelectedActionScreenCoordinates;
std::vector<ImVec2> BaseOverlay::ActionScreenCoordinates;
std::vector<FunscriptAction> BaseOverlay::ActionPositionWindow;
bool BaseOverlay::SplineLines = false;
float BaseOverlay::SplineEasing = 1.5f;

BaseOverlay::BaseOverlay() noexcept
{
    if (speedGradient.getMarks().size() == 0) {
        std::array<ImColor, 4> heatColor{
        IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
        IM_COL32(0x66, 0xff, 0x00, 0xFF),
        IM_COL32(0xFF, 0xff, 0x00, 0xFF),
        IM_COL32(0xFF, 0x00, 0x00, 0xFF),
        };

        float pos = 0.0f;
        for (auto& col : heatColor) {
            speedGradient.addMark(pos, col);
            pos += (1.f / (heatColor.size() - 1));
        }
        speedGradient.refreshCache();
    }
}

void BaseOverlay::update() noexcept
{
    ActionScreenCoordinates.clear();
    ActionPositionWindow.clear();
    SelectedActionScreenCoordinates.clear();
}

void BaseOverlay::DrawSettings() noexcept
{
    ImGui::Checkbox("Spline", &SplineLines);
#ifndef NDEBUG
    ImGui::SameLine();
    ImGui::DragFloat("Easing", &SplineEasing, 0.01f, 1.0f, 100.f);
    Util::Tooltip("Easing");
#endif
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
    BaseOverlay::DrawSettings();
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

void TempoOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& tempo = app->ActiveFunscript()->scriptSettings.tempoSettings;
    BaseOverlay::DrawHeightLines(ctx);
    app->scriptPositions.DrawAudioWaveform(ctx);
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
    auto& tempo = app->ActiveFunscript()->scriptSettings.tempoSettings;

    float beatTimeMs = ((60.f * 1000.f) / tempo.bpm) * beatMultiples[tempo.multiIDX];
    float currentMs = app->player.getCurrentPositionMsInterp();
    int32_t beatIdx = std::ceil(currentMs / beatTimeMs);
    if (std::abs(tempo.beat_offset_seconds) >= 0.001f) {
        beatIdx -= (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
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
    int32_t beatIdx = std::ceil(currentMs / beatTimeMs);
    if (std::abs(tempo.beat_offset_seconds) >= 0.001f) {
        beatIdx -= (tempo.beat_offset_seconds * 1000.f) / beatTimeMs;
    }
    beatIdx -= 1;    
    int32_t newPositionMs = (beatIdx * beatTimeMs) + (tempo.beat_offset_seconds * 1000.f);

    app->player.setPosition(newPositionMs);
}

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
    app->scriptPositions.DrawAudioWaveform(ctx);
    BaseOverlay::DrawActionLines(ctx);
    BaseOverlay::DrawSecondsLabel(ctx);
    BaseOverlay::DrawScriptLabel(ctx);
}

void EmptyOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    BaseOverlay::DrawActionLines(ctx);
}

void BaseOverlay::DrawActionLines(const OverlayDrawingCtx& ctx) noexcept
{
    if (ctx.actionToIdx - ctx.actionFromIdx > 1) {
        auto app = OpenFunscripter::ptr;
        auto& script = *app->LoadedFunscripts[ctx.scriptIdx];
        

        auto startIt = script.Actions().begin() + ctx.actionFromIdx;
        auto endIt = script.Actions().begin() + ctx.actionToIdx;
        ColoredLines.clear();

        auto getPointForAction = [](const OverlayDrawingCtx& ctx, FunscriptAction action) {
            float relative_x = (float)(action.at - ctx.offset_ms) / ctx.visibleSizeMs;
            float x = (ctx.canvas_size.x) * relative_x;
            float y = (ctx.canvas_size.y) * (1 - (action.pos / 100.0));
            x += ctx.canvas_pos.x;
            y += ctx.canvas_pos.y;
            return ImVec2(x, y);
        };

        const FunscriptAction* prevAction = nullptr;
        for (; startIt != endIt; startIt++) {
            auto& action = *startIt;

            auto p1 = getPointForAction(ctx, action);
            ActionScreenCoordinates.emplace_back(p1);
            ActionPositionWindow.emplace_back(action);

            if (prevAction != nullptr) {
                // draw line
                auto p2 = getPointForAction(ctx, *prevAction);
                // calculate speed relative to maximum speed
                float rel_speed = Util::Clamp<float>((std::abs(action.pos - prevAction->pos) / ((action.at - prevAction->at) / 1000.0f)) / max_speed_per_seconds, 0.f, 1.f);
                ImColor speed_color;
                speedGradient.getColorAt(rel_speed, &speed_color.Value.x);
                speed_color.Value.w = 1.f;
                
                if (SplineLines) {
                    float splineEasing = (p1.x - p2.x)/SplineEasing;
                    ctx.draw_list->AddBezierCurve(p1, p1 - ImVec2(splineEasing, 0.f), p2 + ImVec2(splineEasing, 0.f), p2, IM_COL32(0, 0, 0, 255), 7.0f);
                }
                else {
                    ctx.draw_list->AddLine(p1, p2, IM_COL32(0, 0, 0, 255), 7.0f); // border
                }
                ColoredLines.emplace_back(std::move(BaseOverlay::ColoredLine{ p1, p2, ImGui::ColorConvertFloat4ToU32(speed_color) }));
            }

            prevAction = &action;
        }

        // this is so that the black background line gets rendered first
        for (auto&& line : ColoredLines) {
            if (SplineLines) {
                float splineEasing = (line.p1.x - line.p2.x)/SplineEasing;

                ctx.draw_list->AddBezierCurve(line.p1, line.p1 - ImVec2(splineEasing, 0.f), line.p2 + ImVec2(splineEasing, 0.f), line.p2, line.color, 3.0f);
            }
            else {
                ctx.draw_list->AddLine(line.p1, line.p2, line.color, 3.f);
            }
        }

        if (script.HasSelection()) {
            auto startIt = std::find_if(script.Selection().begin(), script.Selection().end(),
                [&](auto& act) { return act.at >= ctx.offset_ms; });
            if (startIt != script.Selection().begin())
                startIt -= 1;

            auto endIt = std::find_if(startIt, script.Selection().end(),
                [&](auto& act) { return act.at >= ctx.offset_ms + ctx.visibleSizeMs; });
            if (endIt != script.Selection().end())
                endIt += 1;

            constexpr auto selectedLines = IM_COL32(3, 194, 252, 255);

            const FunscriptAction* prev_action = nullptr;
            for (; startIt != endIt; startIt++) {
                auto&& action = *startIt;
                auto point = getPointForAction(ctx, action);

                if (prev_action != nullptr) {
                    // draw highlight line
                    if (SplineLines) {
                        auto p1 = getPointForAction(ctx, *prev_action);
                        float splineEasing = (p1.x - point.x) / SplineEasing;
                        ctx.draw_list->AddBezierCurve(p1, p1 - ImVec2(splineEasing, 0.f), point + ImVec2(splineEasing, 0.f), point, selectedLines, 3.0f);
                    }
                    else {
                        ctx.draw_list->AddLine(getPointForAction(ctx, *prev_action), point, selectedLines, 3.0f);
                    }
                }

                SelectedActionScreenCoordinates.emplace_back(point);
                prev_action = &action;
            }
        }
    }
}

void BaseOverlay::DrawSecondsLabel(const OverlayDrawingCtx& ctx) noexcept
{
    auto& style = ImGui::GetStyle();
    if (ctx.scriptIdx == ctx.drawnScriptCount - 1) {
        char tmp[16];
        stbsp_snprintf(tmp, sizeof(tmp), "%.2f seconds", ctx.visibleSizeMs / 1000.f);
        auto textSize = ImGui::CalcTextSize(tmp);
        ctx.draw_list->AddText(
            ctx.canvas_pos + ImVec2(style.FramePadding.x, ctx.canvas_size.y - textSize.y - style.FramePadding.y),
            ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
            tmp
        );
    }
}

void BaseOverlay::DrawHeightLines(const OverlayDrawingCtx& ctx) noexcept
{
    // height indicators
    for (int i = 0; i < 9; i++) {
        auto color = (i == 4) ? IM_COL32(150, 150, 150, 255) : IM_COL32(80, 80, 80, 255);
        auto thickness = (i == 4) ? 2.f : 1.0f;
        ctx.draw_list->AddLine(
            ctx.canvas_pos + ImVec2(0.0, (ctx.canvas_size.y / 10.f) * (i + 1)),
            ctx.canvas_pos + ImVec2(ctx.canvas_size.x, (ctx.canvas_size.y / 10.f) * (i + 1)),
            color,
            thickness
        );
    }
}

void BaseOverlay::DrawScriptLabel(const OverlayDrawingCtx& ctx) noexcept
{
    auto& style = ImGui::GetStyle();
    auto app = OpenFunscripter::ptr;
    if (app->LoadedFunscripts.size() > 1) {
        auto& title = app->LoadedFunscripts[ctx.scriptIdx]->metadata.title;
        auto textSize = ImGui::CalcTextSize(title.c_str());
        ctx.draw_list->AddText(
            ctx.canvas_pos + ctx.canvas_size - style.FramePadding - textSize,
            ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
            title.c_str()
        );
    }
}
