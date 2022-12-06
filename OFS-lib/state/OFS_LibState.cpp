#include "state/OFS_LibState.h"
#include "state/states/VideoplayerWindowState.h"
#include "state/states/BaseOverlayState.h"
#include "state/states/WaveformState.h"
#include "state/states/KeybindingState.h"
#include "state/states/ChapterState.h"

void OFS_LibState::RegisterAll() noexcept
{
    // App state
    OFS_REGISTER_STATE(OFS_KeybindingState);
    OFS_REGISTER_STATE(BaseOverlayState);

    // Project state
    OFS_REGISTER_STATE(VideoPlayerWindowState);
    OFS_REGISTER_STATE(WaveformState);
    OFS_REGISTER_STATE(ChapterState);
}
