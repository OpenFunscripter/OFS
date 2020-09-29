#pragma once
#include "OpenFunscripterUtil.h"
#include "SDL.h"

#include <vector>
#include <functional>

// insanely basic event system lacks ability to unsubscribe
// it doesn't get any simpler than this but this still allows for some nice decoupling

using EventHandlerFunc = std::function<void(SDL_Event&)>;

class EventHandler {
public:
	int32_t eventType;
	EventHandlerFunc func;

	EventHandler(int32_t type, EventHandlerFunc func) : eventType(type), func(func) { }
};

class EventSystem {
private:
	std::vector<EventHandler> handlers;
public:
	// custom events
	static int32_t FunscriptActionsChangedEvent;
	static int32_t FunscriptActionClickedEvent;

	static int32_t WakeupOnMpvEvents;
	static int32_t WakeupOnMpvRenderUpdate;

	static int32_t FileDialogOpenEvent;
	static int32_t FileDialogSaveEvent;

	static int32_t FfmpegAudioProcessingFinished;

	static int32_t MpvVideoLoaded;

	void setup();

	void PushEvent(SDL_Event& event);
	void Subscribe(int32_t eventType, EventHandlerFunc handler);
};

#define EVENT_SYSTEM_BIND(listener, handler) std::bind(handler, listener, std::placeholders::_1)