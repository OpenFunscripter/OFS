#include "OFS_TCodeProducer.h"
#include "EventSystem.h"

TCodeChannelProducer::TCodeChannelProducer() noexcept
	: startAction(0, 50), nextAction(1, 50)
{
	EventSystem::ev().Subscribe(FunscriptEvents::FunscriptActionsChangedEvent, 
		EVENT_SYSTEM_BIND(this, &TCodeChannelProducer::FunscriptChanged));
}