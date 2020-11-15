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

	glm::mat4 projection;
	glm::mat4 view;

	glm::mat4 translation;
	glm::mat4 boxModel;
	glm::mat4 containerModel;
public:
	void setup() noexcept;

	void ShowWindow(bool* open) noexcept;
	void render() noexcept;
};