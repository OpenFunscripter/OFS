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

class ScriptingModeBase 
{
protected:
	Funscript& ctx() noexcept;
public:
	ScriptingModeBase() noexcept;
	virtual ~ScriptingModeBase() noexcept {}
	virtual void DrawModeSettings() noexcept = 0;
	virtual void AddEditAction(FunscriptAction action) noexcept;

	virtual void Undo() noexcept {};
	virtual void Redo() noexcept {};

	virtual void Update() noexcept {}; // called everyframe
	virtual void Finish() noexcept {}; // called when the mode changes
};

class DefaultMode : public ScriptingModeBase
{
public:
	virtual void DrawModeSettings() noexcept override {}
};

class DynamicInjectionMode : public ScriptingModeBase
{
protected:
	static constexpr float MaxSpeed = 500.f;
	static constexpr float MinSpeed = 50.f;
	float targetSpeed = 300;
	float directionBias = 0.f;
	int topBottomDirection = 1; // 1 for top and -1 for bottom injection
public:
	virtual void DrawModeSettings() noexcept override;
	virtual void AddEditAction(FunscriptAction action) noexcept override;
};

class AlternatingMode : public ScriptingModeBase
{
	int32_t fixedBottom = 0;
	int32_t fixedTop = 100;
	bool contextSensitive = false;
	bool fixedRangeEnabled = false;
	bool nextPosition = false;
public:
	virtual void DrawModeSettings() noexcept override;
	virtual void AddEditAction(FunscriptAction action) noexcept override;

	virtual void Undo() noexcept override;
	virtual void Redo() noexcept override;
};

class RecordingMode : public ScriptingModeBase
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
	enum RecordingType : int32_t {
		Mouse,
		Controller,
	};
	RecordingType activeType = RecordingType::Mouse;
	RecordingMode() noexcept;
	~RecordingMode() noexcept;

	void ControllerAxisMotion(SDL_Event& ev) noexcept;
	void setRecordingMode(RecordingType type) noexcept { activeType = type; }
	virtual void DrawModeSettings() noexcept override;
	virtual void Update() noexcept override;
	virtual void Finish() noexcept override;
};

class ScriptingMode {
private:
	ScriptingModeEnum activeMode;
	ScriptingOverlayModes activeOverlay;

	std::array<std::unique_ptr<ScriptingModeBase>, ScriptingModeEnum::COUNT> modes;
	std::unique_ptr<BaseOverlay> overlayImpl = nullptr;
public:
	inline ScriptingModeEnum ActiveMode() const noexcept { return activeMode; }
	inline std::unique_ptr<BaseOverlay>& Overlay() noexcept { return overlayImpl; }
	inline std::unique_ptr<ScriptingModeBase>& Mode() noexcept { 
		FUN_ASSERT(activeMode >= ScriptingModeEnum::DEFAULT_MODE && activeMode < ScriptingModeEnum::COUNT, "invalid mode");
		return modes[static_cast<int>(activeMode)]; 
	}

	static constexpr const char* WindowId = "###SCRIPTING_MODE";
	void Init() noexcept;
	void DrawScriptingMode(bool* open) noexcept;
	void DrawOverlaySettings() noexcept;

	void SetMode(ScriptingModeEnum mode) noexcept;
	void SetOverlay(ScriptingOverlayModes mode) noexcept;
	
	void Undo() noexcept;
	void Redo() noexcept;
	void AddEditAction(FunscriptAction action) noexcept;
	void NextFrame() noexcept;
	void PreviousFrame() noexcept;

	float LogicalFrameTime() noexcept; // may not be the actual frame time
	float SteppingIntervalForward(float fromTime) noexcept;
	float SteppingIntervalBackward(float fromTime) noexcept;

	void Update() noexcept;
};