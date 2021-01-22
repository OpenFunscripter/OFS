#pragma once

#include "FunscriptAction.h"

#include <vector>
#include <map>
#include "glm/gtx/spline.hpp"

class FunscriptSpline
{
	std::map<int32_t, int32_t> SplineActionMap; // maps timeMs to Index in actions vector
	int32_t cacheIdx = 0;

	inline auto catmull_rom_spline(const std::vector<FunscriptAction>& actions, int32_t i, float ms) const noexcept
	{
		int i0 = glm::clamp<int>(i - 1, 0, actions.size() - 1);
		int i1 = glm::clamp<int>(i, 0, actions.size() - 1);
		int i2 = glm::clamp<int>(i + 1, 0, actions.size() - 1);
		int i3 = glm::clamp<int>(i + 2, 0, actions.size() - 1);

		glm::vec2 v0(actions[i0].at / (float)actions.back().at, actions[i0].pos / 100.f);
		glm::vec2 v1(actions[i1].at / (float)actions.back().at, actions[i1].pos / 100.f);
		glm::vec2 v2(actions[i2].at / (float)actions.back().at, actions[i2].pos / 100.f);
		glm::vec2 v3(actions[i3].at / (float)actions.back().at, actions[i3].pos / 100.f);

		float t = ms;
		t -= actions[i1].at;
		t /= actions[i2].at - actions[i1].at;

		return glm::catmullRom(v0, v1, v2, v3, t).y;
	}

public:

	inline void Update(const std::vector<FunscriptAction>& actions) noexcept
	{
		SplineActionMap.clear();
		for (int i = 0; i < actions.size(); i++)
		{
			SplineActionMap.insert(std::make_pair(actions[i].at, i));
		}
	}

	inline float Sample(const std::vector<FunscriptAction>& actions, float timeMs) noexcept 
	{
		if (actions.size() == 0) { return NAN; }
		else if (actions.size() == 1) { return actions.front().pos; }
		else if (cacheIdx + 1 >= actions.size()) { cacheIdx = 0; }

		if (actions[cacheIdx].at <= timeMs && actions[cacheIdx + 1].at >= timeMs)
		{
			// cache hit!
			return catmull_rom_spline(actions, cacheIdx, timeMs);
		}
		else if (cacheIdx + 2 < actions.size() && actions[cacheIdx+1].at <= timeMs && actions[cacheIdx+2].at >= timeMs)
		{
			// sort of a cache hit
			cacheIdx += 1;
			return catmull_rom_spline(actions, cacheIdx, timeMs);
		}
		else
		{
			// cache miss
			// lookup index
			auto it = SplineActionMap.upper_bound((int32_t)timeMs);
			if (it == SplineActionMap.end()) { 
				return actions.back().pos / 100.f; 
			}
			else if (it == SplineActionMap.begin())
			{
				return actions.front().pos / 100.f;
			}

			it--;
			// cache index
			cacheIdx = it->second;
			return catmull_rom_spline(actions, it->second, timeMs);
		}

		// I don't know if I like this probably better to just return 0.f
		return NAN;
	}

	inline float SampleAtIndex(const std::vector<FunscriptAction>& actions, int32_t index, float timeMs) const noexcept
	{
		if (actions.size() == 0) { return NAN; }
		if (index + 1 < actions.size())
		{
			if (actions[index].at <= timeMs && actions[index + 1].at >= timeMs)
			{
				return catmull_rom_spline(actions, index, timeMs);
			}
		}

		return NAN;
	}
};