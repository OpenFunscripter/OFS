#pragma once
#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"
#include "OFS_Reflection.h"

#include "imgui.h"

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

class Simulator3D
{
private:
	enum IsEditing : uint8_t
	{
		No = 0,
		Mousewheel = 0x1,
		ClickDrag = 0x2
	};
	IsEditing Editing = IsEditing::No;
	int EditingIdx = -1;
	float EditingScrollMultiplier = 5.f;

	std::unique_ptr<class LightingShader> lightShader;

	unsigned int VBO = 0;
	unsigned int cubeVAO = 0;
	bool TranslateEnabled = false;

	static constexpr float MaxZoom = 10.f;
	float Zoom = 3.f;

	float rollRange = 60.f;
	float pitchRange = 90.f;
	float twistRange = 360.f;

    glm::mat4 projection;
	glm::mat4 view;

	glm::mat4 translation;
	glm::mat4 boxModel;
	glm::mat4 containerModel;

	glm::mat4 twistBox;

	glm::vec3 direction;
	glm::vec3 viewPos;
	glm::vec3 lightPos;

public:
	int32_t posIndex = 0;
	int32_t rollIndex = 1;
	int32_t pitchIndex = 2;
	int32_t twistIndex = 3;
private:

	ImColor boxColor = IM_COL32(245, 164, 66, (int)(0.8f * 255));
	ImColor containerColor = IM_COL32(66, 135, 245, (int)(0.6f * 255));
	ImColor twistBoxColor = IM_COL32(255, 0, 0, 150);

	float scriptPos = 0.f;
	float roll = 0.f;
	float pitch = 0.f;
	float yaw = 0.f;

	float globalYaw = 0.f;
	float globalPitch = 0.f;

	void reset() noexcept;

	void load(const std::string& path) noexcept;
	void save(const std::string& path) noexcept;
public:
	int32_t RollOverride = -1;
	int32_t PitchOverride = -1;

	~Simulator3D();
	void setup() noexcept;

	void ShowWindow(bool* open, int32_t currentMs, bool easing, std::vector<std::shared_ptr<class Funscript>>& scripts) noexcept;
	void renderSim() noexcept;


	template <class Archive>
	inline void reflect(Archive& ar) {
		OFS_REFLECT(translation, ar);
		OFS_REFLECT(Zoom, ar);
	}
};