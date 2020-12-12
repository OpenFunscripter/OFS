#pragma once

#include <memory>
#include "Funscript.h"

// ATTENTION: no reordering
enum SpecialFunctions : int32_t
{
	RANGE_EXTENDER,
	RAMER_DOUGLAS_PEUCKER, // simplification
	CUSTOM_LUA_FUNCTIONS,
	TOTAL_FUNCTIONS_COUNT
};

class FunctionBase {
protected:
	inline Funscript& ctx() noexcept;
public:
	virtual ~FunctionBase() noexcept {}
	virtual void DrawUI() noexcept = 0;
};

class FunctionRangeExtender : public FunctionBase 
{
	int32_t rangeExtend = 0;
	bool createUndoState = true;
public:
	FunctionRangeExtender() noexcept;
	virtual ~FunctionRangeExtender() noexcept;
	void SelectionChanged(SDL_Event& ev) noexcept;
	virtual void DrawUI() noexcept override;
};

class RamerDouglasPeucker : public FunctionBase
{
	float epsilon = 0.0f;
	bool createUndoState = true;
public:
	RamerDouglasPeucker() noexcept;
	virtual ~RamerDouglasPeucker() noexcept;
	void SelectionChanged(SDL_Event& ev) noexcept;
	virtual void DrawUI() noexcept override;
};

class CustomLua : public FunctionBase 
{
private:
	//struct lua_State* L = nullptr;
	struct LuaThread {
		struct lua_State* L = nullptr;
		std::string path;
		std::string setupScript;
		int result = 0;
		bool running = false;
		int32_t currentScriptIdx = 0;

		int32_t NewPositionMs = 0;
		std::vector<FunscriptAction> collected;
		std::vector<FunscriptAction> selection;
	} Thread;
	std::vector<std::string> scripts;
	bool createUndoState = true;

	void updateScripts() noexcept;
	void resetVM() noexcept;
	void runScript(const std::string& path) noexcept;
	static bool CollectScript(LuaThread& thread, struct lua_State* L) noexcept;
public:
	CustomLua() noexcept;
	virtual ~CustomLua() noexcept;
	void SelectionChanged(SDL_Event& ev) noexcept;
	virtual void DrawUI() noexcept override;
};

class SpecialFunctionsWindow {
	std::unique_ptr<FunctionBase> function;
public:
	static constexpr const char* SpecialFunctionsId = "Special functions";
	SpecialFunctionsWindow() noexcept;
	void SetFunction(SpecialFunctions functionEnum) noexcept;
	void ShowFunctionsWindow(bool* open) noexcept;
};