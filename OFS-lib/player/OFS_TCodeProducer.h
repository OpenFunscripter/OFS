#pragma once

#include "OFS_TCodeChannel.h"
#include "Funscript.h"

#include <array>
#include <vector>
#include <memory>

class TCodeChannelProducer
{
public:
	TCodeChannel* channel = nullptr;
	std::weak_ptr<Funscript> script;
	int32_t currentIndex = 0;

	TCodeChannelProducer() {}

	TCodeChannelProducer(std::weak_ptr<Funscript>&& script, TCodeChannel* channel)
	{
		this->script = std::move(script);
		this->channel = channel;
	}

	inline void sync(int32_t CurrentTimeMs) noexcept {
		if (script.expired() || channel == nullptr) return;

		auto scriptPtr = script.lock();
		auto& actions = scriptPtr->Actions();

		for (int i = 0; i < actions.size(); i++) {
			auto action = actions[i];
			if (action.at >= CurrentTimeMs) 
			{
				currentIndex = std::max(0, i - 1);
				channel->startAction = actions[currentIndex];
				channel->nextAction = actions[currentIndex+1];
				break;
			}
		}
	}

	inline void tick(int32_t CurrentTimeMs) noexcept {
		if (script.expired() || channel == nullptr) return;

		auto scriptPtr = script.lock();
		auto& actions = scriptPtr->Actions();

		const FunscriptAction* currentAction = &actions[currentIndex];
		const FunscriptAction* nextAction;
		if (currentIndex + 1 >= actions.size()) {
			return;
		}
		else {
			nextAction = &actions[currentIndex + 1];
		}

		int newIndex = currentIndex;
		if (CurrentTimeMs > nextAction->at) {
			newIndex++;
		}

		if (currentIndex != newIndex && newIndex < actions.size()) {
			currentIndex = newIndex;
			channel->startAction = actions[currentIndex];
			if (currentIndex + 1 < actions.size()) {
				channel->nextAction = actions[currentIndex+1];
			}
			else {
				channel->nextAction = channel->startAction;
				channel->nextAction.at++;
			}

			LOGF_DEBUG("%s: New stroke! %d -> %d", channel->Id, channel->startAction.pos, channel->nextAction.pos);
		}
	}
};

class TCodeProducer {
public:
	std::array<TCodeChannelProducer, static_cast<size_t>(TChannel::TotalCount)> producers;

	inline void HookupChannels(
		TCodeChannels* tcode,
		std::weak_ptr<Funscript>&& L0, // everything except L0 is optional
		std::weak_ptr<Funscript>&& R0 = std::weak_ptr<Funscript>(),
		std::weak_ptr<Funscript>&& R1 = std::weak_ptr<Funscript>(),
		std::weak_ptr<Funscript>&& R2 = std::weak_ptr<Funscript>()
		/*TODO: add more channels */ ) noexcept {

		GetProd(TChannel::L0).script = L0;
		GetProd(TChannel::L1).script.reset();
		GetProd(TChannel::L2).script.reset();

		GetProd(TChannel::R0).script = R0;
		GetProd(TChannel::R1).script = R1;
		GetProd(TChannel::R2).script = R2;

		GetProd(TChannel::V0).script.reset();
		GetProd(TChannel::V1).script.reset();
		GetProd(TChannel::V2).script.reset();

		SetChannels(tcode);
	}

	TCodeChannelProducer& GetProd(TChannel ch) { return producers[static_cast<size_t>(ch)]; }
	
	void SetChannels(TCodeChannels* tcode) noexcept {
		for (int i = 0; i < producers.size(); i++) {
			producers[i].channel = &tcode->channels[i];
		}
	}

	void ClearChannels() noexcept {
		for (auto& prod : producers) {
			prod.channel = nullptr;
			prod.script.reset();
		}
	}

	inline void tick(int32_t CurrentTimeMs) noexcept {
		for (auto& prod : producers) {
			prod.tick(CurrentTimeMs);
		}
	}

	inline void sync(int32_t CurrentTimeMs) noexcept {
		for (auto& prod : producers) {
			prod.sync(CurrentTimeMs);
		}
	}
};