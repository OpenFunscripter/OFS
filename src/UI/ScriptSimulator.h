#pragma once

#include "imgui.h"
class ScriptSimulator {
private:
	ImVec2 p1;
	ImVec2 p2;
	float width = 100.f;
	ImColor borderColor;
	ImColor frontColor;
	float borderSize = 15.f;
	ImVec2 startDrag;
	ImVec2* dragging = nullptr;

	const bool ShowMovementHandle = false;
public:
	ScriptSimulator() {
		borderColor = IM_COL32(0x0B, 0x4F, 0x6C, 0xFF);
		frontColor = IM_COL32(0x01, 0xBA, 0xEF, 0xFF);
	}
	void setup();
	void CenterSimulator();
	void ShowSimulator(bool* open);
};