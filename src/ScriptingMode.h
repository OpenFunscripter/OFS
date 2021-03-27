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
	COUNT
};

class ScripingModeBaseImpl 
{
protected:
	Funscript& ctx();
public:
	ScripingModeBaseImpl();
	virtual ~ScripingModeBaseImpl() {}
	virtual void DrawModeSettings() noexcept = 0;
	virtual void addEditAction(FunscriptAction action) noexcept;

	virtual void finish() noexcept {};
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
	virtual void addEditAction(FunscriptAction action) noexcept override;
};


class AlternatingImpl : public ScripingModeBaseImpl
{
	int32_t fixedBottom = 0;
	int32_t fixedTop = 100;
	bool fixedRangeEnabled = false;
	bool nextPosition = false;
public:
	virtual void DrawModeSettings() noexcept override;
	virtual void addEditAction(FunscriptAction action) noexcept override;
};

class RecordingImpl : public ScripingModeBaseImpl
{
private:
	float right_x = 0.f, right_y = 0.f;
	float left_x = 0.f, left_y = 0.f;
	float right_trigger = 0.f;
	float left_trigger = 0.f;

	float valueX = 0.f;
	float valueY = 0.f;

	int32_t ControllerDeadzone = 1750;
	int32_t currentPosX = 0;
	int32_t currentPosY = 0;
	bool controllerCenter = true;
	bool automaticRecording = true;
	bool inverted = false;

	bool twoAxesMode = false;

	bool autoBackupTmp = false;
	float epsilon = 0.f;

	bool recordingActive = false;
	bool recordingJustStopped = false;
	bool recordingJustStarted = false;

	void singleAxisRecording() noexcept;
	void twoAxisRecording() noexcept;

	void finishSingleAxisRecording() noexcept;
	void finishTwoAxisRecording() noexcept;
public:
	// Attention: don't change order
	enum RecordingMode : int32_t {
		Mouse,
		Controller,
	};
	RecordingMode activeMode = RecordingMode::Controller;
	RecordingImpl();
	~RecordingImpl();

	void ControllerAxisMotion(SDL_Event& ev);
	void setRecordingMode(RecordingMode mode) noexcept { activeMode = mode; }
	virtual void DrawModeSettings() noexcept override;
	virtual void update() noexcept override;
	virtual void finish() noexcept override;
};

class OpenFunscripter;
class ScriptingMode {
	std::array<std::unique_ptr<ScripingModeBaseImpl>, ScriptingModeEnum::COUNT> modes;
	ScripingModeBaseImpl* impl = nullptr;
	ScriptingModeEnum active_mode;
	ScriptingOverlayModes active_overlay;
public:
	ScriptingModeEnum mode() const { return active_mode; }
	ScripingModeBaseImpl& Impl() { return *impl; }

	static constexpr const char* ScriptingModeId = "Mode";
	void setup();
	void DrawScriptingMode(bool* open) noexcept;
	void setMode(ScriptingModeEnum mode) noexcept;
	void setOverlay(ScriptingOverlayModes mode) noexcept;
	void addEditAction(FunscriptAction action) noexcept;
	void NextFrame() noexcept;
	void PreviousFrame() noexcept;
	void update() noexcept;
};