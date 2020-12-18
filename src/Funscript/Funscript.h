#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include "OpenFunscripterUtil.h"
#include "OpenFunscripterVideoplayer.h"
#include "SDL_mutex.h"

class FunscriptUndoSystem;

class Funscript
{
public:
	struct FunscriptData {
		std::vector<FunscriptAction> Actions;
		std::vector<FunscriptAction> selection;
	};
	
	struct Bookmark {
		enum BookmarkType {
			REGULAR,
			START_MARKER,
			END_MARKER
		};
		int32_t at;
		std::string name;
		BookmarkType type = BookmarkType::REGULAR;

		static constexpr char startMarker[] = "_start";
		static constexpr char endMarker[] = "_end";
		Bookmark() {}

		Bookmark(const std::string& name, int32_t at)
			: name(name), at(at)
		{
			UpdateType();
		}

		inline void UpdateType() noexcept {

			Util::trim(name);

			if (Util::StringEqualsInsensitive(name, startMarker) || Util::StringEqualsInsensitive(name, endMarker)) {
				type = BookmarkType::REGULAR;
				return;
			}

			if (Util::StringEndswith(name, startMarker)) {
				type = BookmarkType::START_MARKER;
				name.erase(name.end() - sizeof(startMarker) + 1, name.end());
			}
			else if (Util::StringEndswith(name, endMarker)) {
				type = BookmarkType::END_MARKER;
				// don't remove _end because it helps distinguish the to markers
				//name.erase(name.end() - sizeof(endMarker) + 1, name.end());
			}
			else {
				type = BookmarkType::REGULAR;
			}
		}

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(at, ar);
			OFS_REFLECT(name, ar);
			OFS_REFLECT(type, ar);
			
			// HACK: convert existing bookmarks to their correct type
			if (type != BookmarkType::START_MARKER) {
				UpdateType();
			}
		}
	};

	struct Settings {
		std::string version = "OFS " FUN_LATEST_GIT_TAG "@" FUN_LATEST_GIT_HASH;
		std::vector<Bookmark> Bookmarks;
		int32_t last_pos_ms = 0;
		VideoplayerWindow::OFS_VideoPlayerSettings* player = nullptr;
		std::vector<std::string> associatedScripts;

		struct TempoModeSettings {
			int bpm = 100;
			float beat_offset_seconds = 0.f;
			int multiIDX = 0;
			
			template <class Archive>
			inline void reflect(Archive& ar) {
				OFS_REFLECT(bpm, ar);
				OFS_REFLECT(beat_offset_seconds, ar);
				OFS_REFLECT(multiIDX, ar);
			}

		} tempoSettings;


		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(version, ar);
			OFS_REFLECT(tempoSettings, ar); 
			OFS_REFLECT(associatedScripts, ar);
			OFS_REFLECT(Bookmarks, ar);
			OFS_REFLECT(last_pos_ms, ar);
			OFS_REFLECT_PTR(player, ar);
		}
	} scriptSettings;

	struct Metadata {
		std::string type = "basic";
		std::string title;
		std::string creator;
		std::string script_url;
		std::string video_url;
		std::vector<std::string> tags;
		std::vector<std::string> performers;
		std::string description;
		std::string license;
		std::string notes;
		int64_t duration = 0;

		template <class Archive>
		inline void reflect(Archive& ar) {
			OFS_REFLECT(type, ar);
			OFS_REFLECT(title, ar);
			OFS_REFLECT(creator, ar);
			OFS_REFLECT(script_url, ar);
			OFS_REFLECT(video_url, ar);
			OFS_REFLECT(tags, ar);
			OFS_REFLECT(performers, ar);
			OFS_REFLECT(license, ar);
			OFS_REFLECT(duration, ar);
			OFS_REFLECT(description, ar);
			OFS_REFLECT(notes, ar);
		}

	} metadata;

private:
	nlohmann::json Json;
	nlohmann::json BaseLoaded;
	std::chrono::system_clock::time_point editTime;
	bool scriptOpened = false;
	bool funscriptChanged = false; // used to fire only one event every frame a change occurs
	bool unsavedEdits = false; // used to track if the script has unsaved changes
	bool selectionChanged = false;
	SDL_mutex* saveMutex = nullptr;

	void setBaseScript(nlohmann::json& base);
	void setScriptTemplate() noexcept;
	void checkForInvalidatedActions() noexcept;
	
	FunscriptData data;
	
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
		NotifyActionsChanged(true);
	}

	void NotifySelectionChanged() noexcept;

	void loadMetadata() noexcept;
	void saveMetadata() noexcept;
	void loadSettings() noexcept;
	void saveSettings() noexcept;

	void startSaveThread(const std::string& path, nlohmann::json&& json) noexcept;
public:
	Funscript();
	~Funscript();

	inline void NotifyActionsChanged(bool isEdit) noexcept {
		funscriptChanged = true;
		if (isEdit && !unsavedEdits) {
			unsavedEdits = true;
			editTime = std::chrono::system_clock::now();
		}
	}

	std::unique_ptr<FunscriptUndoSystem> undoSystem;
	std::string current_path;
	bool Enabled = true;

	inline void rollback(const FunscriptData& data) noexcept { this->data = data; NotifyActionsChanged(true); }

	void update() noexcept;

	bool open(const std::string& file);
	void save() { save(current_path, true); }
	void save(const std::string& path, bool override_location = true);
	void saveMinium(const std::string& path) noexcept;

	inline void reserveActionMemory(int32_t frameCount) { 
		data.Actions.reserve(frameCount);
	}

	const FunscriptData& Data() const noexcept { return data; }
	const std::vector<FunscriptAction>& Selection() const noexcept { return data.selection; }
	const std::vector<FunscriptAction>& Actions() const noexcept { return data.Actions; }

	inline const FunscriptAction* GetAction(FunscriptAction action) noexcept { return getAction(action); }
	inline const FunscriptAction* GetActionAtTime(int32_t time_ms, uint32_t error_ms) noexcept { return getActionAtTime(data.Actions, time_ms, error_ms); }
	inline const FunscriptAction* GetNextActionAhead(int32_t time_ms) noexcept { return getNextActionAhead(time_ms); }
	inline const FunscriptAction* GetPreviousActionBehind(int32_t time_ms) noexcept { return getPreviousActionBehind(time_ms); }
	inline const FunscriptAction* GetClosestAction(int32_t time_ms) noexcept { return getActionAtTime(data.Actions, time_ms, std::numeric_limits<uint32_t>::max()); }

	float GetPositionAtTime(int32_t time_ms, bool easing) noexcept;
	
	inline void AddAction(FunscriptAction newAction) noexcept { addAction(data.Actions, newAction); }
	void AddActionSafe(FunscriptAction newAction) noexcept;

	bool EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept;
	void AddEditAction(FunscriptAction action, float frameTimeMs) noexcept;
	void PasteAction(FunscriptAction paste, int32_t error_ms) noexcept;
	void RemoveAction(FunscriptAction action, bool checkInvalidSelection = true) noexcept;
	void RemoveActions(const std::vector<FunscriptAction>& actions) noexcept;

	std::vector<FunscriptAction> GetLastStroke(int32_t time_ms) noexcept;

	void SetActions(const std::vector<FunscriptAction>& override_with) noexcept;

	// bookmarks
	inline const std::vector<Funscript::Bookmark>& Bookmarks() const noexcept { return scriptSettings.Bookmarks; }
	void AddBookmark(const Funscript::Bookmark& bookmark) noexcept;

	inline bool HasUnsavedEdits() const { return unsavedEdits; }
	inline const std::chrono::system_clock::time_point& EditTime() const { return editTime; }

	// selection api
	void RangeExtendSelection(int32_t rangeExtend) noexcept;
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
	
	void SetSelection(const std::vector<FunscriptAction>& action_to_select, bool unsafe) noexcept;
	bool IsSelected(FunscriptAction action) noexcept;

	void EqualizeSelection() noexcept;
	void InvertSelection() noexcept;
};

