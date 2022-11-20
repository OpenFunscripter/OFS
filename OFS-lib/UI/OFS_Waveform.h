#pragma once

#include <vector>
#include <string>
#include <memory>

#include "OFS_BinarySerialization.h"
#include "OFS_Shader.h"
#include "imgui.h"



// helper class to render audio waves
class OFS_Waveform
{
	bool generating = false;
	std::vector<float> samples;
public:

	inline bool BusyGenerating() noexcept { return generating; }
	bool GenerateAndLoadFlac(const std::string& ffmpegPath, const std::string& videoPath, const std::string& output) noexcept;
	bool LoadFlac(const std::string& path) noexcept;

	inline void Clear() noexcept {
		samples.clear();
	}

	inline void SetSamples(std::vector<float>&& samples) noexcept
	{
		this->samples = std::move(samples);
	}

	inline const std::vector<float>& Samples() const noexcept { return samples; }

	inline size_t SampleCount() const noexcept {
		return samples.size();
	}
};

struct OFS_WaveformLOD
{
	std::vector<float> WaveformLineBuffer;
	std::unique_ptr<WaveformShader> WaveShader;
	ImColor WaveformColor = IM_COL32(227, 66, 52, 255);
	uint32_t WaveformTex = 0;
	float samplingOffset = 0.f;

	float lastCanvasX = 0.f;
	float lastVisibleDuration = 0.f;
	
	int32_t lastMultiple = 0.f;
	OFS_Waveform data;

	void Init() noexcept;
	void Update(const class OverlayDrawingCtx& ctx) noexcept;
	void Upload() noexcept;
};