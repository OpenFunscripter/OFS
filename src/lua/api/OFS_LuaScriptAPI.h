#pragma once
#include "OFS_Lua.h"
#include "Funscript.h"
#include "FunscriptAction.h"

#include <vector>
#include <memory>

struct LuaFunscriptAction
{
    FunscriptAction o;

    LuaFunscriptAction(FunscriptAction action) noexcept
        : o(action) {}
    LuaFunscriptAction(lua_Number at, lua_Integer pos) noexcept
    {
        o.atS = std::max(0.0, at);
        o.pos = Util::Clamp<lua_Integer>(pos, 0, 100);
    }

    inline lua_Number at() noexcept
    {
        return o.atS;
    }

    inline void set_at(lua_Number at) noexcept
    {
        o.atS = std::max(0.0, at);
    }

    inline lua_Integer pos() noexcept
    {
        return o.pos;
    }

    inline void set_pos(lua_Integer pos) noexcept
    {            
        o.pos = Util::Clamp<lua_Integer>(pos, 0, 100);
    }
};

using LuaFunscriptArray = std::vector<LuaFunscriptAction>;

class LuaLazyFunscript
{
    private:
        std::weak_ptr<Funscript> script;
        public:LuaFunscriptArray actions;
        private:
        bool copied = false;
    public:
        LuaLazyFunscript(std::weak_ptr<Funscript> script) noexcept;

        inline LuaFunscriptArray& Actions() noexcept 
        {
            if(!copied) {
                auto ref = script.lock();
                if(ref) {
                    copied = true;
                    auto size = ref->Actions().size();
                    actions.reserve(size);
                    for(auto action : ref->Actions()) {
                        actions.emplace_back(action);
                    }
                }
            }
            return actions;
        }

        inline void Sort() noexcept
        {
            if(!copied) return;
            std::sort(actions.begin(), actions.end(),
                [](auto a1, auto a2)
                {
                    return a1.o.atS < a2.o.atS;
                });
        }

        inline void Commit() noexcept;
};


class OFS_ScriptAPI
{
    private:
        static std::unique_ptr<LuaLazyFunscript> Script(lua_Integer idx) noexcept;
        static lua_Integer ActiveIdx() noexcept;
    public:
        OFS_ScriptAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept;
};