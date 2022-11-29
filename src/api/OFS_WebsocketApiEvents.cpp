#include "OFS_WebsocketApiEvents.h"

void to_json(nlohmann::json& j, const WsPlayChange& p)
{
    j = nlohmann::json{ {"type", "play_change"}, {"playing", p.playing } };
}

void to_json(nlohmann::json& j, const WsTimeChange& p)
{
    j = nlohmann::json{ {"type", "time_change"}, {"time", p.time } };
}

void to_json(nlohmann::json& j, const WsDurationChange& p)
{
    j = nlohmann::json{ {"type", "duration_change"}, {"duration", p.duration } };
}

void to_json(nlohmann::json& j, const WsMediaChange& p)
{
    j = nlohmann::json{ {"type", "media_change"}, {"path", p.mediaPath } };
}

void to_json(nlohmann::json& j, const WsPlaybackSpeedChange& p)
{
    j = nlohmann::json{ {"type", "playbackspeed_change"}, {"speed", p.speed } };
}