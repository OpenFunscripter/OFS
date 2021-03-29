#include "ScriptPositionsOverlayMode.h"
#include "OFS_ScriptTimeline.h"

#include <cmath>

ImGradient BaseOverlay::speedGradient;
std::vector<BaseOverlay::ColoredLine> BaseOverlay::ColoredLines;
std::vector<ImVec2> BaseOverlay::SelectedActionScreenCoordinates;
std::vector<ImVec2> BaseOverlay::ActionScreenCoordinates;
std::vector<FunscriptAction> BaseOverlay::ActionPositionWindow;
bool BaseOverlay::SplineMode = true;
bool BaseOverlay::ShowActions = true;

BaseOverlay::BaseOverlay(ScriptTimeline* timeline) noexcept
{
    this->timeline = timeline;

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

}

void EmptyOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    timeline->DrawAudioWaveform(ctx);
    BaseOverlay::DrawActionLines(ctx);
}

float EmptyOverlay::steppingIntervalForward(float fromMs) noexcept
{
    return timeline->frameTimeMs;
}

float EmptyOverlay::steppingIntervalBackward(float fromMs) noexcept
{
    return -timeline->frameTimeMs;
}

void BaseOverlay::DrawActionLines(const OverlayDrawingCtx& ctx) noexcept
{
    if (!BaseOverlay::ShowActions) return;
    auto& script = *ctx.script;

    auto startIt = script.Actions().begin() + ctx.actionFromIdx;
    auto endIt = script.Actions().begin() + ctx.actionToIdx;
    ColoredLines.clear();

    auto getPointForAction = [](const OverlayDrawingCtx& ctx, FunscriptAction action) {
        float relative_x = (float)(action.at - ctx.offset_ms) / ctx.visibleSizeMs;
        float x = (ctx.canvas_size.x) * relative_x;
        float y = (ctx.canvas_size.y) * (1 - (action.pos / 100.f));
        x += ctx.canvas_pos.x;
        y += ctx.canvas_pos.y;
        return ImVec2(x, y);
    };

    auto drawSpline = [getPointForAction](const OverlayDrawingCtx& ctx, FunscriptAction startAction, FunscriptAction endAction, uint32_t color, float width, bool background = true)
    {
        constexpr float SamplesPerTwothousandPixels = 150.f;
        const float MaximumSamples = SamplesPerTwothousandPixels * (ctx.canvas_size.x / 2000.f);

        auto getPointForTimePos = [](const OverlayDrawingCtx& ctx, float timeMs, float pos) noexcept {
            float relative_x = (float)(timeMs - ctx.offset_ms) / ctx.visibleSizeMs;
            float x = (ctx.canvas_size.x) * relative_x;
            float y = (ctx.canvas_size.y) * (1 - (pos / 100.f));
            x += ctx.canvas_pos.x;
            y += ctx.canvas_pos.y;
            return ImVec2(x, y);
        };
        auto putPoint = [getPointForTimePos](auto& ctx, float timeMs) noexcept {
            float pos = Util::Clamp<float>(ctx.script->Spline(timeMs) * 100.f, 0.f, 100.f);
            ctx.draw_list->PathLineTo(getPointForTimePos(ctx, timeMs, pos));
        };

        ctx.draw_list->PathClear();
        float visibleDuration;
        float currentTime;
        float endTime;
        if (startAction.at >= ctx.offset_ms && endAction.at <= (ctx.offset_ms + ctx.visibleSizeMs)) {
            currentTime = startAction.at;
            endTime = endAction.at;
            visibleDuration = endTime - currentTime;
        }
        else if (startAction.at < ctx.offset_ms && endAction.at > (ctx.offset_ms + ctx.visibleSizeMs)) {
            // clip at the invisible area in both direction
            currentTime = ctx.offset_ms;
            endTime = (ctx.offset_ms + ctx.visibleSizeMs) + 1.f;
            visibleDuration = ctx.visibleSizeMs;
        }
        else if (startAction.at < ctx.offset_ms) {
            // clip invisible area on the left
            currentTime = ctx.offset_ms;
            endTime = endAction.at;
            visibleDuration = endAction.at - ctx.offset_ms;
        }
        else if (endAction.at > (ctx.offset_ms + ctx.visibleSizeMs)) {
            // clip invisble area on the right
            currentTime = startAction.at;
            endTime = (ctx.offset_ms + ctx.visibleSizeMs) + 1.f;
            visibleDuration = endTime - startAction.at;
        }

        // detail gets dynamically reduced by increasing the timeStep,
        // at which is being sampled from the spline
        const float ratio = visibleDuration / ctx.visibleSizeMs;
        const float SampleCount = MaximumSamples * ratio;

        const float timeStep = visibleDuration / SampleCount;
        
        if (SampleCount < 3.f) {
            auto p1 = getPointForAction(ctx, startAction);
            auto p2 = getPointForAction(ctx, endAction);
            if (background) {
                ctx.draw_list->PathLineTo(p1);
                ctx.draw_list->PathLineTo(p2);
                ctx.draw_list->PathStroke(IM_COL32_BLACK, false, 7.f);
            }
            ColoredLines.emplace_back(std::move(BaseOverlay::ColoredLine{ p1, p2, color }));
        }
        else {
            putPoint(ctx, currentTime);
            currentTime += timeStep;
            while (currentTime < endTime) {
                putPoint(ctx, currentTime);
                currentTime += timeStep;
            }
            putPoint(ctx, endAction.at);
            auto tmpSize = ctx.draw_list->_Path.Size;
            ctx.draw_list->PathStroke(IM_COL32_BLACK, false, 7.f);
            ctx.draw_list->_Path.Size = tmpSize;
            ctx.draw_list->PathStroke(color, false, width);
        }
    };

    if (SplineMode) {
        const FunscriptAction* prevAction = nullptr;
        for (; startIt != endIt; startIt++) {
            auto& action = *startIt;
            auto p1 = getPointForAction(ctx, action);
            ActionScreenCoordinates.emplace_back(p1);
            ActionPositionWindow.emplace_back(action);

            if (prevAction != nullptr) {
                // calculate speed relative to maximum speed
                float rel_speed = Util::Clamp<float>((std::abs(action.pos - prevAction->pos) / ((action.at - prevAction->at) / 1000.0f)) / max_speed_per_seconds, 0.f, 1.f);
                ImColor speed_color;
                speedGradient.getColorAt(rel_speed, &speed_color.Value.x);
                speed_color.Value.w = 1.f;

                drawSpline(ctx, *prevAction, action, ImGui::ColorConvertFloat4ToU32(speed_color), 3.f);
            }
            prevAction = &action;
        }
    }
    else {
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
                
                ctx.draw_list->AddLine(p1, p2, IM_COL32(0, 0, 0, 255), 7.0f); // border
                ColoredLines.emplace_back(std::move(BaseOverlay::ColoredLine{ p1, p2, ImGui::ColorConvertFloat4ToU32(speed_color) }));
            }

            prevAction = &action;
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
        if (SplineMode) {
            const FunscriptAction* prev_action = nullptr;
            for (; startIt != endIt; startIt++) {
                auto&& action = *startIt;
                auto point = getPointForAction(ctx, action);

                if (prev_action != nullptr) {
                    // draw highlight line
                    drawSpline(ctx, *prev_action, action, selectedLines, 3.f, false);
                }

                SelectedActionScreenCoordinates.emplace_back(point);
                prev_action = &action;
            }
        }
        else {
            const FunscriptAction* prev_action = nullptr;
            for (; startIt != endIt; startIt++) {
                auto&& action = *startIt;
                auto point = getPointForAction(ctx, action);

                if (prev_action != nullptr) {
                    // draw highlight line
                    //ctx.draw_list->AddLine(getPointForAction(ctx, *prev_action), point, selectedLines, 3.0f);
                    ColoredLines.emplace_back(std::move(BaseOverlay::ColoredLine{ getPointForAction(ctx, *prev_action), point, selectedLines }));
                }

                SelectedActionScreenCoordinates.emplace_back(point);
                prev_action = &action;
            }
        }
    }

    // this is so that the black background line gets rendered first
    for (auto&& line : ColoredLines) {
        ctx.draw_list->AddLine(line.p1, line.p2, line.color, 3.f);
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
    auto& title = ctx.script->metadata.title;
    auto textSize = ImGui::CalcTextSize(title.c_str());
    ctx.draw_list->AddText(
        ctx.canvas_pos + ctx.canvas_size - style.FramePadding - textSize,
        ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
        title.c_str()
    );
}
