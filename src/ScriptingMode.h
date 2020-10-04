#pragma once

#include "Funscript.h"
#include "FunscriptAction.h"

#include <memory>

// ATTENTION: no reordering
enum ScriptingModeEnum {
	DEFAULT_MODE,
	ALTERNATING,
	DYNAMIC_INJECTION,
	RECORDING,
};

class ScripingModeBaseImpl 
{
protected:
	Funscript& ctx();
public:
	ScripingModeBaseImpl();
	virtual void DrawModeSettings() = 0;
	virtual void addAction(const FunscriptAction& action) = 0;
};

class DefaultModeImpl : public ScripingModeBaseImpl
{
public:
	virtual void DrawModeSettings() override {}
	virtual void addAction(const FunscriptAction& action) override {
		ctx().AddAction(action);
	}
};

class DynamicInjectionImpl : public ScripingModeBaseImpl
{
protected:
	const float max_speed = 500;
	const float min_speed = 50;
	float target_speed = 300;
	float direction_bias = 0.f;
	int top_bottom_direction = 1; // 1 for top and -1 for bottom injection
public:
	virtual void DrawModeSettings() override;
	virtual void addAction(const FunscriptAction& action) override;
};


class AlternatingImpl : public ScripingModeBaseImpl
{
	int32_t fixed_bottom = 0;
	int32_t fixed_top = 100;
	bool fixed_range_enabled = false;
public:
	virtual void DrawModeSettings() override;
	virtual void addAction(const FunscriptAction& action) override;
};

class RecordingImpl : public ScripingModeBaseImpl
{
public:
	virtual void DrawModeSettings() override;


	// Inherited via ScripingModeBaseImpl
	virtual void addAction(const FunscriptAction& action) override;

};

class OpenFunscripter;
class ScriptingMode {
	std::unique_ptr<ScripingModeBaseImpl> impl;
	ScriptingModeEnum active_mode;
	OpenFunscripter* ctx;
public:
	void setup();
	void DrawScriptingMode(bool* open);
	void setMode(ScriptingModeEnum mode);
	void addEditAction(const FunscriptAction& action);
	inline void addAction(const FunscriptAction& action) { impl->addAction(action); }
};