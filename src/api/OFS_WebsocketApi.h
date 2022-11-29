#pragma once

#include <cstdint>
#include <vector>

class OFS_WebsocketApi
{
    private:
    void* ctx = nullptr;
    std::vector<uint32_t> scriptUpdateCooldown;

    public:
    OFS_WebsocketApi() noexcept;

    int ClientsConnected() const noexcept;

    bool Init() noexcept;
    void Update() noexcept;
    void Shutdown() noexcept;
};