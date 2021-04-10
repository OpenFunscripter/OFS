#include "OFS_Profiling.h"
#include "OFS_UndoSystem.h"
#include "FunscriptUndoSystem.h"

#include <array>

// this array provides strings for the StateType enum
// for this to work the order needs to be maintained
static std::array<const char*, (int32_t)StateType::TOTAL_UNDOSTATE_TYPES> stateStrings = {
	"Add/Edit actions",
	"Add/Edit action",
	"Add action",

	"Remove actions",
	"Remove action",

	"Mouse moved actions",
	"Actions moved",

	"Cut selection",
	"Remove selection",
	"Paste selection",

	"Equalize",
	"Invert",
	"Isolate",

	"Top points",
	"Mid points",
	"Bottom points",

	"Generate actions",
	"Frame align",
	"Range extend",

	"Repeat stroke",

	"Move to current position",

	"Simplify",
	"Lua script",
};

const char* ScriptState::Description() const noexcept
{
	uint32_t typeIdx = (uint32_t)type;
	FUN_ASSERT(typeIdx < stateStrings.size(), "out of bounds");
	return stateStrings[typeIdx];
}

const char* UndoSystem::UndoContext::Description() const noexcept
{
	uint32_t typeIdx = (uint32_t)Type;
	FUN_ASSERT(typeIdx < stateStrings.size(), "out of bounds");
	return stateStrings[typeIdx];
}

void UndoSystem::ShowUndoRedoHistory(bool* open) noexcept
{
	if (*open) {
		OFS_PROFILE(__FUNCTION__);
		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(200, 200));
		ImGui::Begin(UndoHistoryId, open, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::TextDisabled("Redo stack");

		for (auto it = RedoStack.begin(), end = RedoStack.end(); it != end; ++it) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != end 
				&& copy_it->Type == it->Type 
				&& copy_it->IsMulti() == it->IsMulti()) {
				++count;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", it->Description(), count);
		}
		ImGui::Separator();
		ImGui::TextDisabled("Undo stack");
		for (auto it = UndoStack.rbegin(), end = UndoStack.rend(); it != end; ++it) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != end 
				&& copy_it->Type == it->Type 
				&& copy_it->IsMulti() == it->IsMulti()) {
				++count;
			}
			it = copy_it - 1;
			
			ImGui::BulletText("%s (%d)", it->Description(), count);
		}
		ImGui::End();
	}
}

void UndoSystem::Snapshot(StateType type, const std::weak_ptr<Funscript> active, bool clearRedo) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	UndoStack.push_back() = active.expired() ? UndoContext(type) : UndoContext(active, type);
	if (clearRedo && !RedoStack.empty())
		ClearRedo();

	if (UndoStack.back().IsMulti()) {
		LOG_DEBUG("multi snapshot");
		for (auto&& script : *LoadedScripts) {
			script->undoSystem->Snapshot(type, clearRedo);
		}
	}
	else {
		LOG_DEBUG("single snapshot");
		auto script = active.lock();
		script->undoSystem->Snapshot(type, clearRedo);
	}
}

bool UndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	bool undidSomething = false;
	if (UndoStack.back().IsMulti()) {
		LOG_DEBUG("multi undo");
		for (auto&& script : *LoadedScripts) {
			undidSomething = script->undoSystem->Undo() || undidSomething;
		}
	}
	else {
		LOG_DEBUG("single undo");
		auto script = UndoStack.back().Script.value().lock();
		if (script) {
			undidSomething = script->undoSystem->Undo();
		}
		else {
			// the script for this scriptstate doesn't exist anymore
			// it probably was removed by the user
			// so in order to undo something we call Undo again
			UndoStack.pop_back();
			return Undo();
		}
	}

	RedoStack.push_back() = std::move(UndoStack.back());
	UndoStack.pop_back();
	return undidSomething;
}

bool UndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	bool redidSomething = false;
	if (RedoStack.back().IsMulti()) {
		LOG_DEBUG("multi undo");
		for (auto&& script : *LoadedScripts) {
			redidSomething = script->undoSystem->Redo() || redidSomething;
		}
	}
	else {
		LOG_DEBUG("single redo");
		auto script = RedoStack.back().Script.value().lock();
		if (script) {
			redidSomething = script->undoSystem->Redo();
		}
		else {
			// the script for this scriptstate doesn't exist anymore
			// it probably was removed by the user
			// so in order to redo something we call Redo again
			RedoStack.pop_back();
			return Redo();
		}
	}

	UndoStack.push_back() = std::move(RedoStack.back());
	RedoStack.pop_back();
	return redidSomething;
}

void UndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}
