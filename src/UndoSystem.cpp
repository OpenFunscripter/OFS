#include "UndoSystem.h"

#include "OpenFunscripter.h"

void UndoSystem::SnapshotRedo(const std::string& msg) noexcept
{
	RedoStack.emplace_back(msg, OpenFunscripter::script().Data());
}

void UndoSystem::ShowUndoRedoHistory(bool* open)
{
	if (*open) {
		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(200, 200));
		ImGui::Begin(UndoHistoryId, open, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
		//for(auto it = undoRedoSystem.RedoStack.rbegin(); it != undoRedoSystem.RedoStack.rend(); it++) {
		ImGui::TextDisabled("Redo stack");
		// TODO: get rid of the string comparison but keep the counting
		for (auto it = RedoStack.begin(); it != RedoStack.end(); it++) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != RedoStack.end() && copy_it->Message == it->Message) {
				count++;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", (*it).Message.c_str(), count);
		}
		ImGui::Separator();
		ImGui::TextDisabled("Undo stack");
		for (auto it = UndoStack.rbegin(); it != UndoStack.rend(); it++) {
			int count = 1;
			auto copy_it = it;
			while (++copy_it != UndoStack.rend() && copy_it->Message == it->Message) {
				count++;
			}
			it = copy_it - 1;

			ImGui::BulletText("%s (%d)", (*it).Message.c_str(), count);
		}
		ImGui::End();
	}
}

void UndoSystem::Snapshot(const std::string& msg, bool clearRedo) noexcept
{
	UndoStack.emplace_back(msg, OpenFunscripter::script().Data());

	if (UndoStack.size() > MaxScriptStateInMemory) {
		UndoStack.erase(UndoStack.begin()); // erase first action
	}
	
	// redo gets cleared after every snapshot
	if (clearRedo && !RedoStack.empty())
		ClearRedo();
}

void UndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return;
	SnapshotRedo(UndoStack.back().Message);
	OpenFunscripter::script().rollback(UndoStack.back().Data()); // copy data
	UndoStack.pop_back(); // pop of the stack
}

void UndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return;
	Snapshot(RedoStack.back().Message, false);
	OpenFunscripter::script().rollback(RedoStack.back().Data()); // copy data
	RedoStack.pop_back(); // pop of the stack
}

void UndoSystem::ClearHistory() noexcept
{
	UndoStack.clear();
	RedoStack.clear();
}

void UndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}
