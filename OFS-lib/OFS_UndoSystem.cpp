#include "OFS_UndoSystem.h"
#include "FunscriptUndoSystem.h"

#include <array>

// this array provides strings for the StateType enum
// for this to work the order needs to be maintained
static std::array<const std::string, (int32_t)StateType::TOTAL_UNDOSTATE_TYPES> stateStrings = {
	"Add/Edit actions",
	"Add/Edit action",
	"Add action",

	"Remove actions",
	"Remove action",

	"Mouse moved actions",
	"Actions moved",

	"Cut selection",
	"Remove selection",
	"Paste selection",

	"Equalize",
	"Invert",
	"Isolate",

	"Top points",
	"Mid points",
	"Bottom points",

	"Generate actions",
	"Frame align",
	"Range extend",

	"Repeat stroke",

	"Move to current position",

	"Simplify",
	"Lua script",
};

const std::string& ScriptState::Message() const
{
	uint32_t typeIdx = (uint32_t)type;
	FUN_ASSERT(typeIdx < stateStrings.size(), "out of bounds");
	return stateStrings[typeIdx];
}

void UndoSystem::Snapshot(StateType type, bool multi_script, Funscript* active, bool clearRedo) noexcept
{
	UndoStack.push_back() = UndoContext{ multi_script };
	if (clearRedo && !RedoStack.empty())
		ClearRedo();

	if (multi_script) {
		for (auto&& script : *LoadedScripts) {
			script->undoSystem->Snapshot(type, clearRedo);
		}
	}
	else {
		active->undoSystem->Snapshot(type, clearRedo);
	}
}

bool UndoSystem::Undo(Funscript* active) noexcept
{
	if (UndoStack.empty()) return false;
	bool undidSomething = false;

	if (UndoStack.back().IsMultiscriptModification) {
		for (auto&& script : *LoadedScripts) {
			undidSomething = script->undoSystem->Undo() || undidSomething;
		}
	}
	else {
		undidSomething = active->undoSystem->Undo() || undidSomething;
	}
	RedoStack.push_back(std::move(UndoStack.back()));
	UndoStack.pop_back();

	return undidSomething;
}

bool UndoSystem::Redo(Funscript* active) noexcept
{
	if (RedoStack.empty()) return false;
	bool redidSomething = false;

	if (RedoStack.back().IsMultiscriptModification) {
		for (auto&& script : *LoadedScripts) {
			redidSomething = script->undoSystem->Redo() || redidSomething;
		}
	}
	else {
		redidSomething = active->undoSystem->Redo() || redidSomething;
	}
	UndoStack.push_back(std::move(RedoStack.back()));
	RedoStack.pop_back();

	return redidSomething;
}

void UndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}
