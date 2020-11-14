#include "OFS_Shader.h"
#include "glad/glad.h"
#include "OpenFunscripterUtil.h"

ShaderBase::ShaderBase(const char* vtx_shader, const char* frag_shader)
{
	unsigned int vertex, fragment;
	int success;
	char infoLog[512];
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vtx_shader, NULL);
	glCompileShader(vertex);

	// print compile errors if any
	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertex, 512, NULL, infoLog);
		LOGF_ERROR("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s", infoLog);
	};

	// similiar for Fragment Shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &frag_shader, NULL);
	glCompileShader(fragment);

	// print compile errors if any
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragment, 512, NULL, infoLog);
		LOGF_ERROR("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s", infoLog);
	};

	// shader Program
	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	// print linking errors if any
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		LOGF_ERROR("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s", infoLog);
	}

	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "Texture"), GL_TEXTURE0);

	// delete the shaders as they're linked into our program now and no longer necessary
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void ShaderBase::use() noexcept
{
	glUseProgram(program);
}

void VrShader::ProjMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(glGetUniformLocation(program, "ProjMtx"), 1, GL_FALSE, mat4);
}

void VrShader::Rotation(const float* vec2) noexcept
{
	glUniform2fv(glGetUniformLocation(program, "rotation"), 1, vec2);
}

void VrShader::Zoom(float zoom) noexcept
{
	glUniform1f(glGetUniformLocation(program, "zoom"), zoom);
}

void VrShader::AspectRatio(float aspect) noexcept
{
	glUniform1f(glGetUniformLocation(program, "aspect_ratio"), aspect);
}
