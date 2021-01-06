#pragma once

#include "OFS_Util.h"
#include "FunscriptAction.h"
#include <cstdint>

#include <array>
#include <sstream>

enum class TCodeEasing : int32_t {
	None,
	Cubic,
};

class TCodeChannel {
public:
	// 2 characters + 0 terminator
	char Id[3] = "\0";
	int32_t LastTCodeValue = -1;
	int32_t NextTCodeValue = -1;

	char LastCommand[16] = "???\0";

	static constexpr int32_t MaxChannelValue = 900;
	static constexpr int32_t MinChannelValue = 100;
	std::array<int32_t, 2> limits = { MinChannelValue, MaxChannelValue };
	
	static TCodeEasing EasingMode;

	inline void SetId(const char id[3]) noexcept {
		strcpy(Id, id);
	}

	TCodeChannel() { reset(); }

	inline int32_t GetPos(float relative) noexcept
	{
		FUN_ASSERT(limits[0] <= limits[1], "limits are scuffed");
		relative = Util::Clamp<float>(relative, 0.f, 1.f);
		int32_t tcodeVal = (int32_t)(limits[0] + (relative * (limits[1] - limits[0])));
		return tcodeVal;
	}

	inline void SetNextPos(float relativePos) noexcept
	{
		NextTCodeValue = GetPos(relativePos);
	}

	inline const char* getCommand(int32_t currentTimeMs, int32_t tickrate) noexcept {
		if (NextTCodeValue != LastTCodeValue) {
			stbsp_snprintf(LastCommand, sizeof(LastCommand), "%s%d ", Id, NextTCodeValue);
			LastTCodeValue = NextTCodeValue;
			return LastCommand;
		}
		return nullptr;
	}

	inline void reset() noexcept {
		LastTCodeValue = GetPos(0.5f);
		NextTCodeValue = GetPos(0.5f);
	}

	template <class Archive>
	inline void reflect(Archive& ar) {
		std::string id = Id;
		OFS_REFLECT(id, ar);
		OFS_REFLECT_PTR_NAMED("minimum", &limits[0], ar);
		OFS_REFLECT_PTR_NAMED("maximum", &limits[1], ar);
	}
};


enum class TChannel : int32_t {
	L0 = 0,
	L1 = 1,
	L2 = 2,

	// TODO: add L3/succ

	R0 = 3,
	R1 = 4,
	R2 = 5,

	V0 = 6,
	V1 = 7,
	V2 = 8,

	TotalCount
};

class TCodeChannels {
public:
	static std::array<std::vector<const char*>, static_cast<size_t>(TChannel::TotalCount)> Aliases;

	std::array<TCodeChannel, static_cast<size_t>(TChannel::TotalCount)> channels;

	std::stringstream ss;
	std::string lastCommand;

	TCodeChannels()
	{
		for (auto& c : channels) { c.reset(); }

		Get(TChannel::L0).SetId("L0");
		Get(TChannel::L1).SetId("L1");
		Get(TChannel::L2).SetId("L2");

		Get(TChannel::R0).SetId("R0");
		Get(TChannel::R1).SetId("R1");
		Get(TChannel::R2).SetId("R2");

		Get(TChannel::V0).SetId("V0");
		Get(TChannel::V1).SetId("V1");
		Get(TChannel::V2).SetId("V2");
	}

	~TCodeChannels() { }

	TCodeChannel& Get(TChannel c) noexcept {
		return channels[static_cast<size_t>(c)];
	}

	const char* GetCommand(int32_t CurrentTimeMs, int32_t tickrate) noexcept {
		bool gotCmd = false;

		ss.str("");
		for (auto& c : channels) {
			auto cmd = c.getCommand(CurrentTimeMs, tickrate);
			if (cmd != nullptr) {
				gotCmd = true;
				ss << cmd;
			}
		}
		if (gotCmd) { 
			ss << '\n';
			lastCommand = std::move(ss.str());
			return lastCommand.c_str();
		}
		
		return nullptr;
	}

	void reset() noexcept {
		for (auto& c : channels) c.reset();
	}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(channels, ar);
	}
};