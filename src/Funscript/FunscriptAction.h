#pragma once

#include <cstdint>
#include <limits>

class FunscriptAction
{
public:
	int32_t at;
	int32_t pos;

	FunscriptAction() noexcept
		: at(std::numeric_limits<int32_t>::min()), pos(std::numeric_limits<int32_t>::min()) {}

	FunscriptAction(int32_t at, int32_t pos) noexcept
	{
		this->at = at;
		this->pos = pos;
	}

	inline bool operator==(const FunscriptAction& b) const noexcept {
		return this->at == b.at && this->pos == b.pos; 
	}

	inline bool operator!=(const FunscriptAction& b) const noexcept {
		return !(*this == b);
	}

	inline bool operator<(const FunscriptAction& b) const noexcept {
		return this->at < b.at;
	}
};

