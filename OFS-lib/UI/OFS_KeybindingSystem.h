#pragma once
#include <cstdint>
#include <unordered_map>
#include "state/states/KeybindingState.h"

using ActionFireFn = std::function<void()>;
struct OFS_Action
{
    std::string Id;
    ActionFireFn Action = []() { FUN_ASSERT(false, "Action not set.") };
    bool Dynamic = false;

    OFS_Action(const char* strId, ActionFireFn&& actionFn, bool isDynamic = false) noexcept
        : Id(strId), Action(std::move(actionFn)), Dynamic(isDynamic)
    {
    }

    inline bool operator==(const OFS_Action& b) const noexcept
    {
        return Id == b.Id;
    }
    inline bool operator<(const OFS_Action& b) const noexcept
    {
        return Id < b.Id;
    }
};

struct OFS_ActionGroup
{
    std::string Id;
    TrString GroupName;
    std::vector<uint32_t> actionUiIndices; 
};

struct OFS_ActionUI
{
    std::string ActionId;
    TrString Name;
};

enum class KeyModalType : uint8_t
{
    None,
    Edit,
    New
};

class OFS_KeybindingSystem
{
    private:
    uint32_t stateHandle = 0xFFFF'FFFF;
    std::vector<OFS_ActionGroup> actionGroups;
    std::vector<OFS_ActionUI> actionUI;
    std::unordered_map<std::string, OFS_Action> actions;

    OFS_ActionTrigger tmpTrigger;
    OFS_ActionTrigger editingTrigger;
    std::string editingActionId;
    KeyModalType currentModal = KeyModalType::None;

    bool showMainModal = false;
    std::string actionFilter;

    vector_set<OFS_ActionTrigger> orphanTriggers;
   
    void addTrigger(const OFS_ActionTrigger& newTrigger) noexcept;
    void editTrigger(const OFS_ActionTrigger& oldTrigger, const OFS_ActionTrigger& editTrigger) noexcept;

    void renderGroup(OFS_KeybindingState& state, OFS_ActionGroup& group) noexcept;
    KeyModalType renderActionRow(OFS_ActionUI& ui) noexcept;
    void renderNewTriggerModal() noexcept;

    public:
    OFS_KeybindingSystem() noexcept;
    ~OFS_KeybindingSystem() noexcept;

    void ProcessKeybindings() noexcept;

    void ShowModal() noexcept;
    void RenderKeybindingWindow() noexcept;

    void RegisterGroup(const char* groupId, TrString groupName) noexcept;
    void RegisterAction(OFS_Action&& action, TrString name, const char* groupId, const std::vector<OFS_ActionTrigger>& defaultTriggers = std::vector<OFS_ActionTrigger>()) noexcept;
};