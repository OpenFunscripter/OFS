#pragma once
#include <cstdint>
#include "OFS_StateManager.h"
#include "OFS_Profiling.h"

template<typename T>
class OFS_AppState
{
    public:
    static constexpr uint32_t InvalidId = 0xFFFF'FFFF;
    private:
    uint32_t Id = InvalidId;
    public:

    inline OFS_AppState(uint32_t id = OFS_AppState::InvalidId) noexcept
        : Id(id) {}

    T& Get() noexcept {
        OFS_PROFILE(__FUNCTION__);
        auto mgr = OFS_StateManager::Get();
        return mgr->template GetApp<T>(Id);
    }

    static uint32_t Register(const char* name) noexcept
    {
        auto mgr = OFS_StateManager::Get();
        return mgr->RegisterApp<T>(name);
    }
};

template<typename T>
class OFS_ProjectState
{
    public:
    static constexpr uint32_t InvalidId = 0xFFFF'FFFF;
    private:
    uint32_t Id = InvalidId;
    public:

    inline OFS_ProjectState(uint32_t id = OFS_ProjectState::InvalidId) noexcept
        : Id(id) {}

    T& Get() noexcept {
        OFS_PROFILE(__FUNCTION__);
        auto mgr = OFS_StateManager::Get();
        return mgr->template GetProject<T>(Id);
    }

    static uint32_t Register(const char* name) noexcept
    {
        auto mgr = OFS_StateManager::Get();
        return mgr->RegisterProject<T>(name);
    }
};