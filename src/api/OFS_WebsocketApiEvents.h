#pragma once
#include "OFS_Event.h"
#include "Funscript.h"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

struct ToJsonInterface
{
    virtual void Serialize(nlohmann::json& json) noexcept = 0;
};

void to_json(nlohmann::json& j, const class WsProjectChange& p);
void to_json(nlohmann::json& j, const class WsPlayChange& p);
void to_json(nlohmann::json& j, const class WsTimeChange& p);
void to_json(nlohmann::json& j, const class WsDurationChange& p);
void to_json(nlohmann::json& j, const class WsMediaChange& p);
void to_json(nlohmann::json& j, const class WsPlaybackSpeedChange& p);
void to_json(nlohmann::json& j, const class WsFunscriptChange& p);
void to_json(nlohmann::json& j, const class WsFunscriptRemove& p);

class WsMediaChange : public OFS_Event<WsMediaChange>, public ToJsonInterface
{
    public:
    std::string mediaPath;

    WsMediaChange(const std::string& path) noexcept
        : mediaPath(path) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsPlaybackSpeedChange : public OFS_Event<WsPlaybackSpeedChange>, public ToJsonInterface
{
    public:
    float speed;

    WsPlaybackSpeedChange(float speed) noexcept
        : speed(speed) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsPlayChange : public OFS_Event<WsPlayChange>, public ToJsonInterface
{
    public:
    bool playing = false;
    WsPlayChange(bool playing) noexcept
        : playing(playing) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsTimeChange : public OFS_Event<WsTimeChange>, public ToJsonInterface
{
    public:
    float time;
    WsTimeChange(float time) noexcept
        : time(time) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsDurationChange : public OFS_Event<WsDurationChange>, public ToJsonInterface
{
    public:
    float duration;
    WsDurationChange(float duration) noexcept
        : duration(duration) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsFunscriptChange : public OFS_Event<WsFunscriptChange>, public ToJsonInterface
{
    public:
    std::string name;
    Funscript::FunscriptData funscriptData;
    Funscript::Metadata funscriptMetadata;

    WsFunscriptChange(const std::string& name, Funscript::FunscriptData funscriptData, Funscript::Metadata metadata) noexcept
        : name(name), funscriptData(std::move(funscriptData)), funscriptMetadata(std::move(metadata)) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsProjectChange : public OFS_Event<WsProjectChange>, public ToJsonInterface
{
    public:
    WsProjectChange() noexcept {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

class WsFunscriptRemove : public OFS_Event<WsFunscriptRemove>, public ToJsonInterface
{
    public:
    std::string name;
    WsFunscriptRemove(const std::string& name) noexcept
        : name(name) {}

    void Serialize(nlohmann::json& json) noexcept override { to_json(json, *this); }
};

