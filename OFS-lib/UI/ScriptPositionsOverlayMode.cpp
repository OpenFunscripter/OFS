#include "ScriptPositionsOverlayMode.h"
#include "OFS_ScriptTimeline.h"

ImGradient BaseOverlay::speedGradient;
std::vector<BaseOverlay::ColoredLine> BaseOverlay::ColoredLines;
std::vector<ImVec2> BaseOverlay::SelectedActionScreenCoordinates;
std::vector<ImVec2> BaseOverlay::ActionScreenCoordinates;
std::vector<FunscriptAction> BaseOverlay::ActionPositionWindow;
bool BaseOverlay::SplineMode = false;

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

void BaseOverlay::DrawActionLines(const OverlayDrawingCtx& ctx) noexcept
{
    if (ctx.actionToIdx - ctx.actionFromIdx > 1) {
        auto& script = *ctx.script;

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

        if (SplineMode)
        {
            constexpr int32_t MinSamplesPerSecond = 30;

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

                    ctx.draw_list->PathClear();
                    float currentTime = prevAction->at;
                    float endTime = action.at;
                    const float duration = (endTime - currentTime);
                    const float timeStep = duration / ((duration / 1.f) * (MinSamplesPerSecond/1000.f));

                    auto putPoint = [getPointForAction](auto& ctx, float timeMs) noexcept {
                        int32_t pos = Util::Clamp<int32_t>(std::round(ctx.script->Spline(timeMs) * 100.f), 0, 100);
                        ctx.draw_list->PathLineTo(getPointForAction(ctx, FunscriptAction(timeMs, pos)));
                    };

                    putPoint(ctx, currentTime);
                    currentTime += timeStep;
                    while (currentTime < endTime)
                    {
                        putPoint(ctx, currentTime);
                        currentTime += timeStep;
                    }
                    putPoint(ctx, endTime);

                    auto tmpSize = ctx.draw_list->_Path.Size;
                    ctx.draw_list->PathStroke(IM_COL32_BLACK, false, 3.f);
                    ctx.draw_list->_Path.Size = tmpSize;
                    ctx.draw_list->PathStroke(ImGui::ColorConvertFloat4ToU32(speed_color), false, 3.f);
                }

                prevAction = &action;
            }
        }
        else
        {
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

            // this is so that the black background line gets rendered first
            for (auto&& line : ColoredLines) {
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
                    ctx.draw_list->AddLine(getPointForAction(ctx, *prev_action), point, selectedLines, 3.0f);
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
    auto& title = ctx.script->metadata.title;
    auto textSize = ImGui::CalcTextSize(title.c_str());
    ctx.draw_list->AddText(
        ctx.canvas_pos + ctx.canvas_size - style.FramePadding - textSize,
        ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
        title.c_str()
    );
}
