#include "OFS_WebsocketApiCommands.h"
#include <optional>

WsCommandBuffer::WsCommandBuffer() noexcept
{

}

inline static std::unique_ptr<WsCmd> CreateCommand(const std::string& name, const nlohmann::json& data) noexcept
{
    if(name == "change_time" && data["time"].is_number())
    {
        float time = data["time"].get<float>();
        return std::make_unique<WsTimeChangeCmd>(time);
    }
    else if(name == "change_play" && data["playing"].is_boolean())
    {
        bool playing = data["playing"].get<bool>();
        return std::make_unique<WsPlayChangeCmd>(playing);
    }
    else if(name == "change_playbackspeed" && data["speed"].is_number())
    {
        float speed = data["speed"].get<float>();
        return std::make_unique<WsPlaybackSpeedChangeCmd>(speed);
    }
    return {};
}

bool WsCommandBuffer::AddCmd(const nlohmann::json& jsonCmd) noexcept
{
    auto& type = jsonCmd["type"];
    if(!type.is_string() || type != "command") return false;

    auto& name = jsonCmd["name"];
    if(!name.is_string()) return false;

    auto& data = jsonCmd["data"];
    if(data.is_null()) return false;

    auto cmd = CreateCommand(name.get_ref<const std::string&>(), data);
    if(cmd)
    {
        SDL_AtomicLock(&commandLock);
        commands.emplace_back(std::move(cmd));
        SDL_AtomicUnlock(&commandLock);
        return true;
    }
    return false;
}

void WsCommandBuffer::ProcessCommands() noexcept
{
    if(commands.empty()) return;
    SDL_AtomicLock(&commandLock);
    for(auto& cmd : commands)
    {
        cmd->Run();
    }
    commands.clear();
    SDL_AtomicUnlock(&commandLock);
}


#include "OpenFunscripter.h"

void WsPlayChangeCmd::Run() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->SetPaused(!playing);
}

void WsPlaybackSpeedChangeCmd::Run() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->SetSpeed(speed);
}

void WsTimeChangeCmd::Run() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->SetPositionExact(time);
}