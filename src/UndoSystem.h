#pragma once

#include "Funscript.h"

#include <vector>
#include <memory>
#include <string>


enum class StateType : int32_t {
	ADD_EDIT_ACTIONS = 0,
	ADD_EDIT_ACTION = 1,
	ADD_ACTION = 2,

	REMOVE_ACTIONS = 3,
	REMOVE_ACTION = 4,

	MOUSE_MOVE_ACTION = 5,
	ACTIONS_MOVED = 6,

	CUT_SELECTION = 7,
	REMOVE_SELECTION = 8,
	PASTE_COPIED_ACTIONS = 9,

	EQUALIZE_ACTIONS = 10,
	INVERT_ACTIONS = 11,
	ISOLATE_ACTION = 12,

	TOP_POINTS_ONLY = 13,
	MID_POINTS_ONLY = 14,
	BOTTOM_POINTS_ONLY = 15,

	GENERATE_ACTIONS = 16,
	FRAME_ALIGN = 17, // unused
	RANGE_EXTEND = 18,

	REPEAT_STROKE = 19,

	MOVE_ACTION_TO_CURRENT_POS = 20,
	
	SIMPLIFY = 21,
	CUSTOM_LUA = 22,
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

namespace OFS {
	constexpr int32_t MaxScriptStateInMemory = 1000;
}

// this is part of the funscript class
class FunscriptUndoSystem
{
	friend class UndoSystem;

	Funscript* Script = nullptr;
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
	FunscriptUndoSystem(Funscript* script) : Script(script) {
		FUN_ASSERT(script != nullptr, "no script");
	}
	static constexpr const char* UndoHistoryId = "Undo/Redo history";
	void ShowUndoRedoHistory(bool* open);

	inline bool MatchUndoTop(StateType type) const noexcept { return !UndoEmpty() && UndoStack.back().type == type; }
	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};


// this manages undo/redo accross the whole app
class UndoSystem
{
private:
	struct UndoContext {
		bool IsMultiscriptModification = false;

		UndoContext() {}

		UndoContext(bool multi)
			: IsMultiscriptModification(multi)
		{

		}
	};
	std::vector<UndoContext> UndoStack;
	std::vector<UndoContext> RedoStack;
	void ClearRedo() noexcept;
public:
	void Snapshot(StateType type, bool multi_script, bool clearRedo = true) noexcept;
	void Undo() noexcept;
	void Redo() noexcept;
};
