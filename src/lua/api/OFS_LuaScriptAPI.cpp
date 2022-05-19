#include "OFS_LuaScriptAPI.h"
#include "OpenFunscripter.h"

OFS_ScriptAPI::OFS_ScriptAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept
{
    auto L = sol::state_view(ofs.lua_state());
    auto script = L.new_usertype<LuaLazyFunscript>("Funscript");
    script["actions"] = sol::readonly_property(&LuaLazyFunscript::Actions);
    script["commit"] = &LuaLazyFunscript::Commit;
    script["sort"] = &LuaLazyFunscript::Sort;

    auto action = L.new_usertype<LuaFunscriptAction>("Action",
        sol::constructors<LuaFunscriptAction(lua_Number, lua_Integer)>());
    action["at"] = sol::property(&LuaFunscriptAction::at, &LuaFunscriptAction::set_at);
    action["pos"] = sol::property(&LuaFunscriptAction::pos, &LuaFunscriptAction::set_pos);

    ofs["ActiveIdx"] = OFS_ScriptAPI::ActiveIdx;
    ofs["Script"] = OFS_ScriptAPI::Script;
}

lua_Integer OFS_ScriptAPI::ActiveIdx() noexcept
{
    auto app = OpenFunscripter::ptr;
    return app->ActiveFunscriptIndex() + 1;
}

std::unique_ptr<LuaLazyFunscript> OFS_ScriptAPI::Script(lua_Integer idx) noexcept
{
    auto app = OpenFunscripter::ptr;
    idx -= 1;
    if(idx < 0 || idx >= app->LoadedFunscripts().size()) {
        return nullptr;
    }
    return std::make_unique<LuaLazyFunscript>(app->LoadedFunscripts()[idx]);
}

LuaLazyFunscript::LuaLazyFunscript(std::weak_ptr<Funscript> script) noexcept
    : script(script)
{
    EventSystem::RunOnMain([&](void*) {
        this->Actions();
    }, nullptr)
    ->Wait();
}

void LuaLazyFunscript::Commit() noexcept
{
    if(!copied) return;
    EventSystem::RunOnMain([&](void*){
        auto app = OpenFunscripter::ptr;
        auto ref = script.lock();
        if(ref) {
            FunscriptArray commit;
            commit.reserve(actions.size());
            for(auto action : actions) {
                commit.emplace(action.o);
            }
            app->undoSystem->Snapshot(StateType::CUSTOM_LUA, script);
            ref->SetActions(commit);
        }
    }, nullptr)
    ->Wait();
}