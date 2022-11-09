#include "state/OFS_LibState.h"
#include "state/states/ControllerState.h"
#include "state/states/VideoplayerWindowState.h"

void OFS_LibState::RegisterAll() noexcept
{
    OFS_REGISTER_STATE(ControllerInputState);
    OFS_REGISTER_STATE(VideoPlayerWindowState);
}
