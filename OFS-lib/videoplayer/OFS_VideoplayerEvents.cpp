#include "OFS_VideoplayerEvents.h"
#include "SDL_events.h"

int32_t VideoEvents::VideoLoaded = 0;
int32_t VideoEvents::PlayPauseChanged = 0;

void VideoEvents::RegisterEvents() noexcept
{
	VideoLoaded = SDL_RegisterEvents(1);
	PlayPauseChanged = SDL_RegisterEvents(1);
}
