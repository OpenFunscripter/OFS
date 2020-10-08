#include "Funscript.h"

#include "SDL.h"
#include "OpenFunscripterUtil.h"
#include "EventSystem.h"
#include "OFS_Serialization.h"
#include "OpenFunscripter.h"

#include <algorithm>
#include <limits>
#include <set>

void Funscript::setScriptTemplate()
{
	// setup a base funscript template
	// if no funscript was loaded from disk
	Json["actions"] = nlohmann::json::array();
	Json["rawActions"] = nlohmann::json::array();
	Json["version"] = "1.0";
	Json["inverted"] = false;
	Json["range"] = 90; // I think this is mostly ignored anyway
	Json["OpenFunscripter"] = nlohmann::json::object();
}

void Funscript::NotifyActionsChanged()
{
	if (!funscript_changed) {
		funscript_changed = true;
	}
}

#define LOAD_METADATA(member) if (meta.contains( #member ))\
 {\
	auto ptr = meta[ #member ].get_ptr<decltype( Metadata::member )*>();\
	if(ptr != nullptr) metadata.member = *ptr;\
 }
void Funscript::loadMetadata()
{
	if (Json.contains("metadata")) {
		auto& meta = Json["metadata"];
		OFS::serializer::load(&metadata, &meta);
	}
}
#undef LOAD_METADATA

#define SAVE_METADATA(member) meta[ #member ] = metadata.member;
void Funscript::saveMetadata()
{
	if (!Json.contains("metadata")) {
		Json["metadata"] = nlohmann::json::object();
	}
	OFS::serializer::save(&metadata, &Json["metadata"]);
}
#undef SAVE_METADATA

void Funscript::loadSettings()
{
	if (Json.contains("OpenFunscripter")) {
		auto& settings = Json["OpenFunscripter"];
		OFS::serializer::load(&scriptSettings, &settings);
	}
}

void Funscript::saveSettings()
{
	auto settings = nlohmann::json::object();
	OFS::serializer::save(&scriptSettings, &settings);
	Json["OpenFunscripter"] = settings;
}

void Funscript::update() noexcept
{
	if (funscript_changed)
	{
		funscript_changed = false;
		SDL_Event ev;
		ev.type = EventSystem::FunscriptActionsChangedEvent;
		SDL_PushEvent(&ev);
	}
}

bool Funscript::open(const std::string& file)
{
	current_path = file;
	nlohmann::json json;
	json = Util::LoadJson(file);
	if (!json.is_object() && json["actions"].is_array()) {
		LOGF_ERROR("Failed to parse funscript. \"%s\"", file.c_str());
		return false;
	}

	Json = json;
	auto actions = Json["actions"];
	auto raw_actions = Json["rawActions"];

	data.Actions.clear();
	data.RawActions.clear();

	scriptOpened = true;

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

	std::set<FunscriptAction> rawActionSet;
	if (raw_actions.is_array()) {
		for (auto& action : raw_actions) {
			int32_t time_ms = action["at"];
			int32_t pos = action["pos"];
			if (actionSet.find(FunscriptAction(time_ms, pos)) == actionSet.end()) {
				rawActionSet.emplace(time_ms, pos);
			}
		}
	}
	data.RawActions.assign(rawActionSet.begin(), rawActionSet.end());

	actionSet.clear();

	loadSettings();
	loadMetadata();

	if (metadata.original_name.empty()) {
		metadata.original_name = std::filesystem::path(current_path)
			.replace_extension("")
			.filename()
			.string();
	}

	// sorting is ensured by the std::set
	//sortActions(data.Actions); // make sure it's ordered by time
	//sortActions(data.RawActions);

	NotifyActionsChanged();
	return true;
}

void Funscript::save(const std::string& path, bool override_location)
{
	if (override_location) {
		current_path = path;
	}

	if (!scriptOpened) {
		setScriptTemplate();
	}
	saveSettings();
	saveMetadata();

	auto& actions = Json["actions"];
	auto& raw_actions = Json["rawActions"];
	actions.clear();
	raw_actions.clear();

	// make sure actions are sorted
	sortActions(data.Actions);

	for (auto& action : data.Actions) {
		// a little validation just in case
		if (action.at < 0)
			continue;

		nlohmann::json actionObj;
		actionObj["at"] = action.at;
		actionObj["pos"] = Util::Clamp(action.pos, 0, 100);
		actions.push_back(actionObj);
	}

	for (auto& action : data.RawActions) {
		// a little validation just in case
		if (action.at < 0)
			continue;

		nlohmann::json actionObj;
		actionObj["at"] = action.at;
		actionObj["pos"] = Util::Clamp(action.pos, 0, 100);
		raw_actions.push_back(actionObj);
	}

	Util::WriteJson(Json, path.c_str());
}

int Funscript::GetPositionAtTime(int32_t time_ms) noexcept
{
	if (data.Actions.size() == 0) {	return 0; } 
	else if (data.Actions.size() == 1) return data.Actions[0].pos;

	for (int i = 0; i < data.Actions.size()-1; i++) {
		auto& action = data.Actions[i];
		auto& next = data.Actions[i + 1];
		
		if (time_ms > action.at && time_ms < next.at) {
			// interpolate position
			int32_t last_pos = action.pos;
			int32_t diff = next.pos - action.pos;
			float progress = (float)(time_ms - action.at) / (next.at - action.at);
			int32_t interp = last_pos + (int32_t)(progress * (float)diff);
			return interp;
		}
		else if (action.at == time_ms) {
			return action.pos;
		}

	}

	return data.Actions.back().pos;
}

int Funscript::GetRawPositionAtFrame(int32_t frame_no) noexcept
{
	if (frame_no >= data.RawActions.size()) return 0;
	// this is stupid
	auto pos = data.RawActions[frame_no].pos;
	if (pos >= 0) { 
		return pos; 
	}
	else if((frame_no + 1) < data.RawActions.size()) {
		pos = data.RawActions[frame_no + 1].pos;
		if (pos >= 0) {
			return pos;
		}
		else if((frame_no - 1) >= 0) {
			pos = data.RawActions[frame_no - 1].pos;
			if (pos >= 0) {
				return pos;
			}
		}
	}
	return 0;
}

FunscriptAction* Funscript::getAction(const FunscriptAction& action) noexcept
{
	auto it = std::find(data.Actions.begin(), data.Actions.end(), action);
	if (it != data.Actions.end())
		return &(*it);
	return nullptr;
}

FunscriptAction* Funscript::getActionAtTime(std::vector<FunscriptAction>& actions, int32_t time_ms, uint32_t max_error_ms) noexcept
{
	// gets an action at a time with a margin of error
	int32_t smallest_error = std::numeric_limits<int32_t>::max();
	FunscriptAction* smallest_error_action = nullptr;

	for (int i = 0; i < actions.size(); i++) {
		auto& action = actions[i];
		
		if (action.at > (time_ms + (max_error_ms/2)))
			break;

		int32_t error = std::abs(time_ms - action.at);
		if (error <= max_error_ms) {
			if (error <= smallest_error) {
				smallest_error = error;
				smallest_error_action = &action;
			}
			else {
				break;
			}
		}
	}
	return smallest_error_action;
}

FunscriptAction* Funscript::getNextActionAhead(int32_t time_ms) noexcept
{
	auto it = std::find_if(data.Actions.begin(), data.Actions.end(), 
		[&](auto& action) {
			return action.at > time_ms;
	});

	if (it != data.Actions.end())
		return &(*it);

	return nullptr;
}

FunscriptAction* Funscript::getPreviousActionBehind(int32_t time_ms) noexcept
{
	auto it = std::find_if(data.Actions.rbegin(), data.Actions.rend(),
		[&](auto& action) {
			return action.at < time_ms;
		});
	
	if (it != data.Actions.rend())
		return &(*it);

	return nullptr;
}

bool Funscript::EditAction(const FunscriptAction& oldAction, const FunscriptAction& newAction) noexcept
{
	// update action
	auto act = getAction(oldAction);
	if (act != nullptr) {
		act->at = newAction.at;
		act->pos = newAction.pos;
		checkForInvalidatedActions();
		NotifyActionsChanged();
		return true;
	}
	return false;
}

void Funscript::PasteAction(const FunscriptAction& paste, int32_t error_ms) noexcept
{
	auto act = GetActionAtTime(paste.at, error_ms);
	if (act != nullptr) {
		RemoveAction(*act);
	}
	AddAction(paste);
	NotifyActionsChanged();
}

void Funscript::RemoveActionRaw(const FunscriptAction& action) noexcept
{
	auto it = std::find(data.RawActions.begin(), data.RawActions.end(), action);
	if (it != data.RawActions.end()) {
		data.RawActions.erase(it);
	}
}

void Funscript::checkForInvalidatedActions() noexcept
{
	auto it = std::remove_if(data.selection.begin(), data.selection.end(), [&](auto& selected) {
		auto found = getAction(selected);
		if (found == nullptr)
			return true;
		return false;
	});
	if(it != data.selection.end())
		data.selection.erase(it);
}

void Funscript::RemoveAction(const FunscriptAction& action, bool checkInvalidSelection) noexcept
{
	auto it = std::find(data.Actions.begin(), data.Actions.end(), action);
	if (it != data.Actions.end()) {
		data.Actions.erase(it);
		NotifyActionsChanged();

		if (checkInvalidSelection) { checkForInvalidatedActions(); }
	}
}

void Funscript::RemoveActions(const std::vector<FunscriptAction>& removeActions) noexcept
{
	for (auto& action : removeActions)
		RemoveAction(action, false);
	NotifyActionsChanged();
}

void Funscript::SelectRawFrames(int32_t from_ms, int32_t to_ms) noexcept
{
	if (data.rawSelection.hasSelection()) { data.rawSelection.deselect(); }
	data.selection.clear();
	int i = 0;
	for (; i < data.RawActions.size(); i++) {
		auto& action = data.RawActions[i];
		if (action.at >= from_ms) {
			data.rawSelection.startIndex = i;
			break;
		}
		else if (action.at >= 0) {
			data.rawSelection.startIndex = i;
		}
	}
	for (; i < data.RawActions.size(); i++) {
		auto& action = data.RawActions[i];
		if (action.at >= to_ms) {
			data.rawSelection.endIndex = i;
			break;
		}
		else if (action.at < 0) {
			data.rawSelection.endIndex = i - 1;
		}
	}
}

bool Funscript::ToggleSelection(const FunscriptAction& action) noexcept
{
	auto it = std::find(data.selection.begin(), data.selection.end(), action);
	bool is_selected = it != data.selection.end();
	if (is_selected) {
		data.selection.erase(it);
	}
	else {
		data.selection.emplace_back(action);
	}
	return !is_selected;
}

void Funscript::SetSelection(const FunscriptAction& action, bool selected) noexcept
{
	auto it = std::find(data.selection.begin(), data.selection.end(), action);
	bool is_selected = it != data.selection.end();
	if(is_selected && !selected)
	{
		data.selection.erase(it);
	}
	else if(!is_selected && selected) {
		data.selection.emplace_back(action);
	}
}

void Funscript::SelectTopActions()
{
	if (data.selection.size() < 3) return;
	std::vector<FunscriptAction> deselect;
	for (int i = 1; i < data.selection.size() - 1; i++) {
		auto& prev = data.selection[i - 1];
		auto& current = data.selection[i];
		auto& next = data.selection[i + 1];

		auto& min1 = prev.pos < current.pos ? prev : current;
		auto& min2 = min1.pos < next.pos ? min1 : next;
		deselect.emplace_back(min1);
		if (min1.at != min2.at)
			deselect.emplace_back(min2);

	}
	for (auto& act : deselect)
		SetSelection(act, false);
}

void Funscript::SelectBottomActions()
{
	if (data.selection.size() < 3) return;
	std::vector<FunscriptAction> deselect;
	for (int i = 1; i < data.selection.size() - 1; i++) {
		auto& prev = data.selection[i - 1];
		auto& current = data.selection[i];
		auto& next = data.selection[i + 1];

		auto& max1 = prev.pos > current.pos ? prev : current;
		auto& max2 = max1.pos > next.pos ? max1 : next;
		deselect.emplace_back(max1);
		if (max1.at != max2.at)
			deselect.emplace_back(max2);

	}
	for (auto& act : deselect)
		SetSelection(act, false);
}

void Funscript::SelectMidActions()
{
	if (data.selection.size() < 3) return;
	auto selectionCopy = data.selection;
	SelectTopActions();
	auto topPoints = data.selection;
	data.selection = selectionCopy;
	SelectBottomActions();
	auto bottomPoints = data.selection;

	selectionCopy.erase(std::remove_if(selectionCopy.begin(), selectionCopy.end(),
		[&](auto val) {
			return std::any_of(topPoints.begin(), topPoints.end(), [&](auto a) { return a == val; })
				|| std::any_of(bottomPoints.begin(), bottomPoints.end(), [&](auto a) { return a == val; });
		}), selectionCopy.end());
	data.selection = selectionCopy;
	sortSelection();
}

void Funscript::SelectTime(int32_t from_ms, int32_t to_ms, bool clear) noexcept
{
	if(clear)
		ClearSelection();

	for (auto& action : data.Actions) {
		if (action.at >= from_ms && action.at <= to_ms) {
			ToggleSelection(action);
		}
		else if (action.at > to_ms)
			break;
	}

	if (!clear)
		sortSelection();

}

void Funscript::SelectAction(const FunscriptAction& select) noexcept
{
	auto action = GetAction(select);
	if (action != nullptr) {
		if (ToggleSelection(select)) {
			// keep selection ordered for rendering purposes
			sortSelection();
		}
	}
}

void Funscript::DeselectAction(const FunscriptAction& deselect) noexcept
{
	auto action = GetAction(deselect);
	if (action != nullptr)
		SetSelection(*action, false);
}

void Funscript::SelectAll() noexcept
{
	ClearSelection();
	data.selection.assign(data.Actions.begin(), data.Actions.end());
}

void Funscript::RemoveSelectedActions() noexcept
{
	RemoveActions(data.selection);
	ClearSelection();
	//NotifyActionsChanged(); // already called in RemoveActions
}

void Funscript::moveActionsTime(std::vector<FunscriptAction*> moving, int32_t time_offset)
{
	ClearSelection();
	for (auto move : moving) {
		move->at += time_offset;
	}
	NotifyActionsChanged();
}

void Funscript::moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t pos_offset)
{
	ClearSelection();
	for (auto move : moving) {
		move->pos += pos_offset;
		move->pos = Util::Clamp(move->pos, 0, 100);
	}
	NotifyActionsChanged();
}

void Funscript::MoveSelectionTime(int32_t time_offset) noexcept
{
	if (!HasSelection()) return;
	std::vector<FunscriptAction*> moving;

	// faster path when everything is selected
	if (data.selection.size() == data.Actions.size()) {
		for (auto& action : data.Actions)
			moving.push_back(&action);
		moveActionsTime(moving, time_offset);
		SelectAll();
		return;
	}

	auto prev = GetPreviousActionBehind(data.selection.front().at);
	auto next = GetNextActionAhead(data.selection.back().at);

	int32_t min_bound = 0;
	int32_t max_bound = std::numeric_limits<int32_t>::max();

	float frameTime = OpenFunscripter::ptr->player.getFrameTimeMs();
	if (time_offset > 0) {
		if (next != nullptr) {
			max_bound = next->at - frameTime;
			time_offset = std::min(time_offset, max_bound - data.selection.back().at);
		}
	}
	else
	{
		if (prev != nullptr) {
			min_bound = prev->at + frameTime;
			time_offset = std::max(time_offset, min_bound - data.selection.front().at);
		}
	}

	for (auto& find : data.selection) {
		auto m = getAction(find);
		if(m != nullptr)
			moving.push_back(m);
	}

	ClearSelection();
	for (auto move : moving) {
		move->at += time_offset;
		data.selection.emplace_back(*move);
	}
	NotifyActionsChanged();
}

void Funscript::MoveSelectionPosition(int32_t pos_offset) noexcept
{
	if (!HasSelection()) return;
	std::vector<FunscriptAction*> moving;
	
	// faster path when everything is selected
	if (data.selection.size() == data.Actions.size()) {
		for (auto& action : data.Actions)
			moving.push_back(&action);
		moveActionsPosition(moving, pos_offset);
		SelectAll();
		return;
	}

	for (auto& find : data.selection) {
		auto m = getAction(find);
		if (m != nullptr)
			moving.push_back(m);
	}

	ClearSelection();
	for (auto move : moving) {
		move->pos += pos_offset;
		move->pos = Util::Clamp(move->pos, 0, 100);
		data.selection.emplace_back(*move);
	}
	NotifyActionsChanged();
}

void Funscript::EqualizeSelection() noexcept
{
	if (data.selection.size() < 3) return;
	sortSelection(); // might be unnecessary
	auto first = data.selection.front();
	auto last = data.selection.back();
	float duration = last.at - first.at;
	int32_t step_ms = std::round(duration / (float)(data.selection.size()-1));
		
	auto copySelection = data.selection;
	RemoveSelectedActions(); // clears selection

	for (int i = 1; i < copySelection.size()-1; i++) {
		auto& newAction = copySelection[i];
		newAction.at = first.at + (i * step_ms);
	}

	for (auto& action : copySelection)
		AddAction(action);

	data.selection = std::move(copySelection);
}

void Funscript::InvertSelection() noexcept
{
	if (data.selection.size() == 0) return;
	auto copySelection = data.selection;
	RemoveSelectedActions();
	for (auto& act : copySelection)
	{
		act.pos = std::abs(act.pos - 100);
		AddAction(act);
	}
	data.selection = copySelection;
}
