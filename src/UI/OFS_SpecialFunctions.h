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
	void SelectionChanged(union SDL_Event& ev) noexcept;
	virtual void DrawUI() noexcept override;
};

class RamerDouglasPeucker : public FunctionBase
{
	float epsilon = 0.0f;
	float averageDistance = 0.f;
	bool createUndoState = true;
public:
	RamerDouglasPeucker() noexcept;
	virtual ~RamerDouglasPeucker() noexcept;
	void SelectionChanged(union SDL_Event& ev) noexcept;
	virtual void DrawUI() noexcept override;
};

class SpecialFunctionsWindow {
	FunctionBase* function = nullptr;
public:
	static constexpr const char* WindowId = "###SPECIAL_FUNCTIONS";
	SpecialFunctionsWindow() noexcept;
	void SetFunction(SpecialFunctions functionEnum) noexcept;
	void ShowFunctionsWindow(bool* open) noexcept;
};