#pragma once

#include "OFS_Shader.h"
#include <memory>

class Simulator3D
{
	std::unique_ptr<LightingShader> lightShader;

	unsigned int VBO = 0;
	unsigned int cubeVAO = 0;
public:
	void setup() noexcept;
	void render() noexcept;
};