#pragma once
#include <cstdint>
#include "OFS_StateManager.h"
#include "OFS_Profiling.h"

template<typename T>
class OFS_StateHandle
{
    public:
    static constexpr uint32_t InvalidId = 0xFFFF'FFFF;
    private:
    uint32_t Id = InvalidId;
    public:

    inline OFS_StateHandle(uint32_t id = OFS_StateHandle::InvalidId) noexcept
        : Id(id) {}

    T& Get() noexcept {
        OFS_PROFILE(__FUNCTION__);
        auto mgr = OFS_StateManager::Get();
        return mgr->template Get<T>(Id);
    }

    static uint32_t Register(const char* name) noexcept
    {
        auto mgr = OFS_StateManager::Get();
        return mgr->Register<T>(name);
    }
};