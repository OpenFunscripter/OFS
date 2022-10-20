#pragma once

#include "imgui.h"
#include "OFS_Reflection.h"
#include "OFS_BinarySerialization.h"

class ScriptSimulator {
private:
	ImVec2 startDragP1;
	ImVec2 startDragP2;
	ImVec2* dragging = nullptr;
	float mouseValue;
	bool IsMovingSimulator = false;
	bool EnableVanilla = false;
	bool MouseOnSimulator = false;

	inline int32_t GetColor(const ImColor& col) const noexcept {
		auto color = ImGui::ColorConvertFloat4ToU32(col);
		// apply global opacity
		((uint8_t*)&color)[IM_COL32_A_SHIFT / 8] = ((uint8_t)(255 * col.Value.w * simulator.GlobalOpacity));
		return color;
	}
public:
	static constexpr const char* WindowId = "###SIMULATOR";
	struct SimulatorSettings {
		ImVec2 P1 = {600.f, 300.f};
		ImVec2 P2 = {600.f, 700.f};
		ImColor Text = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
		ImColor Front = IM_COL32(0x01, 0xBA, 0xEF, 0xFF);
		ImColor Back = IM_COL32(0x10, 0x10, 0x10, 0xBF);
		ImColor Border = IM_COL32(0x0B, 0x4F, 0x6C, 0xFF);
		ImColor ExtraLines = IM_COL32(0x0B, 0x4F, 0x6C, 0xFF);
		ImColor Indicator = IM_COL32(0xFF, 0x4F, 0x6C, 0xFF);
		float Width = 120.f;
		float BorderWidth = 8.f;
		float ExtraLineWidth = 4.f;
		float LineWidth = 4.f;
		float GlobalOpacity = 0.75f;

		int32_t ExtraLinesCount = 0;

		bool EnableIndicators = true;
		bool EnablePosition = false;
		bool EnableHeightLines = true;
		bool LockedPosition = false;

		template<typename S>
		void serialize(S& s) {
			s.ext(*this, bitsery::ext::Growable{},
				[](S& s, SimulatorSettings& o) {
					// prevents defaults from being overwritten with 0
					bool HackForSerialize = true;
					s.boolValue(HackForSerialize);
					if (!HackForSerialize) return;

					s.object(o.P1);
					s.object(o.P2);
					s.value4b(o.Width);
					s.value4b(o.BorderWidth);
					s.value4b(o.LineWidth);
					s.value4b(o.ExtraLineWidth);
					s.object(o.Text);
					s.object(o.Front);
					s.object(o.Back);
					s.object(o.Border);
					s.object(o.ExtraLines);
					s.object(o.Indicator);
					s.value4b(o.GlobalOpacity);
					s.boolValue(o.EnableIndicators);
					s.boolValue(o.EnablePosition);
					s.boolValue(o.EnableHeightLines);
					s.boolValue(o.LockedPosition);
					s.value4b(o.ExtraLinesCount);
				});
		}

	} simulator;

	float positionOverride = -1.f;

	void MouseMovement(union SDL_Event& ev);
	void MouseDown(union SDL_Event& ev);

	inline float getMouseValue() const { return mouseValue; }

	void setup();
	void CenterSimulator();
	void ShowSimulator(bool* open);
};

REFL_TYPE(ScriptSimulator::SimulatorSettings)
	REFL_FIELD(P1)
	REFL_FIELD(P2)
	REFL_FIELD(Width)
	REFL_FIELD(BorderWidth)
	REFL_FIELD(LineWidth)
	REFL_FIELD(ExtraLineWidth)
	REFL_FIELD(Text)
	REFL_FIELD(Front)
	REFL_FIELD(Back)
	REFL_FIELD(Border)
	REFL_FIELD(ExtraLines)
	REFL_FIELD(Indicator)
	REFL_FIELD(GlobalOpacity)
	REFL_FIELD(EnableIndicators)
	REFL_FIELD(EnablePosition)
	REFL_FIELD(EnableHeightLines)
	REFL_FIELD(ExtraLinesCount)
	REFL_FIELD(LockedPosition)
REFL_END