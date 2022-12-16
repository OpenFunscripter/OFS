#include "OFS_WebsocketApiClient.h"
#include "OFS_WebsocketApiEvents.h"

#include "OFS_EventSystem.h"

#include "OFS_Profiling.h"
#include "OFS_Util.h"
#include "nlohmann/json.hpp"

#include "civetweb.h"
#include "OpenFunscripter.h"

WsCommandBuffer OFS_WebsocketClient::CommandBuffer = WsCommandBuffer();

OFS_WebsocketClient::OFS_WebsocketClient() noexcept
{
    LOG_DEBUG("Created new websocket client.");
    std::vector<UnsubscribeFn> eventUnsubs;
    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsSerializedEvent::EventType, 
            EV::Queue().appendListener(WsSerializedEvent::EventType, WsSerializedEvent::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handleSerializedEvent)
            ))
        )
    );

    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsProjectChange::EventType, 
            EV::Queue().appendListener(WsProjectChange::EventType, WsProjectChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handleProjectChange)
            ))
        )
    );

    eventUnsub = [eventUnsubs = std::move(eventUnsubs)]()
    {
        for(auto& unsub : eventUnsubs) {
            unsub();
        }
    };
}

OFS_WebsocketClient::~OFS_WebsocketClient() noexcept
{
    LOG_DEBUG("Destroying websocket client.");
    eventUnsub();   
}

void OFS_WebsocketClient::sendMessage(const std::string& msg) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if(conn == nullptr) return;
    if(mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, msg.data(), msg.size()) < 0)
    {
        LOG_ERROR("Failed to send websocket message.");
    }
}

void OFS_WebsocketClient::handleSerializedEvent(const WsSerializedEvent* ev) noexcept
{
    // NOTE: this is not called by the main thread
    OFS_PROFILE(__FUNCTION__);
    sendMessage(ev->serializedEvent);
}

void OFS_WebsocketClient::handleProjectChange(const WsProjectChange* ev) noexcept
{
    // NOTE: this is called by the main thread
    UpdateAll();
}

void OFS_WebsocketClient::UpdateAll() noexcept
{
    // Update everything
    auto app = OpenFunscripter::ptr;

    auto serializeSend = [this](auto&& event) noexcept
    {
        nlohmann::json json = event;
        auto jsonText = Util::SerializeJson(json);
        sendMessage(jsonText);
    };

    serializeSend(std::move(WsProjectChange()));
    serializeSend(std::move(WsMediaChange(app->player->VideoPath())));
    serializeSend(std::move(WsPlaybackSpeedChange(app->player->CurrentSpeed())));
    serializeSend(std::move(WsPlayChange(!app->player->IsPaused())));
    serializeSend(std::move(WsDurationChange(app->player->Duration())));
    serializeSend(std::move(WsTimeChange(app->player->CurrentPlayerTime())));

    auto& projectState = app->LoadedProject->State();
    for(auto& script : app->LoadedFunscripts())
    {
        serializeSend(std::move(WsFunscriptChange(script->Title(), script->Data(), projectState.metadata)));
    }
}

void OFS_WebsocketClient::InitializeConnection(mg_connection* conn) noexcept
{
    if(this->conn) return;
    this->conn = conn;
    /* Send "hello" message. */
	const char* hello = "{\"connected\":\"OFS " OFS_LATEST_GIT_TAG "@" OFS_LATEST_GIT_HASH "\"}";
	mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, hello, strlen(hello));
    UpdateAll();
}

void OFS_WebsocketClient::ReceiveText(char* data, size_t dataLen) noexcept
{
    // NOTE: Assume this function isn't called on the main thread.
    bool succ;
    std::string_view dataView(data, dataLen);
    auto json = nlohmann::json::parse(dataView, nullptr, false, true);
    if(!json.is_discarded())
    {
        // Valid json
        if(CommandBuffer.AddCmd(json))
        {
            // Success
        }
    }
}