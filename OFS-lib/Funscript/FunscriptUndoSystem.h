#pragma once

#include "Funscript.h"
#include "EASTL/bonus/ring_buffer.h"

namespace OFS {
	constexpr int32_t MaxScriptStateInMemory = 1000;
}

// in a previous iteration
// this looked a lot more sophisticated
// while this is not memory efficient this turns out to be incredibly robust and flexible
class ScriptState {
private:
	Funscript::FunscriptData data;
public:
	inline Funscript::FunscriptData& Data() { return data; }
	int32_t type;
	const char* Description() const noexcept;

	ScriptState() noexcept 
		: type(-1) {}
	ScriptState(int32_t type, const Funscript::FunscriptData& data) noexcept
		: type(type), data(data) {}
};

// this is part of the funscript class
class FunscriptUndoSystem
{
	friend class UndoSystem;

	Funscript* script = nullptr;
	void SnapshotRedo(int32_t type) noexcept;
	
	eastl::ring_buffer<ScriptState> UndoStack;
	eastl::ring_buffer<ScriptState> RedoStack;

	void Snapshot(int32_t type, bool clearRedo = true) noexcept;
	bool Undo() noexcept;
	bool Redo() noexcept;
	void ClearRedo() noexcept;
public:
	FunscriptUndoSystem(Funscript* script) : script(script) {
		FUN_ASSERT(script != nullptr, "no script");
		UndoStack.reserve(OFS::MaxScriptStateInMemory);
		RedoStack.reserve(OFS::MaxScriptStateInMemory);
	}

	inline bool MatchUndoTop(int32_t type) const noexcept { return !UndoEmpty() && UndoStack.back().type == type; }
	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};
