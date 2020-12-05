#pragma once

#include <memory>
#include "Funscript.h"

// ATTENTION: no reordering
enum SpecialFunctions : int32_t
{
	RANGE_EXTENDER,
	RAMER_DOUGLAS_PEUCKER, // simplification
	TOTAL_FUNCTIONS_COUNT
};

class FunctionBase {
protected:
	inline Funscript& ctx() noexcept;
public:
	virtual ~FunctionBase() {}
	virtual void DrawUI() noexcept = 0;
};

class FunctionRangeExtender : public FunctionBase 
{
	int32_t rangeExtend = 0;
	bool createUndoState = true;
public:
	FunctionRangeExtender();
	virtual ~FunctionRangeExtender();
	void SelectionChanged(SDL_Event& ev) noexcept;
	virtual void DrawUI() noexcept override;
};

class RamerDouglasPeucker : public FunctionBase
{
	float epsilon = 0.0f;
	bool createUndoState = true;
public:
	RamerDouglasPeucker();
	virtual ~RamerDouglasPeucker();
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