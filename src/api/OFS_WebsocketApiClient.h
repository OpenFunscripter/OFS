#pragma once
#include "OFS_EventSystem.h"
#include "OFS_WebsocketApiEvents.h"

class OFS_WebsocketClient
{
    private:
    UnsubscribeFn eventUnsub;
	struct mg_connection* conn = nullptr;

    void handlePlayChange(const WsPlayChange* ev) noexcept;
    void handleTimeChange(const WsTimeChange* ev) noexcept;
    void handleDurationChange(const WsDurationChange* ev) noexcept;
    void handleMediaChange(const WsMediaChange* ev) noexcept;
    void handlePlaybackSpeedChange(const WsPlaybackSpeedChange* ev) noexcept;
    void handleFunscriptChange(const WsFunscriptChange* ev) noexcept;
    void handleFunscriptRemove(const WsFunscriptRemove* ev) noexcept;
    void handleProjectChange(const WsProjectChange* ev) noexcept;
    
    void sendMessage(const std::string& msg) noexcept;
    public:

    OFS_WebsocketClient() noexcept;
    OFS_WebsocketClient(const OFS_WebsocketClient&) = delete;
    OFS_WebsocketClient(OFS_WebsocketClient&&) = delete;
    ~OFS_WebsocketClient() noexcept;

    void InitializeConnection(struct mg_connection* conn) noexcept;
    void UpdateAll() noexcept;

    void ReceiveText(char* data, size_t dateLen) noexcept;
};