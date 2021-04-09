#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"
#include "OFS_Serialization.h"
#include "OFS_BinarySerialization.h"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <set>

#include "OFS_Util.h"
#include "SDL_mutex.h"

#include "FunscriptSpline.h"
#include "OFS_Profiling.h"

class FunscriptUndoSystem;

class FunscriptEvents
{
public:
	static int32_t FunscriptActionsChangedEvent;
	static int32_t FunscriptSelectionChangedEvent;

	static void RegisterEvents() noexcept;
};

class Funscript
{
public:
	struct FunscriptData {
		std::vector<FunscriptAction> Actions;
		std::vector<FunscriptAction> selection;
	};

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

		bool loadFromFunscript(const std::string& path) noexcept;
		bool writeToFunscript(const std::string& path) noexcept;

		template<typename S>
		void serialize(S& s)
		{
			s.ext(*this, bitsery::ext::Growable{},
				[](S& s, Metadata& o) {
					s.container(o.tags, o.tags.max_size(), [](S& s, std::string& tag) {
						s.text1b(tag, tag.max_size());
						});
					s.container(o.performers, o.performers.max_size(), [](S& s, std::string& performer) {
						s.text1b(performer, performer.max_size());
						});
					s.text1b(o.type, o.type.max_size());
					s.text1b(o.title, o.title.max_size());
					s.text1b(o.creator, o.creator.max_size());
					s.text1b(o.script_url, o.script_url.max_size());
					s.text1b(o.video_url, o.video_url.max_size());
					s.text1b(o.description, o.description.max_size());
					s.text1b(o.license, o.license.max_size());
					s.text1b(o.notes, o.notes.max_size());
					s.value8b(o.duration);
				});
		}
	};
	// this is used when loading from json or serializing to json
	Funscript::Metadata LocalMetadata;

	template<typename S>
	void serialize(S& s)
	{
		s.ext(*this, bitsery::ext::Growable{},
			[](S& s, Funscript& o) {
				s.container(o.data.Actions, o.data.Actions.max_size());
				// Metadata is centralized in OFS_Project
				s.text1b(o.CurrentPath, o.CurrentPath.max_size());
				s.text1b(o.Title, o.Title.max_size());
			});
	}

private:
	nlohmann::json Json;
	nlohmann::json BaseLoaded = nlohmann::json::object();
	std::chrono::system_clock::time_point editTime;
	bool scriptOpened = false;
	bool funscriptChanged = false; // used to fire only one event every frame a change occurs
	bool unsavedEdits = false; // used to track if the script has unsaved changes
	bool selectionChanged = false;
	SDL_mutex* saveMutex = nullptr;

	void setBaseScript(nlohmann::json& base);
	void checkForInvalidatedActions() noexcept;
	
	FunscriptData data;
	bool ActionMapNeedsUpdate = true;
	std::map<int32_t, int32_t> ActionMap; // maps timeMs to Index in actions vector

	inline void updateActionMap(const std::vector<FunscriptAction>& actions) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		ActionMapNeedsUpdate = false;
		ActionMap.clear();
		for (int i = 0; i < actions.size(); i++) {
			ActionMap.emplace(actions[i].at, i);
		}
	}
	
	inline FunscriptAction* getAction(FunscriptAction action) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (!ActionMapNeedsUpdate) {
			auto indexIt = ActionMap.find(action.at);
			if (indexIt != ActionMap.end()) {
				return &data.Actions[indexIt->second];
			}
		}
		else {
			auto it = std::find(data.Actions.begin(), data.Actions.end(), action);
			if (it != data.Actions.end())
				return &(*it);
		}
		return nullptr;
	}

	inline FunscriptAction* getActionAtTime(std::vector<FunscriptAction>& actions, int32_t time_ms, uint32_t max_error_ms) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		// gets an action at a time with a margin of error
		int32_t smallestError = std::numeric_limits<int32_t>::max();
		FunscriptAction* smallestErrorAction = nullptr;

		int i = 0;
		if (!ActionMapNeedsUpdate) {
			auto indexIt = ActionMap.lower_bound(time_ms - max_error_ms);
			if (indexIt != ActionMap.end()) {
				// index is valid
				i = indexIt->second;
			}
		}

		for (; i < actions.size(); i++) {
			auto& action = actions[i];

			if (action.at > (time_ms + (max_error_ms / 2)))
				break;

			int32_t error = std::abs(time_ms - action.at);
			if (error <= max_error_ms) {
				if (error <= smallestError) {
					smallestError = error;
					smallestErrorAction = &action;
				}
				else {
					break;
				}
			}
		}
		return smallestErrorAction;
	}

	inline FunscriptAction* getNextActionAhead(int32_t time_ms) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (!ActionMapNeedsUpdate) {
			auto indexIt = ActionMap.upper_bound(time_ms);
			if (indexIt != ActionMap.end()
				&& indexIt->first > time_ms) {
				int index = indexIt->second;
				return &data.Actions[index];
			}
		}
		else {
			// fallback to linear search
			auto it = std::find_if(data.Actions.begin(), data.Actions.end(),
				[time_ms](auto action) {
					return action.at > time_ms;
				});

			if (it != data.Actions.end())
				return &(*it);
		}
		return nullptr;
	}

	inline FunscriptAction* getPreviousActionBehind(int32_t time_ms) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (!ActionMapNeedsUpdate) {
			auto indexIt = ActionMap.lower_bound(time_ms);
			if (indexIt != ActionMap.end()
				&& indexIt->second > 0) {
				// index is valid
				int index = indexIt->second - 1;
				return &data.Actions[index];
			}
		}
		else {
			// fall back to linear search
			auto it = std::find_if(data.Actions.rbegin(), data.Actions.rend(),
				[time_ms](auto action) {
					return action.at < time_ms;
				});

			if (it != data.Actions.rend())
				return &(*it);
		}
		return nullptr;
	}

	void moveActionsTime(std::vector<FunscriptAction*> moving, int32_t time_offset);
	void moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t pos_offset);
	inline void sortSelection() noexcept { sortActions(data.selection); }
	inline void sortActions(std::vector<FunscriptAction>& actions) noexcept {
		OFS_BENCHMARK(__FUNCTION__);
		OFS_PROFILE(__FUNCTION__);
		std::sort(actions.begin(), actions.end(),
			[](auto& a, auto& b) { return a.at < b.at; }
		);
	}
	inline void addAction(std::vector<FunscriptAction>& actions, FunscriptAction newAction) noexcept {
		OFS_PROFILE(__FUNCTION__);
		auto it = std::find_if(actions.begin(), actions.end(), [newAction](auto action) {
			return newAction.at < action.at;
		});
		actions.insert(it, newAction);
		NotifyActionsChanged(true);
	}

	void NotifySelectionChanged() noexcept;

	void loadMetadata() noexcept;
	void saveMetadata() noexcept;

	void startSaveThread(const std::string& path, std::vector<FunscriptAction>&& actions, nlohmann::json&& json) noexcept;	
	std::string CurrentPath;
public:
	Funscript();
	~Funscript();

	inline void NotifyActionsChanged(bool isEdit) noexcept {
		funscriptChanged = true;
		if (isEdit && !unsavedEdits) {
			unsavedEdits = true;
			editTime = std::chrono::system_clock::now();
		}
		ActionMapNeedsUpdate = true;
	}

	std::unique_ptr<FunscriptUndoSystem> undoSystem;

	std::string Title;
	bool Enabled = true;

	inline void UpdatePath(const std::string& path) noexcept {
		CurrentPath = path;
		Title = Util::PathFromString(CurrentPath)
			.replace_extension("")
			.filename()
			.u8string();
	}

	inline void SetSavedFromOutside() noexcept { unsavedEdits = false;	}

	inline const std::string& Path() const noexcept { return CurrentPath; }

	inline void rollback(const FunscriptData& data) noexcept { this->data = data; NotifyActionsChanged(true); }
	void update() noexcept;

	bool open(const std::string& file);
	void save() noexcept { save(CurrentPath, true); }
	void save(const std::string& path, bool override_location = true);
	
	inline void reserveActionMemory(int32_t frameCount) noexcept { 
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

	float GetPositionAtTime(int32_t time_ms) noexcept;
	
	inline void AddAction(FunscriptAction newAction) noexcept { addAction(data.Actions, newAction); }
	void AddActionSafe(FunscriptAction newAction) noexcept;
	void AddActionRange(const std::vector<FunscriptAction>& range, bool checkDuplicates = true) noexcept;

	bool EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept;
	void AddEditAction(FunscriptAction action, float frameTimeMs) noexcept;
	void PasteAction(FunscriptAction paste, int32_t error_ms) noexcept;
	void RemoveAction(FunscriptAction action, bool checkInvalidSelection = true) noexcept;
	void RemoveActions(const std::vector<FunscriptAction>& actions) noexcept;

	std::vector<FunscriptAction> GetLastStroke(int32_t time_ms) noexcept;

	void SetActions(const std::vector<FunscriptAction>& override_with) noexcept;

	inline bool HasUnsavedEdits() const { return unsavedEdits; }
	inline const std::chrono::system_clock::time_point& EditTime() const { return editTime; }

	void RemoveActionsInInterval(int32_t fromMs, int32_t toMs) noexcept;

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
	void MoveSelectionTime(int32_t time_offset, float frameTimeMs) noexcept;
	void MoveSelectionPosition(int32_t pos_offset) noexcept;
	inline bool HasSelection() const noexcept { return data.selection.size() > 0; }
	inline int32_t SelectionSize() const noexcept { return data.selection.size(); }
	inline void ClearSelection() noexcept { data.selection.clear(); }
	inline const FunscriptAction* GetClosestActionSelection(int32_t time_ms) noexcept { return getActionAtTime(data.selection, time_ms, std::numeric_limits<int32_t>::max()); }
	
	void SetSelection(const std::vector<FunscriptAction>& action_to_select, bool unsafe) noexcept;
	bool IsSelected(FunscriptAction action) noexcept;

	void EqualizeSelection() noexcept;
	void InvertSelection() noexcept;

	FunscriptSpline ScriptSpline;
	inline const float Spline(float timeMs) noexcept {
		if (ActionMapNeedsUpdate) {
			updateActionMap(data.Actions);
		}

		return ScriptSpline.Sample(data.Actions, timeMs, ActionMap);
	}

	inline const float SplineClamped(float timeMs) noexcept {
		return Util::Clamp<float>(Spline(timeMs) * 100.f, 0.f, 100.f);
	}
};

inline bool Funscript::open(const std::string& file)
{
	OFS_BENCHMARK(__FUNCTION__);
	OFS_PROFILE(__FUNCTION__);
	UpdatePath(file);
	scriptOpened = true;

	{
		nlohmann::json json;
		json = Util::LoadJson(file, &scriptOpened);

		if (!scriptOpened || !json.is_object() && json["actions"].is_array()) {
			LOGF_ERROR("Failed to parse funscript. \"%s\"", file.c_str());
			return false;
		}

		setBaseScript(json);
		Json = std::move(json);
	}
	auto actions = Json["actions"];
	data.Actions.clear();

	std::set<FunscriptAction> actionSet;
	if (actions.is_array()) {
		for (auto& action : actions) {
			int32_t time_ms = action["at"];
			int32_t pos = action["pos"];
			if (time_ms >= 0) {
				actionSet.emplace(time_ms, pos);
			}
		}
	}
	data.Actions.assign(actionSet.begin(), actionSet.end());

	loadMetadata();

	NotifyActionsChanged(false);

	Json.erase("version");
	Json.erase("inverted");
	Json.erase("range");
	Json.erase("OpenFunscripter");
	Json.erase("metadata");
	return true;
}

inline void Funscript::save(const std::string& path, bool override_location)
{
	OFS_BENCHMARK(__FUNCTION__);
	OFS_PROFILE(__FUNCTION__);
	saveMetadata();

	auto& actions = Json["actions"];
	actions.clear();

	// make sure actions are sorted
	sortActions(data.Actions);

	if (override_location) {
		CurrentPath = path;
		unsavedEdits = false;
	}

	std::vector<FunscriptAction> copyActions = data.Actions;
	startSaveThread(path, std::move(copyActions), std::move(Json));
}