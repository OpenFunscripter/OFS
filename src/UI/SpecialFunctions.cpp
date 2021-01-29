#include "SpecialFunctions.h"
#include "OpenFunscripter.h"
#include "FunscriptUndoSystem.h"
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
    app->events->Subscribe(FunscriptEvents::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &FunctionRangeExtender::SelectionChanged));
}

FunctionRangeExtender::~FunctionRangeExtender() noexcept
{
    auto app = OpenFunscripter::ptr;
    app->events->Unsubscribe(FunscriptEvents::FunscriptSelectionChangedEvent, this);
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
                app->undoSystem->Snapshot(StateType::RANGE_EXTEND, false, app->ActiveFunscript().get());
            }
            else {
                app->undoSystem->Undo(app->ActiveFunscript().get());
                app->undoSystem->Snapshot(StateType::RANGE_EXTEND, false, app->ActiveFunscript().get());
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
    app->events->Subscribe(FunscriptEvents::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &RamerDouglasPeucker::SelectionChanged));
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

static float PerpendicularDistance(FunscriptAction pt, FunscriptAction lineStart, FunscriptAction lineEnd)
{
    float dx = lineEnd.at - lineStart.at;
    float dy = lineEnd.pos - lineStart.pos;

    // Normalize
    float mag = (float)std::sqrt(dx * dx + dy * dy);
    if (mag > 0.0f)
    {
        dx /= mag;
        dy /= mag;
    }
    float pvx = pt.at - lineStart.at;
    float pvy = pt.pos - lineStart.pos;

    // Get dot product (project pv onto normalized direction)
    float pvdot = dx * pvx + dy * pvy;

    // Scale line direction vector and subtract it from pv
    float ax = pvx - pvdot * dx;
    float ay = pvy - pvdot * dy;

    return (float)std::sqrt(ax * ax + ay * ay);
}

static void RamerDouglasPeuckerIterative(std::vector<FunscriptAction>& points, float epsilon, std::vector<FunscriptAction>& output)
{
    size_t start = 0;
    size_t end = points.size() - 1;

    while (start < end)
    {
        output.push_back(points[start]);
        size_t newEnd = end;
        while (true)
        {
            size_t maxDistanceIndex = 0;
            float maxDistance = 0.0f;
            for (size_t i = start + 1; i < newEnd; i++)
            {
                float d = PerpendicularDistance(points[i], points[start], points[newEnd]);
                if (d > maxDistance)
                {
                    maxDistanceIndex = i;
                    maxDistance = d;
                }
            }
            if (maxDistance <= epsilon)
                break;
            newEnd = maxDistanceIndex;
        }
        start = newEnd;
    }
    output.push_back(points[end]);
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
                app->undoSystem->Undo(app->ActiveFunscript().get());
            }
            app->undoSystem->Snapshot(StateType::SIMPLIFY, false, app->ActiveFunscript().get());

            createUndoState = false;
            auto selection = ctx().Selection();
            ctx().RemoveSelectedActions();
            std::vector<FunscriptAction> newActions;
            newActions.reserve(selection.size());
            RamerDouglasPeuckerIterative(selection, epsilon, newActions);
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
    CustomLua::LuaScript* script = nullptr;
    
    std::string setupScript;
    int result = 0;
    bool running = false;
    bool dry_run = false;

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
    app->events->Subscribe(FunscriptEvents::FunscriptSelectionChangedEvent, EVENT_SYSTEM_BIND(this, &CustomLua::SelectionChanged));
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
         const char* str = lua_tostring(L, i);
         if (str != nullptr) { ss << str; }
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

        auto LuaSetSettings = [](lua_State* L) -> int {
            if (!Thread.dry_run && Thread.script->settings.values.size() > 0) {
                char tmp[1024];
                std::stringstream ss;
                stbsp_snprintf(tmp, sizeof(tmp), "%s = {}\n", Thread.script->settings.name.c_str());
                ss << tmp;
                for (auto&& value : Thread.script->settings.values) {
                    switch (value.type) {
                    case LuaScript::Settings::Value::Type::Bool:
                    {
                        bool* b = (bool*)&Thread.script->settings.buffer[value.offset];
                        stbsp_snprintf(tmp, sizeof(tmp), "%s.%s = %s\n", Thread.script->settings.name.c_str(),
                            value.name.c_str(),
                            *b ? "true" : "false");
                        ss << tmp;
                        break;
                    }
                    case LuaScript::Settings::Value::Type::Float:
                    {
                        float* f = (float*)&Thread.script->settings.buffer[value.offset];
                        stbsp_snprintf(tmp, sizeof(tmp), "%s.%s = %f\n", Thread.script->settings.name.c_str(),
                            value.name.c_str(),
                            *f);
                        ss << tmp;
                        break;
                    }
                    case LuaScript::Settings::Value::Type::String:
                    {
                        std::string* s = (std::string*)&Thread.script->settings.buffer[value.offset];
                        stbsp_snprintf(tmp, sizeof(tmp), "%s.%s = \"%s\"\n", Thread.script->settings.name.c_str(),
                            value.name.c_str(),
                            s->c_str());
                        ss << tmp;
                        break;
                    }
                    }
                }
                stbsp_snprintf(tmp, sizeof(tmp), "return %s\n", Thread.script->settings.name.c_str());
                ss << tmp;
                luaL_dostring(L, ss.str().c_str());
            }
            // only a dry_run will update settings
            else if (lua_istable(L, 1)) {
                // read all fields

                if (Thread.script->settings.values.size() > 0) {
                    Thread.script->settings.Free();
                }
                auto& settings = Thread.script->settings;
                LuaScript::Settings::Value value;
                int32_t buffer_offset = 0;
                constexpr size_t alignement = 4;

                lua_pushnil(L);

                auto alignOffset = [](int32_t& offset, size_t alignement) {
                    if (offset % alignement != 0) {
                        offset += alignement - (offset % alignement);
                    }
                };
                
                while (lua_next(L, -2)) {
                    // stack now contains: -1 => value; -2 => key; -3 => table
                    // copy the key so that lua_tostring does not modify the original
                    lua_pushvalue(L, -2);
                    // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
                    
                    // TODO: validation of types
                    const char* key = lua_tostring(L, -1);

                    switch (lua_type(L, -2)) {
                    case LUA_TBOOLEAN:
                    {
                        value.name = key;
                        value.offset = buffer_offset;
                        value.type = LuaScript::Settings::Value::Type::Bool;
                        settings.values.push_back(value);

                        buffer_offset += sizeof(bool);
                        alignOffset(buffer_offset, alignement);
                        settings.buffer.resize(buffer_offset);
                        bool* b = (bool*)&settings.buffer[value.offset];
                        *b = lua_toboolean(L, -2);
                        break;
                    }
                    case LUA_TNUMBER:
                    {
                        value.name = key;
                        value.offset = buffer_offset;
                        value.type = LuaScript::Settings::Value::Type::Float;
                        settings.values.push_back(value);

                        buffer_offset += sizeof(float);
                        alignOffset(buffer_offset, alignement);
                        settings.buffer.resize(buffer_offset);
                        float* number = (float*)&settings.buffer[value.offset];
                        *number = lua_tonumber(L, -2);
                        break;
                    }
                    case LUA_TSTRING:
                    {
                        value.name = key;
                        value.offset = buffer_offset;
                        value.type = LuaScript::Settings::Value::Type::String;
                        settings.values.push_back(value);

                        buffer_offset += sizeof(std::string);
                        alignOffset(buffer_offset, alignement);
                        settings.buffer.resize(buffer_offset);
                        std::string* str = new (&settings.buffer[value.offset]) std::string();
                        *str = lua_tostring(L, -2);
                        break;
                    }
                    
                    case LUA_TNIL:
                        LOGF_WARN("Unknown type for %s setings variable.", key);
                        break;

                    case LUA_TTABLE:
                    case LUA_TLIGHTUSERDATA:
                    case LUA_TFUNCTION:
                    case LUA_TUSERDATA:
                    case LUA_TTHREAD:
                    case LUA_TNONE:
                    default:
                        break;
                    }
                    const char* value = lua_tostring(L, -2);
                    LOGF_INFO("found settings variable: %s => %s\n", key, value);
                    // pop value + copy of key, leaving original key
                    lua_pop(L, 2);
                    // stack now contains: -1 => key; -2 => table
                }
                lua_pop(L, 1);
                std::sort(Thread.script->settings.values.begin(), Thread.script->settings.values.end(),
                    [](auto& val1, auto& val2) {
                        return val1.name < val2.name;
                    });
            }
            return 0;
        };
        lua_pushcfunction(Thread.L, LuaSetSettings);
        lua_setglobal(Thread.L, "SetSettings");

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


        stbsp_snprintf(tmp, sizeof(tmp), "CurrentTimeMs=%lf\n", app->player->getCurrentPositionMsInterp());
        builder << tmp;
        stbsp_snprintf(tmp, sizeof(tmp), "FrameTimeMs=%lf\n", app->player->getFrameTimeMs());
        builder << tmp;
        stbsp_snprintf(tmp, sizeof(tmp), "TotalTimeMs=%lf\n", static_cast<float>(app->player->getDuration() * 1000.f));
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

    int32_t size;
    int32_t at;
    int32_t pos;
    bool selected;
    
    int32_t i;
    int32_t actionCount;
    int32_t actionIdx;

    thread.outputs.clear();

#define CHECK_OR_FAIL(expr) if(!expr) goto failure

    lua_getglobal(L, "LoadedScripts");
    // global
    CHECK_OR_FAIL(lua_istable(L, -1));

    size = lua_rawlen(L, -1); // script count
    if (size != scriptCount) {
        stbsp_snprintf(tmp, sizeof(tmp), "ERROR: LoadedScripts count doesn't match. expected %d got %d", scriptCount, size);
        WriteToConsole(tmp);
        goto failure;
    }

    // iterate scripts
    for (i = 1; i <= size; i++) {
        auto& currentScript = thread.outputs.emplace_back();

        lua_rawgeti(L, -1, i); // push script
        CHECK_OR_FAIL(lua_istable(L, -1));

        lua_getfield(L, -1, "actions"); // push actions array
        CHECK_OR_FAIL(lua_istable(L, -1));

        actionCount = lua_rawlen(L, -1);
        for (actionIdx = 1; actionIdx <= actionCount; actionIdx++) {
            lua_rawgeti(L, -1, actionIdx); // push single action
            CHECK_OR_FAIL(lua_istable(L, -1));
                
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

void CustomLua::runScript(LuaScript* script, bool dry_run) noexcept
{
    if (Thread.running) return;
    Thread.running = true; 
    Thread.dry_run = dry_run;
    Thread.script = script;

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

        stbsp_snprintf(tmp, sizeof(tmp), "path: %s", data.script->absolutePath.c_str());
        WriteToConsole(tmp);

        WriteToConsole("============= RUN LUA =============");
        startTime = std::chrono::high_resolution_clock::now();
        data.result = LuaDoFile(data.L, data.script->absolutePath.c_str());
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

            if (data.dry_run) {
                data.running = false;
                return 0;
            }

            if (CollectScriptOutputs(data, data.L)) {
                // fire event to the main thread
                EventSystem::SingleShot([](void* ctx) {
                    // script finished handler
                    // this code executes on the main thread during event processing
                    LuaThread& data = *(LuaThread*)ctx;

                    auto app = OpenFunscripter::ptr;
                    std::vector<FunscriptAction> tmpBuffer;
                    app->undoSystem->Snapshot(StateType::CUSTOM_LUA, true, app->ActiveFunscript().get());

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

                    app->player->setPositionExact(data.NewPositionMs);
                    data.running = false;
                }, &data);
            }
            else {
                WriteToConsole("ERROR: Failed to read script outputs.");
                data.running = false;
            }
        }
        WriteToConsole("================ END ===============");
        return 0;
    };
    auto thread = SDL_CreateThread(luaThread, "CustomLuaScript", &Thread);
    SDL_DetachThread(thread);
}


void CustomLua::DrawUI() noexcept
{
    if (ImGui::Button("Reload scripts", ImVec2(-1.f, 0.f))) { updateScripts(); }
    Util::Tooltip("Reload scripts in the script directory.\nOnly has to be pressed when deleting or adding files.");

    if (ImGui::Button("Script directory", ImVec2(-1.f, 0.f))) { Util::OpenFileExplorer(Util::Prefpath("lua").c_str()); }
    ImGui::Spacing(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal); ImGui::Spacing();
    if (Thread.running && !Thread.dry_run) {
        ImGui::TextUnformatted("Running script...");
        ImGui::ProgressBar(Thread.progress);
    }
    else {
        for(int i=0; i < scripts.size(); i++) {
            auto& script = scripts[i];
            ImGui::PushID(i);

            if (ImGui::CollapsingHeader(script.name.c_str())) {
                ImGui::Spacing();
                ImGui::Indent();
                if (script.settings.values.size() > 0) {
                    for (auto& value : script.settings.values) {
                        switch (value.type) {
                        case LuaScript::Settings::Value::Type::Bool:
                        {
                            bool* b = (bool*)&script.settings.buffer[value.offset];
                            ImGui::Checkbox(value.name.c_str(), b);
                            break;
                        }
                        case LuaScript::Settings::Value::Type::Float:
                        {
                            float* f = (float*)&script.settings.buffer[value.offset];
                            ImGui::InputFloat(value.name.c_str(), f);
                            break;
                        }
                        case LuaScript::Settings::Value::Type::String:
                        {
                            std::string* s = (std::string*)&script.settings.buffer[value.offset];
                            ImGui::InputText(value.name.c_str(), s);
                            break;
                        }
                        }
                    }
                }
                else {
                    ImGui::TextDisabled("Script has no settings or they aren't loaded.");
                }
                ImGui::Button("Script", ImVec2(-1.f, 0.f));
                Util::Tooltip("Left click to run.\nMiddle click to load settings.\nRight click to edit.");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    // run the script
                    runScript(&script);
                }
                else if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    // open file in default editor
                    Util::OpenUrl(script.absolutePath.c_str());
                }
                else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                    // reload settings
                    runScript(&script, true);
                }
                ImGui::Unindent();
                ImGui::Spacing();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
            }
            ImGui::PopID();
        }
    }
    ImGui::Spacing();
    
    if (ImGui::Button("Open Lua documentation", ImVec2(-1.f, 0.f))) {
        Util::OpenUrl("https://www.lua.org/manual/5.4/");
    }

    ImGui::BeginChild("ConsoleBuffer", ImVec2(-1.f, 200.f));
    SDL_AtomicLock(&SpinLock);
    ImGui::TextWrapped("%s", LuaConsoleBuffer.c_str());
    SDL_AtomicUnlock(&SpinLock);
    if (Thread.running) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

CustomLua::LuaScript::Settings::~Settings()
{
    Free();
}
