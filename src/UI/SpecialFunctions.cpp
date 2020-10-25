#include "SpecialFunctions.h"
#include "OpenFunscripter.h"
#include "imgui.h"

void SpecialFunctionsWindow::SetFunction(SpecialFunctions functionEnum) noexcept
{
	switch (functionEnum) {
	case SpecialFunctions::RANGE_EXTENDER:
		function = std::make_unique<FunctionRangeExtender>();
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
	ImGui::Combo("##Functions", (int32_t*)&currentFunction, "Range extender\0\0");
	ImGui::Spacing();
	function->DrawUI();
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
    //const std::string SnapshotMsg("Range extend");
    if (app->script().SelectionSize() > 4 || (app->undoRedoSystem.MatchUndoTop(StateType::RANGE_EXTEND))) {
        if (ImGui::SliderInt("Range", &rangeExtend, -75, 75)) {
            if (createUndoState || 
                !app->undoRedoSystem.MatchUndoTop(StateType::RANGE_EXTEND)) {
                app->undoRedoSystem.Snapshot(StateType::RANGE_EXTEND);
            }
            else {
                app->undoRedoSystem.Undo();
                app->undoRedoSystem.Snapshot(StateType::RANGE_EXTEND);
            }
            createUndoState = false;
            ctx().RangeExtendSelection(rangeExtend);
        }
    }
}

