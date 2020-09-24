#pragma once

#include <cstdint>

class FunscriptAction
{
public:
	int32_t at = 0;
	int32_t pos = 0;

	FunscriptAction() noexcept {}

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

