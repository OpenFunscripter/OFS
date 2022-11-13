#include "OFS_Shader.h"
#include "OFS_Util.h"

#include "OFS_GL.h"

ShaderBase::ShaderBase(const char* vtxShader, const char* fragShader) noexcept
{
	unsigned int vertex, fragment;
	int success;
	char infoLog[512];
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vtxShader, NULL);
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
	glShaderSource(fragment, 1, &fragShader, NULL);
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

ShaderBase::~ShaderBase() noexcept
{
	LOG_WARN("Shader destructor called. Might be a resource leak.");
}

void ShaderBase::Use() noexcept
{
	glUseProgram(program);
}

void VrShader::InitUniformLocations() noexcept
{
	ProjMtxLoc = glGetUniformLocation(program, "ProjMtx");
	RotationLoc = glGetUniformLocation(program, "rotation");
	ZoomLoc = glGetUniformLocation(program, "zoom");
	VideoAspectLoc = glGetUniformLocation(program, "video_aspect_ratio");
	AspectLoc = glGetUniformLocation(program, "aspect_ratio");
}

void VrShader::ProjMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(ProjMtxLoc, 1, GL_FALSE, mat4);
}

void VrShader::Rotation(const float* vec2) noexcept
{
	glUniform2fv(RotationLoc, 1, vec2);
}

void VrShader::Zoom(float zoom) noexcept
{
	glUniform1f(ZoomLoc, zoom);
}

void VrShader::VideoAspectRatio(float aspect) noexcept {
	glUniform1f(VideoAspectLoc, aspect);
}

void VrShader::AspectRatio(float aspect) noexcept
{
	glUniform1f(AspectLoc, aspect);
}

void LightingShader::initUniformLocations() noexcept
{
	ModelLoc = glGetUniformLocation(program, "model");
	ProjectionLoc = glGetUniformLocation(program, "projection");
	ViewLoc = glGetUniformLocation(program, "view");
	ObjectColorLoc = glGetUniformLocation(program, "objectColor");
	LightPosLoc = glGetUniformLocation(program, "lightPos");
	ViewPosLoc = glGetUniformLocation(program, "viewPos");

	float lcol[3]{ 1.f, 1.f, 1.f };
	float vpos[3]{ 0.f, 0.f, 0.f };

	glUniform3fv(ViewPosLoc, 1, vpos);
	glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, lcol);
}

void LightingShader::ModelMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(ModelLoc, 1, GL_FALSE, mat4);
}

void LightingShader::ProjectionMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(ProjectionLoc, 1, GL_FALSE, mat4);
}

void LightingShader::ViewMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(ViewLoc, 1, GL_FALSE, mat4);
}

void LightingShader::ObjectColor(const float* vec4) noexcept
{
	glUniform4fv(ObjectColorLoc, 1, vec4);
}

void LightingShader::LightPos(const float* vec3) noexcept
{
	glUniform3fv(LightPosLoc, 1, vec3);
}

void LightingShader::ViewPos(const float* vec3) noexcept {
	glUniform3fv(ViewPosLoc, 1, vec3);
}

void WaveformShader::initUniformLocations() noexcept
{
	ProjMtxLoc = glGetUniformLocation(program, "ProjMtx");
	AudioLoc = glGetUniformLocation(program, "audio");
	AudioScaleLoc = glGetUniformLocation(program, "scaleAudio");
	AudioSamplingOffset = glGetUniformLocation(program, "SamplingOffset");
	ColorLoc = glGetUniformLocation(program, "Color");
}

void WaveformShader::ProjMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(ProjMtxLoc, 1, GL_FALSE, mat4);
}

void WaveformShader::AudioData(uint32_t unit) noexcept
{
	glUniform1i(AudioLoc, unit);
}

void WaveformShader::SampleOffset(float offset) noexcept
{
	glUniform1f(AudioSamplingOffset, offset);
}

void WaveformShader::ScaleFactor(float scale) noexcept
{
	glUniform1f(AudioScaleLoc, scale);
}

void WaveformShader::Color(float* vec3) noexcept
{
	glUniform3fv(ColorLoc, 1, vec3);
}
