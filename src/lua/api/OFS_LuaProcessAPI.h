#pragma once
#include "OFS_Lua.h"
#include "subprocess.h"
#include <memory>

class OFS_LuaProcess
{
    private:
	struct subprocess_s proc = {0};
	bool active = false;

	public:
	
	OFS_LuaProcess(subprocess_s p) noexcept
		: proc(p), active(true) 
	{
        if(proc.stdout_file) 
        {
            fclose(proc.stdout_file);
            proc.stdout_file = nullptr;
        }
        if(proc.stderr_file)
        {
            fclose(proc.stderr_file);
            proc.stderr_file = nullptr;
        }
	}

    ~OFS_LuaProcess() noexcept
    {
        Shutdown();
    }

	static std::unique_ptr<OFS_LuaProcess> CreateProcess(const char* program, sol::variadic_args va) noexcept;

	inline void Shutdown() noexcept
	{
		if(active) {
			if(subprocess_alive(&proc)) {
				subprocess_terminate(&proc);
			}
			subprocess_destroy(&proc);
		}
	}

	inline bool IsAlive() noexcept
	{
		if(active) {
			return subprocess_alive(&proc) > 0;
		}
		return false;
	}

	inline lua_Integer Join() noexcept
	{
		int code = -1;
		if(active) {
			if(subprocess_join(&proc, &code) != 0) {
            	return code;
        	}
		}
		return code;
	}

	inline void Detach() noexcept
	{
		if(active) {
			subprocess_destroy(&proc);
			active = false;
		}
	}
};

class OFS_ProcessAPI
{
    public:
    OFS_ProcessAPI(sol::usertype<class OFS_ExtensionAPI>& ofs) noexcept;
    ~OFS_ProcessAPI() noexcept;
};