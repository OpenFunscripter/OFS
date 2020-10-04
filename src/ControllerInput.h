#pragma once

#include "SDL.h"

// lifted from https://gist.github.com/urkle/6701236
// and rewritten for my purposes

#define MAX_CONTROLLERS 4
class ControllerInput {
private:
	SDL_GameController* gamepad;
	SDL_Haptic* haptic;
	SDL_JoystickID instance_id;
	bool is_connected;
	
	void OpenController(int device);
	void CloseController();

	static ControllerInput controllers[MAX_CONTROLLERS];
	static int GetControllerIndex(SDL_JoystickID instance);

	void ControllerAxisMotion(SDL_Event& ev);
	void ControllerButtonDown(SDL_Event& ev);
	void ControllerButtonUp(SDL_Event& ev);
	void ControllerDeviceAdded(SDL_Event& ev);
	void ControllerDeviceRemoved(SDL_Event& ev);
public:
	void setup();
	void update();

	inline bool connected() const { return is_connected; }
};