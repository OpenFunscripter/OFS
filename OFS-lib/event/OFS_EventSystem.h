#pragma once

#include "OFS_Event.h"
#include "eventpp/eventqueue.h"
#include <vector>

struct OFS_EventPolicy
{
    static OFS_EventType getEvent(const EventPointer& event) 
    {
        return event->Type();
    }
};

class EV
{
    private:
    static EV* instance;
    static uint32_t eventCounter;
    eventpp::EventQueue<OFS_EventType, void(const EventPointer&), OFS_EventPolicy> queue;
    inline bool process() noexcept { return queue.process(); }
    public:

    static bool Init() noexcept;
    inline static void Process() noexcept { Get()->process(); }
    inline static OFS_EventType RegisterEvent() noexcept { return ++eventCounter; }

    inline static EV* Get() noexcept { return instance; }
   
    inline static auto& Queue() noexcept { return Get()->queue; }

    template<typename Handle>
    inline static auto MakeUnsubscibeFn(OFS_EventType eventType, Handle&& handle) noexcept
    {
        return [handle = std::move(handle), eventType]()
        {
            Queue().removeListener(eventType, handle);
        };
    }

    template<typename Event, typename... Args>
    inline static EventPointer Make(Args&&... args) noexcept
    {
        return std::static_pointer_cast<BaseEvent>(
            std::make_shared<Event>(std::forward<Args>(args)...)
        );
    }

    template<typename Event, typename... Args>
    inline static auto MakeTyped(Args&&... args) noexcept
    {
        return std::make_shared<Event>(std::forward<Args>(args)...);
    }

    template<typename Event, typename... Args>
    inline static void Enqueue(Args&&... args) noexcept
    {
        Queue().enqueue(Make<Event>(std::forward<Args>(args)...));
    }
    inline static void Enqueue(EventPointer ev) noexcept
    {
        Queue().enqueue(ev);
    }
};

#define EVENT_SYSTEM_BIND(listener, handler) std::move(std::bind(handler, listener, std::placeholders::_1))
