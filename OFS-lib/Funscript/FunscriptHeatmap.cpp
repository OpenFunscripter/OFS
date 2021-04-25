#include "FunscriptHeatmap.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include <array>

ImGradient HeatmapGradient::Colors;

void HeatmapGradient::Init() noexcept
{
    if (!Colors.getMarks().empty()) return;
    std::array<ImColor, 6> heatColor{
        IM_COL32(0x00, 0x00, 0x00, 0xFF),
        IM_COL32(0x1E, 0x90, 0xFF, 0xFF),
        IM_COL32(0x00, 0xFF, 0xFF, 0xFF),
        IM_COL32(0x00, 0xFF, 0x00, 0xFF),
        IM_COL32(0xFF, 0xFF, 0x00, 0xFF),
        IM_COL32(0xFF, 0x00, 0x00, 0xFF),
    };

    float pos = 0.0f;
    for (auto& col : heatColor) {
        Colors.addMark(pos, col);
        pos += (1.f / (heatColor.size() - 1));
    }
    Colors.refreshCache();
}

HeatmapGradient::HeatmapGradient() noexcept
{
    Gradient.clear();
    Gradient.addMark(0.f, IM_COL32(0, 0, 0, 255));
    Gradient.addMark(1.f, IM_COL32(0, 0, 0, 255));
    Gradient.refreshCache();
}

void HeatmapGradient::Update(float totalDuration, const FunscriptArray& actions) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImColor BackgroundColor(0.f, 0.f, 0.f, 1.f);

    Gradient.clear();
    Gradient.addMark(0.f, BackgroundColor);
    Gradient.addMark(1.f, BackgroundColor);
    if (actions.empty()) { Gradient.refreshCache(); return; }

    constexpr float GapDuration = 10.f;
    constexpr float MinSegmentTime = 2.f;
    constexpr int32_t MaxSegments = 150;
    int SegmentCount = Util::Clamp((int32_t)std::round(totalDuration / MinSegmentTime), 1, MaxSegments);
    Speeds.clear(); Speeds.resize(SegmentCount, 0.f);
    
    float SegmentDuration = totalDuration / MaxSegments;

    auto lastAction = actions.front();
    for (int i = 1; i < actions.size(); ++i) {
        auto& action = actions[i];
        float duration = action.atS - lastAction.atS;
        float length = std::abs(action.pos - lastAction.pos);
        float speed = length / duration; // speed
        speed = Util::Clamp(speed / 530.f, 0.f, 1.f);

        int segmentIdx = Util::Clamp((action.atS + (duration / 2.f)) / totalDuration, 0.f, 1.f) * (Speeds.size()-1);
        auto& segment = Speeds[segmentIdx];
        if (segment > 0.f) {
            segment += speed;
            segment /= 2.f;
        } 
        else {
            segment = speed;
        }
        lastAction = action;
    }

    ImColor color(0.f, 0.f, 0.f, 1.f);
    for (int i = 0; i < Speeds.size(); ++i) {
        float speed = Speeds[i];
        //if (i > 0) {
        //    speed += Speeds[i - 1];
        //    speed /= 2.f;
        //}
        Colors.getColorAt(speed, &color.Value.x);
        float pos = (float)i/Speeds.size();
        Gradient.addMark(pos, color);
    }
    Gradient.refreshCache();
}
