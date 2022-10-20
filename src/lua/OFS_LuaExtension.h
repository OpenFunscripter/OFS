#pragma once
#include <string>
#include "OFS_Lua.h"
#include "OFS_LuaExtensionAPI.h"
#include "OFS_Util.h"

#include <memory>

class OFS_LuaExtension
{
	private:
		sol::state L;
		std::unique_ptr<OFS_ExtensionAPI> api = nullptr;
    public:
		static constexpr const char* MainFile = "main.lua";
		static constexpr const char* BindingTable = "binding";
		static constexpr const char* ScriptChangeFunction = "scriptChange";

		std::string Name;
		std::string NameId;
		std::string Directory;
		std::string Error;
		bool Active = false;
		bool WindowOpen = false;

		inline bool HasError() const noexcept { return !Error.empty(); }
		bool Load() noexcept;
		
		void AddError(const char* str) noexcept {
			LOG_ERROR(str);
			if(!Error.empty()) Error += '\n';
			Error += str;
		}
		void ClearError() noexcept { Error = std::string(); }

		void ShowWindow() noexcept;
		void Update() noexcept;
		void Shutdown() noexcept;
		void Toggle() noexcept;
		void ScriptChanged(uint32_t scriptIdx) noexcept;

		void Execute(const std::string& function) noexcept;
};

REFL_TYPE(OFS_LuaExtension)
	REFL_FIELD(Name)
	REFL_FIELD(NameId)
	REFL_FIELD(Directory)
	REFL_FIELD(Active)
	REFL_FIELD(WindowOpen)
REFL_END