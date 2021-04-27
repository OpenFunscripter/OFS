#include "Funscript.h"

#include "SDL.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#include "EventSystem.h"
#include "OFS_Serialization.h"
#include "FunscriptUndoSystem.h"

#include <algorithm>
#include <limits>

Funscript::Funscript() 
{
	NotifyActionsChanged(false);
	saveMutex = SDL_CreateMutex();
	undoSystem = std::make_unique<FunscriptUndoSystem>(this);
}

Funscript::~Funscript()
{
	SDL_DestroyMutex(saveMutex);
}

void Funscript::loadMetadata() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (Json.contains("metadata")) {
		auto& meta = Json["metadata"];
		OFS::serializer::load(&LocalMetadata, &meta);
	}
}

void Funscript::saveMetadata() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	OFS::serializer::save(&LocalMetadata, &Json["metadata"]);
}

void Funscript::startSaveThread(const std::string& path, FunscriptArray&& actions, nlohmann::json&& json) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	struct SaveThreadData {
		nlohmann::json jsonObj;
		FunscriptArray actions;
		std::string path;
		SDL_mutex* mutex;
	};
	SaveThreadData* threadData = new SaveThreadData();
	threadData->mutex = saveMutex;
	threadData->path = path;
	threadData->jsonObj = std::move(json); // give ownership to the thread
	threadData->actions = std::move(actions);

	auto thread = [](void* user) -> int {
		SaveThreadData* data = static_cast<SaveThreadData*>(user);
		SDL_LockMutex(data->mutex);

		data->jsonObj["actions"] = nlohmann::json::array();
		data->jsonObj["version"] = "1.0";
		data->jsonObj["inverted"] = false;
		data->jsonObj["range"] = 100; // I think this is mostly ignored anyway

		auto& actions = data->jsonObj["actions"];
		for (auto action : data->actions) {
			// a little validation just in case
			if (action.atS < 0.f)
				continue;

			nlohmann::json actionObj = {
				{ "at", (int32_t)std::round(action.atS*1000.0) },
				{ "pos", Util::Clamp<int32_t>(action.pos, 0, 100) }
			};
			actions.emplace_back(std::move(actionObj));
	}

#ifdef NDEBUG
		Util::WriteJson(data->jsonObj, data->path.c_str());
#else
		Util::WriteJson(data->jsonObj, data->path.c_str(), true);
#endif
		SDL_UnlockMutex(data->mutex);
		delete data;
		return 0;
	};
	auto handle = SDL_CreateThread(thread, "SaveScriptThread", threadData);
	SDL_DetachThread(handle);
}

void Funscript::update() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (funscriptChanged) {
		funscriptChanged = false;
		EventSystem::PushEvent(FunscriptEvents::FunscriptActionsChangedEvent, this);
	}
	if (selectionChanged) {
		selectionChanged = false;
		EventSystem::PushEvent(FunscriptEvents::FunscriptSelectionChangedEvent, this);
	}
}

float Funscript::GetPositionAtTime(float time) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (data.Actions.size() == 0) {	return 0; } 
	else if (data.Actions.size() == 1) return data.Actions[0].pos;

	int i = 0;
	auto it = data.Actions.lower_bound(FunscriptAction(time, 0));
	if (it != data.Actions.end()) {
		i = std::distance(data.Actions.begin(), it);
		if (i > 0) --i;
	}

	for (; i < data.Actions.size()-1; i++) {
		auto& action = data.Actions[i];
		auto& next = data.Actions[i + 1];

		if (time > action.atS && time < next.atS) {
			// interpolate position
			int32_t lastPos = action.pos;
			float diff = next.pos - action.pos;
			float progress = (float)(time - action.atS) / (next.atS - action.atS);
			
			float interp = lastPos + (progress * (float)diff);
			return interp;
		}
		else if (action.atS == time) {
			return action.pos;
		}
	}

	return data.Actions.back().pos;
}

void Funscript::AddActionRange(const FunscriptArray& range, bool checkDuplicates) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (checkDuplicates) {
		data.Actions.insert(range.begin(), range.end());
	}
	else {
		for (auto action : range) {
			data.Actions.emplace_back_unsorted(action);
		}
	}

	sortActions(data.Actions);
	NotifyActionsChanged(true);
}

bool Funscript::EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	// update action
	auto act = getAction(oldAction);
	if (act != nullptr) {
		act->atS = newAction.atS;
		act->pos = newAction.pos;
		checkForInvalidatedActions();
		NotifyActionsChanged(true);
		sortActions(data.Actions);
		return true;
	}
	return false;
}

void Funscript::AddEditAction(FunscriptAction action, float frameTime) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto close = getActionAtTime(data.Actions, action.atS, frameTime);
	if (close != nullptr) {
		*close = action;
		NotifyActionsChanged(true);
		checkForInvalidatedActions();
	}
	else {
		AddAction(action);
	}
}

void Funscript::checkForInvalidatedActions() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto it = std::remove_if(data.selection.begin(), data.selection.end(), [this](auto selected) {
		auto found = getAction(selected);
		if (found) return false;
		return true;
	});
	if (it != data.selection.end()) {
		data.selection.erase(it);
		NotifySelectionChanged();
	}
}

void Funscript::RemoveAction(FunscriptAction action, bool checkInvalidSelection) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto it = data.Actions.find(action);
	if (it != data.Actions.end()) {
		data.Actions.erase(it);
		NotifyActionsChanged(true);

		if (checkInvalidSelection) { checkForInvalidatedActions(); }
	}
}

void Funscript::RemoveActions(const FunscriptArray& removeActions) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto it = std::remove_if(data.Actions.begin(), data.Actions.end(),
		[&removeActions, end = removeActions.end()](auto action) {
			if (removeActions.find(action) != end) {
				return true;
			}
			return false;
		});
	data.Actions.erase(it, data.Actions.end());

	NotifyActionsChanged(true);
	checkForInvalidatedActions();
}

std::vector<FunscriptAction> Funscript::GetLastStroke(float time) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	// TODO: refactor...
	// assuming "*it" is a peak bottom or peak top
	// if you went up it would return a down stroke and if you went down it would return a up stroke
	auto it = std::min_element(data.Actions.begin(), data.Actions.end(),
		[time](auto a, auto b) {
			return std::abs(a.atS - time) < std::abs(b.atS - time);
		});
	if (it == data.Actions.end() || it-1 == data.Actions.begin()) return std::vector<FunscriptAction>(0);

	std::vector<FunscriptAction> stroke;
	stroke.reserve(5);

	// search previous stroke
	bool goingUp = (it - 1)->pos > it->pos;
	int32_t prevPos = (it-1)->pos;
	for (auto searchIt = it-1; searchIt != data.Actions.begin(); searchIt--) {
		if ((searchIt - 1)->pos > prevPos != goingUp) {
			break;
		}
		else if ((searchIt - 1)->pos == prevPos && (searchIt-1)->pos != searchIt->pos) {
			break;
		}
		prevPos = (searchIt - 1)->pos;
		it = searchIt;
	}

	it--;
	if (it == data.Actions.begin()) return std::vector<FunscriptAction>(0);
	goingUp = !goingUp;
	prevPos = it->pos;
	stroke.emplace_back(*it);
	it--;
	for (;; it--) {
		bool up = it->pos > prevPos;
		if (up != goingUp) {
			break;
		}
		else if (it->pos == prevPos) {
			break;
		}
		stroke.emplace_back(*it);
		prevPos = it->pos;
		if (it == data.Actions.begin()) break;
	}
	return std::move(stroke);
}

void Funscript::SetActions(const FunscriptArray& override_with) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	//data.Actions.clear();
	//data.Actions.assign(override_with.begin(), override_with.end());
	//sortActions(data.Actions);
	data.Actions = override_with;
	NotifyActionsChanged(true);
}

void Funscript::RemoveActionsInInterval(float fromTime, float toTime) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	data.Actions.erase(
		std::remove_if(data.Actions.begin(), data.Actions.end(),
			[fromTime, toTime](auto action) {
				return action.atS >= fromTime && action.atS <= toTime;
			}), data.Actions.end()
	);
	checkForInvalidatedActions();
	NotifyActionsChanged(true);
}

void Funscript::RangeExtendSelection(int32_t rangeExtend) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto ExtendRange = [](std::vector<FunscriptAction*>& actions, int32_t rangeExtend) -> void {
		if (rangeExtend == 0) { return; }
		if (actions.size() < 0) { return; }

		auto StretchPosition = [](int32_t position, int32_t lowest, int32_t highest, int extension) -> int32_t
		{
			int32_t newHigh = Util::Clamp<int32_t>(highest + extension, 0, 100);
			int32_t newLow = Util::Clamp<int32_t>(lowest - extension, 0, 100);

			double relativePosition = (position - lowest) / (double)(highest - lowest);
			double newposition = relativePosition * (newHigh - newLow) + newLow;

			return Util::Clamp<int32_t>(newposition, 0, 100);
		};

		int lastExtremeIndex = 0;
		int32_t lastValue = (*actions[0]).pos;
		int32_t lastExtremeValue = lastValue;

		int32_t lowest = lastValue;
		int32_t highest = lastValue;

		enum class direction {
			NONE,
			UP,
			DOWN
		};
		direction strokeDir = direction::NONE;

		for (int index = 0; index < actions.size(); index++)
		{
			// Direction unknown
			if (strokeDir == direction::NONE)
			{
				if ((*actions[index]).pos < lastExtremeValue) {
					strokeDir = direction::DOWN;
				}
				else if ((*actions[index]).pos > lastExtremeValue) {
					strokeDir = direction::UP;
				}
			}
			else
			{
				if (((*actions[index]).pos < lastValue && strokeDir == direction::UP)     //previous was highpoint
					|| ((*actions[index]).pos > lastValue && strokeDir == direction::DOWN) //previous was lowpoint
					|| (index == actions.size() - 1))                            //last action
				{
					for (int i = lastExtremeIndex + 1; i < index; i++)
					{
						FunscriptAction& action = *actions[i];
						action.pos = StretchPosition(action.pos, lowest, highest, rangeExtend);
					}

					lastExtremeValue = (*actions[index - 1]).pos;
					lastExtremeIndex = index - 1;

					highest = lastExtremeValue;
					lowest = lastExtremeValue;

					strokeDir = (strokeDir == direction::UP) ? direction::DOWN : direction::UP;
				}

			}
			lastValue = (*actions[index]).pos;
			if (lastValue > highest)
				highest = lastValue;
			if (lastValue < lowest)
				lowest = lastValue;
		}
	};
	std::vector<FunscriptAction*> rangeExtendSelection;
	rangeExtendSelection.reserve(SelectionSize());
	int selectionOffset = 0;
	for (auto&& act : data.Actions) {
		for (int i = selectionOffset; i < data.selection.size(); i++) {
			if (data.selection[i] == act) {
				rangeExtendSelection.push_back(&act);
				selectionOffset = i;
				break;
			}
		}
	}
	if (rangeExtendSelection.size() == 0) { return; }
	ClearSelection();
	ExtendRange(rangeExtendSelection, rangeExtend);
}

bool Funscript::ToggleSelection(FunscriptAction action) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto it = data.selection.find(action);
	bool is_selected = it != data.selection.end();
	if (is_selected) {
		data.selection.erase(it);
	}
	else {
		data.selection.emplace(action);
	}
	NotifySelectionChanged();
	return !is_selected;
}

void Funscript::SetSelected(FunscriptAction action, bool selected) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto it = data.selection.find(action); 
	bool is_selected = it != data.selection.end();
	if(is_selected && !selected) {
		data.selection.erase(it);
	}
	else if(!is_selected && selected) {
		data.selection.emplace(action);
	}
	NotifySelectionChanged();
}

void Funscript::SelectTopActions() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (data.selection.size() < 3) return;
	std::vector<FunscriptAction> deselect;
	for (int i = 1; i < data.selection.size() - 1; i++) {
		auto& prev = data.selection[i - 1];
		auto& current = data.selection[i];
		auto& next = data.selection[i + 1];

		auto& min1 = prev.pos < current.pos ? prev : current;
		auto& min2 = min1.pos < next.pos ? min1 : next;
		deselect.emplace_back(min1);
		if (min1.atS != min2.atS) deselect.emplace_back(min2);
	}
	for (auto& act : deselect) SetSelected(act, false);
	NotifySelectionChanged();
}

void Funscript::SelectBottomActions() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (data.selection.size() < 3) return;
	std::vector<FunscriptAction> deselect;
	for (int i = 1; i < data.selection.size() - 1; i++) {
		auto& prev = data.selection[i - 1];
		auto& current = data.selection[i];
		auto& next = data.selection[i + 1];

		auto& max1 = prev.pos > current.pos ? prev : current;
		auto& max2 = max1.pos > next.pos ? max1 : next;
		deselect.emplace_back(max1);
		if (max1.atS != max2.atS) deselect.emplace_back(max2);
	}
	for (auto& act : deselect) SetSelected(act, false);
	NotifySelectionChanged();
}

void Funscript::SelectMidActions() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (data.selection.size() < 3) return;
	auto selectionCopy = data.selection;
	SelectTopActions();
	auto topPoints = data.selection;
	data.selection = selectionCopy;
	SelectBottomActions();
	auto bottomPoints = data.selection;

	selectionCopy.erase(std::remove_if(selectionCopy.begin(), selectionCopy.end(),
		[&topPoints, &bottomPoints](auto val) {
			return std::any_of(topPoints.begin(), topPoints.end(), [val](auto a) { return a == val; })
				|| std::any_of(bottomPoints.begin(), bottomPoints.end(), [val](auto a) { return a == val; });
		}), selectionCopy.end());
	data.selection = selectionCopy;
	sortSelection();
	NotifySelectionChanged();
}

void Funscript::SelectTime(float fromTime, float toTime, bool clear) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if(clear)
		ClearSelection();

	for (auto& action : data.Actions) {
		if (action.atS >= fromTime && action.atS <= toTime) {
			ToggleSelection(action);
		}
		else if (action.atS > toTime)
			break;
	}

	if (!clear)
		sortSelection();
	NotifySelectionChanged();
}

FunscriptArray Funscript::GetSelection(float fromTime, float toTime) noexcept
{
	FunscriptArray selection;
	if (!data.Actions.empty()) {
		auto start = data.Actions.lower_bound(FunscriptAction(fromTime, 0));
		auto end = data.Actions.upper_bound(FunscriptAction(toTime, 0)) + 1;
		for (; start != end; ++start) {
			auto action = *start;
			if (action.atS >= fromTime && action.atS <= toTime) {
				selection.emplace(action);
			}
		}
	}
	return selection;
}

void Funscript::SelectAction(FunscriptAction select) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto action = GetAction(select);
	if (action != nullptr) {
		if (ToggleSelection(select)) {
			// keep selection ordered for rendering purposes
			sortSelection();
		}
		NotifySelectionChanged();
	}
}

void Funscript::DeselectAction(FunscriptAction deselect) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto action = GetAction(deselect);
	if (action != nullptr)
		SetSelected(*action, false);
	NotifySelectionChanged();
}

void Funscript::SelectAll() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	ClearSelection();
	data.selection.assign(data.Actions.begin(), data.Actions.end());
	NotifySelectionChanged();
}

void Funscript::RemoveSelectedActions() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (data.selection.size() == data.Actions.size()) {
		// assume data.selection == data.Actions
		// aslong as we don't fuck up the selection this is safe 
		data.Actions.clear();
	}
	else {
		RemoveActions(data.selection);
	}

	ClearSelection();
	NotifyActionsChanged(true);
	NotifySelectionChanged();
}

void Funscript::moveActionsTime(std::vector<FunscriptAction*> moving, float timeOffset)
{
	OFS_PROFILE(__FUNCTION__);
	ClearSelection();
	for (auto move : moving) {
		move->atS += timeOffset;
	}
	NotifyActionsChanged(true);
}

void Funscript::moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t posOffset)
{
	OFS_PROFILE(__FUNCTION__);
	ClearSelection();
	for (auto move : moving) {
		move->pos += posOffset;
		move->pos = Util::Clamp<int16_t>(move->pos, 0, 100);
	}
	NotifyActionsChanged(true);
}

void Funscript::MoveSelectionTime(float timeOffset, float frameTime) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (!HasSelection()) return;
	std::vector<FunscriptAction*> moving;

	// faster path when everything is selected
	if (data.selection.size() == data.Actions.size()) {
		for (auto& action : data.Actions)
			moving.push_back(&action);
		moveActionsTime(moving, timeOffset);
		SelectAll();
		return;
	}

	auto prev = GetPreviousActionBehind(data.selection.front().atS);
	auto next = GetNextActionAhead(data.selection.back().atS);

	auto min_bound = 0.f;
	auto max_bound = std::numeric_limits<float>::max();

	if (timeOffset > 0) {
		if (next != nullptr) {
			max_bound = next->atS - frameTime;
			timeOffset = std::min(timeOffset, max_bound - data.selection.back().atS);
		}
	}
	else
	{
		if (prev != nullptr) {
			min_bound = prev->atS + frameTime;
			timeOffset = std::max(timeOffset, min_bound - data.selection.front().atS);
		}
	}

	for (auto& find : data.selection) {
		auto m = getAction(find);
		if(m != nullptr)
			moving.push_back(m);
	}

	ClearSelection();
	for (auto move : moving) {
		move->atS += timeOffset;
		data.selection.emplace(*move);
	}
	NotifyActionsChanged(true);
}

void Funscript::MoveSelectionPosition(int32_t pos_offset) noexcept
{
	OFS_PROFILE(__FUNCTION__);
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
		move->pos = Util::Clamp<int16_t>(move->pos, 0, 100);
		data.selection.emplace_back_unsorted(*move);
	}
	sortSelection();
	NotifyActionsChanged(true);
}

void Funscript::SetSelection(const FunscriptArray& actionsToSelect, bool unsafe) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	ClearSelection();
	if (!unsafe) {
		data.selection.insert(actionsToSelect.begin(), actionsToSelect.end());
	}
	else {
		for (auto action : actionsToSelect)	{
			auto end = data.Actions.end();
			if (data.Actions.find(action) != end) {
				data.selection.insert(action);
			}
		}
	}
}

bool Funscript::IsSelected(FunscriptAction action) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto it = data.selection.find(action);
	return it != data.selection.end();
}

void Funscript::EqualizeSelection() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (data.selection.size() < 3) return;
	sortSelection(); // might be unnecessary
	auto first = data.selection.front();
	auto last = data.selection.back();
	float duration = last.atS - first.atS;
	float stepTime = duration / (float)(data.selection.size()-1);
		
	auto copySelection = data.selection;
	RemoveSelectedActions(); // clears selection

	for (int i = 1; i < copySelection.size()-1; i++) {
		auto& newAction = copySelection[i];
		newAction.atS = first.atS + i * stepTime;
	}

	for (auto& action : copySelection)
		AddAction(action);

	data.selection = std::move(copySelection);
}

void Funscript::InvertSelection() noexcept
{
	OFS_PROFILE(__FUNCTION__);
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

int32_t FunscriptEvents::FunscriptActionsChangedEvent = 0;
int32_t FunscriptEvents::FunscriptSelectionChangedEvent = 0;

void FunscriptEvents::RegisterEvents() noexcept
{
	FunscriptActionsChangedEvent = SDL_RegisterEvents(1);
	FunscriptSelectionChangedEvent = SDL_RegisterEvents(1);
}

bool Funscript::Metadata::loadFromFunscript(const std::string& path) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	bool succ;
	auto json = Util::LoadJson(path, &succ);
	if (succ && json.contains("metadata")) {
		OFS::serializer::load(this, &json["metadata"]);
	}
	return succ;
}

bool Funscript::Metadata::writeToFunscript(const std::string& path) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	bool succ;
	auto json = Util::LoadJson(path, &succ);
	if (succ) {
		json["metadata"] = nlohmann::json::object();
		OFS::serializer::save(this, &json["metadata"]);
		Util::WriteJson(json, path, false);
	}
	return succ;
}
