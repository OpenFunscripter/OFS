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
    static constexpr auto Extension = ".funscript";
	
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
	};
	// this is used when loading from json or serializing to json
	Funscript::Metadata LocalMetadata;

	template<typename S>
	void serialize(S& s)
	{
		s.ext(*this, bitsery::ext::Growable{},
			[](S& s, Funscript& o) {
				s.container(o.data.Actions, std::numeric_limits<uint32_t>::max());
				s.text1b(o.currentPathRelative, o.currentPathRelative.max_size());
				s.text1b(o.title, o.title.max_size());

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
	// FIXME: OFS should be able to retain metadata injected by other programs without overwriting it
	//nlohmann::json JsonOther;

	std::chrono::system_clock::time_point editTime;
	bool scriptOpened = false;
	bool funscriptChanged = false; // used to fire only one event every frame a change occurs
	bool unsavedEdits = false; // used to track if the script has unsaved changes
	bool selectionChanged = false;
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
	inline void sortActions(FunscriptArray& actions) noexcept { std::sort(actions.begin(), actions.end()); }
	inline void addAction(FunscriptArray& actions, FunscriptAction newAction) noexcept { actions.emplace(newAction); notifyActionsChanged(true); }
	inline void notifySelectionChanged() noexcept { selectionChanged = true; }

	void loadMetadata(const nlohmann::json& metadataObj) noexcept;
	void saveMetadata(nlohmann::json& outMetadataObj) noexcept;

	void notifyActionsChanged(bool isEdit) noexcept; 
	std::string currentPathRelative;
	std::string title;
public:
	Funscript() noexcept;
	~Funscript() noexcept;

	bool Enabled = true;
	std::unique_ptr<FunscriptUndoSystem> undoSystem;

	void UpdateRelativePath(const std::string& path) noexcept;
	inline void SetSavedFromOutside() noexcept { unsavedEdits = false;	}
	inline const std::string& RelativePath() const noexcept { return currentPathRelative; }
	inline const std::string& Title() const noexcept { return title; }

	inline void Rollback(FunscriptData&& data) noexcept { this->data = std::move(data); notifyActionsChanged(true); }
	inline void Rollback(const FunscriptData& data) noexcept { this->data = data; notifyActionsChanged(true); }
	void Update() noexcept;

	bool Deserialize(const nlohmann::json& json) noexcept;
	nlohmann::json Serialize() noexcept;
	
	inline const FunscriptData& Data() const noexcept { return data; }
	inline const auto& Selection() const noexcept { return data.Selection; }
	inline const auto& Actions() const noexcept { return data.Actions; }

	inline const FunscriptAction* GetAction(FunscriptAction action) noexcept { return getAction(action); }
	inline const FunscriptAction* GetActionAtTime(float time, float errorTime) noexcept { return getActionAtTime(data.Actions, time, errorTime); }
	inline const FunscriptAction* GetNextActionAhead(float time) noexcept { return getNextActionAhead(time); }
	inline const FunscriptAction* GetPreviousActionBehind(float time) noexcept { return getPreviousActionBehind(time); }
	inline const FunscriptAction* GetClosestAction(float time) noexcept { return getActionAtTime(data.Actions, time, std::numeric_limits<float>::max()); }

	float GetPositionAtTime(float time) noexcept;
	
	inline void AddAction(FunscriptAction newAction) noexcept { addAction(data.Actions, newAction); }
	void AddMultipleActions(const FunscriptArray& actions) noexcept;

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
