#pragma once

#include "Funscript.h"

#include <vector>
#include <memory>
#include <string>


// in a previous iteration
// this looked a lot more sophisticated
// while this is not memory efficient this turns out to be incredibly robust and flexible
struct ScriptState {
	std::string Message;
	FunscriptData Data;

	ScriptState(const std::string& msg, const FunscriptData& data)
		: Message(msg), Data(data) {}
};

class UndoSystem
{
	void SnapshotRedo(const std::string& msg) noexcept;
public:
	std::shared_ptr<Funscript> LoadedFunscript;
	// using vector as a stack...
	// because std::stack can't be iterated
	std::vector<ScriptState> UndoStack;
	std::vector<ScriptState> RedoStack;

	void Snapshot(const std::string& msg, bool clearRedo = true) noexcept;
	void Undo() noexcept;
	void Redo() noexcept;
	void ClearHistory() noexcept;
	void ClearRedo() noexcept;
};

