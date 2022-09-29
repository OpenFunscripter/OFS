#pragma once

#include "Funscript.h"
#include "FunscriptAction.h"
#include "ScriptPositionsOverlayMode.h"
#include "OFS_ScriptPositionsOverlays.h"

#include "SDL_events.h"

#include <memory>
#include <array>

// ATTENTION: no reordering
enum ScriptingModeEnum : int32_t {
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

	virtual void undo() noexcept {};
	virtual void redo() noexcept {};

	virtual void finish() noexcept {}; // called when the mode changes
	virtual void update() noexcept {}; // called everyframe
};

class DefaultModeImpl : public ScripingModeBaseImpl
{
public:
	virtual void DrawModeSettings() noexcept override {}
};

class DynamicInjectionImpl : public ScripingModeBaseImpl
{
protected:
	static constexpr float MaxSpeed = 500.f;
	static constexpr float MinSpeed = 50.f;
	float targetSpeed = 300;
	float directionBias = 0.f;
	int topBottomDirection = 1; // 1 for top and -1 for bottom injection
public:
	virtual void DrawModeSettings() noexcept override;
	virtual void addEditAction(FunscriptAction action) noexcept override;
};

class AlternatingImpl : public ScripingModeBaseImpl
{
	int32_t fixedBottom = 0;
	int32_t fixedTop = 100;
	bool contextSensitive = false;
	bool fixedRangeEnabled = false;
	bool nextPosition = false;
public:
	virtual void DrawModeSettings() noexcept override;
	virtual void addEditAction(FunscriptAction action) noexcept override;

	virtual void undo() noexcept override;
	virtual void redo() noexcept override;
};

class RecordingImpl : public ScripingModeBaseImpl
{
private:
	float rightX = 0.f, rightY = 0.f;
	float leftX = 0.f, leftY = 0.f;
	float rightTrigger = 0.f;
	float leftTrigger = 0.f;

	float valueX = 0.f;
	float valueY = 0.f;

	int32_t ControllerDeadzone = 1750;
	int32_t currentPosX = 0;
	int32_t currentPosY = 0;
	bool controllerCenter = true;
	bool automaticRecording = true;
	bool inverted = false;

	bool twoAxesMode = false;


	bool recordingActive = false;
	std::shared_ptr<Funscript> recordingAxisX;
	std::shared_ptr<Funscript> recordingAxisY;

	void singleAxisRecording() noexcept;
	void twoAxisRecording() noexcept;
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
	virtual void update() noexcept override;
	virtual void finish() noexcept override;
};

class OpenFunscripter;
class ScriptingMode {
	std::array<std::unique_ptr<ScripingModeBaseImpl>, ScriptingModeEnum::COUNT> modes;
	ScripingModeBaseImpl* impl = nullptr;
	ScriptingModeEnum activeMode;
	ScriptingOverlayModes activeOverlay;
public:
	inline ScriptingModeEnum mode() const { return activeMode; }
	inline ScripingModeBaseImpl& Impl() { return *impl; }

	static constexpr const char* WindowId = "###SCRIPTING_MODE";
	void setup();
	void DrawScriptingMode(bool* open) noexcept;
	void setMode(ScriptingModeEnum mode) noexcept;
	void setOverlay(ScriptingOverlayModes mode) noexcept;
	
	void undo() noexcept;
	void redo() noexcept;
	void addEditAction(FunscriptAction action) noexcept;
	void NextFrame() noexcept;
	void PreviousFrame() noexcept;
	void update() noexcept;
};