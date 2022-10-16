#include "ScriptPositionsOverlayMode.h"
#include "OFS_ScriptTimeline.h"
#include "OFS_Profiling.h"
#include "OFS_Localization.h"
#include "FunscriptHeatmap.h"

#include <cmath>

ImGradient BaseOverlay::speedGradient;
std::vector<BaseOverlay::ColoredLine> BaseOverlay::ColoredLines;
std::vector<ImVec2> BaseOverlay::SelectedActionScreenCoordinates;
std::vector<ImVec2> BaseOverlay::ActionScreenCoordinates;
std::vector<FunscriptAction> BaseOverlay::ActionPositionWindow;
float BaseOverlay::PointSize = 7.f;
bool BaseOverlay::SplineMode = true;
bool BaseOverlay::ShowActions = true;
bool BaseOverlay::SyncLineEnable = false;

bool BaseOverlay::ShowMaxSpeedHighlight = false;
ImColor BaseOverlay::MaxSpeedColor = ImColor(0, 0, 255, 255);
float BaseOverlay::MaxSpeedPerSecond = 400.f;

BaseOverlay::BaseOverlay(ScriptTimeline* timeline) noexcept
{
    OFS_PROFILE(__FUNCTION__);
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
    OFS_PROFILE(__FUNCTION__);
    ActionScreenCoordinates.clear();
    ActionPositionWindow.clear();
    SelectedActionScreenCoordinates.clear();
}

void BaseOverlay::DrawSettings() noexcept
{

}

float BaseOverlay::logicalFrameTime(float realFrameTime) noexcept
{
    return realFrameTime;
}

void EmptyOverlay::DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    timeline->DrawAudioWaveform(ctx);
    BaseOverlay::DrawActionLines(ctx);
}

float EmptyOverlay::steppingIntervalForward(float realFrameTime, float fromTime) noexcept
{
    return realFrameTime;
}

float EmptyOverlay::steppingIntervalBackward(float realFrameTime, float fromTime) noexcept
{
    return -realFrameTime;
}

static void getActionLineColor(ImColor* speedColor, ImGradient& speedGradient, FunscriptAction action, FunscriptAction prevAction) noexcept
{
    float speed = std::abs(action.pos - prevAction.pos) / ((action.atS - prevAction.atS));
    float relSpeed = Util::Clamp<float>(speed / HeatmapGradient::MaxSpeedPerSecond, 0.f, 1.f);
    if(BaseOverlay::ShowMaxSpeedHighlight && speed >= BaseOverlay::MaxSpeedPerSecond) {
        *speedColor = BaseOverlay::MaxSpeedColor;
        return;
    }
    speedGradient.getColorAt(relSpeed, &speedColor->Value.x);
    speedColor->Value.w = 1.f;
}

void BaseOverlay::DrawActionLines(const OverlayDrawingCtx& ctx) noexcept
{
    if (!BaseOverlay::ShowActions) return;
    OFS_PROFILE(__FUNCTION__);
    auto& script = *ctx.script;

    auto startIt = script.Actions().begin() + ctx.actionFromIdx;
    auto endIt = script.Actions().begin() + ctx.actionToIdx;
    ColoredLines.clear();

    auto getPointForAction = [](const OverlayDrawingCtx& ctx, FunscriptAction action) {
        float relative_x = (float)(action.atS - ctx.offsetTime) / ctx.visibleTime;
        float x = (ctx.canvasSize.x) * relative_x;
        float y = (ctx.canvasSize.y) * (1 - (action.pos / 100.f));
        x += ctx.canvasPos.x;
        y += ctx.canvasPos.y;
        return ImVec2(x, y);
    };

    auto drawSpline = [getPointForAction](const OverlayDrawingCtx& ctx, FunscriptAction startAction, FunscriptAction endAction, uint32_t color, float width, bool background = true)
    {
        constexpr float SamplesPerTwothousandPixels = 150.f;
        const float MaximumSamples = SamplesPerTwothousandPixels * (ctx.canvasSize.x / 2000.f);

        auto getPointForTimePos = [](const OverlayDrawingCtx& ctx, float time, float pos) noexcept {
            float relative_x = (float)(time - ctx.offsetTime) / ctx.visibleTime;
            float x = (ctx.canvasSize.x) * relative_x;
            float y = (ctx.canvasSize.y) * (1 - (pos / 100.f));
            x += ctx.canvasPos.x;
            y += ctx.canvasPos.y;
            return ImVec2(x, y);
        };
        auto putPoint = [getPointForTimePos](auto& ctx, float time) noexcept {
            float pos = Util::Clamp<float>(ctx.script->Spline(time) * 100.f, 0.f, 100.f);
            ctx.drawList->PathLineTo(getPointForTimePos(ctx, time, pos));
        };

        ctx.drawList->PathClear();
        float visibleDuration;
        float currentTime;
        float endTime;
        if (startAction.atS >= ctx.offsetTime && endAction.atS <= (ctx.offsetTime + ctx.visibleTime)) {
            currentTime = startAction.atS;
            endTime = endAction.atS;
            visibleDuration = endTime - currentTime;
        }
        else if (startAction.atS < ctx.offsetTime && endAction.atS > (ctx.offsetTime + ctx.visibleTime)) {
            // clip at the invisible area in both direction
            currentTime = ctx.offsetTime;
            endTime = (ctx.offsetTime + ctx.visibleTime);
            visibleDuration = ctx.visibleTime;
        }
        else if (startAction.atS < ctx.offsetTime) {
            // clip invisible area on the left
            currentTime = ctx.offsetTime;
            endTime = endAction.atS;
            visibleDuration = endAction.atS - ctx.offsetTime;
        }
        else if (endAction.atS > (ctx.offsetTime + ctx.visibleTime)) {
            // clip invisble area on the right
            currentTime = startAction.atS;
            endTime = (ctx.offsetTime + ctx.visibleTime) + 0.001f;
            visibleDuration = endTime - startAction.atS;
        }

        // detail gets dynamically reduced by increasing the timeStep,
        // at which is being sampled from the spline
        const float ratio = visibleDuration / ctx.visibleTime;
        const float SampleCount = MaximumSamples * ratio;

        const float timeStep = visibleDuration / SampleCount;
        
        if (SampleCount < 3.f) {
            auto p1 = getPointForAction(ctx, startAction);
            auto p2 = getPointForAction(ctx, endAction);
            if (background) {
                ctx.drawList->PathLineTo(p1);
                ctx.drawList->PathLineTo(p2);
                ctx.drawList->PathStroke(IM_COL32_BLACK, false, 7.f);
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
            putPoint(ctx, endAction.atS);
            auto tmpSize = ctx.drawList->_Path.Size;
            ctx.drawList->PathStroke(IM_COL32_BLACK, false, 7.f);
            ctx.drawList->_Path.Size = tmpSize;
            ctx.drawList->PathStroke(color, false, width);
        }
    };

    auto drawLine = [](const OverlayDrawingCtx& ctx, ImVec2 p1, ImVec2 p2, uint32_t color) noexcept
    {
        ctx.drawList->AddLine(p1, p2, IM_COL32(0, 0, 0, 255), 7.0f); // border
        ColoredLines.emplace_back(std::move(BaseOverlay::ColoredLine{ p1, p2, color }));
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
                ImColor speedColor;
                getActionLineColor(&speedColor, speedGradient, action, *prevAction);
                drawSpline(ctx, *prevAction, action, ImGui::ColorConvertFloat4ToU32(speedColor), 3.f);
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
                ImColor speedColor;
                getActionLineColor(&speedColor, speedGradient, action, *prevAction);
                drawLine(ctx, p1, p2, ImGui::ColorConvertFloat4ToU32(speedColor));
            }

            prevAction = &action;
        }

    }

    if (script.HasSelection()) {
        auto startIt = std::find_if(script.Selection().begin(), script.Selection().end(),
            [&](auto act) { return act.atS >= ctx.offsetTime; });
        if (startIt != script.Selection().begin())
            startIt -= 1;

        auto endIt = std::find_if(startIt, script.Selection().end(),
            [&](auto act) { return act.atS >= ctx.offsetTime + ctx.visibleTime; });
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
        ctx.drawList->AddLine(line.p1, line.p2, line.color, 3.f);
    }
}

void BaseOverlay::DrawSecondsLabel(const OverlayDrawingCtx& ctx) noexcept
{
    auto& style = ImGui::GetStyle();
    if (ctx.scriptIdx == ctx.drawnScriptCount - 1) {
        OFS_PROFILE(__FUNCTION__);
        auto tmp = FMT("%.2f %s", ctx.visibleTime, TR(TIMELINE_SECONDS));
        auto textSize = ImGui::CalcTextSize(tmp);
        ctx.drawList->AddText(
            ctx.canvasPos + ImVec2(style.FramePadding.x, ctx.canvasSize.y - textSize.y - style.FramePadding.y),
            ImGui::GetColorU32(ImGuiCol_Text),
            tmp
        );
    }
}

void BaseOverlay::DrawHeightLines(const OverlayDrawingCtx& ctx) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    // height indicators
    for (int i = 0; i < 9; i++) {
        auto color = (i == 4) ? IM_COL32(150, 150, 150, 255) : IM_COL32(80, 80, 80, 255);
        auto thickness = (i == 4) ? 2.f : 1.0f;
        ctx.drawList->AddLine(
            ctx.canvasPos + ImVec2(0.0, (ctx.canvasSize.y / 10.f) * (i + 1)),
            ctx.canvasPos + ImVec2(ctx.canvasSize.x, (ctx.canvasSize.y / 10.f) * (i + 1)),
            color,
            thickness
        );
    }
}

void BaseOverlay::DrawScriptLabel(const OverlayDrawingCtx& ctx) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& style = ImGui::GetStyle();
    auto& title = ctx.script->Title;
    auto textSize = ImGui::CalcTextSize(title.c_str());
    ctx.drawList->AddText(
        ctx.canvasPos + ctx.canvasSize - style.FramePadding - textSize,
        ImGui::GetColorU32(ImGuiCol_Text),
        title.c_str()
    );
}
