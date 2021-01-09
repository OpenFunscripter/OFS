#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"
#include "OFS_Serialization.h"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <set>

#include "OFS_Util.h"
#include "SDL_mutex.h"

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
	} metadata;

	std::shared_ptr<void> userdata = nullptr;
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

	template<class UserSettings>
	void loadSettings(const std::string& name, UserSettings* user) noexcept;

	template<class UserSettings>
	void saveSettings(const std::string& name, UserSettings* user) noexcept;

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

	template<class UserType>
	inline UserType& Userdata() noexcept;

	template<class UserType>
	inline void AllocUser() noexcept;

	inline void rollback(const FunscriptData& data) noexcept { this->data = data; NotifyActionsChanged(true); }

	void update() noexcept;

	template<class UserType>
	bool open(const std::string& file, const std::string& usersettings);

	template<class UserType>
	void save(const std::string& usersettings) noexcept { save<UserType>(current_path, usersettings, true); }

	template<class UserType>
	void save(const std::string& path, const std::string& usersettings, bool override_location = true);
	
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
};


template<class UserSettings>
void Funscript::loadSettings(const std::string& name, UserSettings* user) noexcept {
	if (Json.contains(name)) {
		auto& settings = Json[name];
		OFS::serializer::load(user, &settings);
	}
}

template<class UserSettings>
void Funscript::saveSettings(const std::string& name, UserSettings* user) noexcept
{
	OFS::serializer::save(user, &Json[name]);
}

template<class UserSettings>
inline UserSettings& Funscript::Userdata() noexcept
{
	FUN_ASSERT(userdata != nullptr, "userdata is null");
	return *(UserSettings*)userdata.get();
}

template<class UserSettings>
inline void Funscript::AllocUser() noexcept
{
	FUN_ASSERT(userdata == nullptr, "there was already userdata");
	userdata = std::make_shared<UserSettings>();
}

template<class UserSettings>
inline bool Funscript::open(const std::string& file, const std::string& usersettings)
{
	current_path = file;
	scriptOpened = true;

	nlohmann::json json;
	json = Util::LoadJson(file, &scriptOpened);

	if (!scriptOpened || !json.is_object() && json["actions"].is_array()) {
		LOGF_ERROR("Failed to parse funscript. \"%s\"", file.c_str());
		return false;
	}

	setBaseScript(json);
	Json = json;
	auto actions = json["actions"];
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
	AllocUser<UserSettings>();	
	loadSettings<UserSettings>(usersettings, static_cast<UserSettings*>(userdata.get()));

	if (metadata.title.empty()) {
		metadata.title = std::filesystem::path(current_path)
			.replace_extension("")
			.filename()
			.string();
	}

	NotifyActionsChanged(false);
	return true;
}

template<class UserSettings>
inline void Funscript::save(const std::string& path, const std::string& usersettings, bool override_location)
{
	setScriptTemplate();
	saveMetadata();
	saveSettings<UserSettings>(usersettings, static_cast<UserSettings*>(&userdata->Get<UserSettings>()));

	auto& actions = Json["actions"];
	actions.clear();

	// make sure actions are sorted
	sortActions(data.Actions);

	for (auto& action : data.Actions) {
		// a little validation just in case
		if (action.at < 0)
			continue;

		nlohmann::json actionObj = {
			{ "at", action.at },
			{ "pos", Util::Clamp<int32_t>(action.pos, 0, 100) }
		};
		actions.emplace_back(std::move(actionObj));
	}

	if (override_location) {
		current_path = path;
		unsavedEdits = false;
	}
	startSaveThread(path, std::move(Json));
}
