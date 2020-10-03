#include "UndoSystem.h"

#include "OpenFunscripter.h"

void UndoSystem::SnapshotRedo(const std::string& msg) noexcept
{
	RedoStack.emplace_back(msg, OpenFunscripter::script().Data());
}

void UndoSystem::Snapshot(const std::string& msg, bool clearRedo) noexcept
{
	UndoStack.emplace_back(msg, OpenFunscripter::script().Data());

	if (UndoStack.size() > MaxScriptStateInMemory) {
		auto& state = *(UndoStack.begin() + SystemDiskPointer);
		state.WriteToDisk(SystemDiskPointer);
		SystemDiskPointer++;
	}
	
	// redo gets cleared after every snapshot
	if (clearRedo && !RedoStack.empty())
		ClearRedo();
}

void UndoSystem::Undo() noexcept
{
	if (UndoStack.empty()) return;
	SnapshotRedo(UndoStack.back().Message);
	if (UndoStack.back().IsOnDisk()) {
		SystemDiskPointer = UndoStack.data()->DiskPointer;
	}
	OpenFunscripter::script().rollback(UndoStack.back().Data()); // copy data
	UndoStack.pop_back(); // pop of the stack
}

void UndoSystem::Redo() noexcept
{
	if (RedoStack.empty()) return;
	Snapshot(RedoStack.back().Message, false);
	//if (RedoStack.back().IsOnDisk()) { } // redos are always in memory
	OpenFunscripter::script().rollback(RedoStack.back().Data()); // copy data
	RedoStack.pop_back(); // pop of the stack
}

void UndoSystem::ClearHistory() noexcept
{
	UndoStack.clear();
	RedoStack.clear();
	SystemDiskPointer = 0;
}

void UndoSystem::ClearRedo() noexcept
{
	RedoStack.clear();
}

void ScriptState::WriteToDisk(int32_t diskPointer)
{
	char tmp[512];
	stbsp_snprintf(tmp, sizeof(tmp), "tmp/%d", diskPointer);
	std::filesystem::create_directory("tmp");
	auto handle = SDL_RWFromFile(tmp, "wb");
	if (handle != nullptr) {
		DiskPointer = diskPointer;
		auto header = Header();
		// write header
		SDL_RWwrite(handle, &header, sizeof(header), 1);

		// write data in this layout
		//const char* message;
		//FunscriptAction* actions;
		//FunscriptAction* selection;
		//FunscriptAction* rawActions;
		SDL_RWwrite(handle, Message.data(), Message.size(), 1);
		SDL_RWwrite(handle, data.Actions.data(), sizeof(FunscriptAction), data.Actions.size());
		SDL_RWwrite(handle, data.selection.data(), sizeof(FunscriptAction), data.selection.size());
		SDL_RWwrite(handle, data.RawActions.data(), sizeof(FunscriptAction), data.RawActions.size());
		SDL_RWclose(handle);

		data.Actions.clear();
		data.selection.clear();
		data.RawActions.clear();
		LOGF_INFO("Written undo state \"%s\" to disk.", tmp);
	}
	else {
		LOGF_ERROR("Failed to write undo state to disk.");
	}
}

Funscript::FunscriptData& ScriptState::Data()
{
	if (IsOnDisk()) {
		// load back from disk
		char tmp[512];
		stbsp_snprintf(tmp, sizeof(tmp), "tmp/%d", DiskPointer);
		auto handle = SDL_RWFromFile(tmp, "rb");
		if (handle != nullptr) {
			ScriptStateHeader header;
			SDL_RWread(handle, &header, sizeof(ScriptStateHeader), 1);
			
			// seek over message
			SDL_RWseek(handle, header.message_len, RW_SEEK_CUR);

			data.Actions.resize(header.actions_len);
			SDL_RWread(handle, data.Actions.data(), sizeof(FunscriptAction), header.actions_len) * sizeof(FunscriptAction);

			data.selection.resize(header.selection_len);
			SDL_RWread(handle, data.selection.data(), sizeof(FunscriptAction), header.selection_len) * sizeof(FunscriptAction);

			data.RawActions.resize(header.raw_len);
			SDL_RWread(handle, data.RawActions.data(), sizeof(FunscriptAction), header.raw_len) * sizeof(FunscriptAction);

			SDL_RWclose(handle);
			DiskPointer = -1; // not on disk anymore
			LOGF_INFO("Loaded undo state \"%s\" from disk.", tmp);
		}
		else {
			LOGF_ERROR("Failed to load undo state from disk");
		}

	}
	return data;
}
