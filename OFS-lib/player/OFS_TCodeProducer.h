#pragma once

#include "OFS_TCodeChannel.h"
#include "Funscript.h"

#include <vector>
#include <memory>

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

public:
	float LastValue = 0.f;
	float RawSpeed = 0.f;

#ifndef NDEBUG
	float LastValueRaw = 0.f;
	float FilteredSpeed = 0.f;
#endif

	bool NeedsResync = false;
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
		if (currentTimeMs > nextAction.at) { return LastValue; }

		float progress = Util::Clamp((float)(currentTimeMs - startAction.at) / (nextAction.at - startAction.at), 0.f, 1.f);
		
		float pos;
		std::shared_ptr<const Funscript> ptr;
		if (TCodeChannel::SplineMode && GetScript(ptr))	{
			pos = ptr->ScriptSpline.SampleAtIndex(ptr->Actions(), currentIndex, currentTimeMs);
			if (TCodeChannel::RemapToFullRange) { pos = Util::MapRange<float>(pos, ScriptMinPos / 100.f, ScriptMaxPos / 100.f, 0.f, 1.f); }
		}
		else {
			pos = Util::Lerp<float>(startAction.pos / 100.f, nextAction.pos / 100.f, progress);
		}

		RawSpeed = std::abs(pos - LastValue) / (1.f/freq);

		// detect discontinuity
		if (RawSpeed >= 50.f && !InterpTowards) {
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
#ifndef NDEBUG
		LastValueRaw = pos;
#endif
		return LastValue;
	}

	int32_t currentIndex = 0;
	int32_t scriptIndex = -1;
	
	inline bool GetScript(std::shared_ptr<const Funscript>& ptr) noexcept
	{
		if (scripts != nullptr && scriptIndex < scripts->size())
		{
			ptr = scripts->operator[](scriptIndex).lock();
			if (ptr != nullptr) { return true; }
		}
		ptr = nullptr;
		return false;
	}
public:
	std::vector<std::weak_ptr<const Funscript>>* scripts = nullptr;
	TCodeChannel* channel = nullptr;

	TCodeChannelProducer() noexcept : startAction(0, 50), nextAction(1, 50) {}

	inline void Reset() noexcept { SetScript(-1); }
	inline void SetScript(int32_t index) noexcept
	{
		this->scriptIndex = index;
		this->currentIndex = 0;
		std::shared_ptr<const Funscript> locked;
		if (GetScript(locked)) {
			if (locked->Actions().size() <= 1) { this->scriptIndex = -1; return; }
			auto [min, max] = std::minmax_element(locked->Actions().begin(), locked->Actions().end(),
				[](auto act1, auto act2) {
					return act1.pos < act2.pos;
			});
			ScriptMinPos = min->pos;
			ScriptMaxPos = max->pos;
			LOGF_DEBUG("Script min %f and max %f", ScriptMinPos, ScriptMaxPos);

			NeedsResync = true;
		}
	}

	inline std::weak_ptr<const Funscript> GetScript() noexcept
	{
		if (scriptIndex < scripts->size())
		{
			return (*scripts)[scriptIndex];
		}
		return std::weak_ptr<const Funscript>();
	}

	inline void sync(int32_t CurrentTimeMs, float freq) noexcept {
		std::shared_ptr<const Funscript> scriptPtr;
		if (channel == nullptr || scripts == nullptr) return;
		if (!GetScript(scriptPtr)) return;
		// TODO: check if out of sync first

		auto& actions = scriptPtr->Actions();

		for (int i = 0; i < actions.size(); i++) {
			auto action = actions[i];
			if (action.at >= CurrentTimeMs) 
			{
				currentIndex = std::max(0, i - 1);
				startAction = actions[currentIndex];
				nextAction = actions[currentIndex+1];
				if (TCodeChannel::RemapToFullRange) { MapNewActions(); }
				break;
			}
		}

		float interp = getPos(CurrentTimeMs, freq);
		channel->SetNextPos(interp);
		NeedsResync = false;
	}

#ifndef NDEBUG
	bool foo = false;
#endif

	inline void tick(int32_t CurrentTimeMs, float freq) noexcept {
		std::shared_ptr<const Funscript> scriptPtr;
		if (scripts == nullptr || channel == nullptr) return;
		if (!GetScript(scriptPtr)) return;

		if (NeedsResync) { sync(CurrentTimeMs, freq); }
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

	inline int32_t ScriptIdx() const noexcept { return scriptIndex; }
};

class TCodeProducer {
public:
	std::array<TCodeChannelProducer, static_cast<size_t>(TChannel::TotalCount)> producers;
	std::vector<std::weak_ptr<const Funscript>> LoadedScripts;

	TCodeChannelProducer& GetProd(TChannel ch) { return producers[static_cast<size_t>(ch)]; }
	
	TCodeProducer() noexcept
	{
		for (auto& p : producers)
		{
			p.scripts = &LoadedScripts;
		}
	}

	void SetChannels(TCodeChannels* tcode) noexcept {
		for (int i = 0; i < producers.size(); i++) {
			producers[i].channel = &tcode->channels[i];
		}
	}

	void ClearChannels() noexcept {
		for (auto& prod : producers) {
			prod.Reset();
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