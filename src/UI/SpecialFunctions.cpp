#include "SpecialFunctions.h"
#include "OpenFunscripter.h"
#include "imgui.h"

#include "OFS_Lua.h"

#include <filesystem>

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
    case SpecialFunctions::CUSTOM_LUA_FUNCTIONS:
        function = std::make_unique<CustomLua>();
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
        "Custom functions\0"
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
FunctionRangeExtender::FunctionRangeExtender() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(EventSystem::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &FunctionRangeExtender::SelectionChanged));
}

FunctionRangeExtender::~FunctionRangeExtender() noexcept
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

RamerDouglasPeucker::RamerDouglasPeucker() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(EventSystem::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &RamerDouglasPeucker::SelectionChanged));
}

RamerDouglasPeucker::~RamerDouglasPeucker() noexcept
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

inline static double PerpendicularDistance(const FunscriptAction pt, const FunscriptAction lineStart, const FunscriptAction lineEnd) noexcept
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

inline static void RamerDouglasPeuckerAlgo(const std::vector<FunscriptAction>& pointList, double epsilon, std::vector<FunscriptAction>& out) noexcept
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


CustomLua::CustomLua() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(EventSystem::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &CustomLua::SelectionChanged));
    resetVM();
    if (L != nullptr) {
        updateScripts();
    }
    else {
        LOG_ERROR("Failed to create lua vm.");
    }
}

CustomLua::~CustomLua() noexcept
{
    if (L != nullptr) {
        lua_close(L);
    }
    OpenFunscripter::ptr->events->UnsubscribeAll(this);
}

void CustomLua::SelectionChanged(SDL_Event& ev) noexcept
{
    if (OpenFunscripter::script().SelectionSize() > 0) {
        createUndoState = true;
    }
}

void CustomLua::updateScripts() noexcept
{
    auto luaPathString = Util::Resource("lua");
    auto luaPath = Util::PathFromString(luaPathString);
    Util::CreateDirectories(luaPathString);

    scripts.clear();

    std::error_code ec;
    auto iterator = std::filesystem::directory_iterator(luaPath, ec);
    for (auto it = std::filesystem::begin(iterator); it != std::filesystem::end(iterator); it++) {
        auto filename = it->path().filename().u8string();
        auto name = it->path().filename();
        name.replace_extension("");

        auto extension = it->path().extension().u8string();
        if (!filename.empty() && extension == ".lua" && name != "funscript") {
            scripts.emplace_back(std::move(filename));
        }
    }
}

void CustomLua::resetVM() noexcept
{
    if (L != nullptr) {
        lua_close(L);
        L = nullptr;
    }
    L = luaL_newstate();
    if (L != nullptr) {
        luaL_openlibs(L);

        /*
        auto newarray = [](lua_State* L) -> int {
            int n = luaL_checkinteger(L, 1);
            size_t nbytes = n * sizeof(FunscriptAction);
            FunscriptAction* a = (FunscriptAction*)lua_newuserdata(L, nbytes);
            return 1; 
        };
        lua_pushcfunction(L, newarray);
        lua_setglobal(L, "newarray");
        */
        auto initScript = Util::Resource("lua/funscript.lua");
        int result = luaL_dofile(L, initScript.c_str());
        if (result > 0) {
            LOGF_ERROR("lua init script error: %s", lua_tostring(L, -1));
        }

        // HACK
        auto& actions = OpenFunscripter::ptr->ActiveFunscript()->Actions();
        char tmp[1024];
        for (auto&& action : actions) {
            stbsp_snprintf(tmp, sizeof(tmp), "CurrentScript:AddAction(%d, %d)", action.at, action.pos);
            luaL_dostring(L, tmp);
        }
    }
}

bool CustomLua::runScript(const std::string& path) noexcept
{
    resetVM();
    LOGF_INFO("path: %s", path.c_str());
    LOG_INFO("============= RUN LUA =============");
    auto startTime = std::chrono::high_resolution_clock::now();
    int result = luaL_dofile(L, path.c_str());
    LOGF_INFO("lua result: %d", result);
    if (result > 0) {
        LOGF_ERROR("lua error: %s", lua_tostring(L, -1));
    }
    else {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOGF_INFO("execution time: %ld ms", duration.count());
        collectScript();
    }
    LOG_INFO("================ END ===============");
    return result > 0;
}

void CustomLua::collectScript() noexcept
{
    auto app = OpenFunscripter::ptr;
    std::vector<FunscriptAction> collectedScript;
    collectedScript.reserve(app->ActiveFunscript()->Actions().size());

    lua_getglobal(L, "CurrentScript");
    // global
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "actions");
        // CurrentScript.actions
        if (lua_istable(L, -1)) {
            int32_t size = lua_rawlen(L, -1);
            for (int i = 1; i <= size; i++) {
                lua_rawgeti(L, -1, i); // push action
                if (lua_istable(L, -1)) {
                    int32_t at;
                    int32_t pos;
                    lua_getfield(L, -1, "at");
                    if (lua_isnumber(L, -1)) {
                        at = lua_tonumber(L, -1);
                    }
                    else {
                        LOG_ERROR("Abort. Couldn't read \"at\" timestamp value.");
                        return;
                    }
                    lua_pop(L, 1); // pop at

                    lua_getfield(L, -1, "pos");
                    if (lua_isnumber(L, -1)) {
                        pos = lua_tonumber(L, -1);
                    }
                    else {
                        LOG_ERROR("Abort. Couldn't read position value.");
                        return;
                    }
                    lua_pop(L, 1); // pop pos
                    collectedScript.emplace_back(std::move(FunscriptAction(at, pos)));
                }
                lua_pop(L, 1); // pop action
            }
        }
    }

    // TODO: MASSIVE HACK!!!   
    app->ActiveFunscript()->undoSystem->Snapshot(StateType::CUSTOM_LUA);
    app->ActiveFunscript()->SelectAll();
    app->ActiveFunscript()->RemoveSelectedActions();
    for (auto&& action : collectedScript) {
        action.at = std::max(0, action.at);
        action.pos = Util::Clamp<int16_t>(action.pos, 0, 100);
        app->ActiveFunscript()->AddActionSafe(action);
    }
}

void CustomLua::DrawUI() noexcept
{
    if (ImGui::Button("Reload scripts")) { updateScripts(); }
    ImGui::SameLine();
    if (ImGui::Button("Script directory")) { Util::OpenFileExplorer(Util::Resource("lua").c_str()); }
    ImGui::Spacing(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal); ImGui::Spacing();
    for (auto&& script : scripts) {
        if (ImGui::Button(script.c_str())) {
            auto pathString = Util::Resource("lua");
            auto scriptPath = Util::PathFromString(pathString);
            Util::ConcatPathSafe(scriptPath, script);
            runScript(scriptPath.u8string());
        }
    }
}
