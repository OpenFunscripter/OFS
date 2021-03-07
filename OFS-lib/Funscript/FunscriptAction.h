#pragma once

#include "OFS_Reflection.h"
//#include "OFS_Serialization.h"
#include <cstdint>
#include <limits>

#include<functional>

struct FunscriptAction
{
public:
	int32_t at;
	int16_t pos;
	uint8_t flags; // unused
	uint8_t tag;

	FunscriptAction() noexcept
		: at(std::numeric_limits<int32_t>::min()), pos(std::numeric_limits<int16_t>::min()), flags(0), tag(0) {
		static_assert(sizeof(FunscriptAction) == 8);
	}

	FunscriptAction(int32_t at, int32_t pos) noexcept
	{
		static_assert(sizeof(FunscriptAction) == 8);
		this->at = at;
		this->pos = pos;
		this->flags = 0;
		this->tag = 0;
	}

	FunscriptAction(int32_t at, int32_t pos, uint8_t tag) noexcept
		: FunscriptAction(at, pos)
	{
		static_assert(sizeof(FunscriptAction) == 8);
		this->tag = tag;
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

struct FunscriptActionHashfunction
{
	inline std::size_t operator()(FunscriptAction s) const noexcept
	{
		static_assert(sizeof(FunscriptAction) == sizeof(int64_t));
		return *(int64_t*)&s;
	}
};