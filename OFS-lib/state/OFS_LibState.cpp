#include "state/OFS_LibState.h"
#include "state/states/ControllerState.h"

void OFS_LibState::RegisterAll() noexcept
{
    OFS_REGISTER_STATE(ControllerInputState);
}
