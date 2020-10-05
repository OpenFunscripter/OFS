#pragma once

#include <cstdint>

class FunscriptAction
{
public:
	int32_t at;
	int32_t pos;

	FunscriptAction() noexcept : at(-1), pos(-1) {}

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

