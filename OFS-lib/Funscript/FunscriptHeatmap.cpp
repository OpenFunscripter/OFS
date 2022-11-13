#include "FunscriptHeatmap.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include <array>

ImGradient FunscriptHeatmap::Colors;
ImGradient FunscriptHeatmap::LineColors;

void FunscriptHeatmap::Init() noexcept
{
    if (Colors.getMarks().empty()) 
    {
        std::array<ImColor, 6> heatColor{
            IM_COL32(0x00, 0x00, 0x00, 0xFF),
            IM_COL32(0x1E, 0x90, 0xFF, 0xFF),
            IM_COL32(0x00, 0xFF, 0xFF, 0xFF),
            IM_COL32(0x00, 0xFF, 0x00, 0xFF),
            IM_COL32(0xFF, 0xFF, 0x00, 0xFF),
            IM_COL32(0xFF, 0x00, 0x00, 0xFF),
        };
        Colors.addMark(0.f, heatColor[0]);
        Colors.addMark(std::nextafterf(0.f, 1.f), heatColor[1]);
        Colors.addMark(2.f/(heatColor.size()-1), heatColor[2]);
        Colors.addMark(3.f/(heatColor.size()-1), heatColor[3]);
        Colors.addMark(4.f/(heatColor.size()-1), heatColor[4]);
        Colors.addMark(5.f/(heatColor.size()-1), heatColor[5]);
        Colors.refreshCache();
    }
    if(LineColors.getMarks().empty())
    {
        std::array<ImColor, 4> heatColor {
            IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
            IM_COL32(0x66, 0xff, 0x00, 0xFF),
            IM_COL32(0xFF, 0xff, 0x00, 0xFF),
            IM_COL32(0xFF, 0x00, 0x00, 0xFF),
        };
        float pos = 0.0f;
        for (auto& col : heatColor) {
            LineColors.addMark(pos, col);
            pos += (1.f / (heatColor.size() - 1));
        }
        LineColors.refreshCache();
    }
}

FunscriptHeatmap::FunscriptHeatmap() noexcept
{
    Gradient.clear();
    Gradient.addMark(0.f, IM_COL32(0, 0, 0, 255));
    Gradient.addMark(1.f, IM_COL32(0, 0, 0, 255));
    Gradient.refreshCache();
}

void FunscriptHeatmap::Update(float totalDuration, const FunscriptArray& actions) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ImColor BackgroundColor(0.f, 0.f, 0.f, 1.f);

    Gradient.clear();
    Gradient.addMark(0.f, BackgroundColor);
    Gradient.addMark(1.f, BackgroundColor);
    if (actions.empty()) { Gradient.refreshCache(); return; }

    constexpr float GapDuration = 10.f;
    constexpr float MinSegmentTime = 2.f;
    constexpr int32_t MaxSegments = 200;
    int SegmentCount = Util::Clamp((int32_t)std::round(totalDuration / MinSegmentTime), 1, MaxSegments);
    Speeds.clear(); Speeds.resize(SegmentCount, 0.f);
    
    float SegmentDuration = totalDuration / MaxSegments;

    auto lastAction = actions.front();
    for (int i = 1; i < actions.size(); ++i) {
        auto& action = actions[i];
        float duration = action.atS - lastAction.atS;
        assert(duration > 0.f);
        float length = std::abs(action.pos - lastAction.pos);
        float speed = length / duration; // speed
        speed = Util::Clamp(speed / MaxSpeedPerSecond, 0.f, 1.f);

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

    float offset = (1.f/Speeds.size())/2.f;
    ImColor color(0.f, 0.f, 0.f, 1.f);
    for (int i = 0; i < Speeds.size(); ++i) {
        float speed = Speeds[i];
        Colors.getColorAt(speed, &color.Value.x);
        float pos = ((float)(i+1)/Speeds.size()) + offset;
        Gradient.addMark(pos, color);
    }
    Gradient.refreshCache();
}

void FunscriptHeatmap::GetColorForSpeed(float speed, ImColor* colorOut) const noexcept
{
    float relSpeed = Util::Clamp<float>(speed / FunscriptHeatmap::MaxSpeedPerSecond, 0.f, 1.f);
    Colors.getColorAt(relSpeed, &colorOut->Value.x);
}

void FunscriptHeatmap::DrawHeatmap(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) noexcept
{
    ImGradient::DrawGradientBar(&Gradient, min, max.x - min.x, max.y - min.y);
}