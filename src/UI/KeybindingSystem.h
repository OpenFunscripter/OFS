#pragma once


#include "OFS_Reflection.h"

#include "SDL.h"

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <unordered_map>

using KeybindingAction = std::function<void(void*)>;

struct Keybinding
{
	std::string identifier;
	std::string description;
	std::string key_str;
	SDL_Keycode key;
	Uint16 modifiers;

	bool ignore_repeats = true;
	KeybindingAction action;

	Keybinding() {}

	Keybinding(const std::string& id, const std::string& description, SDL_Keycode key, Uint16 mod, bool ignore_repeat, KeybindingAction action)
		: identifier(id), description(description), key(key), modifiers(mod), ignore_repeats(ignore_repeat), action(action)
	{}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(identifier, ar);
		OFS_REFLECT(description, ar);
		OFS_REFLECT(key_str, ar);
		OFS_REFLECT(key, ar);
		OFS_REFLECT(modifiers, ar);
		OFS_REFLECT(ignore_repeats, ar);
	}
};

class KeybindingSystem 
{
	std::stringstream currentlyHeldKeys;
	Keybinding* currentlyChanging = nullptr;
	std::unordered_map<std::string, std::string> binding_string_cache;

	void addKeyString(const char* name);
	void addKeyString(char name);
	std::vector<Keybinding> ActiveBindings;
	std::string loadKeyString(SDL_Keycode key, int mod);
	void pressed(SDL_Event& ev);
public:
	void setup();
	const std::string& getBindingString(const char* binding_id) noexcept;
	const std::vector<Keybinding>& getBindings() const { return ActiveBindings; }
	void setBindings(const std::vector<Keybinding>& bindings);
	bool ShowWindow = false;
	void registerBinding(const Keybinding& binding);
	bool ShowBindingWindow();
};