#pragma once

#include "Funscript.h"
#include "FunscriptAction.h"
#include "ScriptPositionsOverlayMode.h"
#include "OFS_ScriptPositionsOverlays.h"

#include "SDL_events.h"

#include <memory>
#include <array>

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
	virtual void DrawModeSettings() noexcept = 0;
	virtual void addAction(FunscriptAction action) noexcept {
		ctx().AddAction(action);
	}

	virtual void update() noexcept {};
};

class DefaultModeImpl : public ScripingModeBaseImpl
{
public:
	virtual void DrawModeSettings() noexcept override {}
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
	virtual void DrawModeSettings() noexcept override;
	virtual void addAction(FunscriptAction action) noexcept override;
};


class AlternatingImpl : public ScripingModeBaseImpl
{
	int32_t fixed_bottom = 0;
	int32_t fixed_top = 100;
	bool fixed_range_enabled = false;
public:
	virtual void DrawModeSettings() noexcept override;
	virtual void addAction(FunscriptAction action) noexcept override;
};

class RecordingImpl : public ScripingModeBaseImpl
{
private:
	float right_x = 0.f, right_y = 0.f;
	float left_x = 0.f, left_y = 0.f;
	float right_trigger = 0.f;
	float left_trigger = 0.f;

	float value = 0.f;

	int32_t ControllerDeadzone = 1750;
	int32_t currentPos = 0;
	bool controllerCenter = true;
	bool automaticRecording = true;
	bool inverted = false;

	bool autoBackupTmp = false;
	float epsilon = 0.f;

	bool recordingActive = false;
	bool recordingJustStopped = false;
	bool recordingJustStarted = false;
public:
	// Attention: don't change order
	enum RecordingMode : int32_t {
		Mouse,
		Controller,
	};
	RecordingMode activeMode = RecordingMode::Mouse;
	RecordingImpl();
	~RecordingImpl();

	void ControllerAxisMotion(SDL_Event& ev);
	void setRecordingMode(RecordingMode mode) noexcept { activeMode = mode; }
	virtual void DrawModeSettings() noexcept override;
	virtual void addAction(FunscriptAction action) noexcept override;
	virtual void update() noexcept override;
};

class OpenFunscripter;
class ScriptingMode {
	std::unique_ptr<ScripingModeBaseImpl> impl;
	ScriptingModeEnum active_mode;
	ScriptingOverlayModes active_overlay;
public:
	ScriptingModeEnum mode() const { return active_mode; }
	ScripingModeBaseImpl& Impl() { return *impl.get(); }

	static constexpr const char* ScriptingModeId = "Mode";
	void setup();
	void DrawScriptingMode(bool* open) noexcept;
	void setMode(ScriptingModeEnum mode) noexcept;
	void setOverlay(ScriptingOverlayModes mode) noexcept;
	void addEditAction(FunscriptAction action) noexcept;
	inline void addAction(FunscriptAction action) noexcept { impl->addAction(action); }
	void NextFrame() noexcept;
	void PreviousFrame() noexcept;
	void update() noexcept;

	inline const std::unique_ptr<ScripingModeBaseImpl>& CurrentImpl() const { return impl; }
};