#pragma once
#include "OFS_Util.h"
#include "SDL_events.h"
#include "SDL_thread.h"
#include "OFS_Profiling.h"

#include <vector>
#include <functional>
#include <memory>

// insanely basic event system
// it doesn't get any simpler than this
using EventHandlerFunc = std::function<void(SDL_Event&)>;

class EventHandler {
public:
	int32_t eventType;
	EventHandlerFunc func;
	void* listener = nullptr;

	EventHandler(int32_t type, void* listener, EventHandlerFunc&& func) noexcept
		: eventType(type), func(std::move(func)), listener(listener) { }
};

class EventSystem {
private:
	std::vector<EventHandler> handlers;

	void SingleShotHandler(SDL_Event& ev) noexcept;
	void WaitableSingleShotHandler(SDL_Event& ev) noexcept;
public:
	using SingleShotEventHandler = std::function<void(void*)>;
	struct SingleShotEventData {
		void* ctx = nullptr;
		SingleShotEventHandler handler;
	};
	// this event + SingleShotEventData + SingleShotHandler allows execute arbitrary code
	// back on the main thread comming from another. currently this is used to return filepaths from file dialogs.
	static int32_t SingleShotEvent;
	
	struct WaitableSingleShotEventData {
		void* ctx;
		SingleShotEventHandler handler;
		SDL_sem* waitSemaphore;

		WaitableSingleShotEventData(void* ctx, SingleShotEventHandler&& handler) noexcept
			: ctx(ctx), handler(handler)
		{
			waitSemaphore = SDL_CreateSemaphore(1);
		}
		~WaitableSingleShotEventData() noexcept
		{
			SDL_DestroySemaphore(waitSemaphore);
		}

		void Wait() noexcept { SDL_SemWait(waitSemaphore); }
		bool TryWait() noexcept { return SDL_SemTryWait(waitSemaphore) == 0; }
	};
	static int32_t WaitableSingleShotEvent;

	void setup() noexcept;

	inline void Propagate(SDL_Event& event) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		for (auto& handler : handlers) {
			if (handler.eventType == event.type)
				handler.func(event);
		}
	}

	void Subscribe(int32_t eventType, void* listener, EventHandlerFunc&& handler) noexcept;
	void Unsubscribe(int32_t eventType, void* listener) noexcept;
	void UnsubscribeAll(void* listener) noexcept;

	// helper
	inline static void PushEvent(int32_t type, void* user1 = nullptr) noexcept {
		SDL_Event ev;
		ev.type = type;
		ev.user.data1 = user1;
		SDL_PushEvent(&ev);
	}
	static void SingleShot(SingleShotEventHandler&& handler, void* ctx) noexcept;
	[[nodiscard/*("this must be waited on")*/]]static std::unique_ptr<WaitableSingleShotEventData> WaitableSingleShot(SingleShotEventHandler&& handler, void* ctx) noexcept;

	[[nodiscard]] static std::unique_ptr<WaitableSingleShotEventData> RunOnMain(SingleShotEventHandler&& handler, void* ctx) noexcept;

	static EventSystem* instance;
	static EventSystem& ev() noexcept {
		FUN_ASSERT(instance != nullptr, "null");
		return *instance;
	}
};

#define EVENT_SYSTEM_BIND(listener, handler) listener, std::move(std::bind(handler, listener, std::placeholders::_1))