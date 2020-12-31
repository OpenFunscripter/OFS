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
	FunscriptAction startAction;
	FunscriptAction nextAction;
private:

	inline float getPos(int32_t currentTimeMs) noexcept {
		if (currentTimeMs > nextAction.at) { return nextAction.pos; }

		float progress = Util::Clamp((float)(currentTimeMs - startAction.at) / (nextAction.at - startAction.at), 0.f, 1.f);
		switch (TCodeChannel::EasingMode) {
		case TCodeEasing::Cubic:
		{
			progress = progress < 0.5f
				? 4.f * progress * progress * progress
				: 1.f - ((-2.f * progress + 2.f) * (-2.f * progress + 2.f) * (-2.f * progress + 2.f)) / 2.f;
			break;
		}
		case TCodeEasing::None:
			break;
		}
		float pos = Util::Lerp<float>(startAction.pos, nextAction.pos, progress);
		return pos;
	}
public:
	static TCodeEasing EasingMode;
	// 2 characters + 0 terminator
	char Id[3] = "\0";

	TCodeChannel() {
		startAction.at = -2; startAction.pos = 50;
		nextAction.at = -1; nextAction.pos = 50;
	}

	inline void SetId(char id[3]) noexcept {
		strcpy(Id, id);
	}

	static constexpr int32_t MaxChannelValue = 900;
	static constexpr int32_t MinChannelValue = 100;

	std::array<int32_t, 2> limits = { MinChannelValue, MaxChannelValue };
	int32_t lastTcodeVal = -1;

	inline void SetStroke(FunscriptAction from, FunscriptAction to) noexcept {
		startAction = from;
		nextAction = to;
	}
	inline void SetNext(FunscriptAction next) noexcept {
		startAction = nextAction;
		nextAction = next;
	}
	inline bool StrokeComplete(int32_t currentTimeMs) const noexcept { return nextAction.at > currentTimeMs; }

	char buf[16] = "???\0";
	inline const char* getCommand(int32_t currentTimeMs) noexcept {
		float pos = getPos(currentTimeMs);
		int32_t tcodeVal = (int32_t)(limits[0] + ((pos / 100.f) * (limits[1] - limits[0])));
		if (tcodeVal != lastTcodeVal) {
			lastTcodeVal = tcodeVal;
			stbsp_snprintf(buf, sizeof(buf), "%s%d ", Id, tcodeVal);
			return buf;
		}
		return nullptr;
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

	union
	{
		std::array<TCodeChannel, static_cast<size_t>(TChannel::TotalCount)> channels;
		
		struct {
			// linear movement
			TCodeChannel L0; // up/down
			TCodeChannel L1; //unused
			TCodeChannel L2; //unused
			// TODO: add L3/succ

			// rotation
			TCodeChannel R0; // twist
			TCodeChannel R1; // roll
			TCodeChannel R2; // pitch

			// vibration
			TCodeChannel V0; //unused
			TCodeChannel V1; //unused
			TCodeChannel V2; //unused
		};
	};

	std::stringstream ss;
	std::string lastCommand;

	TCodeChannels()
	{
		for (auto& c : channels) { c = TCodeChannel(); }
		L0.SetId("L0");
		L1.SetId("L1");
		L2.SetId("L2");

		R0.SetId("R0");
		R1.SetId("R1");
		R2.SetId("R2");

		V0.SetId("V0");
		V1.SetId("V1");
		V2.SetId("V2");
	}

	~TCodeChannels() { }

	TCodeChannel& Get(TChannel c) noexcept {
		return channels[static_cast<size_t>(c)];
	}

	const char* GetCommand(int32_t CurrentTimeMs) noexcept {
		bool gotCmd = false;
		ss.str("");
		for (auto& c : channels) {
			auto cmd = c.getCommand(CurrentTimeMs);
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
};