#include "OFS_EventSystem.h"

#include "SDL_events.h"

EV* EV::instance = nullptr;

// In order to not collide with SDL_Event types the counter starts at SDL_USEREVENT
uint32_t EV::eventCounter = SDL_USEREVENT;

static void deferHandler(const OFS_DeferEvent* ev) noexcept
{
    ev->Function();
}

bool EV::Init() noexcept
{
    if(!EV::instance)
    {
        EV::instance = new EV();
        EV::Queue().appendListener(OFS_DeferEvent::EventType,
            OFS_DeferEvent::HandleEvent(deferHandler));
    }
    return true;
}

