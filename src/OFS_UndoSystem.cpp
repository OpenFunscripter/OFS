#include "OFS_Profiling.h"
#include "OFS_UndoSystem.h"
#include "FunscriptUndoSystem.h"
#include "OFS_Localization.h"

#include <array>

// this array provides strings for the StateType enum
// for this to work the order needs to be maintained
static std::array<Tr, (int32_t)StateType::TOTAL_UNDOSTATE_TYPES> stateTranslations = {
    Tr::ADD_EDIT_ACTIONS,
    Tr::ADD_EDIT_ACTION,
    Tr::ADD_ACTION,

    Tr::REMOVE_ACTIONS,
    Tr::REMOVE_ACTION,

    Tr::MOUSE_MOVED_ACTIONS,
    Tr::ACTIONS_MOVED,

    Tr::CUT_SELECTION,
    Tr::REMOVE_SELECTION,
    Tr::PASTE_SELECTION,

    Tr::EQUALIZE,
    Tr::INVERT,
    Tr::ISOLATE,

    Tr::TOP_POINTS,
    Tr::MID_POINTS,
    Tr::BOTTOM_POINTS,

    Tr::GENERATE_ACTIONS,
    Tr::FRAME_ALIGN,
    Tr::RANGE_EXTEND,

    Tr::REPEAT_STROKE,
    Tr::MOVE_TO_CURRENT_POSITION,

    Tr::SIMPLIFY,
    Tr::LUA_SCRIPT
};

const char* ScriptState::Description() const noexcept
{
    uint32_t typeIdx = (uint32_t)type;
    FUN_ASSERT(typeIdx < stateTranslations.size(), "out of bounds");
    return TRD(stateTranslations[typeIdx]);
}

const char* UndoSystem::UndoContext::Description() const noexcept
{
    uint32_t typeIdx = (uint32_t)Type;
    FUN_ASSERT(typeIdx < stateTranslations.size(), "out of bounds");
    return TRD(stateTranslations[typeIdx]);
}

UndoSystem::UndoSystem() noexcept
{
    RedoStack.reserve(100);
    UndoStack.reserve(1000);
}

void UndoSystem::ShowUndoRedoHistory(bool* open) noexcept
{
    if (!*open) return;
    OFS_PROFILE(__FUNCTION__);
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(200, 200));
    ImGui::Begin(TR_ID(UndoSystem::WindowId, Tr::UNDO_REDO_HISTORY), open, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextDisabled(TR(REDO_STACK));

    for (auto it = RedoStack.begin(), end = RedoStack.end(); it != end; ++it) {
        int count = 1;
        auto copyIt = it;
        while (++copyIt != end
            && copyIt->Type == it->Type) {
            ++count;
        }
        it = copyIt - 1;

        ImGui::BulletText("%s (%d)", it->Description(), count);
    }
    ImGui::Separator();
    ImGui::TextDisabled(TR(UNDO_STACK));
    for (auto it = UndoStack.rbegin(), end = UndoStack.rend(); it != end; ++it) {
        int count = 1;
        auto copyIt = it;
        while (++copyIt != end
            && copyIt->Type == it->Type) {
            ++count;
        }
        it = copyIt - 1;

        ImGui::BulletText("%s (%d)", it->Description(), count);
    }
    ImGui::End();
}

void UndoSystem::Snapshot(StateType type, UndoContextScripts&& scriptsToSnapshot, bool clearRedo) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto context = UndoStack.emplace_back(std::move(scriptsToSnapshot), type);
    if (clearRedo)
        ClearRedo();

    for (auto& weak : context.Scripts) {
        if (auto script = weak.lock()) {
            script->undoSystem->Snapshot(type, clearRedo);
        }
        else {
            FUN_ASSERT(false, "Stale weak_ptr.");
        }
    }
}

bool UndoSystem::Undo() noexcept
{
    if (UndoStack.empty()) return false;
    OFS_PROFILE(__FUNCTION__);
    bool undidSomething = false;

    auto context = std::move(UndoStack.back());
    UndoStack.pop_back();
    for (auto& weak : context.Scripts) {
        if (auto script = weak.lock()) {
            undidSomething = script->undoSystem->Undo() || undidSomething;
        }
        else {
            FUN_ASSERT(false, "Stale undo.");
        }
    }

    RedoStack.emplace_back(std::move(context));
    return undidSomething;
}

bool UndoSystem::Redo() noexcept
{
    if (RedoStack.empty()) return false;
    OFS_PROFILE(__FUNCTION__);
    bool redidSomething = false;

    auto context = std::move(RedoStack.back());
    RedoStack.pop_back();
    for (auto& weak : context.Scripts) {
        if (auto script = weak.lock()) {
            redidSomething = script->undoSystem->Redo() || redidSomething;
        }
        else {
            FUN_ASSERT(false, "Stale redo.");
        }
    }


    UndoStack.emplace_back(std::move(context));
    return redidSomething;
}

void UndoSystem::ClearRedo() noexcept
{
    RedoStack.clear();
}
