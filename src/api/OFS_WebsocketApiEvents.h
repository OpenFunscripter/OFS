#pragma once
#include "OFS_Event.h"
#include "Funscript.h"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class WsMediaChange : public OFS_Event<WsMediaChange>
{
    public:
    std::string mediaPath;

    WsMediaChange(const std::string& path) noexcept
        : mediaPath(path) {}
};

class WsPlaybackSpeedChange : public OFS_Event<WsPlaybackSpeedChange>
{
    public:
    float speed;

    WsPlaybackSpeedChange(float speed) noexcept
        : speed(speed) {}
};

class WsPlayChange : public OFS_Event<WsPlayChange>
{
    public:
    bool playing = false;
    WsPlayChange(bool playing) noexcept
        : playing(playing) {}
};

class WsTimeChange : public OFS_Event<WsTimeChange>
{
    public:
    float time;
    WsTimeChange(float time) noexcept
        : time(time) {}
};

class WsDurationChange : public OFS_Event<WsDurationChange>
{
    public:
    float duration;
    WsDurationChange(float duration) noexcept
        : duration(duration) {}
};

class WsFunscriptChange : public OFS_Event<WsFunscriptChange>
{
    public:
    const Funscript* changedScript;

    WsFunscriptChange(const Funscript* changedScript) noexcept
        : changedScript(changedScript) {}
};

void to_json(nlohmann::json& j, const WsPlayChange& p);
void to_json(nlohmann::json& j, const WsTimeChange& p);
void to_json(nlohmann::json& j, const WsDurationChange& p);
void to_json(nlohmann::json& j, const WsMediaChange& p);
void to_json(nlohmann::json& j, const WsPlaybackSpeedChange& p);
