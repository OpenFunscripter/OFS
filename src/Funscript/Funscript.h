#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include <vector>
#include <string>


class Funscript
{
public:
	struct FunscriptData {
		std::vector<FunscriptAction> Actions;
		std::vector<FunscriptAction> selection;
		std::vector<FunscriptAction> RawActions;
	};

	struct Bookmark {
		int32_t at;
		std::string name;
	};

	struct Settings {
		std::vector<Bookmark> Bookmarks;
	};
	Funscript::Settings ScriptSettings;
private:
	nlohmann::json Json;
	bool scriptOpened = false;
	bool funscript_changed = false; // used to fire only one event every frame a change occurs

	void setScriptTemplate();
	void checkForInvalidatedActions() noexcept;
	
	FunscriptData data;
	
	FunscriptAction* getAction(const FunscriptAction& action) noexcept;
	FunscriptAction* getActionAtTime(std::vector<FunscriptAction>& actions, int32_t time_ms, uint32_t error_ms) noexcept;
	FunscriptAction* getNextActionAhead(int32_t time_ms) noexcept;
	FunscriptAction* getPreviousActionBehind(int32_t time_ms) noexcept;

	void moveActionsTime(std::vector<FunscriptAction*> moving, int32_t time_offset);
	void moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t pos_offset);
	inline void sortSelection() { sortActions(data.selection); }
	inline void sortActions(std::vector<FunscriptAction>& actions) {
		std::sort(actions.begin(), actions.end(),
			[](auto& a, auto& b) { return a.at < b.at; }
		);
	}
	inline void addAction(std::vector<FunscriptAction>& actions, const FunscriptAction& newAction) noexcept {
		auto it = std::find_if(actions.begin(), actions.end(), [&](auto& action) {
			return newAction.at < action.at;
			});
		actions.insert(it, newAction);
		NotifyActionsChanged();
	}

	void NotifyActionsChanged();
	void loadSettings();
	void saveSettings();
public:
	Funscript() { NotifyActionsChanged(); }
	std::string current_path;

	inline void rollback(const FunscriptData& data) noexcept { this->data = data; NotifyActionsChanged(); }

	void update() noexcept;

	bool open(const std::string& file);
	void save() { save(current_path); }
	void save(const std::string& path);
	
	const FunscriptData& Data() const noexcept { return data; }
	const std::vector<FunscriptAction>& Selection() const noexcept { return data.selection; }
	const std::vector<FunscriptAction>& Actions() const noexcept { return data.Actions; }
	const std::vector<FunscriptAction>& RawActions() const noexcept { return data.RawActions; }

	inline const FunscriptAction* GetAction(const FunscriptAction& action) noexcept { return getAction(action); }
	inline const FunscriptAction* GetActionAtTime(int32_t time_ms, uint32_t error_ms) noexcept { return getActionAtTime(data.Actions, time_ms, error_ms); }
	inline const FunscriptAction* GetNextActionAhead(int32_t time_ms) noexcept { return getNextActionAhead(time_ms); }
	inline const FunscriptAction* GetPreviousActionBehind(int32_t time_ms) noexcept { return getPreviousActionBehind(time_ms); }
	inline const FunscriptAction* GetClosestAction(int32_t time_ms) noexcept { return getActionAtTime(data.Actions, time_ms, std::numeric_limits<uint32_t>::max()); }

	int GetPositionAtTime(int32_t time_ms) noexcept;
	
	inline void AddAction(const FunscriptAction& newAction) noexcept { addAction(data.Actions, newAction); }
	bool EditAction(const FunscriptAction& oldAction, const FunscriptAction& newAction) noexcept;
	void PasteAction(const FunscriptAction& paste, int32_t error_ms) noexcept;
	void RemoveAction(const FunscriptAction& action, bool checkInvalidSelection = true) noexcept;
	void RemoveActions(const std::vector<FunscriptAction>& actions) noexcept;

	// bookmarks
	inline const std::vector<Funscript::Bookmark>& Bookmarks() const { return ScriptSettings.Bookmarks; }
	inline void AddBookmark(const Funscript::Bookmark& bookmark) { 
		ScriptSettings.Bookmarks.push_back(bookmark); 
		std::sort(ScriptSettings.Bookmarks.begin(), ScriptSettings.Bookmarks.end(),
			[](auto& a, auto& b) { return a.at < b.at; }
		);
	}

	// selection api
	bool ToggleSelection(const FunscriptAction& action) noexcept;
	void SetSelection(const FunscriptAction& action, bool selected) noexcept;
	void SelectTopActions();
	void SelectBottomActions();
	void SelectTime(int32_t from_ms, int32_t to_ms, bool clear=true) noexcept;
	void SelectAction(const FunscriptAction& select) noexcept;
	void DeselectAction(const FunscriptAction& deselect) noexcept;
	void SelectAll() noexcept;
	void RemoveSelectedActions() noexcept;
	void MoveSelectionTime(int32_t time_offset) noexcept;
	void MoveSelectionPosition(int32_t pos_offset) noexcept;
	inline bool HasSelection() const noexcept { return data.selection.size() > 0; }
	inline int32_t SelectionSize() const noexcept { return data.selection.size(); }
	inline void ClearSelection() noexcept { data.selection.clear(); }
	inline const FunscriptAction* GetClosestActionSelection(int32_t time_ms) noexcept { return getActionAtTime(data.selection, time_ms, std::numeric_limits<int32_t>::max()); }
};

