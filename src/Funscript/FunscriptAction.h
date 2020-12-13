#pragma once

#include "OFS_Reflection.h"
//#include "OFS_Serialization.h"
#include <cstdint>
#include <limits>

#include<functional>

enum ActionFlags : uint16_t {
	None = 0x0,
	//MAX = 0x1 << 15
};

struct FunscriptAction
{
public:
	int32_t at;
	int16_t pos;
	uint16_t flags; // unused

	FunscriptAction() noexcept
		: at(std::numeric_limits<int32_t>::min()), pos(std::numeric_limits<int32_t>::min()), flags(ActionFlags::None) {}

	FunscriptAction(int32_t at, int32_t pos) noexcept
	{
		this->at = at;
		this->pos = pos;
		this->flags = ActionFlags::None;
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