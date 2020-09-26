#pragma once

#include "imgui.h"
class ScriptSimulator {
private:
	ImVec2 p1;
	ImVec2 p2;
	float width = 100.f;
public:
	ScriptSimulator()
		: p1(000, 0), p2(100, 300) {}

	void ShowSimulator(bool* open);
};