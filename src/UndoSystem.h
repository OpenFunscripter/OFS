#pragma once

#include "Funscript.h"

#include <vector>
#include <memory>
#include <string>


enum class StateType : int32_t {
	ADD_EDIT_ACTIONS,
	ADD_EDIT_ACTION,
	ADD_ACTION,

	REMOVE_ACTIONS,
	REMOVE_ACTION,

	MOUSE_MOVE_ACTION,
	ACTIONS_MOVED,

	CUT_SELECTION,
	REMOVE_SELECTION,
	PASTE_COPIED_ACTIONS,

	EQUALIZE_ACTIONS,
	INVERT_ACTIONS,
	ISOLATE_ACTION,

	TOP_POINTS_ONLY,
	MID_POINTS_ONLY,
	BOTTOM_POINTS_ONLY,

	GENERATE_ACTIONS,
	FRAME_ALIGN,
	RANGE_EXTEND,
	// add more here & update stateStrings in UndoSystem.cpp


	TOTAL_UNDOSTATE_TYPES
};

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

class UndoSystem
{
	void SnapshotRedo(StateType type) noexcept;
	// using vector as a stack...
	// because std::stack can't be iterated
	std::vector<ScriptState> UndoStack;
	std::vector<ScriptState> RedoStack;
public:
	static constexpr const char* UndoHistoryId = "Undo/Redo history";
	const int32_t MaxScriptStateInMemory = 1000;

	void ShowUndoRedoHistory(bool* open);

	void Snapshot(StateType type, bool clearRedo = true) noexcept;
	void Undo() noexcept;
	void Redo() noexcept;
	void ClearHistory() noexcept;
	void ClearRedo() noexcept;

	//inline bool MatchUndoTop(const std::string& message) const noexcept { return !UndoEmpty() && UndoStack.back().Message == message; }
	inline bool MatchUndoTop(StateType type) const noexcept { return !UndoEmpty() && UndoStack.back().type == type; }
	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};

