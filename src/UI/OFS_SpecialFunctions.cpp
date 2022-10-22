#include "OpenFunscripter.h"
#include "OFS_SpecialFunctions.h"
#include "FunscriptUndoSystem.h"
#include "OFS_ImGui.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "OFS_Lua.h"

#include <vector>
#include <sstream>

#include "SDL_thread.h"
#include "SDL_atomic.h"

#include <cmath>

SpecialFunctionsWindow::SpecialFunctionsWindow() noexcept
{
    // FIXME
    //SetFunction((SpecialFunctions)OpenFunscripter::ptr->settings->data().currentSpecialFunction);
    SetFunction(SpecialFunctions::RAMER_DOUGLAS_PEUCKER);
}

void SpecialFunctionsWindow::SetFunction(SpecialFunctions functionEnum) noexcept
{
    if (function != nullptr) {
        delete function; function = nullptr;
    }

	switch (functionEnum) {
	case SpecialFunctions::RANGE_EXTENDER:
		function = new FunctionRangeExtender();
		break;
    case SpecialFunctions::RAMER_DOUGLAS_PEUCKER:
        function = new RamerDouglasPeucker();
        break;
	default:
        function = new FunctionRangeExtender();
        // FIXME
        //OpenFunscripter::ptr->settings->data().currentSpecialFunction = SpecialFunctions::RANGE_EXTENDER;
		break;
	}
}

void SpecialFunctionsWindow::ShowFunctionsWindow(bool* open) noexcept
{
	if (open != nullptr && !(*open)) { return; }
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
	ImGui::Begin(TR_ID(WindowId, Tr::SPECIAL_FUNCTIONS), open, ImGuiWindowFlags_None);
	ImGui::SetNextItemWidth(-1.f);

    auto functionToString = [](SpecialFunctions func) -> const char*
    {
        switch(func)
        {
            case SpecialFunctions::RAMER_DOUGLAS_PEUCKER: return TR(FUNCTIONS_SIMPLIFY);
            case SpecialFunctions::RANGE_EXTENDER: return TR(FUNCTIONS_RANGE_EXTENDER);
        }
        return "";
    };

    // FIXME
    //if(ImGui::BeginCombo("##Functions", functionToString((SpecialFunctions)app->settings->data().currentSpecialFunction), ImGuiComboFlags_None))
    //{
    //    auto& func = (SpecialFunctions&)app->settings->data().currentSpecialFunction;
    //    if(ImGui::Selectable(TR(FUNCTIONS_SIMPLIFY), func == SpecialFunctions::RAMER_DOUGLAS_PEUCKER))
    //    {
    //        func = SpecialFunctions::RAMER_DOUGLAS_PEUCKER;
    //        SetFunction(func);
    //    }
    //    if(ImGui::Selectable(TR(FUNCTIONS_RANGE_EXTENDER), func == SpecialFunctions::RANGE_EXTENDER))
    //    {
    //        func = SpecialFunctions::RANGE_EXTENDER;
    //        SetFunction(func);
    //    }
    //    ImGui::EndCombo();
    //}

	ImGui::Spacing();
	function->DrawUI();
    Util::ForceMinumumWindowSize(ImGui::GetCurrentWindow());
	ImGui::End();
}

inline Funscript& FunctionBase::ctx() noexcept
{
    auto app = OpenFunscripter::ptr;
	return *app->ActiveFunscript().get();
}

// range extender
FunctionRangeExtender::FunctionRangeExtender() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(FunscriptEvents::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &FunctionRangeExtender::SelectionChanged));
}

FunctionRangeExtender::~FunctionRangeExtender() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Unsubscribe(FunscriptEvents::FunscriptSelectionChangedEvent, this);
}

void FunctionRangeExtender::SelectionChanged(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (!app->ActiveFunscript()->Selection().empty()) {
        rangeExtend = 0;
        createUndoState = true;
    }
}

void FunctionRangeExtender::DrawUI() noexcept
{
    auto app = OpenFunscripter::ptr;
    auto& undoSystem = app->ActiveFunscript()->undoSystem;
    if (app->ActiveFunscript()->SelectionSize() > 4 || (undoSystem->MatchUndoTop(StateType::RANGE_EXTEND))) {
        if (ImGui::SliderInt(TR(RANGE), &rangeExtend, -50, 100)) {
            rangeExtend = Util::Clamp<int32_t>(rangeExtend, -50, 100);
            if (createUndoState || 
                !undoSystem->MatchUndoTop(StateType::RANGE_EXTEND)) {
                app->undoSystem->Snapshot(StateType::RANGE_EXTEND, app->ActiveFunscript());
            }
            else {
                app->Undo();
                app->undoSystem->Snapshot(StateType::RANGE_EXTEND, app->ActiveFunscript());
            }
            createUndoState = false;
            ctx().RangeExtendSelection(rangeExtend);
        }
    }
    else
    {
        ImGui::Text(TR(RANGE_EXTENDER_TXT));
    }
}

RamerDouglasPeucker::RamerDouglasPeucker() noexcept
{
    auto app = OpenFunscripter::ptr;
    EventSystem::ev().Subscribe(FunscriptEvents::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &RamerDouglasPeucker::SelectionChanged));
}

RamerDouglasPeucker::~RamerDouglasPeucker() noexcept
{
    EventSystem::ev().UnsubscribeAll(this);
}

void RamerDouglasPeucker::SelectionChanged(SDL_Event& ev) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (!app->ActiveFunscript()->Selection().empty()) {
        epsilon = 0.f;
        createUndoState = true;
    }
}

inline static float PointLineDistance(FunscriptAction pt, FunscriptAction lineStart, FunscriptAction lineEnd) noexcept {
    float dx = lineEnd.atS - lineStart.atS;
    float dy = lineEnd.pos - lineStart.pos;

    // Normalize
    float mag = sqrtf(dx * dx + dy * dy);
    if (mag > 0.0f) {
        dx /= mag;
        dy /= mag;
    }
    float pvx = pt.atS - lineStart.atS;
    float pvy = pt.pos - lineStart.pos;

    // Get dot product (project pv onto normalized direction)
    float pvdot = dx * pvx + dy * pvy;

    // Scale line direction vector and subtract it from pv
    float ax = pvx - pvdot * dx;
    float ay = pvy - pvdot * dy;

    return sqrtf(ax * ax + ay * ay);
}

static std::vector<bool> DouglasPeucker(const FunscriptArray& points, int startIndex, int lastIndex, float epsilon) noexcept {
    OFS_PROFILE(__FUNCTION__);
    std::vector<std::pair<int, int>> stk;
    stk.push_back(std::make_pair(startIndex, lastIndex));
    
    int globalStartIndex = startIndex;
    auto bitArray = std::vector<bool>();
    bitArray.resize(lastIndex - startIndex + 1, true);

    while (!stk.empty()) {
        startIndex = stk.back().first;
        lastIndex = stk.back().second;
        stk.pop_back();

        float dmax = 0.f;
        int index = startIndex;

        for (int i = index + 1; i < lastIndex; ++i) {
            if (bitArray[i - globalStartIndex]) {
                float d = PointLineDistance(points[i], points[startIndex], points[lastIndex]);

                if (d > dmax) {
                    index = i;
                    dmax = d;
                }
            }
        }

        if (dmax > epsilon) {
            stk.push_back(std::make_pair(startIndex, index));
            stk.push_back(std::make_pair(index, lastIndex));
        }
        else {
            for (int i = startIndex + 1; i < lastIndex; ++i) {
                bitArray[i - globalStartIndex] = false;
            }
        }
    }

    return bitArray;
}

static void DouglasPeucker(const FunscriptArray& points, float epsilon, FunscriptArray& newActions) noexcept {
    OFS_PROFILE(__FUNCTION__);
    auto bitArray = DouglasPeucker(points, 0, points.size() - 1, epsilon);
    newActions.reserve(points.size());

    for (int i = 0, n = points.size(); i < n; ++i) {
        if (bitArray[i]) {
            // we can safely assume points to be sorted
            newActions.emplace_back_unsorted(points[i]);
        }
    }
}

void RamerDouglasPeucker::DrawUI() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto app = OpenFunscripter::ptr;
    if (app->ActiveFunscript()->SelectionSize() > 4 || (app->ActiveFunscript()->undoSystem->MatchUndoTop(StateType::SIMPLIFY))) {
        if (ImGui::DragFloat(TR(EPSILON), &epsilon, 0.001f, 0.f, 0.f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
            epsilon = std::max(epsilon, 0.f);
            if (createUndoState ||
                !app->ActiveFunscript()->undoSystem->MatchUndoTop(StateType::SIMPLIFY)) {
                // calculate average distance in selection
                int count = 0;
                for (int i = 0, size = ctx().Selection().size(); i < size - 1; ++i) {
                    auto action1 = ctx().Selection()[i];
                    auto action2 = ctx().Selection()[i + 1];
                    
                    float dx = action1.atS - action2.atS;
                    float dy = action1.pos - action2.pos;
                    float distance = sqrtf((dx * dx) + (dy * dy));
                    averageDistance += distance;
                    ++count;
                }
                averageDistance /= (float)count;
            }
            else {
                app->undoSystem->Undo();
            }
            app->undoSystem->Snapshot(StateType::SIMPLIFY, app->ActiveFunscript());

            createUndoState = false;
            auto selection = ctx().Selection();
            ctx().RemoveSelectedActions();
            FunscriptArray newActions;
            newActions.reserve(selection.size());
            float scaledEpsilon = epsilon * averageDistance;
            DouglasPeucker(selection, scaledEpsilon, newActions);
            ctx().AddMultipleActions(newActions);
        }
    }
    else {
        ImGui::Text(TR(SIMPLIFY_TXT));
    }
}