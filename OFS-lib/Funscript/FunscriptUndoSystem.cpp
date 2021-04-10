#include "FunscriptUndoSystem.h"

void FunscriptUndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}

void FunscriptUndoSystem::SnapshotRedo(int32_t type) noexcept
{
	RedoStack.push_back() = std::move(ScriptState(type, script->Data()));
}

void FunscriptUndoSystem::Snapshot(int32_t type, bool clearRedo) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	UndoStack.push_back() = std::move(ScriptState(type, script->Data()));

	// redo gets cleared after every snapshot
	if (clearRedo && !RedoStack.empty())
		ClearRedo();
}

bool FunscriptUndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	SnapshotRedo(UndoStack.back().type); // copy data to redo
	script->rollback(std::move(UndoStack.back().Data())); // move data
	UndoStack.pop_back(); // pop of the stack
	return true;
}

bool FunscriptUndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	Snapshot(RedoStack.back().type, false); // copy data to undo
	script->rollback(std::move(RedoStack.back().Data())); // move data
	RedoStack.pop_back(); // pop of the stack
	return true;
}