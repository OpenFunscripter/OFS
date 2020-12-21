#pragma once

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

namespace OFS {
	constexpr int32_t MaxScriptStateInMemory = 1000;
}

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

	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};
