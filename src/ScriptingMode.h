#pragma once

#include "Funscript.h"
#include "FunscriptAction.h"

#include "SDL_events.h"

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
	virtual ~ScripingModeBaseImpl() {}
	virtual void DrawModeSettings() = 0;
	virtual void addAction(const FunscriptAction& action) = 0;
	virtual void update() noexcept {};
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
private:
	float right_x = 0.f, right_y = 0.f;
	float left_x = 0.f, left_y = 0.f;
	float right_trigger = 0.f;
	float left_trigger = 0.f;

	float value = 0.f;

	int16_t ControllerDeadzone = 1750;
	int32_t currentPos = 0;
	bool automaticRecording = true;
	bool recordingActive = false;
	bool inverted = false;

	// Attention: don't change order
	enum RecordingMode : int32_t {
		Mouse,
		Controller,
	};
	RecordingMode activeMode = RecordingMode::Mouse;
public:
	RecordingImpl();
	~RecordingImpl();

	void ControllerAxisMotion(SDL_Event& ev);

	virtual void DrawModeSettings() override;
	virtual void addAction(const FunscriptAction& action) override;
	virtual void update() noexcept override;
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
	inline void addAction(const FunscriptAction& action) noexcept { impl->addAction(action); }
	inline void update() noexcept { impl->update(); }
};