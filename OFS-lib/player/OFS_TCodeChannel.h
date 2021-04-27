#pragma once

#include "OFS_Util.h"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"

#include <array>

#include "EASTL/string.h"

class TCodeChannel {
public:
	// 2 characters + 0 terminator
	char Id[3] = "\0";
	int32_t LastTCodeValue = -1;
	int32_t NextTCodeValue = -1;

	char LastCommand[16] = "?????\0";

	static constexpr int32_t MaxChannelValue = 999;
	static constexpr int32_t MinChannelValue = 0;
	std::array<int32_t, 2> limits = { MinChannelValue, MaxChannelValue };
	
	static bool SplineMode;
	static bool RemapToFullRange;

	bool Enabled = true;
	bool Rebalance = false;
	bool Invert = false;

	inline void SetId(const char id[3]) noexcept {
		strcpy(Id, id);
	}

	TCodeChannel() { reset(); }

	inline int32_t GetPos(float relative) noexcept
	{
		FUN_ASSERT(limits[0] <= limits[1], "limits are scuffed");
		relative = Util::Clamp<float>(relative, 0.f, 1.f);
		int32_t tcodeVal;
		if (Rebalance)
		{
			tcodeVal = relative < 0.5f 
				? (int32_t)(limits[0] + (relative*2.f) * (500 - limits[0]))
				: (int32_t)(500 + ((relative-0.5f)*2.f) * (limits[1] - 500));
		}
		else
		{
			tcodeVal = (int32_t)(limits[0] + (relative * (limits[1] - limits[0])));
		}
		FUN_ASSERT(tcodeVal >= TCodeChannel::MinChannelValue && tcodeVal <= TCodeChannel::MaxChannelValue, "f");
		return tcodeVal;
	}

	inline void SetNextPos(float relativePos) noexcept
	{
		if (std::isnan(relativePos)) return;
		if (Invert) { relativePos = std::abs(relativePos - 1.f); }
		NextTCodeValue = GetPos(relativePos);
	}

	inline const char* getCommand() noexcept {
		if (Enabled && NextTCodeValue != LastTCodeValue) {
			stbsp_snprintf(LastCommand, sizeof(LastCommand), "%s%03d", Id, NextTCodeValue);
			LastTCodeValue = NextTCodeValue;
			return LastCommand;
		}
		return nullptr;
	}

	inline const char* getCommandSpeed(int32_t speed) noexcept
	{
		if (Enabled && NextTCodeValue != LastTCodeValue) {
			stbsp_snprintf(LastCommand, sizeof(LastCommand), "%s%03dS%d", Id, NextTCodeValue, speed);
			LastTCodeValue = NextTCodeValue;
			return LastCommand;
		}
		return nullptr;
	}

	inline void reset() noexcept {
		LastTCodeValue = 499;
		NextTCodeValue = 500;
	}

	template <class Archive>
	inline void reflect(Archive& ar) {
		std::string id = Id;
		OFS_REFLECT(id, ar);
		OFS_REFLECT_PTR_NAMED("minimum", &limits[0], ar);
		OFS_REFLECT_PTR_NAMED("maximum", &limits[1], ar);
		OFS_REFLECT(Rebalance, ar);
		OFS_REFLECT(Invert, ar);
		OFS_REFLECT(Enabled, ar);
	}
};


enum class TChannel : int32_t {
	L0 = 0,
	L1 = 1,
	L2 = 2,
	L3 = 3,

	R0 = 4,
	R1 = 5,
	R2 = 6,

	V0 = 7,
	V1 = 8,
	V2 = 9,

	TotalCount
};

class TCodeChannels {
public:
	static std::array<const std::vector<const char*>, static_cast<size_t>(TChannel::TotalCount)> Aliases;

	std::array<TCodeChannel, static_cast<size_t>(TChannel::TotalCount)> channels;

	eastl::string commandBuffer;

	TCodeChannels() noexcept
	{
		for (auto& c : channels) { c.reset(); }

		Get(TChannel::L0).SetId("L0");
		Get(TChannel::L1).SetId("L1");
		Get(TChannel::L2).SetId("L2");
		Get(TChannel::L3).SetId("L3");

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

	inline const char* GetCommand() noexcept {
		OFS_PROFILE(__FUNCTION__);
		bool gotCmd = false;
		commandBuffer.clear();
		for (auto& c : channels) {
			auto cmd = c.getCommand();
			if (cmd != nullptr) {
				gotCmd = true;
				commandBuffer.append(cmd);
				commandBuffer.append(1, ' ');
			}
		}
		if (gotCmd) { 
			commandBuffer.append(1, '\n');
			return commandBuffer.c_str();
		}
		
		return nullptr;
	}

	inline const char* GetCommandSpeed(int32_t speed) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		bool gotCmd = false;
		commandBuffer.clear();
		for (auto& c : channels) {
			auto cmd = c.getCommandSpeed(speed);
			if (cmd != nullptr) {
				gotCmd = true;
				commandBuffer.append(cmd);
				commandBuffer.append(1, ' ');
			}
		}
		if (gotCmd) {
			commandBuffer.append(1, '\n');
			return commandBuffer.c_str();
		}

		return nullptr;
	}

	inline void reset() noexcept {
		OFS_PROFILE(__FUNCTION__);
		for (auto& c : channels) c.reset();
	}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(channels, ar);
	}
};