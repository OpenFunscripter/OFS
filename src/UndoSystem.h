#pragma once

#include "Funscript.h"

#include <vector>
#include <memory>
#include <string>


// in a previous iteration
// this looked a lot more sophisticated
// while this is not memory efficient this turns out to be incredibly robust and flexible
class ScriptState {
private:
	Funscript::FunscriptData data;
public:
	inline Funscript::FunscriptData& Data() { return data; }
	std::string Message;

	ScriptState(const std::string& msg, const Funscript::FunscriptData& data)
		: Message(msg), data(data) {}
};

class UndoSystem
{
	void SnapshotRedo(const std::string& msg) noexcept;
	// using vector as a stack...
	// because std::stack can't be iterated
	std::vector<ScriptState> UndoStack;
	std::vector<ScriptState> RedoStack;
public:
	static constexpr const char* UndoHistoryId = "Undo/Redo history";
	const int32_t MaxScriptStateInMemory = 1000;

	void ShowUndoRedoHistory(bool* open);

	void Snapshot(const std::string& msg, bool clearRedo = true) noexcept;
	void Undo() noexcept;
	void Redo() noexcept;
	void ClearHistory() noexcept;
	void ClearRedo() noexcept;

	inline bool MatchUndoTop(const std::string& message) const noexcept { return !UndoEmpty() && UndoStack.back().Message == message; }
	inline bool UndoEmpty() const noexcept { return UndoStack.empty(); }
	inline bool RedoEmpty() const noexcept { return RedoStack.empty(); }
};

