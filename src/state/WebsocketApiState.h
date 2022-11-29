#pragma once
#include "OFS_StateHandle.h"
#include <string>

struct WebsocketApiState
{
    static constexpr auto StateName = "WebsocketApi";
    std::string port = "8080";
    bool serverActive = false;

    static inline WebsocketApiState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<WebsocketApiState>(stateHandle).Get();
    }
};

REFL_TYPE(WebsocketApiState)
    REFL_FIELD(port)
    REFL_FIELD(serverActive)
REFL_END