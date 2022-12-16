#pragma once

#include <vector>
#include <variant>
#include <memory>

#include "SDL_atomic.h"
#include "OFS_Util.h"

class WsCmd 
{
    public:
    virtual void Run() noexcept = 0;
};

class WsPlayChangeCmd : public WsCmd
{
    public:
    bool playing = false;
    WsPlayChangeCmd(bool playing) noexcept
        : playing(playing) {}
    
    void Run() noexcept override;
};

class WsPlaybackSpeedChangeCmd : public WsCmd
{
    public:
    float speed = 1.f;
    WsPlaybackSpeedChangeCmd(float speed) noexcept
        : speed(speed) {}

    void Run() noexcept override;
};

class WsTimeChangeCmd : public WsCmd
{
    public:
    float time = 0.f;
    WsTimeChangeCmd(float time) noexcept
        : time(time) {}

    void Run() noexcept override;
};

class WsCommandBuffer
{
    private:
    std::vector<std::unique_ptr<WsCmd>> commands;
    SDL_SpinLock commandLock = {0};
    public:

    WsCommandBuffer() noexcept;
    bool AddCmd(const nlohmann::json& jsonCmd) noexcept;
    void ProcessCommands() noexcept;
};
