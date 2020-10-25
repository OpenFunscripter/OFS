#include "ControllerInput.h"

#include "OpenFunscripter.h"
#include "SDL.h"

std::array<int64_t, SDL_CONTROLLER_BUTTON_MAX> ButtonsHeldDown = {-1};
std::array<ControllerInput, 4> ControllerInput::Controllers;
int32_t ControllerInput::activeControllers = 0;

void ControllerInput::OpenController(int device)
{
	gamepad = SDL_GameControllerOpen(device);
	SDL_Joystick* j = SDL_GameControllerGetJoystick(gamepad);
	instance_id = SDL_JoystickInstanceID(j);
	is_connected = true;
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

void ControllerInput::CloseController()
{
	if (is_connected) {
		is_connected = false;
		if (haptic) {
			SDL_HapticClose(haptic);
			haptic = 0;
		}
		SDL_GameControllerClose(gamepad);
		gamepad = 0;
	}
}

int ControllerInput::GetControllerIndex(SDL_JoystickID instance)
{
	for (int i = 0; i < Controllers.size(); ++i)
	{
		if (Controllers[i].is_connected && Controllers[i].instance_id == instance) {
			return i;
		}
	}
	return -1;
}

void ControllerInput::ControllerButtonDown(SDL_Event& ev) const noexcept
{
	const int64_t RepeatDelayMs = 300;
	auto& cbutton = ev.cbutton;
	ButtonsHeldDown[cbutton.button] = (int64_t)SDL_GetTicks() + RepeatDelayMs;
}

void ControllerInput::ControllerButtonUp(SDL_Event& ev) const noexcept
{
	auto& cbutton = ev.cbutton;
	ButtonsHeldDown[cbutton.button] = -1;
}

void ControllerInput::ControllerDeviceAdded(SDL_Event& ev) noexcept
{
	if (ev.cdevice.which < Controllers.size()) {
		ControllerInput& jc = Controllers[ev.cdevice.which];
		jc.OpenController(ev.cdevice.which);
		activeControllers++;
	}
}

void ControllerInput::ControllerDeviceRemoved(SDL_Event& ev) noexcept
{
	int cIndex = GetControllerIndex(ev.cdevice.which);
	if (cIndex < 0) return; // unknown controller?
	ControllerInput& jc = Controllers[cIndex];
	jc.CloseController();
	activeControllers--;
}

void ControllerInput::setup()
{
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	auto app = OpenFunscripter::ptr;
	app->events->Subscribe(SDL_CONTROLLERDEVICEADDED, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerDeviceAdded));
	app->events->Subscribe(SDL_CONTROLLERDEVICEREMOVED, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerDeviceRemoved));
	app->events->Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerButtonDown));
	app->events->Subscribe(SDL_CONTROLLERBUTTONUP, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerButtonUp));
}

void ControllerInput::update() noexcept
{
	const int64_t ButtonRepeatMS = 50;

	int buttonEnumVal = 0;
	for (auto&& button : ButtonsHeldDown) {
		if (button > 0 && ((int64_t)SDL_GetTicks() - button) >= ButtonRepeatMS) {
			SDL_Event ev;
			ev.type = EventSystem::ControllerButtonRepeat;
			ev.cbutton.button = buttonEnumVal;
			SDL_PushEvent(&ev);
			button = SDL_GetTicks();
		}
		buttonEnumVal++;
	}
}
