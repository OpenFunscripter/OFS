#pragma once

#include "Funscript.h"
#include "FunscriptAction.h"

#include "SDL_events.h"

#include <memory>
#include <array>

// ATTENTION: no reordering
enum ScriptingModeEnum {
	DEFAULT_MODE,
	ALTERNATING,
	DYNAMIC_INJECTION,
	RECORDING,
	TEMPO,
};

class ScripingModeBaseImpl 
{
protected:
	Funscript& ctx();
public:
	ScripingModeBaseImpl();
	virtual ~ScripingModeBaseImpl() {}
	virtual void DrawModeSettings() noexcept = 0;
	virtual void DrawScriptPositionContent(ImDrawList* draw_list, float visibleSizeMs, float offset_ms, ImVec2 canvas_pos, ImVec2 canvas_size) noexcept;
	virtual void addAction(FunscriptAction action) noexcept {
		ctx().AddAction(action);
	}

	virtual void nextFrame() noexcept;
	virtual void previousFrame() noexcept;

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
	bool recordingActive = false;
	bool inverted = false;

	bool OpenRecordingsWindow = false;
	bool rollingBackupTmp = false;
	float epsilon = 0.f;
	Funscript::FunscriptRawData::Recording GeneratedRecording; // TODO: get rid of this?

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


class TempoImpl : public ScripingModeBaseImpl {
private:
	int bpm = 151;
	float beat_offset_seconds = 0.066f;
	int multiIDX = 1;
	static constexpr std::array<int32_t, 11> beatMultiples {
		1, //???

		2,
		4,
		8,
		12,
		16,
		24,
		32,
		48,
		64,
		192
	};
	static constexpr std::array<uint32_t, 11> beatMultipleColor {
		IM_COL32(0xbb, 0xbe, 0xbc, 0xFF), // 1st ???

		IM_COL32(0x53, 0xd3, 0xdf, 0xFF), // 2nds
		IM_COL32(0xc1, 0x65, 0x77, 0xFF), // 4ths
		IM_COL32(0x24, 0x54, 0x99, 0xFF), // 8ths
		IM_COL32(0xc8, 0x86, 0xee, 0xFF), // 12ths
		IM_COL32(0xd2, 0xcc, 0x23, 0xFF), // 16ths
		IM_COL32(0xea, 0x8d, 0xe0, 0xFF), // 24ths
		IM_COL32(0xe7, 0x97, 0x5c, 0xFF), // 32nds
		IM_COL32(0xeb, 0x38, 0x99, 0xFF), // 48ths
		IM_COL32(0x23, 0xd2, 0x54, 0xFF), // 64ths
		IM_COL32(0xbb, 0xbe, 0xbc, 0xFF), // 192ths
	};
	static constexpr std::array<const char*, 11> beatMultiplesStrings {
		"1st", //???

		"2nd",
		"4th",
		"8th",
		"12th",
		"16th",
		"24th",
		"32nd",
		"48th",
		"64th",
		"192nd"
	};
public:
	virtual void DrawScriptPositionContent(ImDrawList* draw_list, float visibleSizeMs, float offset_ms, ImVec2 canvas_pos, ImVec2 canvas_size) noexcept override;
	virtual void DrawModeSettings() noexcept override;
	virtual void nextFrame() noexcept override;
	virtual void previousFrame() noexcept override;
};

class OpenFunscripter;
class ScriptingMode {
	std::unique_ptr<ScripingModeBaseImpl> impl;
	ScriptingModeEnum active_mode;
public:
	ScriptingModeEnum mode() const { return active_mode; }
	ScripingModeBaseImpl& Impl() { return *impl.get(); }

	static constexpr const char* ScriptingModeId = "Mode";
	void setup();
	void DrawScriptingMode(bool* open);
	void setMode(ScriptingModeEnum mode);
	void addEditAction(FunscriptAction action) noexcept;
	inline void addAction(FunscriptAction action) noexcept { impl->addAction(action); }
	inline void DrawScriptPositionContent(ImDrawList* draw_list, float visibleSizeMs, float offset_ms, ImVec2 canvas_pos, ImVec2 canvas_size) noexcept { impl->DrawScriptPositionContent(draw_list, visibleSizeMs, offset_ms, canvas_pos, canvas_size); }
	inline void NextFrame() noexcept { impl->nextFrame(); }
	inline void PreviousFrame() noexcept { impl->previousFrame(); }
	inline void update() noexcept { impl->update(); }
};