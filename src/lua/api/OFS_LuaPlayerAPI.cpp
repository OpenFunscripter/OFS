#include "OFS_LuaPlayerAPI.h"
#include "OFS_LuaExtensionAPI.h"
#include "OFS_Util.h"
#include "OpenFunscripter.h"

OFS_PlayerAPI::~OFS_PlayerAPI() noexcept
{
}

OFS_PlayerAPI::OFS_PlayerAPI(sol::state_view& L) noexcept
{
    auto player = L.new_usertype<OFS_PlayerAPI>(OFS_ExtensionAPI::PlayerNamespace);
    player.set_function("Play", 
        sol::overload(
            OFS_PlayerAPI::Play,
            OFS_PlayerAPI::TogglePlay
        )
    );
    player["Seek"] = OFS_PlayerAPI::Seek;
    player["CurrentTime"] = OFS_PlayerAPI::CurrentTime;
    player["Duration"] = OFS_PlayerAPI::Duration;
    player["IsPlaying"] = OFS_PlayerAPI::IsPlaying;
    player["CurrentVideo"] = OFS_PlayerAPI::CurrentVideo;
    player["FPS"] = OFS_PlayerAPI::FPS;

    player["playbackSpeed"] = sol::property(OFS_PlayerAPI::getPlaybackSpeed, OFS_PlayerAPI::setPlaybackSpeed);
}

void OFS_PlayerAPI::TogglePlay() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->togglePlay();    
}

void OFS_PlayerAPI::Play(bool shouldPlay) noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->setPaused(!shouldPlay);
}

void OFS_PlayerAPI::Seek(lua_Number time) noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->setPositionExact(time);
}

lua_Number OFS_PlayerAPI::CurrentTime() noexcept
{
    auto app = OpenFunscripter::ptr;
    return app->player->getCurrentPositionSecondsInterp();
}

lua_Number OFS_PlayerAPI::Duration() noexcept
{
    auto app = OpenFunscripter::ptr;
    return app->player->getDuration();
}

bool OFS_PlayerAPI::IsPlaying() noexcept
{
    auto app = OpenFunscripter::ptr;
    return !app->player->isPaused();
}

std::string OFS_PlayerAPI::CurrentVideo() noexcept
{
    auto app = OpenFunscripter::ptr;
    return app->player->getVideoPath();
}

lua_Number OFS_PlayerAPI::FPS() noexcept
{
    auto app = OpenFunscripter::ptr;
    return app->player->getFps();
}

void OFS_PlayerAPI::setPlaybackSpeed(lua_Number speed) noexcept
{
    auto app = OpenFunscripter::ptr;
    app->player->setSpeed(speed);
}

lua_Number OFS_PlayerAPI::getPlaybackSpeed() noexcept
{
    auto app = OpenFunscripter::ptr;
    return app->player->getSpeed();
}