#pragma once
#include "OFS_Lua.h"

class OFS_PlayerAPI
{
    private:
    static void Play(bool shouldPlay) noexcept;
    static void TogglePlay() noexcept;
    
    static void Seek(lua_Number time) noexcept;
    static lua_Number CurrentTime() noexcept;
    static lua_Number Duration() noexcept;
    static bool IsPlaying() noexcept;
    static std::string CurrentVideo() noexcept;
    static lua_Number FPS() noexcept;

    public:
    OFS_PlayerAPI(sol::state_view& L) noexcept;
    ~OFS_PlayerAPI() noexcept;
};
