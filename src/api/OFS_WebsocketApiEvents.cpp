#include "OFS_WebsocketApiEvents.h"

void to_json(nlohmann::json& j, const WsPlayChange& p)
{
    j = nlohmann::json{ {"type", "event"}, {"ev", "play_change"}, {"playing", p.playing } };
}

void to_json(nlohmann::json& j, const WsTimeChange& p)
{
    j = nlohmann::json{ {"type", "event"}, {"ev", "time_change"}, {"time", p.time } };
}

void to_json(nlohmann::json& j, const WsDurationChange& p)
{
    j = nlohmann::json{ {"type", "event"}, {"ev", "duration_change"}, {"duration", p.duration } };
}

void to_json(nlohmann::json& j, const WsMediaChange& p)
{
    j = nlohmann::json{ {"type", "event"}, {"ev", "media_change"}, {"path", p.mediaPath } };
}

void to_json(nlohmann::json& j, const WsPlaybackSpeedChange& p)
{
    j = nlohmann::json{ {"type", "event"}, {"ev", "playbackspeed_change"}, {"speed", p.speed } };
}

void to_json(nlohmann::json& j, const WsProjectChange& p)
{
    j = nlohmann::json{ {"type", "event"}, { "ev", "project_change" } };
}