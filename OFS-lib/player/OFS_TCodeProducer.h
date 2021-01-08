#pragma once

#include "OFS_TCodeChannel.h"
#include "Funscript.h"

#include <array>
#include <vector>
#include <memory>
#include <algorithm>

#include "SDL_timer.h"

class TCodeChannelProducer
{
private:
	FunscriptAction startAction;
	FunscriptAction nextAction;

	bool InterpTowards = false;
	float InterpStart = 0.f;
	float InterpEnd = 0.f;
	int32_t InterpStartTime = 0;
	static constexpr int32_t MaxInterpTimeMs = 1000;

public:float LastValue = 0.f;
	  float LastValueRaw = 0.f;
	  float FilteredSpeed = 0.f;
	  float RawSpeed = 0.f;
	  bool Invert = false;
private:
	float ScriptMinPos;
	float ScriptMaxPos;

	// remaps actions to 0 to 100 range
	inline void MapNewActions() noexcept
	{
		startAction.pos = Util::MapRange<float>(startAction.pos, ScriptMinPos, ScriptMaxPos, 0.f, 100.f);
		nextAction.pos = Util::MapRange<float>(nextAction.pos, ScriptMinPos, ScriptMaxPos, 0.f, 100.f);
	}

	inline float getPos(int32_t currentTimeMs, float freq) noexcept {
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
		float pos;
		if (Invert)
		{
			pos = Util::Lerp<float>(nextAction.pos / 100.f, startAction.pos / 100.f,  progress);
		}
		else
		{
			pos = Util::Lerp<float>(startAction.pos/100.f, nextAction.pos/100.f, progress);
		}

		
		RawSpeed = std::abs(pos - LastValue) / (1.f/freq);

		// detect discontinuity
		if (RawSpeed >= 35.f && !InterpTowards) {
			InterpTowards = true;
			InterpStart = LastValue;
			InterpEnd = pos;
			InterpStartTime = SDL_GetTicks();
			LOGF_INFO("InterpTowards: %f", RawSpeed);
		}

		if (InterpTowards) {
			float t = Util::Clamp((SDL_GetTicks() - InterpStartTime) / (float)MaxInterpTimeMs, 0.f, 1.f);
			float diff = std::abs(LastValue - pos);
			
			LastValue = Util::Lerp(InterpStart, InterpEnd, t);
			if (t >= 1.f || std::abs(LastValue - pos) > diff) {
				InterpTowards = false;
			}
		}
		else {
			LastValue = pos;
		}
		LastValueRaw = pos;

		return LastValue;
	}

	int32_t currentIndex = 0;
	std::weak_ptr<const Funscript> script;
public:
	TCodeChannel* channel = nullptr;

	TCodeChannelProducer() {}

	inline void SetScript(std::weak_ptr<const Funscript>&& script) noexcept
	{
		this->currentIndex = 0;
		this->script = std::move(script);
		if (!this->script.expired()) {
			auto locked = this->script.lock();
			auto [min, max] = std::minmax_element(locked->Actions().begin(), locked->Actions().end(),
				[](auto act1, auto act2) {
					return act1.pos < act2.pos;
			});
			ScriptMinPos = min->pos;
			ScriptMaxPos = max->pos;
			LOGF_DEBUG("Script min %f and max %f", ScriptMinPos, ScriptMaxPos);
		}
	}

	inline std::weak_ptr<const Funscript>& GetScript() noexcept
	{
		return script;
	}

	inline void sync(int32_t CurrentTimeMs, float freq) noexcept {
		if (script.expired() || channel == nullptr) return;

		auto scriptPtr = script.lock();
		auto& actions = scriptPtr->Actions();

		for (int i = 0; i < actions.size(); i++) {
			auto action = actions[i];
			if (action.at >= CurrentTimeMs) 
			{
				currentIndex = std::max(0, i - 1);
				startAction = actions[currentIndex];
				nextAction = actions[currentIndex+1];
				MapNewActions();
				break;
			}
		}

		float interp = getPos(CurrentTimeMs, freq);
		channel->SetNextPos(interp);
	}

#ifndef NDEBUG
	bool foo = false;
#endif

	inline void tick(int32_t CurrentTimeMs, float freq) noexcept {
		if (script.expired() || channel == nullptr) return;

		auto scriptPtr = script.lock();
		auto& actions = scriptPtr->Actions();

		int newIndex = currentIndex;
		if (CurrentTimeMs > nextAction.at) {
			newIndex++;
		}

		if (currentIndex != newIndex && newIndex < actions.size()) {
#ifndef NDEBUG
			if (foo && newIndex-currentIndex <= -1) {
				FUN_ASSERT(false, "bug???");
			}
			foo = true;
#endif
			currentIndex = newIndex;
			startAction = actions[currentIndex];
			if (currentIndex + 1 < actions.size()) {
				nextAction = actions[currentIndex+1];
			}
			else {
				nextAction = startAction;
				nextAction.at++;
			}
			MapNewActions();
			LOGF_DEBUG("%s: New stroke! %d -> %d", channel->Id, startAction.pos, nextAction.pos);
		}
#ifndef NDEBUG
		else {
			foo = false;
		}
#endif
		float interp = getPos(CurrentTimeMs, freq);
		channel->SetNextPos(interp);
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

		GetProd(TChannel::L0).SetScript(std::move(L0));
		GetProd(TChannel::L1).SetScript(std::weak_ptr<Funscript>());
		GetProd(TChannel::L2).SetScript(std::weak_ptr<Funscript>());

		GetProd(TChannel::R0).SetScript(std::move(R0));
		GetProd(TChannel::R1).SetScript(std::move(R1));
		GetProd(TChannel::R2).SetScript(std::move(R2));

		GetProd(TChannel::V0).SetScript(std::weak_ptr<Funscript>());
		GetProd(TChannel::V1).SetScript(std::weak_ptr<Funscript>());
		GetProd(TChannel::V2).SetScript(std::weak_ptr<Funscript>());

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
			prod.SetScript(std::weak_ptr<Funscript>());
		}
	}

	inline void tick(int32_t CurrentTimeMs, float freq) noexcept {
		for (auto& prod : producers) {
			prod.tick(CurrentTimeMs, freq);
		}
	}

	inline void sync(int32_t CurrentTimeMs, float freq) noexcept {
		for (auto& prod : producers) {
			prod.sync(CurrentTimeMs, freq);
		}
	}
};