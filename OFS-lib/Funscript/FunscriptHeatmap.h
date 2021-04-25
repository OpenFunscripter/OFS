#pragma once
#include "GradientBar.h"
#include "FunscriptAction.h"

class HeatmapGradient
{
public:
	static ImGradient Colors;
	static void Init() noexcept;

	std::vector<float> Speeds;
	ImGradient Gradient;

	HeatmapGradient() noexcept;
	void Update(float totalDuration , const FunscriptArray& actions) noexcept;
};
