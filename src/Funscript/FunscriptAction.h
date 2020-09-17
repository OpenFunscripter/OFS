#pragma once

#include <cstdint>

//enum FunscriptActionFlag {
//	None = 0,
//	RawAction = 0x1
//};

class FunscriptAction
{
public:
	int32_t at = 0;
	int32_t pos = 0;
	//int32_t flags = 0;

	FunscriptAction() noexcept {}

	FunscriptAction(int32_t at, int32_t pos) noexcept
	{
		this->at = at;
		this->pos = pos;
	}

	//FunscriptAction(int32_t at, int32_t pos, int32_t flags) noexcept
	//{
	//	this->at = at;
	//	this->pos = pos;
	//	this->flags = flags;
	//}

	//inline bool hasFlags(int32_t flags) const { return this->flags & flags; }

	inline bool operator==(const FunscriptAction& b) const noexcept {
		return this->at == b.at && this->pos == b.pos; //&& this->flags == b.flags;
	}

	inline bool operator!=(const FunscriptAction& b) const noexcept {
		return this->at != b.at && this->pos != b.pos; //&& this->flags != b.flags;
	}
};

