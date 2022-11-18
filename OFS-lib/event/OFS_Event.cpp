#include "OFS_Event.h"
#include "OFS_EventSystem.h"

OFS_EventType BaseEvent::RegisterNewEvent() noexcept
{
    return EV::RegisterEvent();
}
