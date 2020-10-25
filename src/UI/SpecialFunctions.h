#pragma once

#include <memory>
#include "Funscript.h"

enum SpecialFunctions : int32_t
{
	RANGE_EXTENDER,
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

class SpecialFunctionsWindow {
	std::unique_ptr<FunctionBase> function;
	SpecialFunctions currentFunction = SpecialFunctions::RANGE_EXTENDER;
public:
	static constexpr const char* SpecialFunctionsId = "Special functions";
	SpecialFunctionsWindow() { SetFunction(currentFunction); }
	void SetFunction(SpecialFunctions functionEnum) noexcept;
	void ShowFunctionsWindow(bool* open) noexcept;
};