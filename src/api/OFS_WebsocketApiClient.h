#pragma once
#include "OFS_EventSystem.h"
#include "OFS_WebsocketApiEvents.h"
#include "OFS_WebsocketApiCommands.h"

#include <string>

// This event is pushed to the internal websocket clients and not part of the API
class WsSerializedEvent : public OFS_Event<WsSerializedEvent>
{
    public:
    std::string serializedEvent;
    WsSerializedEvent(std::string&& json) noexcept
        : serializedEvent(std::move(json)) {}
};

class OFS_WebsocketClient
{
    private:
    UnsubscribeFn eventUnsub;
	struct mg_connection* conn = nullptr;

    void handleSerializedEvent(const WsSerializedEvent* ev) noexcept;
    void handleProjectChange(const WsProjectChange* ev) noexcept;
    void sendMessage(const std::string& msg) noexcept;
    
    public:
    static WsCommandBuffer CommandBuffer;

    OFS_WebsocketClient() noexcept;
    OFS_WebsocketClient(const OFS_WebsocketClient&) = delete;
    OFS_WebsocketClient(OFS_WebsocketClient&&) = delete;
    ~OFS_WebsocketClient() noexcept;

    void InitializeConnection(struct mg_connection* conn) noexcept;
    void UpdateAll() noexcept;
    void ReceiveText(char* data, size_t dateLen) noexcept;
};