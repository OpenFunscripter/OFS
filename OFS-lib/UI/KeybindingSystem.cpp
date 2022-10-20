#include "KeybindingSystem.h"

#include "OFS_Serialization.h"
#include "EventSystem.h"
#include "OFS_Profiling.h"
#include "OFS_Localization.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <array>

#include "SDL_timer.h"
#include "SDL_gamecontroller.h"

int32_t KeybindingEvents::ControllerButtonRepeat = 0;
void KeybindingEvents::RegisterEvents() noexcept
{
    ControllerButtonRepeat = SDL_RegisterEvents(1);
}

KeybindingSystem* KeybindingSystem::ptr = nullptr;

// strings for SDL_GameControllerButton enum
static const std::array<Tr, SDL_CONTROLLER_BUTTON_MAX> gameButtonTr {
    Tr::CONTROLLER_BUTTON_A,
    Tr::CONTROLLER_BUTTON_B,
    Tr::CONTROLLER_BUTTON_X,
    Tr::CONTROLLER_BUTTON_Y,
    Tr::CONTROLLER_BUTTON_BACK,
    Tr::CONTROLLER_BUTTON_GUIDE,
    Tr::CONTROLLER_BUTTON_START,
    Tr::CONTROLLER_BUTTON_LEFTSTICK,
    Tr::CONTROLLER_BUTTON_RIGHTSTICK,
    Tr::CONTROLLER_BUTTON_LEFTSHOULDER,
    Tr::CONTROLLER_BUTTON_RIGHTSHOULDER,
    Tr::CONTROLLER_BUTTON_DPAD_UP,
    Tr::CONTROLLER_BUTTON_DPAD_DOWN,
    Tr::CONTROLLER_BUTTON_DPAD_LEFT,
    Tr::CONTROLLER_BUTTON_DPAD_RIGHT,
};

static constexpr std::array<uint16_t, 3> possibleModifiers {
    KMOD_SHIFT,
    KMOD_CTRL,
    KMOD_ALT
};

static const char* GetButtonString(int32_t button) noexcept {
    if (button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX) {
        return TRD(gameButtonTr[button]);
    }
    else {
        return TR(CONTROLLER_NOT_SET);
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
    OFS_PROFILE(__FUNCTION__);
    if (key.keysym.sym == SDLK_ESCAPE) {
        if (changingController) {
            currentlyChanging->controller.button = SDL_CONTROLLER_BUTTON_INVALID;
        }
        else {
            currentlyChanging->key.key = SDLK_UNKNOWN;
            currentlyChanging->key.modifiers = 0;
            currentlyChanging->key.keyStr = "";
        }
        currentlyChanging = nullptr;
        return;
    }
    changeModalText = std::string();

    if (modstate & KMOD_CTRL) {
        addKeyString(TR(KEY_MOD_CTRL));
        switch (key.keysym.sym) {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            return;
        }
    }

    if (modstate & KMOD_ALT) {
        addKeyString(TR(KEY_MOD_ALT));
        switch (key.keysym.sym) {
        case SDLK_LALT:
        case SDLK_RALT:
            return;
        }
    }

    if (modstate & KMOD_SHIFT) {
        addKeyString(TR(KEY_MOD_SHIFT));
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
                LOGF_INFO("Key already bound for \"%s\"", TRD(binding.displayName));
                Util::MessageBoxAlert(TR(KEY_ALREADY_IN_USE), FMT(TR(KEY_ALREADY_IN_USE_MSG), TRD(binding.displayName)));
                changeModalText = std::string();
                return;
            }
        }
    }

breaking_out_of_nested_loop_lol:

    addKeyString(SDL_GetKeyName(key.keysym.sym));
    currentlyChanging->key.key = key.keysym.sym;
    currentlyChanging->key.keyStr = changeModalText;
    bindingStrings[currentlyChanging->identifier] = currentlyChanging->key.keyStr;


    currentlyChanging->key.modifiers = modstate;
    currentlyChanging = nullptr;
}

void KeybindingSystem::handlePassiveBindingModification(SDL_Event& ev, uint16_t modstate) noexcept
{
    auto& key = ev.key;
    if (key.repeat) return;
    OFS_PROFILE(__FUNCTION__);
    changeModalText = std::string();

    if (modstate & KMOD_CTRL) {
        addKeyString(TR(KEY_MOD_CTRL));
    }

    if (modstate & KMOD_ALT) {
        addKeyString(TR(KEY_MOD_ALT));
    }

    if (modstate & KMOD_SHIFT) {
        addKeyString(TR(KEY_MOD_ALT));
    }

    passiveChangingTempModifiers = modstate;
}

bool KeybindingSystem::load(const std::string& path) noexcept
{
    keybindingPath = path;
    bool succ = false;
    auto jsonText = Util::ReadFileString(path.c_str());
    auto json = Util::ParseJson(jsonText, &succ);
    if (succ) {
        Keybindings bindings;
        OFS::Serializer::Deserialize(bindings, json["keybindings"]);
        setBindings(bindings);
    }
    return succ;
}

void KeybindingSystem::save() noexcept
{
    nlohmann::json json;
    OFS::Serializer::Serialize(ActiveBindings, json["keybindings"]);
    auto jsonText = Util::SerializeJson(json, true);
    Util::WriteFile(keybindingPath.c_str(), jsonText.data(), jsonText.size());
}

void KeybindingSystem::setup(EventSystem& events)
{
    FUN_ASSERT(ptr == nullptr, "there can only be one instance");
    KeybindingSystem::ptr = this;
    events.Subscribe(SDL_KEYDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::KeyPressed));
    events.Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonDown));
    events.Subscribe(KeybindingEvents::ControllerButtonRepeat, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonRepeat));
}

void KeybindingSystem::KeyPressed(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    const auto& key = ev.key;

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

    auto& io = ImGui::GetIO();
    // this prevents keybindings from being processed when typing into a textbox etc.
    if (io.WantCaptureKeyboard) return;

    // process dynamic bindings
    for (auto& binding : ActiveBindings.DynamicBindings.bindings) {
        if (key.repeat && binding.ignoreRepeats) continue;
        
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
        auto handler = dynamicHandlers.find(binding.dynamicHandlerId);
        if (handler != dynamicHandlers.end()) {
            handler->second(&binding);
        }
        return;
    }

    // process bindings
    for (auto& group : ActiveBindings.groups) {
        for (auto& binding : group.bindings) {
            if (key.repeat && binding.ignoreRepeats) continue;

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
    OFS_PROFILE(__FUNCTION__);
    const auto& cbutton = ev.cbutton;
    bool navmodeActive = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad;

    // process dynamic bindings
    for (auto& binding : ActiveBindings.DynamicBindings.bindings) {
        if ((binding.ignoreRepeats && repeat) || binding.controller.button < 0) { continue; }

        if (binding.controller.button == cbutton.button) {
            if (navmodeActive) {
                // navmode bindings get processed during navmode
                // everything else doesn't get processed during navmode
                if (binding.controller.navmode) {
                    // execute binding
                    auto handler = dynamicHandlers.find(binding.dynamicHandlerId);
                    if (handler != dynamicHandlers.end()) {
                        handler->second(&binding);
                    }
                }
            }
            else {
                // execute binding
                auto handler = dynamicHandlers.find(binding.dynamicHandlerId);
                if (handler != dynamicHandlers.end()) {
                    handler->second(&binding);
                }
            }
        }
        return;
    }

    // process bindings
    for (auto& group : ActiveBindings.groups) {
        for (auto& binding : group.bindings) {
            if ((binding.ignoreRepeats && repeat) || binding.controller.button < 0) { continue; }
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
    OFS_PROFILE(__FUNCTION__);
    if (currentlyChanging != nullptr) return;
    if (ShowWindow) return;
    //auto& cbutton = ev.cbutton; // only cbutton.button is set
    ProcessControllerBindings(ev, true);
}

void KeybindingSystem::ControllerButtonDown(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (currentlyChanging != nullptr) {
        auto& cbutton = ev.cbutton;
        // check duplicate
        for (auto& group : ActiveBindings.groups) {
            for (auto& binding : group.bindings) {
                if (binding.controller.button == cbutton.button) {
                    if (binding.identifier == currentlyChanging->identifier) {
                        // the binding is being set to the key it already has which is fine
                        goto breaking_out_of_nested_loop_lol;
                    }
                    LOGF_INFO("The button is already bound for \"%s\"", TRD(binding.displayName));
                    Util::MessageBoxAlert(TR(BUTTON_ALREADY_IN_USE), FMT(TR(BUTTON_ALREADY_IN_USE_MSG), TRD(binding.displayName)));
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
    if(changeModalText.empty()) {
        changeModalText = name;
    }
    else {
        changeModalText += "+";
        changeModalText += name;
    }
}

void KeybindingSystem::addKeyString(char name) noexcept
{
    if(changeModalText.empty()) {
        changeModalText = name;
    }
    else {
        changeModalText += "+";
        changeModalText += name;
    }
}

std::string KeybindingSystem::loadKeyString(SDL_Keycode key, int mod) noexcept
{
    changeModalText = std::string();
    if (mod & KMOD_CTRL) {
        addKeyString(TR(KEY_MOD_CTRL));
    }

    if (mod & KMOD_ALT) {
        addKeyString(TR(KEY_MOD_ALT));
    }

    if (mod & KMOD_SHIFT) {
        addKeyString(TR(KEY_MOD_SHIFT));
    }
    if (key > 0) { addKeyString(SDL_GetKeyName(key)); }

    return changeModalText;
}

const char* KeybindingSystem::getBindingString(const char* bindingId) noexcept
{
    auto it = bindingStrings.find(bindingId);
    if (it != bindingStrings.end())
        return it->second.c_str();
    return "";
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
                    it->ignoreRepeats = keybind.ignoreRepeats;
                    it->key.key = keybind.key.key;
                    it->key.modifiers = keybind.key.modifiers;
                    it->key.keyStr = loadKeyString(keybind.key.key, keybind.key.modifiers);
                    bindingStrings[it->identifier] = it->key.keyStr;

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
                    it->key.keyStr = loadKeyString(keybind.key.key, keybind.key.modifiers);
                    it->active = keybind.active;

                    passiveBindings[it->identifier] = *it;
                    bindingStrings[it->identifier] = it->key.keyStr;
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
        binding.key.keyStr = loadKeyString(binding.key.key, binding.key.modifiers);
        bindingStrings[binding.identifier] = binding.key.keyStr;
    }
}

void KeybindingSystem::registerPassiveBindingGroup(PassiveBindingGroup&& pgroup) noexcept
{
    auto& pair = ActiveBindings.passiveGroups.emplace_back(std::move(pgroup));
    for (auto& binding : pair.bindings) {
        binding.key.keyStr = loadKeyString(binding.key.key, binding.key.modifiers);
        bindingStrings[binding.identifier] = binding.key.keyStr;
        passiveBindings.insert(std::move(std::make_pair(binding.identifier, binding)));
    }
}

void KeybindingSystem::addDynamicBinding(Binding&& binding) noexcept
{
    auto it = std::find_if(ActiveBindings.DynamicBindings.bindings.begin(), ActiveBindings.DynamicBindings.bindings.end(),
        [&](auto& b) {
            return b.identifier == binding.identifier;
    });
    binding.key.keyStr = loadKeyString(binding.key.key, binding.key.modifiers);
    if (it == ActiveBindings.DynamicBindings.bindings.end()) {
        ActiveBindings.DynamicBindings.bindings.emplace_back(std::move(binding));
    }
    else {
        it->dynamicHandlerId = std::move(binding.dynamicHandlerId);
        it->identifier = std::move(binding.identifier);
        it->displayName = std::move(binding.displayName);
        it->ignoreRepeats = binding.ignoreRepeats;
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
        ImGui::Columns(3);
        ImGui::Separator();
        ImGui::Text(TR(DESCRIPTION)); ImGui::NextColumn();
        ImGui::Text(TR(KEYBOARD)); ImGui::NextColumn();
        ImGui::Text(TR(ACTIVE)); ImGui::NextColumn();
        for (auto& binding : group.bindings) {
            ImGui::PushID(binding.identifier.c_str());
            ImGui::TextUnformatted(TRD(binding.displayName)); ImGui::NextColumn();
            if (ImGui::Button(!binding.key.keyStr.empty() ? binding.key.keyStr.c_str() : TR(KEY_NOT_SET), ImVec2(-1.f, 0.f))) {
                changingController = false;
                currentlyChangingPassive = &binding;
                passiveChangingTempModifiers = binding.key.modifiers;
                passiveChangingStartTimer = SDL_GetTicks();
                changeModalText = std::string();
                ImGui::OpenPopup(TR_ID("CHANGE_KEY", Tr::CHANGE_KEY));
            }
            ImGui::NextColumn(); 
            save = ImGui::Checkbox("##passiveActive", &binding.active) || save;
            ImGui::NextColumn();
            if (ImGui::BeginPopupModal(TR_ID("CHANGE_KEY", Tr::CHANGE_KEY), 0, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (changeModalText.empty()) { 
                    ImGui::TextUnformatted(TR(CHANGE_KEY_MSG)); 
                }
                else {
                    ImGui::Text(changeModalText.c_str());
                }

                uint32_t currentTime = SDL_GetTicks() - passiveChangingStartTimer;        
                ImGui::ProgressBar((currentTime / (float)PassiveChangingTimeMs), ImVec2(150.f, 0.f), Util::Format("%d ms", PassiveChangingTimeMs - currentTime));

                if (currentTime >= PassiveChangingTimeMs) {
                    if (passiveChangingTempModifiers != currentlyChangingPassive->key.modifiers) {
                        currentlyChangingPassive->key.key = 0;
                        currentlyChangingPassive->key.keyStr = changeModalText;
                        bindingStrings[currentlyChangingPassive->identifier] = currentlyChangingPassive->key.keyStr;
                        currentlyChangingPassive->key.modifiers = GetModifierState(passiveChangingTempModifiers);
                        passiveBindings[currentlyChangingPassive->identifier] = *currentlyChangingPassive;
                    }

                    currentlyChangingPassive = nullptr;
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
    ImGui::TextUnformatted(TR(PASSIVE_BINDING_TXT1));
    ImGui::TextUnformatted(TR(PASSIVE_BINDING_TXT2));
    for(int i=0; i < ActiveBindings.passiveGroups.size(); i++) {
        auto& group = ActiveBindings.passiveGroups[i];
        ImGui::PushID(i);
        addPassiveBindingGroup(group, save);
        ImGui::PopID();
    }

    if (save) {
        // update LUT
        passiveBindings.clear();
        for (auto& group : ActiveBindings.passiveGroups) {
            for (auto& binding : group.bindings) {
                passiveBindings.emplace(binding.identifier, binding);
            }
        }
    }
}

bool KeybindingSystem::ShowBindingWindow() noexcept
{
    bool save = false;
    if (ShowWindow)
        ImGui::OpenPopup(TR_ID("KEYS", Tr::KEYS));

    if (ImGui::BeginPopupModal(TR_ID("KEYS", Tr::KEYS), &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        OFS_PROFILE(__FUNCTION__);

        if (ImGui::BeginTabBar("##KeysTabBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem(TR_ID("BINDINGS", Tr::BINDINGS)))
            {
                constexpr bool disable_indent = true;
                if constexpr (disable_indent)
                    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

                ImGui::TextUnformatted(TR(BINDING_TXT1));
                ImGui::TextUnformatted(TR(BINDING_TXT2));
                ImGui::TextUnformatted(TR(BINDING_TXT3));
                ImGui::TextUnformatted(TR(BINDING_TXT4));
        
                ImGui::Spacing();
                ImGui::InputText(TR(FILTER), &filterString); 
                ImGui::SameLine(); ImGui::Checkbox(FMT(" " ICON_GAMEPAD " %s", TR(GAMEPAD_ONLY)), &ControllerOnly);
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
            if (ImGui::BeginTabItem(TR_ID("MODIFIERS", Tr::MODIFIERS))) {
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
    auto it = ptr->passiveBindings.find(name);
    if (it != ptr->passiveBindings.end() && it->second.active) {
        uint16_t modstate = GetModifierState(SDL_GetModState());
        if (modstate == it->second.key.modifiers) {
            return true;
        }
    }
    return false;
}

void KeybindingSystem::changeModals(bool& save) noexcept
{
    if (ImGui::BeginPopupModal(TR_ID("CHANGE_KEY", Tr::CHANGE_KEY), 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (changeModalText.empty()) {
            ImGui::TextUnformatted(TR(CHANGE_KEY_MSG));
        }
        else {
            ImGui::Text(changeModalText.c_str());
        }
        if (!currentlyChanging) {
            save = true; // autosave
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(TR_ID("CHANGE_BUTTON", Tr::CHANGE_BUTTON))) {
        ImGui::TextUnformatted(TR(CHANGE_BUTTON_MSG));
        if (!currentlyChanging) {
            save = true; // autosave
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void KeybindingSystem::addBindingsGroup(KeybindingGroup& group, bool& save, bool deletable) noexcept
{
    int32_t headerFlags = ImGuiTreeNodeFlags_None;
    std::vector<Binding*> filteredBindings;
    auto& style = ImGui::GetStyle();

    for (auto&& binding : group.bindings) {
        if (ControllerOnly && binding.controller.button < 0) { continue; }
        if (!filterString.empty() && !Util::ContainsInsensitive(TRD(binding.displayName), filterString)) { continue; }
        filteredBindings.emplace_back(&binding);
    }
    if (filteredBindings.size() == 0) { return; }
    if (ControllerOnly || !filterString.empty()) {
        headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    ImGui::Columns(1);
    if (ImGui::CollapsingHeader(TRD(group.displayName), headerFlags)) {
        ImGui::PushID(group.name.c_str());

        ImGui::Columns(4, "bindings");
        ImGui::Separator();
        ImGui::TextUnformatted(TR(ACTION)); ImGui::NextColumn();
        ImGui::TextUnformatted(TR(KEYBOARD)); ImGui::NextColumn();
        ImGui::TextUnformatted(TR(CONTROLLER)); ImGui::NextColumn();
        ImGui::TextUnformatted(TR(IGNORE_REPEATS)); ImGui::NextColumn();

        Binding* deleteBinding = nullptr;
        for (auto bindingPtr : filteredBindings) {
            auto& binding = *bindingPtr;
            ImGui::PushID(binding.identifier.c_str());
            ImGui::TextUnformatted(binding.name());
            ImGui::NextColumn();
            if (ImGui::Button(!binding.key.keyStr.empty() ? binding.key.keyStr.c_str() : TR(KEY_NOT_SET), ImVec2(-1.f, 0.f))) {
                changingController = false;
                currentlyChanging = &binding;
                changeModalText = std::string();
                ImGui::OpenPopup(TR_ID("CHANGE_KEY", Tr::CHANGE_KEY));
            }
            ImGui::NextColumn();
            if (ImGui::Button(GetButtonString(binding.controller.button), ImVec2(-1.f, 0.f))) {
                changingController = true;
                currentlyChanging = &binding;
                ImGui::OpenPopup(TR_ID("CHANGE_BUTTON", Tr::CHANGE_BUTTON));
            }
            ImGui::NextColumn();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth(3) / 2.f) - (2.f * ImGui::GetFontSize()) + style.ItemSpacing.x);
            if (ImGui::Checkbox("", &binding.ignoreRepeats)) { save = true; }
            if (deletable) {
                ImGui::SameLine();
                if (ImGui::Button(ICON_TRASH)) {
                    deleteBinding = bindingPtr;
                }
            }
            ImGui::NextColumn();
            changeModals(save);
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