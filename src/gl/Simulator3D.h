#pragma once

#include "OFS_Shader.h"
#include <memory>

#include "imgui.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


class Simulator3D
{
private:
	std::unique_ptr<LightingShader> lightShader;

	unsigned int VBO = 0;
	unsigned int cubeVAO = 0;
	bool TranslateEnabled = false;

	static constexpr float MaxZoom = 10.f;
	float Zoom = 3.f;


	float rollRange = 60.f;
	float pitchRange = 90.f;
	float twistRange = 180.f;

    glm::mat4 projection;
	glm::mat4 view;

	glm::mat4 translation;
	glm::mat4 boxModel;
	glm::mat4 containerModel;

	glm::vec3 viewPos;
	glm::vec3 lightPos;

	int32_t posIndex = 0;
	int32_t rollIndex = 1;
	int32_t pitchIndex = 2;
	int32_t twistIndex = 3;
	
	ImColor boxColor = IM_COL32(245, 164, 66, (int)(0.8f * 255));
	ImColor containerColor = IM_COL32(66, 135, 245, (int)(0.6f * 255));

	void reset() noexcept;
public:
	void setup() noexcept;

	void ShowWindow(bool* open) noexcept;
	void render() noexcept;
};