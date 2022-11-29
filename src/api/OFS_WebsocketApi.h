#pragma once

#include <cstdint>
#include <vector>

class OFS_WebsocketApi
{
    private:
    void* ctx = nullptr;
    uint32_t stateHandle = 0xFFFF'FFFF;
    std::vector<uint32_t> scriptUpdateCooldown;

    public:
    OFS_WebsocketApi() noexcept;
    OFS_WebsocketApi(const OFS_WebsocketApi&) = delete;
    OFS_WebsocketApi(OFS_WebsocketApi&&) = delete;

    bool Init() noexcept;

    bool StartServer() noexcept;
    void StopServer() noexcept;

    void Update() noexcept;
    void ShowWindow(bool* open) noexcept;
    void Shutdown() noexcept;

    int ClientsConnected() const noexcept;
};