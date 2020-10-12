#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"
#include <vector>
#include <string>
#include "OpenFunscripterUtil.h"
#include "OpenFunscripterVideoplayer.h"
#include "SDL_mutex.h"

class Funscript
{
public:
	struct FunscriptData {
		std::vector<FunscriptAction> Actions;
		std::vector<FunscriptAction> selection;
	};

	struct FunscriptRawData {
		struct Recording {
			std::vector<FunscriptAction> RawActions;
			
			template <class Archive>
			inline void reflect(Archive& ar) {
				OFS_REFLECT(RawActions, ar);
			}
		};

		int32_t RecordingIdx = 0;
		std::vector<Recording> Recordings;
		
		inline bool HasRecording() const { return Recordings.size() > 0; }
		
		inline Recording& Recording() { 
			FUN_ASSERT(HasRecording(), "no recording");
			return Recordings[RecordingIdx]; 
		}
		
		inline void NewRecording(int32_t frame_no) {
			Recordings.emplace_back();
			Recordings.back().RawActions.resize(frame_no);
			RecordingIdx = Recordings.size() - 1;
		}

		inline void RemoveActiveRecording() {
			if (!HasRecording()) return;
			Recordings.erase(Recordings.begin() + RecordingIdx);
			if (RecordingIdx > 0 && RecordingIdx >= Recordings.size() - 1) {
				RecordingIdx--;
			}
		}
	};

	struct Bookmark {
		int32_t at;
		std::string name;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(at, ar);
			OFS_REFLECT(name, ar);
		}
	};

	struct Settings {
		std::vector<Bookmark> Bookmarks;
		int32_t last_pos_ms = 0;
		VideoplayerWindow::OFS_VideoPlayerSettings* player;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(Bookmarks, ar);
			OFS_REFLECT(last_pos_ms, ar);
			OFS_REFLECT_PTR(player, ar);
		}
	} scriptSettings;

	struct Metadata {
		std::string creator;
		std::string original_name;
		std::string url;
		std::string url_video;
		std::vector<std::string> tags;
		std::vector<std::string> performers;
		std::string comment;
		std::string license;
		int64_t original_total_duration_s = 0;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(creator, ar);
			OFS_REFLECT(original_name, ar);
			OFS_REFLECT(url, ar);
			OFS_REFLECT(url_video, ar);
			OFS_REFLECT(tags, ar);
			OFS_REFLECT(performers, ar);
			OFS_REFLECT(license, ar);
			OFS_REFLECT(original_total_duration_s, ar);
		}

	} metadata;

private:
	nlohmann::json Json;
	nlohmann::json BaseLoaded;
	bool scriptOpened = false;
	bool funscript_changed = false; // used to fire only one event every frame a change occurs
	SDL_mutex* saveMutex = nullptr;

	void setBaseScript(nlohmann::json& base);
	void setScriptTemplate() noexcept;
	void checkForInvalidatedActions() noexcept;
	
	FunscriptData data;
	FunscriptRawData rawData;
	
	FunscriptAction* getAction(FunscriptAction action) noexcept;
	FunscriptAction* getActionAtTime(std::vector<FunscriptAction>& actions, int32_t time_ms, uint32_t error_ms) noexcept;
	FunscriptAction* getNextActionAhead(int32_t time_ms) noexcept;
	FunscriptAction* getPreviousActionBehind(int32_t time_ms) noexcept;

	void moveActionsTime(std::vector<FunscriptAction*> moving, int32_t time_offset);
	void moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t pos_offset);
	inline void sortSelection() noexcept { sortActions(data.selection); }
	inline void sortActions(std::vector<FunscriptAction>& actions) noexcept {
		std::sort(actions.begin(), actions.end(),
			[](auto& a, auto& b) { return a.at < b.at; }
		);
	}
	inline void addAction(std::vector<FunscriptAction>& actions, FunscriptAction newAction) noexcept {
		auto it = std::find_if(actions.begin(), actions.end(), [&](auto& action) {
			return newAction.at < action.at;
			});
		actions.insert(it, newAction);
		NotifyActionsChanged();
	}

	void NotifyActionsChanged() noexcept;

	void loadMetadata() noexcept;
	void saveMetadata() noexcept;
	void loadSettings() noexcept;
	void saveSettings() noexcept;
public:
	Funscript();
	~Funscript();

	std::string current_path;

	inline void rollback(const FunscriptData& data) noexcept { this->data = data; NotifyActionsChanged(); }

	void update() noexcept;

	bool open(const std::string& file);
	void save() { save(current_path, false); }
	void save(const std::string& path, bool override_location = true);
	
	inline void reserveActionMemory(int32_t frameCount) { 
		data.Actions.reserve(frameCount);
	}

	const FunscriptData& Data() const noexcept { return data; }
	const std::vector<FunscriptAction>& Selection() const noexcept { return data.selection; }
	const std::vector<FunscriptAction>& Actions() const noexcept { return data.Actions; }
	const std::vector<FunscriptAction>& Recording() const noexcept { 
		FUN_ASSERT(rawData.HasRecording(), "no recording");
		return rawData.Recordings[rawData.RecordingIdx].RawActions; 
	}

	inline const FunscriptAction* GetAction(FunscriptAction action) noexcept { return getAction(action); }
	inline const FunscriptAction* GetActionAtTime(int32_t time_ms, uint32_t error_ms) noexcept { return getActionAtTime(data.Actions, time_ms, error_ms); }
	inline const FunscriptAction* GetNextActionAhead(int32_t time_ms) noexcept { return getNextActionAhead(time_ms); }
	inline const FunscriptAction* GetPreviousActionBehind(int32_t time_ms) noexcept { return getPreviousActionBehind(time_ms); }
	inline const FunscriptAction* GetClosestAction(int32_t time_ms) noexcept { return getActionAtTime(data.Actions, time_ms, std::numeric_limits<uint32_t>::max()); }

	float GetPositionAtTime(int32_t time_ms) noexcept;
	float GetRawPositionAtFrame(int32_t frame_no) noexcept;
	
	inline void AddAction(FunscriptAction newAction) noexcept { addAction(data.Actions, newAction); }
	inline void AddActionRaw(int32_t frame_no, int32_t at, int32_t pos, float frameTimeMs) noexcept {
		auto& recording = rawData.Recording();
		if (frame_no >= recording.RawActions.size()) return;
		recording.RawActions[frame_no].at = at - frameTimeMs;
		recording.RawActions[frame_no].pos = pos;
	}
	bool EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept;
	void PasteAction(FunscriptAction paste, int32_t error_ms) noexcept;
	void RemoveAction(FunscriptAction action, bool checkInvalidSelection = true) noexcept;
	void RemoveActions(const std::vector<FunscriptAction>& actions) noexcept;

	// bookmarks
	inline const std::vector<Funscript::Bookmark>& Bookmarks() const noexcept { return scriptSettings.Bookmarks; }
	inline void AddBookmark(const Funscript::Bookmark& bookmark) noexcept { 
		scriptSettings.Bookmarks.push_back(bookmark); 
		std::sort(scriptSettings.Bookmarks.begin(), scriptSettings.Bookmarks.end(),
			[](auto& a, auto& b) { return a.at < b.at; }
		);
	}

	// recording stuff
	inline FunscriptRawData& Raw() { return rawData; }

	// selection api
	bool ToggleSelection(FunscriptAction action) noexcept;
	void SetSelection(FunscriptAction action, bool selected) noexcept;
	void SelectTopActions();
	void SelectBottomActions();
	void SelectMidActions();
	void SelectTime(int32_t from_ms, int32_t to_ms, bool clear=true) noexcept;
	void SelectAction(FunscriptAction select) noexcept;
	void DeselectAction(FunscriptAction deselect) noexcept;
	void SelectAll() noexcept;
	void RemoveSelectedActions() noexcept;
	void MoveSelectionTime(int32_t time_offset) noexcept;
	void MoveSelectionPosition(int32_t pos_offset) noexcept;
	inline bool HasSelection() const noexcept { return data.selection.size() > 0; }
	inline int32_t SelectionSize() const noexcept { return data.selection.size(); }
	inline void ClearSelection() noexcept { data.selection.clear(); }
	inline const FunscriptAction* GetClosestActionSelection(int32_t time_ms) noexcept { return getActionAtTime(data.selection, time_ms, std::numeric_limits<int32_t>::max()); }
	
	void EqualizeSelection() noexcept;
	void InvertSelection() noexcept;
	void AlignWithFrameTimeSelection(float frameTimeMs) noexcept;
};

