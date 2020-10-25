#pragma once

#include "SDL.h"
#include <array>

// lifted from https://gist.github.com/urkle/6701236
// and rewritten for my purposes

class ControllerInput {
private:
	SDL_GameController* gamepad;
	SDL_Haptic* haptic;
	SDL_JoystickID instance_id;
	bool is_connected = false;
	void OpenController(int device);
	void CloseController();

	static int32_t activeControllers;
	static int GetControllerIndex(SDL_JoystickID instance);

	void ControllerButtonDown(SDL_Event& ev) const noexcept;
	void ControllerButtonUp(SDL_Event& ev) const noexcept;
	void ControllerDeviceAdded(SDL_Event& ev) noexcept;
	void ControllerDeviceRemoved(SDL_Event& ev) noexcept;
public:
	static std::array<ControllerInput, 4> Controllers;

	void setup();
	void update() noexcept;

	inline static void UpdateControllers() {
		for (auto&& controller : Controllers) {
			if (controller.connected()) {
				controller.update();
			}
		}
	}
	inline const char* GetName() const noexcept { return SDL_GameControllerName(gamepad); }
	inline bool connected() const noexcept { return is_connected; }
	static inline bool AnythingConnected() noexcept { return activeControllers > 0; }
};