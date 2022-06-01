#pragma once
#include <string>
#include "OFS_Lua.h"
#include "OFS_LuaExtensionAPI.h"
#include "OFS_Util.h"

#include <memory>

#include "EASTL/vector_map.h"

class OFS_LuaExtension
{
	private:
		sol::state L;
		std::unique_ptr<OFS_ExtensionAPI> api = nullptr;
		bool Active = false;
    public:
		static constexpr const char* MainFile = "main.lua";
		static constexpr const char* BindingTable = "binding";
		static constexpr const char* ScriptChangeFunction = "scriptChange";

		std::string Name;
		std::string NameId;
		std::string Directory;
		std::string Error;

		bool WindowOpen = false;

		inline bool IsActive() const noexcept { return Active; } 
		inline bool HasError() const noexcept { return !Error.empty(); }

		template<class Archive>
		void reflect(Archive& ar)
		{
			OFS_REFLECT(Name, ar);
			OFS_REFLECT(NameId, ar);
			OFS_REFLECT(Directory, ar);
			OFS_REFLECT(Active, ar);
			OFS_REFLECT(WindowOpen, ar);
			//Hash = Util::Hash(Directory.c_str(), Directory.size());
		}

		bool Load() noexcept;
		void SetError(const char* str) noexcept
		{
			LOG_ERROR(str);
			Error = str;
		}
		void ClearError() noexcept
		{
			Error = std::string();
		}

		void ShowWindow() noexcept;
		void Update() noexcept;
		void Shutdown() noexcept;
		void Toggle() noexcept;
		void ScriptChanged(uint32_t scriptIdx) noexcept;

		void Execute(const std::string& function) noexcept;
};