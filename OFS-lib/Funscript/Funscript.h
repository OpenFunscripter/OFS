#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"
#include "OFS_Serialization.h"
#include "OFS_BinarySerialization.h"

#include <string>
#include <memory>
#include <chrono>

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
		FunscriptArray Actions;
		FunscriptArray Selection;
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
				s.container(o.data.Actions, std::numeric_limits<uint32_t>::max());
				s.text1b(o.CurrentPath, o.CurrentPath.max_size());
				s.text1b(o.Title, o.Title.max_size());

				// this code can be deleted after a couple releases
				// it just makes sure "Enabled" doesn't get 0 initialized
				// when the project is from an older OFS version
				if constexpr (std::is_same<S, ContextDeserializer>::value) {
					auto& a = s.adapter();
					if (a.currentReadEndPos() != a.currentReadPos()) {
						s.boolValue(o.Enabled);
					}
				}
				else {
					s.boolValue(o.Enabled);
				}
			});
	}

private:
	nlohmann::json Json;
	std::chrono::system_clock::time_point editTime;
	bool scriptOpened = false;
	bool funscriptChanged = false; // used to fire only one event every frame a change occurs
	bool unsavedEdits = false; // used to track if the script has unsaved changes
	bool selectionChanged = false;
	SDL_mutex* saveMutex = nullptr;
	FunscriptData data;

	void checkForInvalidatedActions() noexcept;

	inline FunscriptAction* getAction(FunscriptAction action) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (data.Actions.empty()) return nullptr;
		auto it = data.Actions.find(action);
		if(it != data.Actions.end()) {
			return &*it;
		}
		return nullptr;
	}

	public:
	static inline FunscriptAction* getActionAtTime(FunscriptArray& actions, float time, float maxErrorTime) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (actions.empty()) return nullptr;
		// gets an action at a time with a margin of error
		float smallestError = std::numeric_limits<float>::max();
		FunscriptAction* smallestErrorAction = nullptr;

		int i = 0;
		auto it = actions.lower_bound(FunscriptAction(time - maxErrorTime, 0));
		if (it != actions.end()) {
			i = std::distance(actions.begin(), it);
			if (i > 0) --i;
		}

		for (; i < actions.size(); i++) {
			auto& action = actions[i];

			if (action.atS > (time + (maxErrorTime / 2)))
				break;

			auto error = std::abs(time - action.atS);
			if (error <= maxErrorTime) {
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
	private:
	inline FunscriptAction* getNextActionAhead(float time) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (data.Actions.empty()) return nullptr;
		auto it = data.Actions.upper_bound(FunscriptAction(time, 0));
		return it != data.Actions.end() ? &*it : nullptr;
	}

	inline FunscriptAction* getPreviousActionBehind(float time) noexcept
	{
		OFS_PROFILE(__FUNCTION__);
		if (data.Actions.empty()) return nullptr;
		auto it = data.Actions.lower_bound(FunscriptAction(time, 0));
		if(it != data.Actions.begin()) {
			return &*(--it);
		}
		return nullptr;
	}

	void moveAllActionsTime(float timeOffset);
	void moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t posOffset);
	inline void sortSelection() noexcept { sortActions(data.Selection); }
	inline void sortActions(FunscriptArray& actions) noexcept {
		OFS_PROFILE(__FUNCTION__);
		std::sort(actions.begin(), actions.end());
	}
	inline void addAction(FunscriptArray& actions, FunscriptAction newAction) noexcept {
		OFS_PROFILE(__FUNCTION__);
		actions.emplace(newAction);
		NotifyActionsChanged(true);
	}

	inline void NotifySelectionChanged() noexcept {
		selectionChanged = true;
	}

	void loadMetadata() noexcept;
	void saveMetadata() noexcept;

	void startSaveThread(const std::string& path, FunscriptArray&& actions, nlohmann::json&& json) noexcept;	
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

	inline void rollback(FunscriptData&& data) noexcept { this->data = std::move(data); NotifyActionsChanged(true); }
	inline void rollback(const FunscriptData& data) noexcept { this->data = data; NotifyActionsChanged(true); }
	void update() noexcept;

	bool open(const std::string& file);
	void save() noexcept { save(CurrentPath, true); }
	void save(const std::string& path, bool override_location = true);
	
	const FunscriptData& Data() const noexcept { return data; }
	const auto& Selection() const noexcept { return data.Selection; }
	const auto& Actions() const noexcept { return data.Actions; }

	inline const FunscriptAction* GetAction(FunscriptAction action) noexcept { return getAction(action); }
	inline const FunscriptAction* GetActionAtTime(float time, float errorTime) noexcept { return getActionAtTime(data.Actions, time, errorTime); }
	inline const FunscriptAction* GetNextActionAhead(float time) noexcept { return getNextActionAhead(time); }
	inline const FunscriptAction* GetPreviousActionBehind(float time) noexcept { return getPreviousActionBehind(time); }
	inline const FunscriptAction* GetClosestAction(float time) noexcept { return getActionAtTime(data.Actions, time, std::numeric_limits<float>::max()); }

	float GetPositionAtTime(float time) noexcept;
	
	inline void AddAction(FunscriptAction newAction) noexcept { addAction(data.Actions, newAction); }
	void AddMultipleActions(const FunscriptArray& actions) noexcept;

	//void EditActionUnsafe(FunscriptAction* edit, FunscriptAction action) noexcept;
	bool EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept;
	void AddEditAction(FunscriptAction action, float frameTime) noexcept;
	void RemoveAction(FunscriptAction action, bool checkInvalidSelection = true) noexcept;
	void RemoveActions(const FunscriptArray& actions) noexcept;

	std::vector<FunscriptAction> GetLastStroke(float time) noexcept;

	void SetActions(const FunscriptArray& override_with) noexcept;

	inline bool HasUnsavedEdits() const { return unsavedEdits; }
	inline const std::chrono::system_clock::time_point& EditTime() const { return editTime; }

	void RemoveActionsInInterval(float fromTime, float toTime) noexcept;

	// selection api
	void RangeExtendSelection(int32_t rangeExtend) noexcept;
	bool ToggleSelection(FunscriptAction action) noexcept;
	
	void SetSelected(FunscriptAction action, bool selected) noexcept;
	void SelectTopActions() noexcept;
	void SelectBottomActions() noexcept;
	void SelectMidActions() noexcept;
	void SelectTime(float fromTime, float toTime, bool clear=true) noexcept;
	FunscriptArray GetSelection(float fromTime, float toTime) noexcept;

	void SelectAction(FunscriptAction select) noexcept;
	void DeselectAction(FunscriptAction deselect) noexcept;
	void SelectAll() noexcept;
	void RemoveSelectedActions() noexcept;
	void MoveSelectionTime(float time_offset, float frameTime) noexcept;
	void MoveSelectionPosition(int32_t pos_offset) noexcept;
	inline bool HasSelection() const noexcept { return !data.Selection.empty(); }
	inline uint32_t SelectionSize() const noexcept { return data.Selection.size(); }
	inline void ClearSelection() noexcept { data.Selection.clear(); }
	inline const FunscriptAction* GetClosestActionSelection(float time) noexcept { return getActionAtTime(data.Selection, time, std::numeric_limits<float>::max()); }
	
	void SetSelection(const FunscriptArray& actions) noexcept;
	bool IsSelected(FunscriptAction action) noexcept;

	void EqualizeSelection() noexcept;
	void InvertSelection() noexcept;

	FunscriptSpline ScriptSpline;
	inline const float Spline(float time) noexcept {
		return ScriptSpline.Sample(data.Actions, time);
	}

	inline const float SplineClamped(float time) noexcept {
		return Util::Clamp<float>(Spline(time) * 100.f, 0.f, 100.f);
	}
};

inline bool Funscript::open(const std::string& file)
{
	OFS_PROFILE(__FUNCTION__);
	UpdatePath(file);
	scriptOpened = false;

	{
		nlohmann::json json;
		auto jsonText = Util::ReadFileString(file.c_str());
		if(!jsonText.empty()) {
			json = Util::ParseJson(jsonText, &scriptOpened);
		}

		if (!scriptOpened || !json.is_object() || !json["actions"].is_array()) {
			LOGF_ERROR("Failed to parse funscript. \"%s\"", file.c_str());
			return false;
		}

		Json = std::move(json);
	}
	auto actions = Json["actions"];
	data.Actions.clear();

	for (auto& action : actions) {
		float time = action["at"].get<double>() / 1000.0;
		int32_t pos = action["pos"];
		if (time >= 0.f) {
			data.Actions.emplace(time, pos);
		}
	}

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

	auto copyActions = data.Actions;
	startSaveThread(path, std::move(copyActions), std::move(Json));
}

REFL_TYPE(Funscript::Metadata)
	REFL_FIELD(type)
	REFL_FIELD(title)
	REFL_FIELD(creator)
	REFL_FIELD(script_url)
	REFL_FIELD(video_url)
	REFL_FIELD(tags)
	REFL_FIELD(performers)
	REFL_FIELD(description)
	REFL_FIELD(license)
	REFL_FIELD(notes)
	REFL_FIELD(duration)
REFL_END

