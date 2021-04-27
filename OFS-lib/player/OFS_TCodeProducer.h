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

	inline float getPos(float currentTime, float freq) noexcept {
		if (currentTime > nextAction.atS) { return LastValue; }
		OFS_PROFILE(__FUNCTION__);

		float progress = Util::Clamp((float)(currentTime - startAction.atS) / (nextAction.atS - startAction.atS), 0.f, 1.f);
		
		float pos;
		if (TCodeChannel::SplineMode)	{
			pos = FunscriptSpline::SampleAtIndex(Script->Actions(), currentIndex, currentTime);
			if (TCodeChannel::RemapToFullRange) { pos = Util::MapRange<float>(pos, ScriptMinPos / 100.f, ScriptMaxPos / 100.f, 0.f, 1.f); }
		}
		else {
			pos = Util::Lerp<float>(startAction.pos / 100.f, nextAction.pos / 100.f, progress);
		}

		RawSpeed = std::abs(pos - LastValue) / (1.f/freq);
		LastValue = pos;
		
#ifndef NDEBUG
		LastValueRaw = pos;
#endif
		return LastValue;
	}

	int32_t currentIndex = 0;
	int32_t scriptIndex = -1;
	
	inline bool GetScript(std::shared_ptr<const Funscript>& ptr) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (scripts != nullptr && scriptIndex < scripts->size()) {
			ptr = scripts->operator[](scriptIndex);
			if (ptr != nullptr) { return true; }
		}
		ptr = nullptr;
		return false;
	}
	std::shared_ptr<const Funscript> Script;
public:
	std::vector<std::shared_ptr<const Funscript>>* scripts = nullptr;
	TCodeChannel* channel = nullptr;

	TCodeChannelProducer() noexcept;

	inline void Reset() noexcept { SetScript(-1); }
	inline void SetScript(int32_t index) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		this->scriptIndex = index;
		this->currentIndex = 0;
		if (GetScript(Script)) {
			if (Script->Actions().size() <= 1) { this->scriptIndex = -1; return; }
			auto [min, max] = std::minmax_element(Script->Actions().begin(), Script->Actions().end(),
				[](auto act1, auto act2) {
					return act1.pos < act2.pos;
			});
			ScriptMinPos = min->pos;
			ScriptMaxPos = max->pos;
			LOGF_DEBUG("Script min %f and max %f", ScriptMinPos, ScriptMaxPos);

			NeedsResync = true;
		}
	}

	void FunscriptChanged(union SDL_Event& ev) noexcept
	{
		NeedsResync = true;
	}

	inline void sync(float currentTime, float freq) noexcept {
		if (channel == nullptr || scripts == nullptr) return;
		if (!NeedsResync && currentTime >= startAction.atS && currentTime <= nextAction.atS) return;
		if (!Script) return;
		OFS_PROFILE(__FUNCTION__);

		auto& actions = Script->Actions();
		if (!actions.empty()) {
			auto startIt = actions.upper_bound(FunscriptAction(currentTime, 0));
			if (startIt-1 >= actions.begin()) {
				currentIndex = std::distance(actions.begin(), startIt-1);
				startAction = *(startIt-1);
				nextAction = *(startIt);
				if (TCodeChannel::RemapToFullRange) { MapNewActions(); }
			}
			else {
				currentIndex = 0;
				startAction = *actions.begin();
				nextAction = *(actions.begin()+1);
				if (TCodeChannel::RemapToFullRange) { MapNewActions(); }
			}
		}

		NeedsResync = false;
	}

#ifndef NDEBUG
	bool foo = false;
#endif

	inline void tick(float currentTime, float freq) noexcept {
		if (scripts == nullptr || channel == nullptr) return;
		if (!Script) return;

		OFS_PROFILE(__FUNCTION__);
		if (NeedsResync) { sync(currentTime, freq); }
		auto& actions = Script->Actions();

		int newIndex = currentIndex;
		if (currentTime > nextAction.atS) {
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
				nextAction.atS += 0.1f;
			}
			MapNewActions();
			//LOGF_DEBUG("%s: New stroke! %d -> %d", channel->Id, startAction.pos, nextAction.pos);
		}
#ifndef NDEBUG
		else {
			foo = false;
		}
#endif
		float interp = getPos(currentTime, freq);
		channel->SetNextPos(interp);
	}

	inline int32_t ScriptIdx() const noexcept { return scriptIndex; }
};

class TCodeProducer {
public:
	std::array<TCodeChannelProducer, static_cast<size_t>(TChannel::TotalCount)> producers;
	std::vector<std::shared_ptr<const Funscript>> LoadedScripts;

	TCodeChannelProducer& GetProd(TChannel ch) { return producers[static_cast<size_t>(ch)]; }
	
	TCodeProducer() noexcept
	{
		for (auto& p : producers) {
			p.scripts = &LoadedScripts;
		}
	}

	inline void SetChannels(TCodeChannels* tcode) noexcept {
		for (int i = 0; i < producers.size(); i++) {
			producers[i].channel = &tcode->channels[i];
		}
	}

	inline void ClearChannels() noexcept {
		for (auto& prod : producers) {
			prod.Reset();
		}
	}

	inline void tick(float currentTime, float freq) noexcept {
		for (auto& prod : producers) {
			prod.tick(currentTime, freq);
		}
	}

	inline void sync(float currentTime, float freq) noexcept {
		for (auto& prod : producers) {
			prod.sync(currentTime, freq);
		}
	}
};