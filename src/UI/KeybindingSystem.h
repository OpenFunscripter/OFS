#pragma once


#include "OFS_Reflection.h"

#include "SDL.h"

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <unordered_map>

using BindingAction = std::function<void(void*)>;

struct Keybinding
{
	std::string key_str;
	SDL_Keycode key;
	Uint16 modifiers;

	Keybinding()
		: key(0), modifiers(0)/*, ignore_repeats(false)*/
	{}

	Keybinding(SDL_Keycode key, Uint16 mod/*, bool ignore_repeat*/)
		: key(key), modifiers(mod)/*, ignore_repeats(ignore_repeat)*/
	{}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(key_str, ar);
		OFS_REFLECT(key, ar);
		OFS_REFLECT(modifiers, ar);
		/*OFS_REFLECT(ignore_repeats, ar);*/
	}
};

struct ControllerBinding {
	int32_t button = -1;
	bool navmode = false;

	ControllerBinding()
		: button(-1), navmode(false) {}
	ControllerBinding(int32_t button, bool navmode)
		: button(button), navmode(navmode) {}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(button, ar);
		OFS_REFLECT(navmode, ar);
	}
};

struct Binding {
	std::string identifier;
	std::string description;
	bool ignore_repeats = true;
	Keybinding key;
	ControllerBinding controller;
	BindingAction action;

	Binding() {}

	Binding(const std::string& id, const std::string& description, bool ignore_repeats, BindingAction action)
		: identifier(id), description(description), ignore_repeats(ignore_repeats), action(action) {}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(identifier, ar);
		OFS_REFLECT(description, ar);
		OFS_REFLECT(key, ar);
		OFS_REFLECT(controller, ar);
		OFS_REFLECT(ignore_repeats, ar);
	}
};

struct KeybindingGroup {
	std::string name;
	std::vector<Binding> bindings;

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(name, ar);
		OFS_REFLECT(bindings, ar);
	}
};

class KeybindingSystem 
{
	std::stringstream currentlyHeldKeys;
	Binding* currentlyChanging = nullptr;
	std::unordered_map<std::string, std::string> binding_string_cache;
	bool ControllerOnly = false;
	std::string filterString;

	bool SetControllerSpeed = false;

	void addKeyString(const char* name);
	void addKeyString(char name);
	std::vector<KeybindingGroup> ActiveBindings;
	std::string loadKeyString(SDL_Keycode key, int mod);
	
	void ProcessControllerBindings(SDL_Event& ev, bool repeat) noexcept;

	void KeyPressed(SDL_Event& ev) noexcept;
	void ControllerButtonRepeat(SDL_Event& ev) noexcept;
	void ControllerButtonDown(SDL_Event& ev) noexcept;

	int32_t lastAxis = 0;
	void ControllerAxis(SDL_Event& ev) noexcept;
public:
	bool ShowWindow = false;

	void setup();
	const std::string& getBindingString(const char* binding_id) noexcept;
	const std::vector<KeybindingGroup>& getBindings() const { return ActiveBindings; }
	void setBindings(const std::vector<KeybindingGroup>& bindings);
	void registerBinding(const KeybindingGroup& group);
	bool ShowBindingWindow();

	// TODO: get this out of here including SetControllerSpeed, ControllerAxis & lastAxis
	inline void SetControllerPlaybackSpeed() { SetControllerSpeed = true; }
};