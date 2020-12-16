#include "UndoSystem.h"

void FunscriptUndoSystem::SnapshotRedo(StateType type) noexcept
{
	RedoStack.emplace_back(type, Script->Data());
}

void FunscriptUndoSystem::ShowUndoRedoHistory(bool* open)
{
	if (*open) {
		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(200, 200));
		ImGui::Begin(UndoHistoryId, open, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::TextDisabled("Redo stack");
		for (auto it = RedoStack.begin(); it != RedoStack.end(); it++) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != RedoStack.end() && copy_it->type == it->type) {
				count++;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", (*it).Message().c_str(), count);
		}
		ImGui::Separator();
		ImGui::TextDisabled("Undo stack");
		for (auto it = UndoStack.rbegin(); it != UndoStack.rend(); it++) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != UndoStack.rend() && copy_it->type == it->type) {
				count++;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", (*it).Message().c_str(), count);
		}
		ImGui::End();
	}
}

void FunscriptUndoSystem::Snapshot(StateType type, bool clearRedo) noexcept
{
	UndoStack.emplace_back(type, Script->Data());

	if (UndoStack.size() > OFS::MaxScriptStateInMemory) {
		UndoStack.erase(UndoStack.begin()); // erase first action
	}

	// redo gets cleared after every snapshot
	if (clearRedo && !RedoStack.empty())
		ClearRedo();
}

void FunscriptUndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return;
	SnapshotRedo(UndoStack.back().type);
	Script->rollback(UndoStack.back().Data()); // copy data
	UndoStack.pop_back(); // pop of the stack
}

void FunscriptUndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return;
	Snapshot(RedoStack.back().type, false);
	Script->rollback(RedoStack.back().Data()); // copy data
	RedoStack.pop_back(); // pop of the stack
}

void FunscriptUndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}
