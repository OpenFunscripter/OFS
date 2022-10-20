#pragma once
#include "OFS_TCodeProducer.h"
#include "OFS_Util.h"
#include "FunscriptAction.h"


class TCodePlayer {
	std::string loadPath;
public:
	int current_port = 0;
	int port_count = 0;
	struct sp_port** port_list = nullptr;
	struct sp_port* port = nullptr;

	volatile int32_t tickrate = 250;
	volatile float delay = 0;

	TCodeChannels tcode;
	TCodeProducer prod;

	TCodePlayer() noexcept;
	~TCodePlayer() noexcept;
	
	bool openPort(struct sp_port* port) noexcept;
	void loadSettings(const std::string& path) noexcept;
	void save() noexcept;

	void DrawWindow(bool* open, float currentTimeMs) noexcept;

	void setScripts(std::vector<std::shared_ptr<const Funscript>>&& scripts) noexcept;
	void play(float currentTime, std::vector<std::shared_ptr<const Funscript>>&& scripts) noexcept;
	void stop() noexcept;
	void sync(float currentTime, float speed) noexcept;
	void reset() noexcept;
};

REFL_TYPE(TCodePlayer)
	REFL_FIELD(tickrate)
	REFL_FIELD(delay)
	REFL_FIELD(tcode)
REFL_END