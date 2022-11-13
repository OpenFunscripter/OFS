#pragma once
#include "GradientBar.h"
#include "FunscriptAction.h"

class FunscriptHeatmap
{
public:
	static constexpr float MaxSpeedPerSecond = 530.f; // arbitrarily choosen maximum tuned for coloring

	static ImGradient LineColors;
	static ImGradient Colors;

	static void Init() noexcept;

	std::vector<float> Speeds;
	ImGradient Gradient;

	FunscriptHeatmap() noexcept;

	void GetColorForSpeed(float speed, ImColor* colorOut) const noexcept;
	void DrawHeatmap(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) noexcept;
	void Update(float totalDuration , const FunscriptArray& actions) noexcept;
};
