#include "FunscriptHeatmap.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include <array>

void OFS::UpdateHeatmapGradient(float totalDuration, ImGradient& grad, const FunscriptArray& actions) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    grad.clear();
    grad.addMark(0.f, IM_COL32(0, 0, 0, 255));
    grad.addMark(1.f, IM_COL32(0, 0, 0, 255));

    if (actions.size() == 0) {
        return;
    }

    std::array<ImColor, 6> heatColor {
        IM_COL32(0x00, 0x00, 0x00, 0xFF),
        IM_COL32(0x1E, 0x90, 0xFF, 0xFF),
        IM_COL32(0x00, 0xFF, 0xFF, 0xFF),
        IM_COL32(0x00, 0xFF, 0x00, 0xFF),
        IM_COL32(0xFF, 0xFF, 0x00, 0xFF),
        IM_COL32(0xFF, 0x00, 0x00, 0xFF),
    };


    ImGradient HeatMap;
    float pos = 0.0f;
    for (auto& col : heatColor) {
        HeatMap.addMark(pos, col);
        pos += (1.f / (heatColor.size() - 1));
    }
    HeatMap.refreshCache();

    auto getSegments = [](const FunscriptArray& actions, float gapDuration) -> std::vector<std::vector<FunscriptAction>> {
        int prev_direction = 0; // 0 neutral 0< up 0> down
        std::vector<std::vector<FunscriptAction>> segments;
        {
            FunscriptAction previous(0, 0);

            for (auto& action : actions)
            {
                if (previous.pos == action.pos) {
                    continue;
                }

                // filter out actions which don't change direction
                int direction = action.pos - previous.pos;
                if (direction > 0 && prev_direction > 0) {
                    previous = action;
                    continue;
                }
                else if (direction < 0 && prev_direction < 0) {
                    previous = action;
                    continue;
                }

                prev_direction = direction;

                if (action.atS - previous.atS >= gapDuration) {
                    segments.emplace_back();
                }
                if (segments.size() == 0) { segments.emplace_back(); }
                segments.back().emplace_back(action);

                previous = action;
            }

            return segments;
        }
    };


    // this comes fairly close to what ScriptPlayer's heatmap looks like
    constexpr float kernelSizeTime = 2.5f;
    constexpr float maxActionsInKernel = 24.5f / (5.f / kernelSizeTime);

    ImColor color(0.f, 0.f, 0.f, 1.f);

    constexpr int32_t max_samples = 3;
    std::vector<float> samples;
    samples.reserve(max_samples);

    auto segments = getSegments(actions, 10);
    for (auto& segment : segments) {
        const float duration = segment.back().atS - segment.front().atS;
        float kernelOffset = segment.front().atS;
        grad.addMark(kernelOffset / totalDuration, IM_COL32(0, 0, 0, 255));
        do {
            int actionsInKernel = 0;
            float kernelStart = kernelOffset;
            float kernelEnd = kernelOffset + kernelSizeTime;

            if (kernelOffset < segment.back().atS)
            {
                for (int i = 0; i < segment.size(); i++) {
                    auto& action = segment[i];
                    if (action.atS >= kernelStart && action.atS <= kernelEnd)
                        actionsInKernel++;
                    else if (action.atS > kernelEnd)
                        break;
                }
            }
            kernelOffset += kernelSizeTime;

            float actionsRelToMax = Util::Clamp((float)actionsInKernel / maxActionsInKernel, 0.0f, 1.0f);
            if (samples.size() == max_samples + 1) {
                samples.erase(samples.begin());
            }
            samples.emplace_back(actionsRelToMax);

            auto getAverage = [](std::vector<float>& samples) {
                float result = 0.f;
                for (auto&& sample : samples) {
                    result += sample;
                }
                result /= (float)samples.size();
                return result;
            };

            if (samples.size() > 1) {
                actionsRelToMax = getAverage(samples);
            }

            HeatMap.getColorAt(actionsRelToMax, (float*)&color.Value);
            float markPos = kernelOffset / totalDuration;
            grad.addMark(markPos, color);

        } while (kernelOffset < (segment.front().atS + duration));
        grad.addMark((kernelOffset + 1.f) / totalDuration, IM_COL32(0, 0, 0, 255));
    }
    grad.refreshCache();
}
