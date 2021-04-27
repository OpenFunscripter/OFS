#pragma once
#include "GradientBar.h"
#include "FunscriptAction.h"

class HeatmapGradient
{
public:
	static constexpr float MaxSpeedPerSecond = 530.f; // arbitrarily choosen maximum tuned for coloring

	static ImGradient Colors;
	static void Init() noexcept;

	std::vector<float> Speeds;
	ImGradient Gradient;

	HeatmapGradient() noexcept;
	void Update(float totalDuration , const FunscriptArray& actions) noexcept;
};
