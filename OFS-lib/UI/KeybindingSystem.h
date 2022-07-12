#pragma once
#include "OFS_Reflection.h"
#include "OFS_Localization.h"
#include "OFS_Util.h"

#include "EventSystem.h"

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <unordered_map>

#include "SDL_events.h"

class KeybindingEvents
{
public:
	static int32_t ControllerButtonRepeat;
	static void RegisterEvents() noexcept;
};

using BindingAction = std::function<void(void*)>;
using DynamicBindingHandler = std::function<void(class Binding*)>;

struct Keybinding
{
	std::string key_str;
	SDL_Keycode key;
	Uint16 modifiers;

	Keybinding() noexcept
		: key(0), modifiers(0)
	{}

	Keybinding(SDL_Keycode key, Uint16 mod) noexcept
		: key(key), modifiers(mod)
	{}

	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(key_str, ar);
		OFS_REFLECT(key, ar);
		OFS_REFLECT(modifiers, ar);
	}
};

struct ControllerBinding {
	int32_t button = -1;
	bool navmode = false;

	ControllerBinding() noexcept
		: button(-1), navmode(false) {}
	ControllerBinding(int32_t button, bool navmode) noexcept
		: button(button), navmode(navmode) {}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(button, ar);
		OFS_REFLECT(navmode, ar);
	}
};

struct Binding {
	std::string identifier;
	Tr displayName;
	Keybinding key;
	ControllerBinding controller;
	BindingAction action;
	void* userdata = nullptr;
	bool ignoreRepeats = true;

	std::string dynamicHandlerId = "";
	std::string dynamicName = "";

	Binding() noexcept {}

	Binding(const std::string& id, Tr displayName, bool ignoreRepeats, BindingAction action) noexcept
		: identifier(id), displayName(displayName), ignoreRepeats(ignoreRepeats), action(action) {}
	
	Binding(const std::string& id, const std::string dynamicName, bool ignoreRepeats, BindingAction action) noexcept
		: identifier(id), dynamicName(dynamicName), ignoreRepeats(ignoreRepeats), action(action), displayName(Tr::INVALID_TR) {}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(identifier, ar);
		OFS_REFLECT(key, ar);
		OFS_REFLECT(controller, ar);
		OFS_REFLECT(ignoreRepeats, ar);
		OFS_REFLECT(dynamicHandlerId, ar);
		OFS_REFLECT(dynamicName, ar);
	}

	inline void execute() noexcept {
		if (action != nullptr) {
			action(userdata == nullptr ? this : userdata);
		}
	}

	inline const char* name() const noexcept
	{
		if(dynamicHandlerId.empty()) {
			return TRD(displayName);
		}
		else {
			return dynamicName.c_str();
		}
	}
};

struct KeybindingGroup {
	std::string name;
	Tr displayName;
	std::vector<Binding> bindings;

	KeybindingGroup() noexcept
		: name(std::string()), displayName(Tr::INVALID_TR)
	{}
	KeybindingGroup(const char* name_id, Tr displayName) noexcept
		: name(name_id), displayName(displayName)
	{}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(name, ar);
		OFS_REFLECT(bindings, ar);
	}
};


struct PassiveBinding
{
	std::string identifier;
	Tr displayName;
	Keybinding key;
	bool active = true;

	PassiveBinding() noexcept {}

	PassiveBinding(const std::string& id, Tr displayName, bool active = true) noexcept
		: identifier(id), displayName(displayName), active(active) {}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(identifier, ar);
		OFS_REFLECT(key, ar);
		OFS_REFLECT(active, ar);
	}
};

struct PassiveBindingGroup
{
	std::string name;
	Tr displayName;
	std::vector<PassiveBinding> bindings;

	PassiveBindingGroup() noexcept
		: name(std::string()), displayName(Tr::INVALID_TR)
	{}
	PassiveBindingGroup(const char* name_id, Tr displayName) noexcept
		: name(name_id), displayName(displayName)
	{}

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(name, ar);
		OFS_REFLECT(bindings, ar);
	}
};

constexpr const char* CurrentKeybindingsVersion = "2";
struct Keybindings {
	std::string config_version = CurrentKeybindingsVersion;
	std::vector<KeybindingGroup> groups;
	std::vector<PassiveBindingGroup> passiveGroups;

	KeybindingGroup DynamicBindings{ "Dynamic", Tr::DYNAMIC_BINDING_GROUP };

	template<class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(config_version, ar);
		if (config_version != CurrentKeybindingsVersion) {
			LOGF_WARN("Keybindings version \"%s\" didn't match \"%s\". Bindings are reset.", config_version.c_str(), CurrentKeybindingsVersion);
			config_version = CurrentKeybindingsVersion;
			return;
		}
		OFS_REFLECT(groups, ar);
		OFS_REFLECT(passiveGroups, ar);
		OFS_REFLECT(DynamicBindings, ar);
	}
};

class KeybindingSystem 
{
private:
	std::string changeModalText;
	Binding* currentlyChanging = nullptr;
	PassiveBinding* currentlyChangingPassive = nullptr;
	uint32_t passiveChangingStartTimer = 0;
	uint16_t passiveChangingTempModifiers = 0;
	static constexpr uint32_t PassiveChangingTimeMs = 3000;

	bool changingController = false;
	bool ControllerOnly = false;
	std::string filterString;

	Keybindings ActiveBindings;
	std::unordered_map<std::string, DynamicBindingHandler> dynamicHandlers;
	std::unordered_map<std::string, std::string> bindingStrings;
	std::unordered_map<std::string, PassiveBinding> passiveBindings;
	std::string keybindingPath;

	void addKeyString(const char* name) noexcept;
	void addKeyString(char name) noexcept;
	std::string loadKeyString(SDL_Keycode key, int mod) noexcept;
	
	void ProcessControllerBindings(SDL_Event& ev, bool repeat) noexcept;
	void handleBindingModification(SDL_Event& ev, uint16_t modstate) noexcept;
	void handlePassiveBindingModification(SDL_Event& ev, uint16_t modstate) noexcept;

	void KeyPressed(SDL_Event& ev) noexcept;
	void ControllerButtonRepeat(SDL_Event& ev) noexcept;
	void ControllerButtonDown(SDL_Event& ev) noexcept;

	void addBindingsGroup(KeybindingGroup& group, bool& save, bool deletable = false) noexcept;

	void addPassiveBindingGroup(PassiveBindingGroup& group, bool& save) noexcept;
	void passiveBindingTab(bool& save) noexcept;

	void changeModals(bool& save) noexcept;
public:
	bool ShowWindow = false;

	bool load(const std::string& path) noexcept;
	void save() noexcept;

	void setup(class EventSystem& events);
	const char* getBindingString(const char* binding_id) noexcept;
	const Keybindings& getBindings() const noexcept { return ActiveBindings; }
	void setBindings(const Keybindings& bindings) noexcept;
	void registerBinding(KeybindingGroup&& group) noexcept;
	void registerPassiveBindingGroup(PassiveBindingGroup&& pgroup) noexcept;

	void addDynamicBinding(Binding&& binding) noexcept;
	void removeDynamicBinding(const std::string& id) noexcept;

	void registerDynamicHandler(const std::string& id, DynamicBindingHandler&& handler) noexcept {
		dynamicHandlers.insert(std::make_pair(id, handler));
	}

	bool ShowBindingWindow() noexcept;

	static KeybindingSystem* ptr;
	static bool PassiveModifier(const char* name) noexcept;
};