#pragma once
#include "GradientBar.h"
#include "Funscript.h"

class FunscriptHeatmap
{
public:
	static constexpr float MaxSpeedPerSecond = 400.f;

	static ImGradient LineColors;
	static ImGradient Colors;

	static void Init() noexcept;

	uint32_t speedTexture = 0;

	FunscriptHeatmap() noexcept;
	void ShowMenu() noexcept;

	void DrawHeatmap(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) noexcept;
	void Update(float totalDuration , const FunscriptArray& actions) noexcept;
};
