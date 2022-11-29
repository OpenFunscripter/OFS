#include "OFS_WebsocketApiClient.h"
#include "OFS_WebsocketApiEvents.h"

#include "OFS_EventSystem.h"

#include "OFS_Profiling.h"
#include "OFS_Util.h"
#include "nlohmann/json.hpp"

#include "civetweb.h"

#include "OpenFunscripter.h"

OFS_WebsocketClient::OFS_WebsocketClient() noexcept
{
    // FIXME: this is a lot of ugly code just to be able to unsubscribe from events
    std::vector<UnsubscribeFn> eventUnsubs;
    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsPlayChange::EventType, 
            EV::Queue().appendListener(WsPlayChange::EventType, WsPlayChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handlePlayChange)
            ))
        )
    );

    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsTimeChange::EventType, 
            EV::Queue().appendListener(WsTimeChange::EventType, WsTimeChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handleTimeChange)
            ))
        )
    );

    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsDurationChange::EventType, 
            EV::Queue().appendListener(WsDurationChange::EventType, WsDurationChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handleDurationChange)
            ))
        )
    );

    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsMediaChange::EventType, 
            EV::Queue().appendListener(WsMediaChange::EventType, WsMediaChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handleMediaChange)
            ))
        )
    );

    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsPlaybackSpeedChange::EventType, 
            EV::Queue().appendListener(WsPlaybackSpeedChange::EventType, WsPlaybackSpeedChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handlePlaybackSpeedChange)
            ))
        )
    );

    eventUnsubs.emplace_back(
        EV::MakeUnsubscibeFn(WsFunscriptChange::EventType, 
            EV::Queue().appendListener(WsFunscriptChange::EventType, WsFunscriptChange::HandleEvent(
                EVENT_SYSTEM_BIND(this, &OFS_WebsocketClient::handleFunscriptChange)
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

void OFS_WebsocketClient::handlePlayChange(const WsPlayChange* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    nlohmann::json json = *ev;
    auto jsonText = Util::SerializeJson(json);
    sendMessage(jsonText);
}

void OFS_WebsocketClient::handleTimeChange(const WsTimeChange* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    nlohmann::json json = *ev;
    auto jsonText = Util::SerializeJson(json);
    sendMessage(jsonText);
}

void OFS_WebsocketClient::handleDurationChange(const WsDurationChange* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    nlohmann::json json = *ev;
    auto jsonText = Util::SerializeJson(json);
    sendMessage(jsonText);
}

void OFS_WebsocketClient::handleMediaChange(const WsMediaChange* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    nlohmann::json json = *ev;
    auto jsonText = Util::SerializeJson(json);
    sendMessage(jsonText);
}

void OFS_WebsocketClient::handlePlaybackSpeedChange(const WsPlaybackSpeedChange* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    nlohmann::json json = *ev;
    auto jsonText = Util::SerializeJson(json);
    sendMessage(jsonText);
}

void OFS_WebsocketClient::handleFunscriptChange(const WsFunscriptChange* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    nlohmann::json json = { 
        {"type", "funscript_change"},
        {"name", ev->changedScript->Title()},
        {"funscript", ev->changedScript->Serialize(true)}
    };
    auto jsonText = Util::SerializeJson(json);
    sendMessage(jsonText);
}

void OFS_WebsocketClient::UpdateAll() noexcept
{
    // Update everything
    auto app = OpenFunscripter::ptr;
    WsMediaChange media(app->player->VideoPath());
    handleMediaChange(&media);
    WsPlaybackSpeedChange speed(app->player->CurrentSpeed());
    handlePlaybackSpeedChange(&speed);
    WsPlayChange playing(!app->player->IsPaused());
    handlePlayChange(&playing);
    WsDurationChange duration(app->player->Duration());
    handleDurationChange(&duration);
    WsTimeChange time(app->player->CurrentPlayerTime());
    handleTimeChange(&time);

    for(auto& script : app->LoadedFunscripts())
    {
        WsFunscriptChange scriptChange(script.get());
        handleFunscriptChange(&scriptChange);
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