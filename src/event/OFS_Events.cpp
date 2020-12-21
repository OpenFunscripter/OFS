#include "OFS_Events.h"
#include "SDL_events.h"


int32_t OFS_Events::FfmpegAudioProcessingFinished = 0;
int32_t OFS_Events::ControllerButtonRepeat = 0;

void OFS_Events::RegisterEvents() noexcept
{
	FfmpegAudioProcessingFinished = SDL_RegisterEvents(1);

	ControllerButtonRepeat = SDL_RegisterEvents(1);
}
