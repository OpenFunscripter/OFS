#pragma once

#include "imgui.h"

#include "OFS_Reflection.h"

class ScriptSimulator {
private:
	ImVec2 startDragP1;
	ImVec2 startDragP2;
	ImVec2* dragging = nullptr;
	bool movingBar = false;
	bool EnableIndicators = true;
	bool EnableVanilla = false;
	const bool ShowMovementHandle = false;
public:
	struct SimulatorSettings {
		ImVec2 P1;
		ImVec2 P2;
		ImColor Text = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
		ImColor Front = IM_COL32(0x01, 0xBA, 0xEF, 180);
		ImColor Back = IM_COL32(0x10, 0x10, 0x10, 150);
		ImColor Border = IM_COL32(0x0B, 0x4F, 0x6C, 180);
		ImColor Indicator = IM_COL32(0xFF, 0x4F, 0x6C, 220);
		float Width = 120.f;
		float BorderWidth = 12.f;
		
		template <class Archive>
		inline void reflect(Archive& ar)
		{
			OFS_REFLECT(P1, ar);
			OFS_REFLECT(P2, ar);
			OFS_REFLECT(Width, ar);
			OFS_REFLECT(BorderWidth, ar);
			OFS_REFLECT(Text, ar);
			OFS_REFLECT(Front, ar);
			OFS_REFLECT(Back, ar);
			OFS_REFLECT(Border, ar);
			OFS_REFLECT(Indicator, ar);
		}
	} simulator;

	ScriptSimulator() {}
	void setup();
	void CenterSimulator();
	void ShowSimulator(bool* open);
};