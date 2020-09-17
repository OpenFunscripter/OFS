#pragma once

#include <vector>

#include "imgui.h"

// lifted from: https://gist.github.com/galloscript/8a5d179e432e062550972afcd1ecf112
// rewritten for my usecase

struct ImGradientMark
{
    float color[4];
    float position; //0 to 1

    ImGradientMark(float r, float g, float b, float a, float pos) {
        color[0] = r;
        color[1] = g;
        color[2] = b;
        color[3] = a;
        position = pos;
    }

    inline bool operator==(const ImGradientMark& b) const {
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
    ImGradient() {}
    ~ImGradient();

    void getColorAt(float position, float* color) const noexcept;
    void addMark(float position, ImColor const color) noexcept;
    void removeMark(const ImGradientMark& mark) noexcept;
    void refreshCache() noexcept;
    void clear() noexcept { m_marks.clear(); }
    std::vector<ImGradientMark>& getMarks() noexcept { return m_marks; }

    static void DrawGradientBar(ImGradient* gradient,const ImVec2& bar_pos, float maxWidth, float height) noexcept;

    void computeColorAt(float position, float* color) const noexcept;
private:
    std::vector<ImGradientMark> m_marks;
    float m_cachedValues[256 * 3];
};

