#pragma once

#include "SDL_events.h"
#include <cstdint>
#include <functional>
#include <memory>

using OFS_EventType = uint32_t;

using UnsubscribeFn = std::function<void()>;

class BaseEvent 
{
    public:
    static constexpr OFS_EventType InvalidType = 0;
    virtual ~BaseEvent() noexcept {}
    virtual OFS_EventType Type() const noexcept = 0;
    static OFS_EventType RegisterNewEvent() noexcept;
};

using EventPointer = std::shared_ptr<BaseEvent>;

template<typename Event>
class OFS_Event : public BaseEvent
{
    public:
    static OFS_EventType EventType;
    OFS_EventType Type() const noexcept override { return EventType; }

    template<typename Handler>
    static auto HandleEvent(Handler&& handler) noexcept
    {
        return [handler = std::move(handler)](const EventPointer& ev) noexcept
        {
            handler(static_cast<const Event*>(ev.get()));
        };
    }
};

template<typename Event>
OFS_EventType OFS_Event<Event>::EventType = BaseEvent::RegisterNewEvent();

class OFS_SDL_Event : public OFS_Event<OFS_SDL_Event>
{
    public:
    SDL_Event sdl = {0};
};

using OFS_DeferEventFn = std::function<void()>;
class OFS_DeferEvent : public OFS_Event<OFS_DeferEvent>
{
    public:
    OFS_DeferEventFn Function;
    OFS_DeferEvent(OFS_DeferEventFn&& fn) noexcept
        : Function(std::move(fn)) {}
};
