#pragma once

#include "OFS_BinarySerialization.h"

#include <cstdint>
#include <limits>

#include "EASTL/vector_set.h"

struct FunscriptAction
{
public:
	// timestamp as floating point seconds
	// instead of integer milliseconds
	float atS;
	int16_t pos;
	uint8_t flags; // unused
	uint8_t tag;

	template<typename S>
	void serialize(S& s)
	{
		s.ext(*this, bitsery::ext::Growable{},
			[](S& s, FunscriptAction& o) {
				s.value4b(o.atS);
				s.value2b(o.pos);
				s.value1b(o.flags);
				s.value1b(o.tag);
			});
	}

	FunscriptAction() noexcept
		: atS(std::numeric_limits<float>::min()), pos(std::numeric_limits<int16_t>::min()), flags(0), tag(0) {
		static_assert(sizeof(FunscriptAction) == 8);
	}

	FunscriptAction(float at, int32_t pos) noexcept
	{
		static_assert(sizeof(FunscriptAction) == 8);
		this->atS = at;
		this->pos = pos;
		this->flags = 0;
		this->tag = 0;
	}

	FunscriptAction(float at, int32_t pos, uint8_t tag) noexcept
		: FunscriptAction(at, pos)
	{
		static_assert(sizeof(FunscriptAction) == 8);
		this->tag = tag;
	}

	inline bool operator==(FunscriptAction b) const noexcept {
		return this->atS == b.atS && this->pos == b.pos;
	}

	inline bool operator!=(FunscriptAction b) const noexcept {
		return !(*this == b);
	}

	inline bool operator<(FunscriptAction b) const noexcept {
		return this->atS < b.atS;
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

struct ActionLess
{
	bool operator()(const FunscriptAction& a, const FunscriptAction& b) const noexcept
	{
		return a.atS < b.atS;
	}
};

using FunscriptArray = eastl::vector_set<FunscriptAction, ActionLess>;
