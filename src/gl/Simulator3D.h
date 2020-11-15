#pragma once

#include "OFS_Shader.h"
#include <memory>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


class Simulator3D
{
	std::unique_ptr<LightingShader> lightShader;

	unsigned int VBO = 0;
	unsigned int cubeVAO = 0;
	bool Enabled = true;
	bool TranslateEnabled = false;
	bool Gimbal = false;

	glm::mat4 projection;
	glm::mat4 view;

	glm::mat4 translation;
	glm::mat4 boxModel;
	glm::mat4 containerModel;

	glm::vec3 viewPos;

	int32_t posIndex = 0;
	int32_t rollIndex = 1;
	int32_t pitchIndex = 2;
	int32_t twistIndex = 3;
		
	void reset() noexcept;
public:
	void setup() noexcept;

	void ShowWindow(bool* open) noexcept;
	void render() noexcept;
};