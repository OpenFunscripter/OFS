#pragma once
#include "OFS_Profiling.h"
#include "FunscriptAction.h"
#include <vector>
#include "glm/gtx/spline.hpp"

#include "EASTL/vector_set.h"

class FunscriptSpline
{
	int32_t cacheIdx = 0;

	inline float catmull_rom_spline(const FunscriptArray& actions, int32_t i, float ms) const noexcept
	{
		OFS_PROFILE(__FUNCTION__);
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
	inline float Sample(const FunscriptArray& actions, float timeMs) noexcept 
	{
		OFS_PROFILE(__FUNCTION__);
		if (actions.size() == 0) { return 0.f; }
		else if (actions.size() == 1) { return actions.front().pos / 100.f; }
		else if (cacheIdx + 1 >= actions.size()) { cacheIdx = 0; }

		if (actions[cacheIdx].at <= timeMs && actions[cacheIdx + 1].at >= timeMs) {
			// cache hit!
			return catmull_rom_spline(actions, cacheIdx, timeMs);
		}
		else if (cacheIdx + 2 < actions.size() && actions[cacheIdx+1].at <= timeMs && actions[cacheIdx+2].at >= timeMs) {
			// sort of a cache hit
			cacheIdx += 1;
			return catmull_rom_spline(actions, cacheIdx, timeMs);
		}
		else {
			// cache miss
			// lookup index
			auto it = actions.upper_bound(FunscriptAction((int32_t)timeMs, 0));
			if (it == actions.end()) { 
				return actions.back().pos / 100.f; 
			}
			else if (it == actions.begin())			{
				return actions.front().pos / 100.f;
			}

			it--;
			// cache index
			cacheIdx = std::distance(actions.begin(), it);
			return catmull_rom_spline(actions, cacheIdx, timeMs);
		}

		return 0.f;
	}

	inline float SampleAtIndex(const FunscriptArray& actions, int32_t index, float timeMs) const noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (actions.size() == 0) { return 0.f; }
		if (index + 1 < actions.size())	{
			if (actions[index].at <= timeMs && actions[index + 1].at >= timeMs) {
				return catmull_rom_spline(actions, index, timeMs);
			}
		}

		return 0.f;
	}
};