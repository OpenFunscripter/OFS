#pragma once
#include "OFS_Profiling.h"
#include "FunscriptAction.h"
#include <vector>
#include "glm/gtx/spline.hpp"

#include "EASTL/vector_set.h"

class FunscriptSpline
{
	int32_t cacheIdx = 0;
public:
	static inline float catmull_rom_spline(const FunscriptArray& actions, int32_t i, float time) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		int i0 = glm::clamp<int>(i - 1, 0, actions.size() - 1);
		int i1 = glm::clamp<int>(i, 0, actions.size() - 1);
		int i2 = glm::clamp<int>(i + 1, 0, actions.size() - 1);
		int i3 = glm::clamp<int>(i + 2, 0, actions.size() - 1);

		glm::vec1 v0(actions[i0].pos / 100.f);
		glm::vec1 v1(actions[i1].pos / 100.f);
		glm::vec1 v2(actions[i2].pos / 100.f);
		glm::vec1 v3(actions[i3].pos / 100.f);
		
		time -= actions[i1].atS;
		time /= actions[i2].atS - actions[i1].atS;

		return glm::catmullRom(v0, v1, v2, v3, time).x;
	}

	inline float Sample(const FunscriptArray& actions, float time) noexcept 
	{
		OFS_PROFILE(__FUNCTION__);
		if (actions.size() == 0) { return 0.f; }
		else if (actions.size() == 1) { return actions.front().pos / 100.f; }
		else if (cacheIdx + 1 >= actions.size()) { cacheIdx = 0; }

		if (actions[cacheIdx].atS <= time && actions[cacheIdx + 1].atS >= time) {
			// cache hit!
			return catmull_rom_spline(actions, cacheIdx, time);
		}
		else if (cacheIdx + 2 < actions.size() && actions[cacheIdx+1].atS <= time && actions[cacheIdx+2].atS >= time) {
			// sort of a cache hit
			cacheIdx += 1;
			return catmull_rom_spline(actions, cacheIdx, time);
		}
		else {
			// cache miss
			// lookup index
			auto it = actions.upper_bound(FunscriptAction(time, 0));
			if (it == actions.end()) { 
				return actions.back().pos / 100.f; 
			}
			else if (it == actions.begin())			{
				return actions.front().pos / 100.f;
			}

			it--;
			// cache index
			cacheIdx = std::distance(actions.begin(), it);
			return catmull_rom_spline(actions, cacheIdx, time);
		}

		return 0.f;
	}

	inline static float SampleAtIndex(const FunscriptArray& actions, int32_t index, float time) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (actions.empty()) { return 0.f; }
		if (index + 1 < actions.size())	{
			if (actions[index].atS <= time && actions[index + 1].atS >= time) {
				return catmull_rom_spline(actions, index, time);
			}
		}

		return actions.back().pos / 100.f;
	}
};