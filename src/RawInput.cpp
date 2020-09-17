#include "RawInput.h"

#include "OpenFunscripter.h"
#include "SDL.h"
RawInput RawInput::controllers[MAX_CONTROLLERS];

void RawInput::OpenController(int device)
{
	gamepad = SDL_GameControllerOpen(device);
	SDL_Joystick* j = SDL_GameControllerGetJoystick(gamepad);
	instance_id = SDL_JoystickInstanceID(j);
	is_connected = true;
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

void RawInput::CloseController()
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

int RawInput::GetControllerIndex(SDL_JoystickID instance)
{
	for (int i = 0; i < MAX_CONTROLLERS; ++i)
	{
		if (controllers[i].is_connected && controllers[i].instance_id == instance) {
			return i;
		}
	}
	return -1;
}

void RawInput::AxisMotion(SDL_Event& ev)
{
	auto& axis = ev.caxis;
	const float range = (float)std::numeric_limits<int16_t>::max() - ControllerDeadzone;

	if (axis.value >= 0 && axis.value < ControllerDeadzone)
		axis.value = 0;
	else if (axis.value < 0 && axis.value > -ControllerDeadzone)
		axis.value = 0;
	else if (axis.value >= ControllerDeadzone)
		axis.value -= ControllerDeadzone;
	else if (axis.value <= ControllerDeadzone)
		axis.value += ControllerDeadzone;


	switch (axis.axis) {
	case SDL_CONTROLLER_AXIS_LEFTX:
		left_x = axis.value / range;
		break;
	case SDL_CONTROLLER_AXIS_LEFTY:
		left_y = axis.value / range;
		break;
	case SDL_CONTROLLER_AXIS_RIGHTX:
		right_x = axis.value / range;
		break;
	case SDL_CONTROLLER_AXIS_RIGHTY:
		right_y = axis.value / range;
		break;
	case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
		left_trigger = axis.value / range;
		break;
	case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
		right_trigger = axis.value / range;
		break;
	}
}

void RawInput::ControllerButtonDown(SDL_Event& ev)
{

}

void RawInput::ControllerDeviceAdded(SDL_Event& ev)
{
	if (ev.cdevice.which < MAX_CONTROLLERS) {
		RawInput& jc = controllers[ev.cdevice.which];
		jc.OpenController(ev.cdevice.which);
	}
}

void RawInput::ControllerDeviceRemoved(SDL_Event& ev)
{
	int cIndex = GetControllerIndex(ev.cdevice.which);
	if (cIndex < 0) return; // unknown controller?
	RawInput& jc = controllers[cIndex];
	jc.CloseController();
}

void RawInput::setup()
{
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &RawInput::AxisMotion));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &RawInput::ControllerButtonDown));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERDEVICEADDED, EVENT_SYSTEM_BIND(this, &RawInput::ControllerDeviceAdded));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERDEVICEREMOVED, EVENT_SYSTEM_BIND(this, &RawInput::ControllerDeviceRemoved));
}

void RawInput::update()
{
	if (!RecordData) return;

	float right_len = std::sqrt((right_x * right_x) + (right_y * right_y)) ;
	float left_len =  std::sqrt((left_x * left_x) + (left_y * left_y))     ;

	float value = std::max(right_len, left_len);
	value = std::max(value, left_trigger);
	value = std::max(value, right_trigger);


	auto ctx = OpenFunscripter::ptr;
	int current_ms = ctx->player.getCurrentPositionMs();
	int pos = Util::Clamp(100.0 * value, 0.0, 100.0);

	//auto action = ctx->LoadedFunscript->GetActionAtTime(current_ms, ctx->player.getFrameTimeMs(), 0);
	//if (action != nullptr) {
	//	ctx->LoadedFunscript->EditAction(*action, FunscriptAction(action->at, pos, FunscriptActionFlag::RawAction));
	//}
	//else {
	//	ctx->LoadedFunscript->AddAction(FunscriptAction(current_ms, pos, FunscriptActionFlag::RawAction));
	//}
}
