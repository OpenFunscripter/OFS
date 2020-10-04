#include "ControllerInput.h"

#include "OpenFunscripter.h"
#include "SDL.h"
ControllerInput ControllerInput::controllers[MAX_CONTROLLERS];

void ControllerInput::OpenController(int device)
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
	for (int i = 0; i < MAX_CONTROLLERS; ++i)
	{
		if (controllers[i].is_connected && controllers[i].instance_id == instance) {
			return i;
		}
	}
	return -1;
}

void ControllerInput::ControllerAxisMotion(SDL_Event& ev)
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

void ControllerInput::ControllerButtonDown(SDL_Event& ev)
{
	auto& cbutton = ev.cbutton;
	LOGF_DEBUG("down cbutton: %d", cbutton.button);
}

void ControllerInput::ControllerButtonUp(SDL_Event& ev)
{
	auto& cbutton = ev.cbutton;
	LOGF_DEBUG("up cbutton: %d", cbutton.button);
}

void ControllerInput::ControllerDeviceAdded(SDL_Event& ev)
{
	if (ev.cdevice.which < MAX_CONTROLLERS) {
		ControllerInput& jc = controllers[ev.cdevice.which];
		jc.OpenController(ev.cdevice.which);
	}
}

void ControllerInput::ControllerDeviceRemoved(SDL_Event& ev)
{
	int cIndex = GetControllerIndex(ev.cdevice.which);
	if (cIndex < 0) return; // unknown controller?
	ControllerInput& jc = controllers[cIndex];
	jc.CloseController();
}

void ControllerInput::setup()
{
	SDL_JoystickEventState(SDL_ENABLE);
	SDL_GameControllerEventState(SDL_ENABLE);
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERAXISMOTION, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerAxisMotion));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERBUTTONUP, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerButtonUp));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerButtonDown));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERDEVICEADDED, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerDeviceAdded));
	OpenFunscripter::ptr->events.Subscribe(SDL_CONTROLLERDEVICEREMOVED, EVENT_SYSTEM_BIND(this, &ControllerInput::ControllerDeviceRemoved));
}

void ControllerInput::update()
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

	static int prev_pos = 0;
	if (prev_pos != pos) {
		LOGF_INFO("%d", pos);
	}
	prev_pos = pos;
	//auto action = ctx->LoadedFunscript->GetActionAtTime(current_ms, ctx->player.getFrameTimeMs(), 0);
	//if (action != nullptr) {
	//	ctx->LoadedFunscript->EditAction(*action, FunscriptAction(action->at, pos, FunscriptActionFlag::RawAction));
	//}
	//else {
	//	ctx->LoadedFunscript->AddAction(FunscriptAction(current_ms, pos, FunscriptActionFlag::RawAction));
	//}
}
