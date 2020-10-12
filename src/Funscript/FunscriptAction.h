#pragma once

#include "OFS_Reflection.h"
#include <cstdint>
#include <limits>

struct FunscriptAction
{
	int32_t at;
	int32_t pos;

	FunscriptAction() noexcept
		: at(std::numeric_limits<int32_t>::min()), pos(std::numeric_limits<int32_t>::min()) {}

	FunscriptAction(int32_t at, int32_t pos) noexcept
	{
		this->at = at;
		this->pos = pos;
	}

	inline bool operator==(FunscriptAction b) const noexcept {
		return this->at == b.at && this->pos == b.pos; 
	}

	inline bool operator!=(FunscriptAction b) const noexcept {
		return !(*this == b);
	}

	inline bool operator<(FunscriptAction b) const noexcept {
		return this->at < b.at;
	}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(at, ar);
		OFS_REFLECT(pos, ar);
	}
};

