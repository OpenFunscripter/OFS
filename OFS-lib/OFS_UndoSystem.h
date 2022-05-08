#pragma once
#include <vector>
#include <memory>

#include "EASTL/optional.h"
#include "FunscriptUndoSystem.h"

enum StateType : int32_t {
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

// this manages undo/redo accross the whole app
class UndoSystem
{
private:
	struct UndoContext {
		int32_t Type;
		eastl::optional<std::weak_ptr<const class Funscript>> Script;
		UndoContext() noexcept : Type(-1) {}
		UndoContext(StateType type) noexcept : Type((int32_t)type) {}
		UndoContext(const std::weak_ptr<class Funscript>& ptr, StateType type) noexcept
			: Script(ptr), Type((int32_t)type)
		{ }

		const char* Description() const noexcept;
		inline bool IsMulti() const noexcept { return !Script.has_value(); }
	};
	eastl::ring_buffer<UndoContext> UndoStack;
	eastl::ring_buffer<UndoContext> RedoStack;
	void ClearRedo() noexcept;
public:
	std::vector<std::shared_ptr<class Funscript>>* LoadedScripts = nullptr;

	UndoSystem(std::vector<std::shared_ptr<class Funscript>>* scripts) noexcept {
		LoadedScripts = scripts;
		UndoStack.reserve(OFS::MaxScriptStateInMemory);
		RedoStack.reserve(OFS::MaxScriptStateInMemory);
	}

	static constexpr const char* WindowId = "UNDO_REDO_HISTORY";
	void ShowUndoRedoHistory(bool* open) noexcept;

	void Snapshot(StateType type, 
		const std::weak_ptr<class Funscript> active = std::weak_ptr<class Funscript>(),
		bool clearRedo = true) noexcept;
	bool Undo() noexcept;
	bool Redo() noexcept;

	inline bool MatchUndoTop(int32_t type) const noexcept { return !UndoEmpty() && UndoStack.back().Type == type; }
	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};
