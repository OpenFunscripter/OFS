#include "OFS_ControllerInput.h"
#include "OFS_Util.h"
#include "KeybindingSystem.h"
#include "OFS_Profiling.h"
#include "OFS_EventSystem.h"

#include "SDL_gamecontroller.h"

#include "state/states/ControllerState.h"

std::array<int64_t, SDL_CONTROLLER_BUTTON_MAX> ButtonsHeldDown = { -1 };
std::array<ControllerInput, 4> ControllerInput::Controllers;
int32_t ControllerInput::activeControllers = 0;

uint32_t ControllerInput::stateHandle = 0xFFFF'FFFF;

void ControllerInput::OpenController(int device) noexcept
{
    gamepad = SDL_GameControllerOpen(device);
    SDL_Joystick* j = SDL_GameControllerGetJoystick(gamepad);
    instance_id = SDL_JoystickInstanceID(j);
    isConnected = true;
    LOGF_INFO("Controller \"%s\" connected!", SDL_GameControllerName(gamepad));
    if (SDL_JoystickIsHaptic(j)) {
        haptic = SDL_HapticOpenFromJoystick(j);
        LOGF_DEBUG("Haptic Effects: %d\n", SDL_HapticNumEffects(haptic));
        LOGF_DEBUG("Haptic Query: %x\n", SDL_HapticQuery(haptic));
        if (SDL_HapticRumbleSupported(haptic)) {
            if (SDL_HapticRumbleInit(haptic) != 0) {
                LOGF_DEBUG("Haptic Rumble Init: %s\n", SDL_GetError());
                SDL_HapticClose(haptic);
                haptic = 0;
            }
        }
        else {
            SDL_HapticClose(haptic);
            haptic = 0;
        }
    }
}

void ControllerInput::CloseController() noexcept
{
    if (isConnected) {
        isConnected = false;
        if (haptic) {
            SDL_HapticClose(haptic);
            haptic = 0;
        }
        SDL_GameControllerClose(gamepad);
        gamepad = 0;
    }
}

int ControllerInput::GetControllerIndex(SDL_JoystickID instance) noexcept
{
    for (int i = 0; i < Controllers.size(); i += 1) {
        if (Controllers[i].isConnected && Controllers[i].instance_id == instance) {
            return i;
        }
    }
    return -1;
}

void ControllerInput::ControllerButtonDown(const OFS_SDL_Event* ev) const noexcept
{
    OFS_PROFILE(__FUNCTION__);
    constexpr int32_t RepeatDelayMs = 300;
    auto& cbutton = ev->sdl.cbutton;
    ButtonsHeldDown[cbutton.button] = (int64_t)SDL_GetTicks() + RepeatDelayMs;
}

void ControllerInput::ControllerButtonUp(const OFS_SDL_Event* ev) const noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& cbutton = ev->sdl.cbutton;
    ButtonsHeldDown[cbutton.button] = -1;
}

void ControllerInput::ControllerDeviceAdded(const OFS_SDL_Event* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto& cdevice = ev->sdl.cdevice;
    if (cdevice.which < Controllers.size()) {
        auto& jc = Controllers[cdevice.which];
        jc.OpenController(cdevice.which);
        activeControllers += 1;
    }
}

void ControllerInput::ControllerDeviceRemoved(const OFS_SDL_Event* ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    int cIndex = GetControllerIndex(ev->sdl.cdevice.which);
    if (cIndex < 0) return; // unknown controller?
    auto& jc = Controllers[cIndex];
    jc.CloseController();
    activeControllers -= 1;
}

void ControllerInput::Init() noexcept
{
    FUN_ASSERT(ControllerInput::stateHandle == 0xFFFF'FFFF, "state already initialized");
    stateHandle = OFS_AppState<ControllerInputState>::Register(ControllerInputState::StateName);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);

    EV::Queue().appendListener(SDL_CONTROLLERDEVICEADDED,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerDeviceAdded)));
    EV::Queue().appendListener(SDL_CONTROLLERDEVICEREMOVED,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerDeviceRemoved)));
    EV::Queue().appendListener(SDL_CONTROLLERBUTTONDOWN,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerButtonDown)));
    EV::Queue().appendListener(SDL_CONTROLLERBUTTONUP,
        OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerButtonUp)));
}

void ControllerInput::Update(uint32_t buttonRepeatIntervalMs) noexcept
{
    int buttonEnumVal = 0;
    for (auto& button : ButtonsHeldDown) {
        if (button > 0 && ((int64_t)SDL_GetTicks() - button) >= buttonRepeatIntervalMs) {
            EV::Enqueue<ControllerButtonRepeatEvent>(buttonEnumVal);
            button = SDL_GetTicks();
        }
        buttonEnumVal += 1;
    }
}

void ControllerInput::UpdateControllers() noexcept
{
    auto& state = ControllerInputState::State(ControllerInput::StateHandle());
    for (auto& controller : Controllers) {
        if (controller.Connected()) {
            controller.Update(state.buttonRepeatIntervalMs);
        }
    }
}