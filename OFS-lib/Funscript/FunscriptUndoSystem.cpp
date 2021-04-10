#include "FunscriptUndoSystem.h"

void FunscriptUndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}

void FunscriptUndoSystem::SnapshotRedo(int32_t type) noexcept
{
	RedoStack.push_back() = std::move(ScriptState(type, script->Data()));
}

void FunscriptUndoSystem::ShowUndoRedoHistory(bool* open)
{
	if (*open) {
		OFS_PROFILE(__FUNCTION__);
		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(200, 200));
		ImGui::Begin(UndoHistoryId, open, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::TextDisabled("Redo stack");
		
		for (auto it = RedoStack.begin(), end = RedoStack.end(); it != end; ++it) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != end && copy_it->type == it->type) {
				++count;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", it->Message(), count);
		}
		ImGui::Separator();
		ImGui::TextDisabled("Undo stack");
		for (auto it = UndoStack.rbegin(), end = UndoStack.rend(); it != end; ++it) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != end && copy_it->type == it->type) {
				++count;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", it->Message(), count);
		}
		ImGui::End();
	}
}

void FunscriptUndoSystem::Snapshot(int32_t type, bool clearRedo) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	UndoStack.push_back() = std::move(ScriptState(type, script->Data()));

	// redo gets cleared after every snapshot
	if (clearRedo && !RedoStack.empty())
		ClearRedo();
}

bool FunscriptUndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	SnapshotRedo(UndoStack.back().type); // copy data to redo
	script->rollback(std::move(UndoStack.back().Data())); // move data
	UndoStack.pop_back(); // pop of the stack
	return true;
}

bool FunscriptUndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return false;
	OFS_PROFILE(__FUNCTION__);
	Snapshot(RedoStack.back().type, false); // copy data to undo
	script->rollback(std::move(RedoStack.back().Data())); // move data
	RedoStack.pop_back(); // pop of the stack
	return true;
}