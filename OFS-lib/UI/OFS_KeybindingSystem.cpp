#include "OFS_KeybindingSystem.h"
#include "OFS_Util.h"
#include "OFS_Localization.h"
#include "imgui_stdlib.h"

#include <array>

static constexpr std::array<ImGuiKey, 12> ModifierKeys
{
    ImGuiKey_LeftCtrl, ImGuiKey_LeftShift, ImGuiKey_LeftAlt, ImGuiKey_LeftSuper,
    ImGuiKey_RightCtrl, ImGuiKey_RightShift, ImGuiKey_RightAlt, ImGuiKey_RightSuper,
    ImGuiKey_ReservedForModCtrl, ImGuiKey_ReservedForModShift, ImGuiKey_ReservedForModAlt, ImGuiKey_ReservedForModSuper
};

static std::array<uint64_t, 4> DissallowedTriggers = 
{
    OFS_ActionTrigger{ ImGuiKey_None, ImGuiKey_MouseLeft }.Hash(),
    OFS_ActionTrigger{ ImGuiKey_None, ImGuiKey_MouseRight }.Hash(),
    OFS_ActionTrigger{ ImGuiKey_None, ImGuiKey_MouseMiddle }.Hash(),
    OFS_ActionTrigger{ ImGuiKey_None, ImGuiKey_Escape }.Hash(),
};

inline static bool isAllowedTrigger(const OFS_ActionTrigger& trigger) noexcept
{
    return std::find(DissallowedTriggers.begin(), DissallowedTriggers.end(), trigger.Hash()) == DissallowedTriggers.end();
}

inline static bool isModifierKey(ImGuiKey key) noexcept
{
    return std::find(ModifierKeys.begin(), ModifierKeys.end(), key) != ModifierKeys.end();
}

inline static const char* getTriggerText(const OFS_ActionTrigger& trigger) noexcept
{
    const char* key = nullptr;
    std::string mods;

    if(trigger.Mod != ImGuiKey_None)
    {
        auto addMod = [](std::string& str, const char* mod) noexcept { 
            if(!str.empty()) str += '+';
            str += mod;
        };
        if(trigger.Mod & ImGuiMod_Ctrl)
        {
            addMod(mods, TR(KEY_MOD_CTRL));
        }
        if(trigger.Mod & ImGuiMod_Alt)
        {
            addMod(mods, TR(KEY_MOD_ALT));
        }
        if(trigger.Mod & ImGuiMod_Shift)
        {
            addMod(mods, TR(KEY_MOD_SHIFT));
        }
    }

    if(trigger.Key != ImGuiKey_None)
    {
        key = ImGui::GetKeyName(trigger.ImKey());
    }

    if(!mods.empty() && key != nullptr)
    {
        FMT("%s+%s", mods.c_str(), key);
    }
    else if(!mods.empty() && key == nullptr) 
    {
        FMT("%s", mods.c_str());
    }
    else 
    {
        FMT("%s", key);
    }

    return Util::FormatBuffer;
}

OFS_KeybindingSystem::OFS_KeybindingSystem() noexcept
{
    stateHandle = OFS_AppState<OFS_KeybindingState>::Register(OFS_KeybindingState::StateName);
    auto& state = OFS_KeybindingState::State(stateHandle);
    if(!state.convertedToImGui)
    {
        state.ConvertToImGui();
    }
}

OFS_KeybindingSystem::~OFS_KeybindingSystem() noexcept
{
    auto& state = OFS_KeybindingState::State(stateHandle);
    if(state.convertedToImGui)
    {
        state.ConvertToOFS();
    }
}

void OFS_KeybindingSystem::ProcessKeybindings() noexcept
{
    auto& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    auto& state = OFS_KeybindingState::State(stateHandle);

    for(int i=0, size = state.Triggers.size(); i < size; i += 1)
    {
        const auto& trigger = state.Triggers[i];
        if((trigger.Mod & ImGuiMod_Mask_) != io.KeyMods)
            continue;

        if(trigger.Key != ImGuiKey_None)
        {
            if(!ImGui::IsKeyPressed(trigger.ImKey(), trigger.ShouldRepeat))
                continue;
        }

        // Only trigger actions when NavMode is disabled
        if(ImGui::IsGamepadKey(trigger.ImKey()) && (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad))
        {
            if(trigger.MappedActionId != "toggle_controller_navmode")
                continue;
        }
        
        // Handle mouse scroll direction
        if(trigger.Key == ImGuiKey_MouseWheelY || trigger.Key == ImGuiKey_MouseWheelX)
        {
            bool direction = trigger.Mod & OFS_ActionTriggerFlags::MouseWheelDirection;
            bool directionMatch = trigger.Key == ImGuiKey_MouseWheelY 
                ? io.MouseWheel > 0.f == direction
                : io.MouseWheelH > 0.f == direction;
            if(!directionMatch) continue;
        }

        // Find action
        auto actionIt = actions.find(trigger.MappedActionId);
        if(actionIt != actions.end())
        {
            // Fire action
            actionIt->second.Action();
        }
        else 
        {
            LOGF_ERROR("Couldn't find action: \"%s\"", trigger.MappedActionId.c_str());
        }
    }
}


inline static OFS_ActionGroup* getGroupById(const char* groupId, std::vector<OFS_ActionGroup>& groups) noexcept
{
    auto it = std::find_if(groups.begin(), groups.end(), 
        [groupId](auto& group) 
        {
            return group.Id == groupId;
        });
    if(it != groups.end()) return &(*it);
    return nullptr;
}

void OFS_KeybindingSystem::RegisterGroup(const char* groupId, TrString groupName) noexcept
{
    FUN_ASSERT(getGroupById(groupId, actionGroups) == nullptr, "Group already registered");
    actionGroups.emplace_back(OFS_ActionGroup{groupId, groupName});
}

void OFS_KeybindingSystem::RegisterAction(OFS_Action&& action, TrString name, const char* groupId, const std::vector<OFS_ActionTrigger>& defaultTriggers) noexcept
{
    auto& state = OFS_KeybindingState::State(stateHandle);
    auto group = getGroupById(groupId, actionGroups);
    FUN_ASSERT(group, "Couldn't find group.");

    auto it = actions.emplace(std::move(std::make_pair(action.Id, std::move(action))));

    if(it.second)
    {
        uint32_t uiIdx = actionUI.size();
        auto& ui = actionUI.emplace_back();
        ui.ActionId = it.first->second.Id;
        ui.Name = std::move(name);
        group->actionUiIndices.emplace_back(uiIdx);

        for(auto& trigger : defaultTriggers)
        {
            auto it = state.Triggers.find(trigger);
            if(it == state.Triggers.end())
            {
                auto newTrigger = trigger;
                newTrigger.MappedActionId = ui.ActionId;
                state.Triggers.emplace(newTrigger);
            }
        }
    }
    else 
    {
        FUN_ASSERT(it.first->second.Dynamic, "This shouldn't happen for static actions.");
        LOGF_DEBUG("Action \"%s\" already exists", it.first->second.Id.c_str());
    }
}

void OFS_KeybindingSystem::addTrigger(const OFS_ActionTrigger& newTrigger) noexcept
{
    auto& state = OFS_KeybindingState::State(stateHandle);
    auto it = state.Triggers.find(newTrigger);    
    if(it == state.Triggers.end())
    {
        state.Triggers.emplace(newTrigger);
    }
    else 
    {
        // if they are the same do nothing
        if(it->Hash() == newTrigger.Hash() && it->MappedActionId == newTrigger.MappedActionId)
            return;

        std::stringstream ss;
        ss << '[' << getTriggerText(newTrigger) << ']';
        ss << '\n' << "Is already in use for {" << it->MappedActionId << '}';
        ss << '\n' << "Do you want to remove the existing trigger?";
        

        Util::YesNoCancelDialog("Trigger is already in use",
            ss.str(),
            [stateHandle = stateHandle, newTrigger](auto result)
            {
                if(result == Util::YesNoCancel::Yes)
                {
                    auto& state = OFS_KeybindingState::State(stateHandle);
                    auto it = state.Triggers.find(newTrigger);
                    if(it != state.Triggers.end())
                    {
                        it->MappedActionId = newTrigger.MappedActionId;
                    }
                }
            });
    }
}

void OFS_KeybindingSystem::editTrigger(const OFS_ActionTrigger& oldTrigger, const OFS_ActionTrigger& editTrigger) noexcept
{
    auto& state = OFS_KeybindingState::State(stateHandle);
    auto it = state.Triggers.find(oldTrigger);
    FUN_ASSERT(it != state.Triggers.end(), "Editing trigger not found.");

    if(it != state.Triggers.end())
    {
        it->Key = editTrigger.Key;
        it->Mod = editTrigger.Mod;
    }
}

void OFS_KeybindingSystem::renderNewTriggerModal() noexcept
{
    if(ImGui::BeginPopupModal(TR_ID("ADD_EDIT_TRIGGER", Tr::ADD_EDIT_TRIGGER), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextDisabled("[%s]", editingActionId.c_str());
        ImGui::TextUnformatted(TR(CHANGE_KEY_MSG));

        ImGui::TextUnformatted(getTriggerText(tmpTrigger));

        const auto oldMod = tmpTrigger.Mod;
        const auto oldKey = tmpTrigger.Key;
        tmpTrigger.Mod = ImGuiKey_None;
        tmpTrigger.Key = ImGuiKey_None;

        auto& io = ImGui::GetIO();
        bool timerShouldReset = true;
        for(int idx = 0; idx < ImGuiKey_KeysData_SIZE; idx += 1)
        {
            auto key = (ImGuiKey)(ImGuiKey_NamedKey_BEGIN + idx);
            if(ImGui::IsKeyDown(key))
            {
                if(key == ImGuiKey_Escape) 
                {
                    ImGui::CloseCurrentPopup();
                    break;
                }

                if(!isModifierKey(key))
                {
                    bool unchanged = oldKey == key && oldMod == (ImGuiKey)io.KeyMods;
                    tmpTrigger.Key = key;
                    tmpTrigger.Mod = (ImGuiKey)io.KeyMods;
                    
                    if(key == ImGuiKey_MouseWheelY || key == ImGuiKey_MouseWheelX)
                    {
                        bool direction = key == ImGuiKey_MouseWheelY
                            ? io.MouseWheel > 0.f
                            : io.MouseWheelH > 0.f;
                        tmpTrigger.SetFlag(OFS_ActionTriggerFlags::MouseWheelDirection, direction);
                    }

                    if(isAllowedTrigger(tmpTrigger)) 
                    {
                        tmpTrigger.MappedActionId = editingActionId;
                        switch (currentModal)
                        {
                            case KeyModalType::New:
                                addTrigger(tmpTrigger);
                                break;
                            case KeyModalType::Edit:
                                editTrigger(editingTrigger, tmpTrigger);
                                break;
                            default: 
                                FUN_ASSERT(false, "unreachable");
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    break;
                }
                else 
                {
                    tmpTrigger.Mod = (ImGuiKey)io.KeyMods;
                    timerShouldReset = true;
                    // No break here
                }
            }
        }
        
        ImGui::EndPopup();
    }
}

KeyModalType OFS_KeybindingSystem::renderActionRow(OFS_ActionUI& ui) noexcept
{
    auto& state = OFS_KeybindingState::State(stateHandle);

    ImGui::Indent(ImGui::GetFontSize());
    auto keyModal  = KeyModalType::None;
    bool nodeOpen = ImGui::TreeNodeEx(ui.Name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
    
    if(nodeOpen)
    {
        int32_t deleteIdx = -1;
        for(int32_t i=0; i < state.Triggers.size(); i += 1)
        {
            auto& trigger = state.Triggers[i];
            // FIXME: this is obviously bad for perf
            if(trigger.MappedActionId != ui.ActionId) continue;

            ImGui::Columns(3);
            ImGui::PushID(i);
            ImGui::Bullet();
            ImGui::SameLine();
            if(ImGui::Selectable(getTriggerText(trigger), false, ImGuiSelectableFlags_DontClosePopups))
            {
                editingTrigger = trigger;
                keyModal = KeyModalType::Edit;
            }
            ImGui::NextColumn();
            ImGui::Checkbox(TR(REPEAT), &trigger.ShouldRepeat);
            ImGui::NextColumn();
            if(ImGui::Button(FMT("%s " ICON_TRASH, TR(DELETE)), ImVec2(-1.f, 0.f)))
            { 
                deleteIdx = i; 
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }
        if(deleteIdx >= 0) 
        {
            state.Triggers.erase(state.Triggers.begin() + deleteIdx);
        }

        if(ImGui::Button("Add +", ImVec2(-1.f, 0.f))) 
        {
            keyModal = KeyModalType::New;
        }
        ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::Columns(1);

        ImGui::TreePop();
    }
    ImGui::Indent(-ImGui::GetFontSize());
    return keyModal;
}

void OFS_KeybindingSystem::renderGroup(OFS_KeybindingState& state, OFS_ActionGroup& group) noexcept
{
    if(!actionFilter.empty()) 
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        
    if(ImGui::CollapsingHeader(group.GroupName.c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for(uint32_t idx : group.actionUiIndices)
        {
            auto& ui = actionUI[idx];
            if(!actionFilter.empty() && !Util::ContainsInsensitive(ui.Name.c_str(), actionFilter.c_str()))
                continue;
            auto modal = renderActionRow(ui);
            if(modal == KeyModalType::New)
            {
                ImGui::OpenPopup(TR_ID("ADD_EDIT_TRIGGER", Tr::ADD_EDIT_TRIGGER));
                editingActionId = ui.ActionId;
                tmpTrigger = OFS_ActionTrigger();
                currentModal = modal;
            }
            else if(modal == KeyModalType::Edit)
            {
                ImGui::OpenPopup(TR_ID("ADD_EDIT_TRIGGER", Tr::ADD_EDIT_TRIGGER));
                editingActionId = ui.ActionId;
                tmpTrigger = OFS_ActionTrigger();
                currentModal = modal;
            }
        }
    }
}

void OFS_KeybindingSystem::ShowModal() noexcept
{
    showMainModal = true;
}

static void findOrphanTriggers(const OFS_KeybindingState& state, const std::unordered_map<std::string, OFS_Action>& actions, vector_set<OFS_ActionTrigger>& out) noexcept
{
    out.clear();
    for(auto& trigger : state.Triggers)
    {
        auto it = actions.find(trigger.MappedActionId);
        if(it == actions.end())
        {
            out.emplace(trigger);
        }
    }
}

void OFS_KeybindingSystem::RenderKeybindingWindow() noexcept
{
    if(showMainModal)
    {
        ImGui::OpenPopup(TR_ID("KEYS", Tr::KEYS));
        showMainModal = false;
    }

    bool showWindow = true;
    if(ImGui::BeginPopupModal(TR_ID("KEYS", Tr::KEYS), &showWindow, ImGuiWindowFlags_None))
    {
        auto& state = OFS_KeybindingState::State(stateHandle);
        
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint(TR(FILTER), TR(FILTER), &actionFilter);
        ImGui::Spacing();

        for(auto& group : actionGroups)
        {
            renderGroup(state, group);
        }

        ImGui::Spacing();
        if(ImGui::Button(TR(VALIDATE)))
        {
            findOrphanTriggers(state, actions, orphanTriggers);
            if(orphanTriggers.empty())
            {
                Util::MessageBoxAlert(TR(OK_RESULT), TR(OK_RESULT));
            }
        }

        if(!orphanTriggers.empty())
        {
            ImGui::TextUnformatted(TR(ORPHAN_TRIGGER_MESSAGE));
            int deleteIdx = -1;
            for(int i=0, size=orphanTriggers.size(); i < size; i += 1)
            {
                auto& orphanTrigger = orphanTriggers[i];
                ImGui::Text("%s [%s]", orphanTrigger.MappedActionId.c_str(), getTriggerText(orphanTrigger));
                ImGui::SameLine();
                if(ImGui::Button(FMT("%s " ICON_TRASH, TR(DELETE))))
                {
                    deleteIdx = i;
                }
            }
            if(deleteIdx >= 0) 
            {
                auto it = state.Triggers.find(orphanTriggers[deleteIdx]);
                if(it != state.Triggers.end())
                {
                    state.Triggers.erase(it);
                    findOrphanTriggers(state, actions, orphanTriggers);
                }
            }
        }

        renderNewTriggerModal();
        if(!showWindow) 
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}