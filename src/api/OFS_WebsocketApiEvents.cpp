#include "OFS_WebsocketApiEvents.h"

inline static void initializeEvent(nlohmann::json& j, const char* eventName)
{
    j = { { "type", "event" }, { "name", eventName } };
}

void to_json(nlohmann::json& j, const WsPlayChange& p)
{
    initializeEvent(j, "play_change");
    j["data"] = { { "playing",  p.playing } };
}

void to_json(nlohmann::json& j, const WsTimeChange& p)
{
    initializeEvent(j, "time_change");
    j["data"] = { {"time", p.time } };
}

void to_json(nlohmann::json& j, const WsDurationChange& p)
{
    initializeEvent(j, "duration_change");
    j["data"] = { {"duration", p.duration } };
}

void to_json(nlohmann::json& j, const WsMediaChange& p)
{
    initializeEvent(j, "media_change");
    j["data"] = { {"path", p.mediaPath } };
}

void to_json(nlohmann::json& j, const WsPlaybackSpeedChange& p)
{
    initializeEvent(j, "playbackspeed_change");
    j["data"] = { {"speed", p.speed } };
}

void to_json(nlohmann::json& j, const WsProjectChange& p)
{
    initializeEvent(j, "project_change");
    j["data"] = nlohmann::json::object();
}

void to_json(nlohmann::json& j, const WsFunscriptChange& p) 
{
    initializeEvent(j, "funscript_change");
    nlohmann::json funscript;
    Funscript::Serialize(funscript, p.funscriptData, p.funscriptMetadata);
    j["data"] = { { "name", p.name }, { "funscript",  std::move(funscript) } };
}

void to_json(nlohmann::json& j, const WsFunscriptRemove& p)
{
    initializeEvent(j, "funscript_remove");
    j["data"] = { {"name", p.name } };
}