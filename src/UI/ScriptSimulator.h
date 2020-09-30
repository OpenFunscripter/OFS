#pragma once

#include "imgui.h"
class ScriptSimulator {
private:
	ImVec2 p1;
	ImVec2 p2;
	float width = 120.f;
	ImColor borderColor;
	ImColor frontColor;
	ImColor indicatorColor;
	float borderSize = 12.f;
	
	ImVec2 startDragP1;
	ImVec2 startDragP2;
	ImVec2* dragging = nullptr;
	bool movingBar = false;
	bool EnableIndicators = true;
	const bool ShowMovementHandle = false;
public:
	ScriptSimulator() {
		borderColor = IM_COL32(0x0B, 0x4F, 0x6C, 180);
		frontColor = IM_COL32(0x01, 0xBA, 0xEF, 180);
		indicatorColor = IM_COL32(0xFF, 0x4F, 0x6C, 220);
	}
	void setup();
	void CenterSimulator();
	void ShowSimulator(bool* open);
};