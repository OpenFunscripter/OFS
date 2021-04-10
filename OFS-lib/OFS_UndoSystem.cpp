#include "OFS_Profiling.h"
#include "OFS_UndoSystem.h"
#include "FunscriptUndoSystem.h"

#include <array>

// this array provides strings for the StateType enum
// for this to work the order needs to be maintained
static std::array<const char*, (int32_t)StateType::TOTAL_UNDOSTATE_TYPES> stateStrings = {
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

const char* ScriptState::Message() const noexcept
{
	uint32_t typeIdx = (uint32_t)type;
	FUN_ASSERT(typeIdx < stateStrings.size(), "out of bounds");
	return stateStrings[typeIdx];
}

void UndoSystem::Snapshot(StateType type, const std::weak_ptr<Funscript> active, bool clearRedo) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	UndoStack.push_back() = UndoContext{ active };
	if (clearRedo && !RedoStack.empty())
		ClearRedo();

	if (active.expired()) {
		LOG_DEBUG("multi snapshot");
		for (auto&& script : *LoadedScripts) {
			script->undoSystem->Snapshot(type, clearRedo);
		}
	}
	else {
		LOG_DEBUG("single snapshot");
		auto script = active.lock();
		script->undoSystem->Snapshot(type, clearRedo);
	}
}

bool UndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	bool undidSomething = false;
	if (UndoStack.back().Changed.expired()) {
		LOG_DEBUG("multi undo");
		for (auto&& script : *LoadedScripts) {
			undidSomething = script->undoSystem->Undo() || undidSomething;
		}
	}
	else {
		LOG_DEBUG("single undo");
		auto script = UndoStack.back().Changed.lock();
		undidSomething = script->undoSystem->Undo();
	}
	RedoStack.push_back(std::move(UndoStack.back()));
	UndoStack.pop_back();

	return undidSomething;
}

bool UndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	bool redidSomething = false;
	if (RedoStack.back().Changed.expired()) {
		LOG_DEBUG("multi undo");
		for (auto&& script : *LoadedScripts) {
			redidSomething = script->undoSystem->Redo() || redidSomething;
		}
	}
	else {
		LOG_DEBUG("single undo");
		auto script = RedoStack.back().Changed.lock();
		redidSomething = script->undoSystem->Redo();
	}
	UndoStack.push_back(std::move(RedoStack.back()));
	RedoStack.pop_back();

	return redidSomething;
}

void UndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}
