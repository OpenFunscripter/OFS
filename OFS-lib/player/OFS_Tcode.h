#pragma once
#include "c_serial.h"
#include <cstdint>

#include "OFS_Util.h"
#include "FunscriptAction.h"


class TCodeChannel {
private:

	int32_t id = 0;
	std::vector<FunscriptAction>::iterator current;
	std::vector<FunscriptAction>::iterator next;

	inline float getPos(int32_t currentTimeMs) noexcept {
		if (currentTimeMs > next->at) {
			if (next + 1 != actions.end()) {
				current += 1;
				next += 1;
				LOGF_DEBUG("L%d: Next action! ", id);
			}
		}

		float progress = Util::Clamp((float)(currentTimeMs - current->at) / (next->at - current->at), 0.f, 1.f);
		if (easing) {
			progress = progress < 0.5f ? 4.f * progress * progress * progress : 1.f - std::pow(-2.f * progress + 2.f, 3) / 2.f;
		}
		float pos = Util::Lerp<float>(current->pos, next->pos, progress);
		return pos;
	}
public:
	static bool easing;
	static int32_t limits[2];
	std::vector<FunscriptAction> actions;
	int32_t lastTcodeVal = -1;
	char buf[32];
	inline const char* getCommand(int32_t currentTimeMs) noexcept {
		float pos = getPos(currentTimeMs);
		int32_t tcodeVal = (int32_t)(limits[0] + ((pos / 100.f) * (limits[1] - limits[0])));
		if (tcodeVal != lastTcodeVal) {
			lastTcodeVal = tcodeVal;
			stbsp_snprintf(buf, sizeof(buf), "L%d%d\n", id, tcodeVal);
			return buf;
		}
		return nullptr;
	}

	inline void Sync(int32_t currentTimeMs) noexcept {
		current = std::find_if(actions.begin(), actions.end(),
			[&](auto&& action) {
				return action.at > currentTimeMs;
			});
		FUN_ASSERT(current != actions.end(), "fix me");
		next =  current + 1;
	}
};

class TCodePlayer {
public:
	uint8_t data[255];
	int data_length;

	int current_port = 0;
	int port_count = 0;
	const char** port_list = nullptr;

	int status = -1;

	void openPort(const char* name) noexcept;
	void tick() noexcept;
	c_serial_port_t* port = nullptr;
	c_serial_control_lines_t lines;

	TCodePlayer();
	void DrawWindow(bool* open) noexcept;

	void play(float currentTimeMs, const std::vector<FunscriptAction>& actions) noexcept;
	void stop() noexcept;
};