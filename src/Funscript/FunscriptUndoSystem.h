#pragma once

#include "UndoSystem.h"
#include "Funscript.h"

// in a previous iteration
// this looked a lot more sophisticated
// while this is not memory efficient this turns out to be incredibly robust and flexible
class ScriptState {
private:
	Funscript::FunscriptData data;
public:
	inline Funscript::FunscriptData& Data() { return data; }
	StateType type;
	const std::string& Message() const;

	ScriptState(StateType type, const Funscript::FunscriptData& data)
		: type(type), data(data) {}
};


// this is part of the funscript class
class FunscriptUndoSystem
{
	friend class UndoSystem;

	Funscript* script = nullptr;
	void SnapshotRedo(StateType type) noexcept;
	// using vector as a stack...
	// because std::stack can't be iterated
	std::vector<ScriptState> UndoStack;
	std::vector<ScriptState> RedoStack;

	void Snapshot(StateType type, bool clearRedo = true) noexcept;
	void Undo() noexcept;
	void Redo() noexcept;
	void ClearRedo() noexcept;

public:
	FunscriptUndoSystem(Funscript* script) : script(script) {
		FUN_ASSERT(script != nullptr, "no script");
	}
	static constexpr const char* UndoHistoryId = "Undo/Redo history";
	void ShowUndoRedoHistory(bool* open);

	inline bool MatchUndoTop(StateType type) const noexcept { return !UndoEmpty() && UndoStack.back().type == type; }
	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};
