#include "state/OFS_LibState.h"
#include "state/states/VideoplayerWindowState.h"
#include "state/states/BaseOverlayState.h"
#include "state/states/WaveformState.h"
#include "state/states/KeybindingState.h"

void OFS_LibState::RegisterAll() noexcept
{
    OFS_REGISTER_STATE(VideoPlayerWindowState);
    OFS_REGISTER_STATE(BaseOverlayState);
    OFS_REGISTER_STATE(WaveformState);
    OFS_REGISTER_STATE(OFS_KeybindingState);
}
