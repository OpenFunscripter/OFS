#include "UndoSystem.h"

void UndoSystem::SnapshotRedo(const std::string& msg) noexcept
{
	RedoStack.emplace_back(msg, LoadedFunscript->Data());
}

void UndoSystem::Snapshot(const std::string& msg, bool clearRedo) noexcept
{
	UndoStack.emplace_back(msg, LoadedFunscript->Data());
	
	// redo gets cleared after every snapshot
	if (clearRedo && !RedoStack.empty())
		ClearRedo();
}

void UndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return;
	SnapshotRedo(UndoStack.back().Message);
	LoadedFunscript->rollback(UndoStack.back().Data); // copy data
	UndoStack.pop_back(); // pop of the stack
}

void UndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return;
	Snapshot(RedoStack.back().Message, false);
	LoadedFunscript->rollback(RedoStack.back().Data); // copy data
	RedoStack.pop_back(); // pop of the stack
}

void UndoSystem::ClearHistory() noexcept
{
	UndoStack.clear();
	RedoStack.clear();
}

void UndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}
