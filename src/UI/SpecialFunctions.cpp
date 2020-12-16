#include "SpecialFunctions.h"
#include "OpenFunscripter.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"

#include "OFS_Lua.h"

#include <filesystem>
#include <sstream>
#include <unordered_set>

#include "SDL_thread.h"
#include "SDL_atomic.h"

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
    auto& undoSystem = app->ActiveFunscript()->undoSystem;
    if (app->script().SelectionSize() > 4 || (undoSystem->MatchUndoTop(StateType::RANGE_EXTEND))) {
        if (ImGui::SliderInt("Range", &rangeExtend, -50, 100)) {
            rangeExtend = Util::Clamp<int32_t>(rangeExtend, -50, 100);
            if (createUndoState || 
                !undoSystem->MatchUndoTop(StateType::RANGE_EXTEND)) {
                app->undoSystem->Snapshot(StateType::RANGE_EXTEND, false);
            }
            else {
                app->undoSystem->Undo();
                app->undoSystem->Snapshot(StateType::RANGE_EXTEND, false);
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
    if (app->script().SelectionSize() > 4 || (app->script().undoSystem->MatchUndoTop(StateType::SIMPLIFY))) {
        if (ImGui::DragFloat("Epsilon", &epsilon, 0.1f)) {
            epsilon = std::max(epsilon, 0.f);
            if (createUndoState ||
                !app->script().undoSystem->MatchUndoTop(StateType::SIMPLIFY)) {
                // NOOP
            }
            else {
                app->undoSystem->Undo();
            }
            app->undoSystem->Snapshot(StateType::SIMPLIFY, false);

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

//====================================================================================================

struct LuaThread {
    lua_State* L = nullptr;
    std::string path;
    std::string setupScript;
    int result = 0;
    bool running = false;

    int32_t TotalActionCount = 0;
    int32_t ClipboardCount = 0;

    float progress = 0.f;

    int32_t NewPositionMs = 0;
    struct ScriptOutput {
        std::unordered_set<FunscriptAction, FunscriptActionHashfunction> actions;
        std::unordered_set<FunscriptAction, FunscriptActionHashfunction> selection;
    };
    std::vector<ScriptOutput> outputs;
};

static LuaThread Thread;
static std::string LuaConsoleBuffer;
static SDL_SpinLock SpinLock = 0;

CustomLua::CustomLua() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(EventSystem::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &CustomLua::SelectionChanged));
    resetVM();
    if (Thread.L != nullptr) {
        updateScripts();
    }
    else {
        LOG_ERROR("Failed to create lua vm.");
    }

}

CustomLua::~CustomLua() noexcept
{
    if (Thread.L != nullptr) {
        lua_close(Thread.L);
        Thread.L = nullptr;
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
    auto luaCorePathString = Util::Resource("lua");
    auto luaUserPathString = Util::Prefpath("lua");

    auto luaCorePath = Util::PathFromString(luaCorePathString);
    auto luaUserPath = Util::PathFromString(luaUserPathString);
    Util::CreateDirectories(luaCorePath);
    Util::CreateDirectories(luaUserPath);

    scripts.clear();

    auto gatherScriptsInPath = [&](const std::filesystem::path& path) {
        std::error_code ec;
        auto iterator = std::filesystem::directory_iterator(path, ec);
        for (auto it = std::filesystem::begin(iterator); it != std::filesystem::end(iterator); it++) {
            auto filename = it->path().filename().u8string();
            auto name = it->path().filename();
            name.replace_extension("");

            auto extension = it->path().extension().u8string();
            if (!filename.empty() && extension == ".lua") {
    #ifdef NDEBUG
                if (name == "funscript") { continue; }
    #endif
                LuaScript newScript;
                newScript.name = std::move(filename);
                newScript.absolutePath = it->path().u8string();
                scripts.emplace_back(newScript);
            }
        }
    };

    gatherScriptsInPath(luaCorePath);
    gatherScriptsInPath(luaUserPath);
}

static void WriteToConsole(const std::string& str) noexcept
{
    SDL_AtomicLock(&SpinLock);
    LuaConsoleBuffer += str;
    LuaConsoleBuffer += "\n";
    SDL_AtomicUnlock(&SpinLock);
}

static int LuaPrint(lua_State* L) noexcept
{
    int nargs = lua_gettop(L);
    
    std::stringstream ss;
    for (int i = 1; i <= nargs; ++i) {
         ss << lua_tostring(L, i);
    }
    ss << std::endl;

    SDL_AtomicLock(&SpinLock);
    LuaConsoleBuffer += ss.str();
    SDL_AtomicUnlock(&SpinLock);
    return 0;
}

static constexpr struct luaL_Reg printlib[] = {
  {"print", LuaPrint},
  {NULL, NULL} /* end of array */
};

static int LuaDoFile(lua_State* L, const char* path) {
    // luaL_dofile doesn't handle spaces in paths ...
    auto handle = SDL_RWFromFile(path, "r");
    if (handle != nullptr) {
        size_t buf_size = SDL_RWsize(handle);
        char* buffer = new char[buf_size+1];
        SDL_RWread(handle, buffer, sizeof(char), buf_size);
        buffer[buf_size] = '\0';
        SDL_RWclose(handle);
        int result = luaL_dostring(L, buffer);
        delete[] buffer;
        return result;
    }
    return -1;
}

void CustomLua::resetVM() noexcept
{
    SDL_AtomicLock(&SpinLock);
    LuaConsoleBuffer.clear();
    SDL_AtomicUnlock(&SpinLock);
    WriteToConsole("Running " LUA_VERSION " ...");
    
    if (Thread.L != nullptr) {
        lua_close(Thread.L);
        Thread.L = nullptr;
    }
    Thread.L = luaL_newstate();
    if (Thread.L != nullptr) {
        char tmp[1024];
        luaL_openlibs(Thread.L);
        // override print
        lua_getglobal(Thread.L, "_G");
        luaL_setfuncs(Thread.L, printlib, 0);
        lua_pop(Thread.L, 1);

        auto LuaSetProgress = [](lua_State* L) -> int {
            if (lua_isnumber(L, 1)) {
                double progress = lua_tonumber(L, 1);
                Thread.progress = progress;
            }
            return 0;
        };
        lua_pushcfunction(Thread.L, LuaSetProgress);
        lua_setglobal(Thread.L, "SetProgress");

        auto initScript = Util::Resource("lua/funscript.lua");
        
        int result = LuaDoFile(Thread.L, initScript.c_str());

        if (result > 0) {
            stbsp_snprintf(tmp, sizeof(tmp), "lua init script error: %s", lua_tostring(Thread.L, -1));
            WriteToConsole(tmp);
            LOG_ERROR(tmp);
        }

        auto app = OpenFunscripter::ptr;
        auto& script = app->ActiveFunscript();
        const int32_t scriptIndex = app->ActiveFunscriptIndex();
        auto& actions = script->Actions();

        int32_t index = 0;
        int32_t totalWork = actions.size() + app->FunscriptClipboard().size();

        Thread.TotalActionCount = 0;
        for (auto&& script : app->LoadedFunscripts) {
            Thread.TotalActionCount += script->Actions().size();
        }
        
        Thread.ClipboardCount = app->FunscriptClipboard().size();

        std::stringstream builder;
        for (auto&& action : actions) {
            stbsp_snprintf(tmp, sizeof(tmp), "CurrentScript:AddActionUnordered(%d, %d, %s)\n",
                action.at,
                action.pos,
                script->IsSelected(action) ? "true" : "false" // script->IsSelected is really slow. maybe for large selections checking an std::unordered_set  is probably alot faster
            );
            builder << tmp;
            
            // update progressbar
            index++;
            if (index % 100 == 0 && totalWork > 0) {
                stbsp_snprintf(tmp, sizeof(tmp), "SetProgress(%f)\n", (float)index / totalWork);
                builder << tmp;
            }
        }
        for (auto&& action : app->FunscriptClipboard()) {
            stbsp_snprintf(tmp, sizeof(tmp), "Clipboard:AddActionUnordered(%d, %d, false)\n",
                action.at,
                action.pos
            );
            builder << tmp;

            index++;
            // update progressbar
            index++;
            if (index % 100 == 0 && totalWork > 0) {
                stbsp_snprintf(tmp, sizeof(tmp), "SetProgress(%f)\n", (float)index / totalWork);
                builder << tmp;
            }
        }
        stbsp_snprintf(tmp, sizeof(tmp), "CurrentScript.title=\"%s\"\n", script->metadata.title.c_str());
        builder << tmp;

        stbsp_snprintf(tmp, sizeof(tmp), "CurrentScriptIdx=%d\n", scriptIndex+1); // !!! lua indexing starts at 1 !!!
        builder << tmp;


        for (int i = 0; i < app->LoadedFunscripts.size(); i++) {
            if (i == scriptIndex) { 
                builder << "table.insert(LoadedScripts, CurrentScript)\n";
                continue; 
            }
            auto& loadedScript = app->LoadedFunscripts[i];
            builder << "table.insert(LoadedScripts, Funscript:new())\n";
            // i+1 because lua indexing starts at 1 !!!
            stbsp_snprintf(tmp, sizeof(tmp), "LoadedScripts[%d].title = \"%s\"\n", i+1, loadedScript->metadata.title.c_str());
            builder << tmp;

            for (auto&& action : loadedScript->Actions()) {
                stbsp_snprintf(tmp, sizeof(tmp), "LoadedScripts[%d]:AddActionUnordered(%d, %d, %s)\n",
                    i + 1, // !!! lua indexing starts at 1 !!!
                    action.at,
                    action.pos,
                    loadedScript->IsSelected(action) ? "true" : "false" // script->IsSelected is really slow. maybe for large selections checking an std::unordered_set  is probably alot faster
                );
                builder << tmp;
            }
        }


        stbsp_snprintf(tmp, sizeof(tmp), "CurrentScript.title=\"%s\"\n", script->metadata.title.c_str());
        builder << tmp;


        stbsp_snprintf(tmp, sizeof(tmp), "CurrentTimeMs=%lf\n", app->player.getCurrentPositionMsInterp());
        builder << tmp;
        stbsp_snprintf(tmp, sizeof(tmp), "FrameTimeMs=%lf\n", app->player.getFrameTimeMs());
        builder << tmp;
        stbsp_snprintf(tmp, sizeof(tmp), "TotalTimeMs=%lf\n", static_cast<float>(app->player.getDuration() * 1000.f));
        builder << tmp;


        // reset progress for user script
        stbsp_snprintf(tmp, sizeof(tmp), "SetProgress(%f)\n", 0.f);
        builder << tmp;

        Thread.setupScript = builder.str();
    }
}

bool CollectScriptOutputs(LuaThread& thread, lua_State* L) noexcept
{
    auto app = OpenFunscripter::ptr;
    int32_t scriptCount = app->LoadedFunscripts.size();
    char tmp[1024];

    thread.outputs.clear();

#define CHECK_OR_FAIL(expr) if(!expr) goto failure

    lua_getglobal(L, "LoadedScripts");
    // global
    CHECK_OR_FAIL(lua_istable(L, -1));

    int32_t size = lua_rawlen(L, -1); // script count
    if (size != scriptCount) {
        stbsp_snprintf(tmp, sizeof(tmp), "ERROR: LoadedScripts count doesn't match. expected %d got %d", scriptCount, size);
        WriteToConsole(tmp);
        goto failure;
    }


    // iterate scripts
    for (int i = 1; i <= size; i++) {
        auto& currentScript = thread.outputs.emplace_back();

        lua_rawgeti(L, -1, i); // push script
        CHECK_OR_FAIL(lua_istable(L, -1));

        lua_getfield(L, -1, "actions"); // push actions array
        CHECK_OR_FAIL(lua_istable(L, -1));

        int32_t actionCount = lua_rawlen(L, -1);
        for (int actionIdx = 1; actionIdx <= actionCount; actionIdx++) {
            lua_rawgeti(L, -1, actionIdx); // push single action
            CHECK_OR_FAIL(lua_istable(L, -1));
            int32_t at;
            int32_t pos;
            bool selected;
                
            lua_getfield(L, -1, "at"); // push action
            CHECK_OR_FAIL(lua_isnumber(L, -1));
            at = lua_tonumber(L, -1);
            lua_pop(L, 1); // pop at

            lua_getfield(L, -1, "pos"); // push pos
            CHECK_OR_FAIL(lua_isnumber(L, -1));
            pos = lua_tonumber(L, -1);
            lua_pop(L, 1); // pop pos

            lua_getfield(L, -1, "selected"); // push selected
            CHECK_OR_FAIL(lua_isboolean(L, -1));
            selected = lua_toboolean(L, -1);
            lua_pop(L, 1); // pop selected

            pos = Util::Clamp(pos, 0, 100);
            at = std::max(at, 0);

            currentScript.actions.insert(FunscriptAction(at, pos));
            if (selected) { currentScript.selection.insert(FunscriptAction(at, pos)); }

            lua_pop(L, 1); // pop single action
        }
        lua_pop(L, 2); // pop actions array & script
    }

    lua_getglobal(L, "CurrentTimeMs");
    CHECK_OR_FAIL(lua_isnumber(L, -1));
    thread.NewPositionMs = lua_tonumber(L, -1);

    return true;

#undef CHECK_OR_FAIL
    failure:
    return false;
}

void CustomLua::runScript(const std::string& path) noexcept
{
    if (Thread.running) return;
    Thread.running = true; 

    resetVM();
    auto luaThread = [](void* user) -> int {
        LuaThread& data = *(LuaThread*)user;
        char tmp[1024];

        WriteToConsole("============= SETUP =============");
        stbsp_snprintf(tmp, sizeof(tmp), "Loading %d actions\nand %d clipboard actions into lua...", Thread.TotalActionCount, Thread.ClipboardCount);
        WriteToConsole(tmp);

        auto startTime = std::chrono::high_resolution_clock::now();
        luaL_dostring(data.L, data.setupScript.c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
      
        stbsp_snprintf(tmp, sizeof(tmp), "setup time: %ld ms", duration.count());
        WriteToConsole(tmp);

        stbsp_snprintf(tmp, sizeof(tmp), "path: %s", data.path.c_str());
        WriteToConsole(tmp);

        WriteToConsole("============= RUN LUA =============");
        startTime = std::chrono::high_resolution_clock::now();
        data.result = LuaDoFile(data.L, data.path.c_str());
        stbsp_snprintf(tmp, sizeof(tmp), "lua result: %d", data.result);
        WriteToConsole(tmp);

        if (data.result > 0) {
            stbsp_snprintf(tmp, sizeof(tmp), "lua error: %s", lua_tostring(data.L, -1));
            WriteToConsole(tmp);
            LOG_ERROR(tmp);
            data.running = false;
        }
        else {
            endTime = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            stbsp_snprintf(tmp, sizeof(tmp), "execution time: %ld ms", duration.count());
            WriteToConsole(tmp);

            if (CollectScriptOutputs(data, data.L)) {
                // fire event to the main thread
                auto eventData = new EventSystem::SingleShotEventData;
                eventData->ctx = &data;
                
                eventData->handler = [](void* ctx) {
                    // script finished handler
                    // this code executes on the main thread during event processing
                    LuaThread& data = *(LuaThread*)ctx;

                    auto app = OpenFunscripter::ptr;
                    std::vector<FunscriptAction> tmpBuffer;
                    app->undoSystem->Snapshot(StateType::CUSTOM_LUA, true);

                    for (int i = 0; i < app->LoadedFunscripts.size(); i++) {
                        auto& script = app->LoadedFunscripts[i];
                        auto& output = data.outputs[i];

                        tmpBuffer.clear();
                        tmpBuffer.reserve(output.actions.size());
                        tmpBuffer.insert(tmpBuffer.end(), output.actions.begin(), output.actions.end());
                        script->SetActions(tmpBuffer);

                        tmpBuffer.clear();
                        tmpBuffer.insert(tmpBuffer.end(), output.selection.begin(), output.selection.end());
                        script->SetSelection(tmpBuffer, true);
                    }

                    app->player.setPosition(data.NewPositionMs);
                    data.running = false;
                };

                SDL_Event ev;
                ev.type = EventSystem::SingleShotEvent;
                ev.user.data1 = eventData;
                SDL_PushEvent(&ev);
            }
            else {
                WriteToConsole("ERROR: Failed to read script outputs.");
                data.running = false;
            }
        }
        WriteToConsole("================ END ===============");
        return 0;
    };
    Thread.path = path;
    auto thread = SDL_CreateThread(luaThread, "CustomLuaScript", &Thread);
    SDL_DetachThread(thread);
}


void CustomLua::DrawUI() noexcept
{
    if (ImGui::Button("Reload scripts", ImVec2(-1.f, 0.f))) { updateScripts(); }
    Util::Tooltip("Reload scripts in the script directory.\nOnly has to be pressed when deleting or adding files.");

    if (ImGui::Button("Script directory", ImVec2(-1.f, 0.f))) { Util::OpenFileExplorer(Util::Prefpath("lua").c_str()); }
    ImGui::Spacing(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal); ImGui::Spacing();
    if (Thread.running) {
        ImGui::TextUnformatted("Running script...");
        ImGui::ProgressBar(Thread.progress);
    }
    else {
        for (auto&& script : scripts) {
            ImGui::SetNextItemOpen(false, ImGuiCond_Always);
            if (ImGui::CollapsingHeader(script.name.c_str())) {
                runScript(script.absolutePath);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                Util::OpenUrl(script.absolutePath.c_str());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            Util::Tooltip("Left click to run.\nRight click to edit.");
        }
    }
    ImGui::Spacing(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal); ImGui::Spacing();
    
    if (ImGui::Button("Open Lua documentation", ImVec2(-1.f, 0.f))) {
        Util::OpenUrl("https://www.lua.org/manual/5.4/");
    }

    SDL_AtomicLock(&SpinLock);
    if (Thread.running && ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.f);
    }
    ImGui::TextWrapped("%s", LuaConsoleBuffer.c_str());
    //ImGui::InputTextMultiline("##Console", &LuaConsoleBuffer, ImVec2(-1.f, -1.f), ImGuiInputTextFlags_ReadOnly);
    SDL_AtomicUnlock(&SpinLock);
    if (Thread.running) {
        ImGui::SetScrollHereY();
    }
}
