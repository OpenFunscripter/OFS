#pragma once
#include "OFS_Profiling.h"
#include "OFS_Util.h"

#include <vector>
#include "imgui.h"

// lifted from: https://gist.github.com/galloscript/8a5d179e432e062550972afcd1ecf112
// rewritten for my use case

struct ImGradientMark
{
    float color[4];
    float position; //0 to 1

    ImGradientMark(float r, float g, float b, float a, float pos) noexcept {
        color[0] = r;
        color[1] = g;
        color[2] = b;
        color[3] = a;
        position = pos;
    }

    inline bool operator==(const ImGradientMark& b) const noexcept {
        return this->position == b.position
            && this->color[0] == b.color[0]
            && this->color[1] == b.color[1]
            && this->color[2] == b.color[2]
            && this->color[3] == b.color[3];
    }
};

class ImGradient
{
public:
    ImGradient() noexcept {}
    ~ImGradient() noexcept {};

    inline void getColorAt(float position, float* color) const noexcept
    {
        OFS_PROFILE(__FUNCTION__);
        position = Util::Clamp(position, 0.f, 1.f);
        int cachePos = (position * 255);
        cachePos *= 3;
        color[0] = m_cachedValues[cachePos + 0];
        color[1] = m_cachedValues[cachePos + 1];
        color[2] = m_cachedValues[cachePos + 2];
    }

    void addMark(float position, const ImColor& color) noexcept;
    void removeMark(const ImGradientMark& mark) noexcept;
    void refreshCache() noexcept;
    void clear() noexcept { m_marks.clear(); }
    const std::vector<ImGradientMark>& getMarks() noexcept { return m_marks; }

    static void DrawGradientBar(ImGradient* gradient,const ImVec2& bar_pos, float maxWidth, float height) noexcept;

    void computeColorAt(float position, float* color) const noexcept;
private:
    std::vector<ImGradientMark> m_marks;
    float m_cachedValues[256 * 3];
};

