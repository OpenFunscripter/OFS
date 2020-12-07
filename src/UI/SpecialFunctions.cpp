#include "SpecialFunctions.h"
#include "OpenFunscripter.h"
#include "imgui.h"

SpecialFunctionsWindow::SpecialFunctionsWindow() noexcept
{
    SetFunction((SpecialFunctions)OpenFunscripter::ptr->settings->data().currentSpecialFunction);
}

void SpecialFunctionsWindow::SetFunction(SpecialFunctions functionEnum) noexcept
{
	switch (functionEnum) {
	case SpecialFunctions::RANGE_EXTENDER:
		function = std::make_unique<FunctionRangeExtender>();
		break;
    case SpecialFunctions::RAMER_DOUGLAS_PEUCKER:
        function = std::make_unique<RamerDouglasPeucker>();
        break;
	default:
		break;
	}
}

void SpecialFunctionsWindow::ShowFunctionsWindow(bool* open) noexcept
{
	if (open != nullptr && !(*open)) { return; }
	ImGui::Begin(SpecialFunctionsId, open, ImGuiWindowFlags_None);
	ImGui::SetNextItemWidth(-1.f);
    if (ImGui::Combo("##Functions", (int32_t*)&OpenFunscripter::ptr->settings->data().currentSpecialFunction,
        "Range extender\0"
        "Simplify (Ramer-Douglas-Peucker)\0"
        "\0")) {
        SetFunction((SpecialFunctions)OpenFunscripter::ptr->settings->data().currentSpecialFunction);
    }
	ImGui::Spacing();
	function->DrawUI();
    Util::ForceMinumumWindowSize(ImGui::GetCurrentWindow());
	ImGui::End();
}

inline Funscript& FunctionBase::ctx() noexcept
{
	return OpenFunscripter::script();
}

// range extender
FunctionRangeExtender::FunctionRangeExtender()
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(EventSystem::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &FunctionRangeExtender::SelectionChanged));
}

FunctionRangeExtender::~FunctionRangeExtender()
{
    auto app = OpenFunscripter::ptr;
    app->events->Unsubscribe(EventSystem::FunscriptSelectionChangedEvent, this);
}

void FunctionRangeExtender::SelectionChanged(SDL_Event& ev) noexcept
{
    if (OpenFunscripter::script().SelectionSize() > 0) {
        rangeExtend = 0;
        createUndoState = true;
    }
}

void FunctionRangeExtender::DrawUI() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto undoSystem = app->script().undoSystem.get();
    if (app->script().SelectionSize() > 4 || (undoSystem->MatchUndoTop(StateType::RANGE_EXTEND))) {
        if (ImGui::SliderInt("Range", &rangeExtend, -50, 100)) {
            rangeExtend = Util::Clamp<int32_t>(rangeExtend, -50, 100);
            if (createUndoState || 
                !undoSystem->MatchUndoTop(StateType::RANGE_EXTEND)) {
                undoSystem->Snapshot(StateType::RANGE_EXTEND);
            }
            else {
                undoSystem->Undo();
                undoSystem->Snapshot(StateType::RANGE_EXTEND);
            }
            createUndoState = false;
            ctx().RangeExtendSelection(rangeExtend);
        }
    }
    else
    {
        ImGui::Text("Select atleast 5 actions to extend.");
    }
}

RamerDouglasPeucker::RamerDouglasPeucker()
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(EventSystem::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &RamerDouglasPeucker::SelectionChanged));
}

RamerDouglasPeucker::~RamerDouglasPeucker()
{
    OpenFunscripter::ptr->events->UnsubscribeAll(this);
}

void RamerDouglasPeucker::SelectionChanged(SDL_Event& ev) noexcept
{
    if (OpenFunscripter::script().SelectionSize() > 0) {
        epsilon = 0.f;
        createUndoState = true;
    }
}

inline static double PerpendicularDistance(const FunscriptAction pt, const FunscriptAction lineStart, const FunscriptAction lineEnd)
{
    double dx = (double)lineEnd.at - lineStart.at;
    double dy = (double)lineEnd.pos - lineStart.pos;

    //Normalise
    double mag = std::sqrt(dx * dx + dy * dy);
    if (mag > 0.0)
    {
        dx /= mag; dy /= mag;
    }

    double pvx = (double)pt.at - lineStart.at;
    double pvy = (double)pt.pos - lineStart.pos;

    //Get dot product (project pv onto normalized direction)
    double pvdot = dx * pvx + dy * pvy;

    //Scale line direction vector
    double dsx = pvdot * dx;
    double dsy = pvdot * dy;

    //Subtract this from pv
    double ax = pvx - dsx;
    double ay = pvy - dsy;

    return std::sqrt(ax * ax + ay * ay);
}

inline static void RamerDouglasPeuckerAlgo(const std::vector<FunscriptAction>& pointList, double epsilon, std::vector<FunscriptAction>& out)
{
    // Find the point with the maximum distance from line between start and end
    double dmax = 0.0;
    size_t index = 0;
    size_t end = pointList.size() - 1;
    for (size_t i = 1; i < end; i++)
    {
        double d = PerpendicularDistance(pointList[i], pointList[0], pointList[end]);
        if (d > dmax)
        {
            index = i;
            dmax = d;
        }
    }

    // If max distance is greater than epsilon, recursively simplify
    if (dmax > epsilon)
    {
        // Recursive call
        std::vector<FunscriptAction> recResults1;
        std::vector<FunscriptAction> recResults2;
        std::vector<FunscriptAction> firstLine(pointList.begin(), pointList.begin() + index + 1);
        std::vector<FunscriptAction> lastLine(pointList.begin() + index, pointList.end());
        RamerDouglasPeuckerAlgo(firstLine, epsilon, recResults1);
        RamerDouglasPeuckerAlgo(lastLine, epsilon, recResults2);

        // Build the result list
        out.assign(recResults1.begin(), recResults1.end() - 1);
        out.insert(out.end(), recResults2.begin(), recResults2.end());
    }
    else
    {
        //Just return start and end points
        out.clear();
        out.push_back(pointList[0]);
        out.push_back(pointList[end]);
    }
}

void RamerDouglasPeucker::DrawUI() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto undoSystem = app->script().undoSystem.get();
    if (app->script().SelectionSize() > 4 || (undoSystem->MatchUndoTop(StateType::SIMPLIFY))) {
        if (ImGui::DragFloat("Epsilon", &epsilon, 0.1f)) {
            epsilon = std::max(epsilon, 0.f);
            if (createUndoState ||
                !undoSystem->MatchUndoTop(StateType::SIMPLIFY)) {
                undoSystem->Snapshot(StateType::SIMPLIFY);
            }
            else {
                undoSystem->Undo();
                undoSystem->Snapshot(StateType::SIMPLIFY);
            }
            createUndoState = false;
            auto selection = ctx().Selection();
            ctx().RemoveSelectedActions();
            std::vector<FunscriptAction> newActions;
            newActions.reserve(selection.size());
            RamerDouglasPeuckerAlgo(selection, epsilon, newActions);
            for (auto&& action : newActions) {
                ctx().AddAction(action);
            }
        }
    }
    else
    {
        ImGui::Text("Select atleast 5 actions to simplify.");
    }
}
