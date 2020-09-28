#pragma once

#include "SDL.h"

// lifted from https://gist.github.com/urkle/6701236
// and rewritten for my purposes

#define MAX_CONTROLLERS 4
class RawInput {
private:
	SDL_GameController* gamepad;
	SDL_Haptic* haptic;
	SDL_JoystickID instance_id;
	bool is_connected;
	
	float right_x = 0.f, right_y = 0.f;
	float left_x = 0.f, left_y = 0.f;
	float right_trigger = 0.f;
	float left_trigger = 0.f;

	const int16_t ControllerDeadzone = 2500;
	void OpenController(int device);
	void CloseController();

	static RawInput controllers[MAX_CONTROLLERS];
	static int GetControllerIndex(SDL_JoystickID instance);

	void ControllerAxisMotion(SDL_Event& ev);
	void ControllerButtonDown(SDL_Event& ev);
	void ControllerButtonUp(SDL_Event& ev);
	void ControllerDeviceAdded(SDL_Event& ev);
	void ControllerDeviceRemoved(SDL_Event& ev);
public:
	bool RecordData = false;
	void setup();
	void update();

	inline bool connected() const { return is_connected; }
};