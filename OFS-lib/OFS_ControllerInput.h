#pragma once
#include "OFS_Event.h"

#include "SDL_gamecontroller.h"
#include "SDL_haptic.h"
#include <array>

class ControllerInput {
private:
    SDL_GameController* gamepad;
    SDL_Haptic* haptic;
    SDL_JoystickID instance_id;
    bool isConnected = false;

    void OpenController(int device) noexcept;
    void CloseController() noexcept;

    static int32_t activeControllers;
    static int GetControllerIndex(SDL_JoystickID instance) noexcept;

    void ControllerButtonDown(const OFS_SDL_Event* ev) const noexcept;
    void ControllerButtonUp(const OFS_SDL_Event* ev) const noexcept;
    void ControllerDeviceAdded(const OFS_SDL_Event* ev) noexcept;
    void ControllerDeviceRemoved(const OFS_SDL_Event* ev) noexcept;

public:
    static std::array<ControllerInput, 4> Controllers;

    void Init() noexcept;
    void Update() noexcept;

    static void UpdateControllers() noexcept;

    inline const char* GetName() const noexcept { return SDL_GameControllerName(gamepad); }
    inline bool Connected() const noexcept { return isConnected; }
    static inline bool AnythingConnected() noexcept { return activeControllers > 0; }
};