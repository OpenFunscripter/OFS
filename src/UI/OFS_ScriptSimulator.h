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
	uint32_t stateHandle = 0xFFFF'FFFF;
	bool IsMovingSimulator = false;
	bool EnableVanilla = false;
	bool MouseOnSimulator = false;
public:
	static constexpr const char* WindowId = "###SIMULATOR";

	float positionOverride = -1.f;

	void MouseMovement(union SDL_Event& ev);
	void MouseDown(union SDL_Event& ev);

	inline float getMouseValue() const { return mouseValue; }

	void Init();
	void CenterSimulator();
	void ShowSimulator(bool* open, bool splineMode);
};

