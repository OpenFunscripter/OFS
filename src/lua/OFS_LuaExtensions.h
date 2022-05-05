#pragma once
#include <vector>
#include <string>

#include "OFS_Lua.h"
#include "OFS_LuaExtension.h"

#include "OFS_ImGui.h"


class OFS_LuaExtensions
{
    private:
        std::string LastConfigPath;
        void load(const std::string& path) noexcept;
        void save() noexcept;
        void removeNonExisting() noexcept;
    public:
        static constexpr const char* ExtensionDir = "extensions";
        static constexpr const char* DynamicBindingHandler = "OFS_LuaExtensions";
        static bool DevMode;
        static bool ShowLogs;
        static OFS::AppLog ExtensionLogBuffer;
        std::vector<OFS_LuaExtension> Extensions;

        // tables/fields
        static constexpr const char* GlobalExtensionPtr = "OFS_ExtensionPtr";
        static constexpr const char* GlobalActionMetaTable = "OFS_TmpActionMetaTable";
        static constexpr const char* ScriptIdxUserdata = "OFS_ScriptIdx";
        static constexpr const char* ScriptDataUserdata = "OFS_ScriptData";
        static constexpr const char* ScriptActionsField = "actions";

        // functions
        static constexpr const char* InitFunction = "init";
        static constexpr const char* UpdateFunction = "update";
        static constexpr const char* RenderGui = "gui";

        OFS_LuaExtensions() noexcept;
        ~OFS_LuaExtensions() noexcept;

        void UpdateExtensionList() noexcept;

        void Update(float delta) noexcept;
        void ShowExtensions() noexcept;
        void ReloadEnabledExtensions() noexcept;


        template<typename Archive>
        void reflect(Archive& ar)
        {
            OFS_REFLECT(Extensions, ar);
            OFS_REFLECT(DevMode, ar);
            OFS_REFLECT(ShowLogs, ar);
        }
};