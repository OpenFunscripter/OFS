#include "KeybindingSystem.h"

#include "OFS_Serialization.h"
#include "EventSystem.h"
#include "OFS_Profiling.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <array>
#include "SDL_gamecontroller.h"

KeybindingSystem* KeybindingSystem::ptr = nullptr;

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

static constexpr std::array<uint16_t, 3> possibleModifiers{
    KMOD_SHIFT,
    KMOD_CTRL,
    KMOD_ALT
};

static const char* GetButtonString(int32_t button) noexcept {
    if (button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX) {
        //return SDL_GameControllerGetStringForButton((SDL_GameControllerButton)button);
        return gameButtonString[button];
    }
    else {
        return "- Not set -";
    }
}

static uint16_t GetModifierState(uint16_t modstate) noexcept
{
    // filter out modifiers which shouldn't affect bindings
    modstate &= ~(KMOD_NUM | KMOD_CAPS | KMOD_GUI | KMOD_MODE);

    // convert KMOD_LSHIFT / KMOD_RSHIFT -> KMOD_SHIFT etc.
    uint16_t genericModifiers = 0;
    for (auto possibleModifier : possibleModifiers) {
        if (modstate & possibleModifier)
            genericModifiers |= possibleModifier;
    }
    modstate = genericModifiers;

    return modstate;
}

void KeybindingSystem::handleBindingModification(SDL_Event& ev, uint16_t modstate) noexcept
{
    auto& key = ev.key;
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

    if (modstate & KMOD_CTRL) {
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
    bindingStringLUT[currentlyChanging->identifier] = currentlyChanging->key.key_str;


    currentlyChanging->key.modifiers = modstate;
    currentlyChanging = nullptr;
}

void KeybindingSystem::handlePassiveBindingModification(SDL_Event& ev, uint16_t modstate) noexcept
{
    auto& key = ev.key;
    if (key.repeat) return;
    currentlyHeldKeys.str("");

    if (modstate & KMOD_CTRL) {
        addKeyString("Ctrl");
    }

    if (modstate & KMOD_ALT) {
        addKeyString("Alt");
    }

    if (modstate & KMOD_SHIFT) {
        addKeyString("Shift");
    }

    currentlyChangingPassive->key.key = 0;
    currentlyChangingPassive->key.key_str = currentlyHeldKeys.str();
    bindingStringLUT[currentlyChangingPassive->identifier] = currentlyChangingPassive->key.key_str;
    currentlyChangingPassive->key.modifiers = modstate;

    passiveBindingLUT[currentlyChangingPassive->identifier] = *currentlyChangingPassive;

    currentlyChangingPassive = nullptr;
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
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    ptr = this;
    events.Subscribe(SDL_KEYDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::KeyPressed));
    events.Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonDown));
    events.Subscribe(KeybindingEvents::ControllerButtonRepeat, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonRepeat));
}

void KeybindingSystem::KeyPressed(SDL_Event& ev) noexcept
{
    auto key = ev.key;
    auto modstate = GetModifierState(key.keysym.mod);
   
    if (currentlyChanging != nullptr) {
        handleBindingModification(ev, modstate);
        return;
    }
    else if (currentlyChangingPassive != nullptr) {
        handlePassiveBindingModification(ev, modstate);
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
        OFS_BENCHMARK(binding.identifier.c_str());
        auto handler = dynamicHandlers.find(binding.dynamicHandlerId);
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
            OFS_BENCHMARK(binding.identifier.c_str());
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
                    OFS_BENCHMARK(binding.identifier.c_str());
                    auto handler = dynamicHandlers.find(binding.dynamicHandlerId);
                    handler->second(&binding);
                }
            }
            else {
                // execute binding
                OFS_BENCHMARK(binding.identifier.c_str());
                auto handler = dynamicHandlers.find(binding.dynamicHandlerId);
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
                        OFS_BENCHMARK(binding.identifier.c_str());
                        binding.execute();
                    }
                }
                else {
                    OFS_BENCHMARK(binding.identifier.c_str());
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



void KeybindingSystem::addKeyString(const char* name) noexcept
{
    if (currentlyHeldKeys.tellp() == 0)
        currentlyHeldKeys << name;
    else
        currentlyHeldKeys << "+" << name;
}

void KeybindingSystem::addKeyString(char name) noexcept
{
    if (currentlyHeldKeys.tellp() == 0)
        currentlyHeldKeys << name;
    else
        currentlyHeldKeys << "+" << name;
}

std::string KeybindingSystem::loadKeyString(SDL_Keycode key, int mod) noexcept
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
    if (key > 0) { addKeyString(SDL_GetKeyName(key)); }

    return currentlyHeldKeys.str();
}

const std::string& KeybindingSystem::getBindingString(const char* binding_id) noexcept
{
    auto it = bindingStringLUT.find(binding_id);
    if (it != bindingStringLUT.end())
        return bindingStringLUT[binding_id];
    static std::string empty("", 0);
    return empty;
}

void KeybindingSystem::setBindings(const Keybindings& bindings) noexcept
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
                    bindingStringLUT[it->identifier] = it->key.key_str;

                    // controller
                    it->controller.button = keybind.controller.button;
                    it->controller.navmode = keybind.controller.navmode;
                }
            }
        }
    }

    // load passive modifier
    for (auto& group : bindings.passiveGroups) {
        for (auto& keybind : group.bindings)
        {
            auto groupIt = std::find_if(ActiveBindings.passiveGroups.begin(), ActiveBindings.passiveGroups.end(),
                [&](auto& activeGroup) { return activeGroup.name == group.name; });

            if (groupIt != ActiveBindings.passiveGroups.end()) {
                auto it = std::find_if(groupIt->bindings.begin(), groupIt->bindings.end(),
                    [&](auto& active) { return active.identifier == keybind.identifier; });

                if (it != groupIt->bindings.end()) {
                    // override defaults
                    it->key.key = keybind.key.key;
                    it->key.modifiers = keybind.key.modifiers;
                    it->key.key_str = loadKeyString(keybind.key.key, keybind.key.modifiers);

                    passiveBindingLUT[it->identifier] = *it;
                    bindingStringLUT[it->identifier] = it->key.key_str;
                }
            }
        }
    }

    ActiveBindings.DynamicBindings = bindings.DynamicBindings;
}

void KeybindingSystem::registerBinding(KeybindingGroup&& group) noexcept
{
    ActiveBindings.groups.emplace_back(std::move(group));
    for (auto& binding : ActiveBindings.groups.back().bindings) {
        binding.key.key_str = loadKeyString(binding.key.key, binding.key.modifiers);
        bindingStringLUT[binding.identifier] = binding.key.key_str;
    }
}

void KeybindingSystem::registerPassiveBindingGroup(PassiveBindingGroup&& pgroup) noexcept
{
    auto& pair = ActiveBindings.passiveGroups.emplace_back(std::move(pgroup));
    for (auto& binding : pair.bindings) {
        binding.key.key_str = loadKeyString(binding.key.key, binding.key.modifiers);
        bindingStringLUT[binding.identifier] = binding.key.key_str;
        passiveBindingLUT.insert(std::move(std::make_pair(binding.identifier, binding)));
    }
}

void KeybindingSystem::addDynamicBinding(Binding&& binding) noexcept
{
    auto it = std::find_if(ActiveBindings.DynamicBindings.bindings.begin(), ActiveBindings.DynamicBindings.bindings.end(),
        [&](auto& b) {
            return b.identifier == binding.identifier;
    });
    binding.key.key_str = loadKeyString(binding.key.key, binding.key.modifiers);
    if (it == ActiveBindings.DynamicBindings.bindings.end()) {
        ActiveBindings.DynamicBindings.bindings.emplace_back(std::move(binding));
    }
    else {
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


void KeybindingSystem::addPassiveBindingGroup(PassiveBindingGroup& group, bool& save) noexcept
{
    ImGui::Columns(1);
    if (ImGui::CollapsingHeader(group.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2);
        ImGui::Separator();
        ImGui::Text("Description"); ImGui::NextColumn();
        ImGui::Text("Keyboard"); ImGui::NextColumn();

        for (auto& binding : group.bindings) {
            ImGui::PushID(binding.description.c_str());
            ImGui::TextUnformatted(binding.description.c_str()); ImGui::NextColumn();
            if (ImGui::Button(!binding.key.key_str.empty() ? binding.key.key_str.c_str() : "-- Not set --", ImVec2(-1.f, 0.f))) {
                changingController = false;
                currentlyChangingPassive = &binding;
                currentlyHeldKeys.str("");
                ImGui::OpenPopup("Change key");
            }
            if (ImGui::BeginPopupModal("Change key", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (currentlyHeldKeys.tellp() == 0) { 
                    ImGui::TextUnformatted("Press any key...\nEscape to clear."); 
                }
                else {
                    ImGui::Text(currentlyHeldKeys.str().c_str());
                }
                if (!currentlyChangingPassive) {
                    save = true; // autosave
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }

}

void KeybindingSystem::passiveBindingTab(bool& save) noexcept
{
    ImGui::TextUnformatted("Here modifiers can be configured which change certain behaviour.");
    ImGui::TextUnformatted("Right now only a single modifier can be assigned. (Shift, Ctrl, Alt)");
    for(int i=0; i < ActiveBindings.passiveGroups.size(); i++) {
        auto& group = ActiveBindings.passiveGroups[i];
        ImGui::PushID(i);
        addPassiveBindingGroup(group, save);
        ImGui::PopID();
    }
}

bool KeybindingSystem::ShowBindingWindow() noexcept
{
    bool save = false;
    if (ShowWindow)
        ImGui::OpenPopup("Keys");

    if (ImGui::BeginPopupModal("Keys", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        OFS_PROFILE(__FUNCTION__);

        if (ImGui::BeginTabBar("##KeysTabBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Bindings"))
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
                for(auto&& group : ActiveBindings.groups) {
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
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Modifiers")) {
                passiveBindingTab(save);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::EndPopup();
    }
    return save;
}

bool KeybindingSystem::PassiveModifier(const char* name) noexcept
{
    auto it = ptr->passiveBindingLUT.find(name);
    if (it != ptr->passiveBindingLUT.end()) {
        uint16_t modstate = GetModifierState(SDL_GetModState());
        if (modstate & it->second.key.modifiers) {
            return true;
        }
    }
    return false;
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