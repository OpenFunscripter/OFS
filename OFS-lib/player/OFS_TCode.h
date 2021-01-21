#pragma once
#include <cstdint>

#include "OFS_TCodeProducer.h"
#include "OFS_Util.h"
#include "FunscriptAction.h"


class TCodePlayer {
	std::string loadPath;
public:
	int current_port = 0;
	int port_count = 0;
	const char** port_list = nullptr;

	int status = -1;

	int32_t tickrate = 60;
	int32_t delay = 0;


	bool openPort(const char* name) noexcept;
	struct c_serial_port* port = nullptr;

	TCodeChannels tcode;
	TCodeProducer prod;

	TCodePlayer();
	~TCodePlayer();
	
	void loadSettings(const std::string& path) noexcept;
	void save() noexcept;

	void DrawWindow(bool* open) noexcept;

	void play(float currentTimeMs, 	std::vector<std::weak_ptr<const Funscript>> scripts) noexcept;
	void stop() noexcept;
	void sync(float currentTimeMs, float speed) noexcept;
	void reset() noexcept;

	inline bool IgnoreChannel(TChannel c) const noexcept {
		switch (c) {
			// handled channels
			case TChannel::L0:
			case TChannel::R0:
			case TChannel::R1:
			case TChannel::R2:
			{
				return false;
			}

			// ignored channels
			case TChannel::L2:
			case TChannel::L1:
			case TChannel::V0:
			case TChannel::V1:
			case TChannel::V2:
			{
				return true;
			}
		}

		return true;
	}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(tcode, ar);
		OFS_REFLECT(tickrate, ar);
		OFS_REFLECT(delay, ar);
		OFS_REFLECT_NAMED("easingMode", TCodeChannel::EasingMode, ar);
	}
};
