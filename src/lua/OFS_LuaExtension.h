#pragma once
#include <string>
#include "OFS_Lua.h"
#include "OFS_LuaExtensionApi.h"

#include <memory>

class OFS_LuaExtension
{
	private:
		lua_State* L = nullptr;
		std::unique_ptr<OFS_ExtensionAPI> api = nullptr;
    public:
		static constexpr const char* MainFile = "main.lua";
		std::string Name;
		std::string NameId;
		std::string Directory;
		std::string Error;

		bool WindowOpen = false;
		bool Active = false;

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
			Error = str;
		}

		void ClearError() noexcept
		{
			Error = std::string();
		}

		void ShowWindow() noexcept;

		void Shutdown() noexcept;
		void Toggle() noexcept;
};