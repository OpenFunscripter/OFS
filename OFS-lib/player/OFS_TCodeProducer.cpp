#include "OFS_TCodeProducer.h"
#include "OFS_EventSystem.h"

TCodeChannelProducer::TCodeChannelProducer() noexcept
	: startAction(0, 50), nextAction(1, 50)
{
	EV::Queue().appendListener(FunscriptActionsChangedEvent::EventType,
		FunscriptActionsChangedEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &TCodeChannelProducer::FunscriptChanged)));
}