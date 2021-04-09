#include "EventSystem.h"

EventSystem* EventSystem::instance = nullptr;

int32_t EventSystem::SingleShotEvent = 0;
int32_t EventSystem::WaitableSingleShotEvent = 0;

void EventSystem::SingleShotHandler(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	SingleShotEventData* data = (SingleShotEventData*)ev.user.data1;
	data->handler(data->ctx);
	delete data;
}

void EventSystem::WaitableSingleShotHandler(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	WaitableSingleShotEventData* data = (WaitableSingleShotEventData*)ev.user.data1;
	data->handler(data->ctx);
	SDL_SemPost(data->waitSemaphore);
	// gets deleted by the waiting thread
}

void EventSystem::setup() noexcept
{
	FUN_ASSERT(instance == nullptr, "only one instance");
	instance = this;
	SingleShotEvent = SDL_RegisterEvents(1);
	WaitableSingleShotEvent = SDL_RegisterEvents(1);
	Subscribe(SingleShotEvent, EVENT_SYSTEM_BIND(this, &EventSystem::SingleShotHandler));
	Subscribe(WaitableSingleShotEvent, EVENT_SYSTEM_BIND(this, &EventSystem::WaitableSingleShotHandler));
}

void EventSystem::Subscribe(int32_t eventType, void* listener, EventHandlerFunc&& handler) noexcept
{
	// this excects the listener to never relocate
	handlers.emplace_back(eventType, listener, std::move(handler));
}

void EventSystem::Unsubscribe(int32_t eventType, void* listener) noexcept
{
	// this excects the listener to never relocate
	auto it = std::find_if(handlers.begin(), handlers.end(),
		[eventType, listener](auto& handler) {
			return handler.listener == listener && handler.eventType == eventType;
	});

	if (it != handlers.end()) {
		handlers.erase(it);
	}
	else {
		LOGF_ERROR("Failed to unsubscribe event. \"%d\"", eventType);
		FUN_ASSERT(false, "please investigate");
	}
}

void EventSystem::UnsubscribeAll(void* listener) noexcept
{
	auto it = handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
		[listener](auto&& h) {
			return h.listener == listener;
		}), handlers.end());
}

void EventSystem::SingleShot(SingleShotEventHandler&& handler, void* ctx) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	// evData is freed after the event got processed
	auto evData = new SingleShotEventData();
	evData->ctx = ctx;
	evData->handler = std::move(handler);
	SDL_Event ev;
	ev.type = EventSystem::SingleShotEvent;
	ev.user.data1 = evData;
	SDL_PushEvent(&ev);
}

std::unique_ptr<EventSystem::WaitableSingleShotEventData> EventSystem::WaitableSingleShot(SingleShotEventHandler&& handler, void* ctx) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto data = std::make_unique<WaitableSingleShotEventData>(ctx,  std::move(handler));
	SDL_Event ev;
	ev.type = EventSystem::WaitableSingleShotEvent;
	ev.user.data1 = (void*)data.get();
	SDL_SemWait(data->waitSemaphore);
	SDL_PushEvent(&ev);
	return std::move(data);
}
