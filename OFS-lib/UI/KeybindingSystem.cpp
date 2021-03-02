#include "KeybindingSystem.h"

#include "OFS_Serialization.h"
#include "EventSystem.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <array>
#include "SDL_gamecontroller.h"

// strings for SDL_GameControllerButton enum
static const std::array<const char*, SDL_CONTROLLER_BUTTON_MAX> gameButtonString {
    "A / X",
    "B / Circle",
    "X / Square",
    "Y / Triangle",
    "Back / Share",
    "Guide",
    "Start",
    "Leftstick",
    "Rightstick",
    "Leftshoulder",
    "Rightshoulder",
    "DPAD Up",
    "DPAD Down",
    "DPAD Left",
    "DPAD Right"
};


static const char* GetButtonString(int32_t button) {
    if (button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX) {
        //return SDL_GameControllerGetStringForButton((SDL_GameControllerButton)button);
        return gameButtonString[button];
    }
    else {
        return "- Not set -";
    }
}

bool KeybindingSystem::load(const std::string& path) noexcept
{
    keybindingPath = path;
    bool succ = false;
    auto json = Util::LoadJson(path, &succ);
    if (succ) {
        Keybindings bindings;
        OFS::serializer::load(&bindings, &json["keybindings"]);
        setBindings(bindings);
    }
    return succ;
}

void KeybindingSystem::save() noexcept
{
    nlohmann::json json;
    OFS::serializer::save(&ActiveBindings, &json["keybindings"]);
    Util::WriteJson(json, keybindingPath, true);
}

void KeybindingSystem::setup(EventSystem& events)
{
    events.Subscribe(SDL_KEYDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::KeyPressed));
    events.Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonDown));
    events.Subscribe(KeybindingEvents::ControllerButtonRepeat, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonRepeat));
}

void KeybindingSystem::KeyPressed(SDL_Event& ev) noexcept
{
    auto key = ev.key;
    auto modstate = key.keysym.mod;
    
    std::array<uint16_t, 3> possibleModifiers{
        KMOD_SHIFT,
        KMOD_CTRL,
        KMOD_ALT
    };
    // filter out modifiers which shouldn't affect bindings
    modstate &= ~(KMOD_NUM | KMOD_CAPS | KMOD_GUI | KMOD_MODE);

    // convert KMOD_LSHIFT / KMOD_RSHIFT -> KMOD_SHIFT etc.
    uint16_t genericModifiers = 0;
    for (auto possibleModifier : possibleModifiers) {
        if (modstate & possibleModifier)
            genericModifiers |= possibleModifier;
    }
    modstate = genericModifiers;

    if (currentlyChanging != nullptr) {
        if (key.repeat) return;
        if (key.keysym.sym == SDLK_ESCAPE) {
            if (changingController) {
                currentlyChanging->controller.button = SDL_CONTROLLER_BUTTON_INVALID;
            }
            else {
                currentlyChanging->key.key = SDLK_UNKNOWN;
                currentlyChanging->key.modifiers = 0;
                currentlyChanging->key.key_str = "";
            }
            currentlyChanging = nullptr;
            return;
        }
        currentlyHeldKeys.str("");
        
        if(modstate & KMOD_CTRL) {
            addKeyString("Ctrl");
            switch (key.keysym.sym) {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
                return;
            }
        }

        if (modstate & KMOD_ALT) {
            addKeyString("Alt");
            switch (key.keysym.sym) {
            case SDLK_LALT:
            case SDLK_RALT:
                return;
            }
        }

        if (modstate & KMOD_SHIFT) {
            addKeyString("Shift");
            switch (key.keysym.sym) {
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
            return;
            }
        }
        
        // check duplicate
        for (auto&& group : ActiveBindings.groups) {
            for (auto&& binding : group.bindings) {
                if (binding.key.key == key.keysym.sym && binding.key.modifiers == modstate) {
                    if (binding.identifier == currentlyChanging->identifier) {
                        // the binding is being set to the key it already has which is fine
                        goto breaking_out_of_nested_loop_lol;
                    }
                    LOGF_INFO("Key already bound for \"%s\"", binding.description.c_str());
                    currentlyHeldKeys.str("");
                    return;
                }
            }
        }

        breaking_out_of_nested_loop_lol:

        addKeyString(SDL_GetKeyName(key.keysym.sym));
        currentlyChanging->key.key = key.keysym.sym;
        currentlyChanging->key.key_str = currentlyHeldKeys.str();
        binding_string_cache[currentlyChanging->identifier] = currentlyChanging->key.key_str;


        currentlyChanging->key.modifiers = modstate;
        currentlyChanging = nullptr;
        return;
    }
    if (ShowWindow) return;

    // this prevents keybindings from being processed when typing into a textbox etc.
    if (ImGui::IsAnyItemActive()) return;

    // process dynamic bindings
    for (auto& binding : ActiveBindings.DynamicBindings.bindings)
    {
        if (key.repeat && binding.ignore_repeats) continue;
        
        if (key.keysym.sym == binding.key.key) {
            bool modifierMismatch = false;
            for (auto possibleModifier : possibleModifiers) {
                if ((modstate & possibleModifier) != (binding.key.modifiers & possibleModifier)) {
                    modifierMismatch = true;
                    break;
                }
            }
            if (modifierMismatch) continue;
        }
        else { continue; }
        // execute binding
        auto& handler = dynamicHandlers.find(binding.dynamicHandlerId);
        handler->second(&binding);
        return;
    }

    // process bindings
    for (auto& group : ActiveBindings.groups) {
        for (auto& binding : group.bindings) {
            if (key.repeat && binding.ignore_repeats) continue;

            if (key.keysym.sym == binding.key.key) {
                bool modifierMismatch = false;
                for (auto possibleModifier : possibleModifiers) {
                    if ((modstate & possibleModifier) != (binding.key.modifiers & possibleModifier)) {
                        modifierMismatch = true;
                        break;
                    }
                }
                if (modifierMismatch) continue;
            }
            else {
                continue;
            }

            // execute binding
            binding.execute();
            return;
        }
    }
}

void KeybindingSystem::ProcessControllerBindings(SDL_Event& ev, bool repeat) noexcept
{
    auto& cbutton = ev.cbutton;
    bool navmodeActive = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad;

    // process dynamic bindings
    for (auto& binding : ActiveBindings.DynamicBindings.bindings)
    {
        if ((binding.ignore_repeats && repeat) || binding.controller.button < 0) { continue; }

        if (binding.controller.button == cbutton.button) {
            if (navmodeActive) {
                // navmode bindings get processed during navmode
                // everything else doesn't get processed during navmode
                if (binding.controller.navmode) {
                    // execute binding
                    auto& handler = dynamicHandlers.find(binding.dynamicHandlerId);
                    handler->second(&binding);
                }
            }
            else {
                // execute binding
                auto& handler = dynamicHandlers.find(binding.dynamicHandlerId);
                handler->second(&binding);
            }
        }
        return;
    }

    // process bindings
    for (auto&& group : ActiveBindings.groups) {
        for (auto& binding : group.bindings) {
            if ((binding.ignore_repeats && repeat) || binding.controller.button < 0) { continue; }
            if (binding.controller.button == cbutton.button) {
                if (navmodeActive) {
                    // navmode bindings get processed during navmode
                    // everything else doesn't get processed during navmode
                    if (binding.controller.navmode) {
                        binding.execute();
                    }
                }
                else {
                    binding.execute();
                }
            }
        }
    }
}

void KeybindingSystem::ControllerButtonRepeat(SDL_Event& ev) noexcept
{
    if (currentlyChanging != nullptr) return;
    if (ShowWindow) return;
    //auto& cbutton = ev.cbutton; // only cbutton.button is set
    ProcessControllerBindings(ev, true);
}

void KeybindingSystem::ControllerButtonDown(SDL_Event& ev) noexcept
{
    if (currentlyChanging != nullptr) {
        auto& cbutton = ev.cbutton;
        // check duplicate
        for (auto&& group : ActiveBindings.groups) {
            for (auto&& binding : group.bindings) {
                if (binding.controller.button == cbutton.button) {
                    if (binding.identifier == currentlyChanging->identifier) {
                        // the binding is being set to the key it already has which is fine
                        goto breaking_out_of_nested_loop_lol;
                    }
                    LOGF_INFO("The button is already bound for \"%s\"", binding.description.c_str());
                    return;
                }
            }
        }
        breaking_out_of_nested_loop_lol:
        currentlyChanging->controller.button = cbutton.button;
        currentlyChanging = nullptr;
        return;
    }
    if (ShowWindow) return;
    ProcessControllerBindings(ev, false);
}



void KeybindingSystem::addKeyString(const char* name)
{
    if (currentlyHeldKeys.tellp() == 0)
        currentlyHeldKeys << name;
    else
        currentlyHeldKeys << "+" << name;
}

void KeybindingSystem::addKeyString(char name)
{
    if (currentlyHeldKeys.tellp() == 0)
        currentlyHeldKeys << name;
    else
        currentlyHeldKeys << "+" << name;
}

std::string KeybindingSystem::loadKeyString(SDL_Keycode key, int mod)
{
    currentlyHeldKeys.str("");
    if (mod & KMOD_CTRL) {
        addKeyString("Ctrl");
    }

    if (mod & KMOD_ALT) {
        addKeyString("Alt");
    }

    if (mod & KMOD_SHIFT) {
        addKeyString("Shift");
    }
    addKeyString(SDL_GetKeyName(key));

    return currentlyHeldKeys.str();
}

const std::string& KeybindingSystem::getBindingString(const char* binding_id) noexcept
{
    auto it = binding_string_cache.find(binding_id);
    if (it != binding_string_cache.end())
        return binding_string_cache[binding_id];
    static std::string empty("", 0);
    return empty;
}

void KeybindingSystem::setBindings(const Keybindings& bindings)
{
    // setBindings only does something if the bindings were previously registered
    for (auto& group : bindings.groups) {
        for (auto& keybind : group.bindings) {
            auto groupIt = std::find_if(ActiveBindings.groups.begin(), ActiveBindings.groups.end(),
                [&](auto& activeGroup) { return activeGroup.name == group.name; });

            if (groupIt != ActiveBindings.groups.end()) {
                auto it = std::find_if(groupIt->bindings.begin(), groupIt->bindings.end(),
                    [&](auto& active) { return active.identifier == keybind.identifier; });

                if (it != groupIt->bindings.end()) {
                    // override defaults
                    it->ignore_repeats = keybind.ignore_repeats;
                    it->key.key = keybind.key.key;
                    it->key.modifiers = keybind.key.modifiers;
                    it->key.key_str = loadKeyString(keybind.key.key, keybind.key.modifiers);
                    binding_string_cache[it->identifier] = it->key.key_str;

                    // controller
                    it->controller.button = keybind.controller.button;
                    it->controller.navmode = keybind.controller.navmode;
                }
            }
        }
    }
    ActiveBindings.DynamicBindings = bindings.DynamicBindings;
}

void KeybindingSystem::registerBinding(const KeybindingGroup& group)
{
    ActiveBindings.groups.emplace_back(std::move(group));
    for (auto& binding : ActiveBindings.groups.back().bindings) {
        binding.key.key_str = loadKeyString(binding.key.key, binding.key.modifiers);
        binding_string_cache[binding.identifier] = binding.key.key_str;
    }
}

void KeybindingSystem::addDynamicBinding(Binding&& binding) noexcept
{
    auto it = std::find_if(ActiveBindings.DynamicBindings.bindings.begin(), ActiveBindings.DynamicBindings.bindings.end(),
        [&](auto& b) {
            return b.identifier == binding.identifier;
    });
    binding.key.key_str = loadKeyString(binding.key.key, binding.key.modifiers);
    if (it == ActiveBindings.DynamicBindings.bindings.end())
    {
        ActiveBindings.DynamicBindings.bindings.emplace_back(std::move(binding));
    }
    else
    {
        *it = std::move(binding);
    }
}

void KeybindingSystem::removeDynamicBinding(const std::string& identifier) noexcept
{
    auto it = std::find_if(ActiveBindings.DynamicBindings.bindings.begin(), ActiveBindings.DynamicBindings.bindings.end(),
        [&](auto& b) {
            return b.identifier == identifier;
        });
    if (it != ActiveBindings.DynamicBindings.bindings.end()) {
        // fast remove
        *it = ActiveBindings.DynamicBindings.bindings.back();
        ActiveBindings.DynamicBindings.bindings.pop_back();
    }
}

bool KeybindingSystem::ShowBindingWindow()
{
    bool save = false;
    if (ShowWindow)
        ImGui::OpenPopup("Keys");

    if (ImGui::BeginPopupModal("Keys", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        constexpr bool disable_indent = true;
        if constexpr (disable_indent)
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

        ImGui::TextUnformatted("You can use CTRL, SHIFT & ALT as modifiers.");
        ImGui::TextUnformatted("Only controller buttons can be bound. The DPAD directions count as a buttons.");
        ImGui::TextUnformatted("The bindings get saved everytime a change is made.");
        ImGui::TextUnformatted("Config: \"data/keybindings.json\"\nIf you wan't to revert to defaults delete the config.");
        
        ImGui::Spacing();
        ImGui::InputText("Filter", &filterString); 
        ImGui::SameLine(); ImGui::Checkbox(" " ICON_GAMEPAD " only", &ControllerOnly);
        ImGui::Spacing();

        auto& style = ImGui::GetStyle();
        ImGui::Separator();
        int id = 0;
        for(auto&& group : ActiveBindings.groups)
        {
            ImGui::PushID(id++);
            addBindingsGroup(group, save);
            ImGui::PopID();
        }
        ImGui::PushID(id++);
        addBindingsGroup(ActiveBindings.DynamicBindings, save, true);
        ImGui::PopID();
        ImGui::Columns(1);
        ImGui::Separator();

        if constexpr (disable_indent)
            ImGui::PopStyleVar();

        ImGui::EndPopup();
    }
    return save;
}

int32_t KeybindingEvents::ControllerButtonRepeat = 0;
void KeybindingEvents::RegisterEvents() noexcept
{
    ControllerButtonRepeat = SDL_RegisterEvents(1);
}

void KeybindingSystem::addBindingsGroup(KeybindingGroup& group, bool& save, bool deletable) noexcept
{
    int32_t headerFlags = ImGuiTreeNodeFlags_None;
    std::vector<Binding*> filteredBindings;
    auto& style = ImGui::GetStyle();

    for (auto&& binding : group.bindings) {
        if (ControllerOnly && binding.controller.button < 0) { continue; }
        if (!filterString.empty() && !Util::ContainsInsensitive(binding.description, filterString)) { continue; }
        filteredBindings.emplace_back(&binding);
    }
    if (filteredBindings.size() == 0) { return; }
    if (ControllerOnly || !filterString.empty()) {
        headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    ImGui::Columns(1);
    if (ImGui::CollapsingHeader(group.name.c_str(), headerFlags)) {
        ImGui::PushID(group.name.c_str());

        ImGui::Columns(4, "bindings");
        ImGui::Separator();
        ImGui::Text("Action"); ImGui::NextColumn();
        ImGui::Text("Keyboard"); ImGui::NextColumn();
        ImGui::Text("Controller"); ImGui::NextColumn();
        ImGui::Text("Ignore repeats"); ImGui::NextColumn();

        Binding* deleteBinding = nullptr;
        for (auto bindingPtr : filteredBindings) {
            auto& binding = *bindingPtr;
            ImGui::PushID(bindingPtr->description.c_str());
            ImGui::TextUnformatted(binding.description.c_str()); ImGui::NextColumn();
            if (ImGui::Button(!binding.key.key_str.empty() ? binding.key.key_str.c_str() : "-- Not set --", ImVec2(-1.f, 0.f))) {
                changingController = false;
                currentlyChanging = &binding;
                currentlyHeldKeys.str("");
                ImGui::OpenPopup("Change key");
            }
            ImGui::NextColumn();
            if (ImGui::Button(GetButtonString(binding.controller.button), ImVec2(-1.f, 0.f))) {
                changingController = true;
                currentlyChanging = &binding;
                ImGui::OpenPopup("Change button");
            }
            ImGui::NextColumn();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth(3) / 2.f) - (2.f * ImGui::GetFontSize()) + style.ItemSpacing.x);
            if (ImGui::Checkbox("", &binding.ignore_repeats)) { save = true; }
            if (deletable) {
                ImGui::SameLine();
                if (ImGui::Button(ICON_TRASH))
                {
                    deleteBinding = bindingPtr;
                }
            }
            ImGui::NextColumn();
            if (ImGui::BeginPopupModal("Change key", 0, ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (currentlyHeldKeys.tellp() == 0)
                    ImGui::TextUnformatted("Press any key...\nEscape to clear.");
                else
                    ImGui::Text(currentlyHeldKeys.str().c_str());
                if (!currentlyChanging) {
                    save = true; // autosave
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal("Change button")) {
                ImGui::TextUnformatted("Press any button...\nEscape to clear.");
                if (!currentlyChanging) {
                    save = true; // autosave
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        // online dynamic bindings are deletable
        if (deleteBinding) {
            auto it = std::find_if(ActiveBindings.DynamicBindings.bindings.begin(), ActiveBindings.DynamicBindings.bindings.end(),
                [&](auto& b) {
                    return &b == deleteBinding;
                });
            if (it != ActiveBindings.DynamicBindings.bindings.end()) {
                ActiveBindings.DynamicBindings.bindings.erase(it);
                deleteBinding = nullptr;
                save = true;
            }
        }
        ImGui::PopID();
    }
}