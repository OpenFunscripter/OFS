#include "OFS_Waveform.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

static bool LoadFlac(OFS_Waveform* waveform, const std::string& output) noexcept
{
	drflac* flac = drflac_open_file(output.c_str(), NULL);
	if (!flac) return false;

	constexpr float LowRangeMin = 0.f; constexpr float LowRangeMax = 500.f;
	constexpr float MidRangeMin = 501.f; constexpr float MidRangeMax = 6000.f;
	constexpr float HighRangeMin = 6001.f; constexpr float HighRangeMax = 20000.f;

	std::vector<drflac_int16> ChunkSamples; ChunkSamples.resize(48000);
	constexpr int SamplesPerLine = 40; 

	uint32_t sampleCount = 0;
	float lowPeak = 0.f;
	float midPeak = 0.f;
	float highPeak = 0.f;
	while ((sampleCount = drflac_read_pcm_frames_s16(flac, ChunkSamples.size(), ChunkSamples.data())) > 0) {
		for (int sampleIdx = 0; sampleIdx < sampleCount; sampleIdx += SamplesPerLine) {
			int samplesInThisLine = std::min(SamplesPerLine, (int)sampleCount - sampleIdx);
			for (int i = 0; i < samplesInThisLine; i++) {
				auto sample = ChunkSamples[sampleIdx + i];
				if (sample == 0) continue;
				auto floatSample = sample / 32768.f;

				if (sample <= LowRangeMax) {
					// low range
					lowPeak += floatSample;
				}
				if (sample <= MidRangeMax) {
					// mid range
					midPeak += floatSample;
				}
				if (sample <= HighRangeMax) {
					// high range
					highPeak += floatSample;
				}
			}
			lowPeak /= (float)SamplesPerLine;
			midPeak /= (float)SamplesPerLine;
			highPeak /= (float)SamplesPerLine;

			waveform->SamplesLow.emplace_back(lowPeak);
			waveform->SamplesMid.emplace_back(midPeak);
			waveform->SamplesHigh.emplace_back(highPeak);
		}
	}
	drflac_close(flac);

	waveform->SamplesLow.shrink_to_fit();
	waveform->SamplesMid.shrink_to_fit();
	waveform->SamplesHigh.shrink_to_fit();
	
	auto mapSamples = [](std::vector<float>& samples, float min, float max) noexcept
	{
		if (samples.size() <= 1) return;
		if (min != 0.f || max != 1.f) {
			for (auto& val : samples) {
				val = Util::MapRange<float>(val, min, max, 0.f, 1.f);
			}
		}
	};
	
	//auto [lowMin, lowMax] = std::minmax_element(waveform->SamplesLow.begin(), waveform->SamplesLow.end());
	//auto [midMin, midMax] = std::minmax_element(waveform->SamplesMid.begin(), waveform->SamplesMid.end());
	//auto [highMin, highMax] = std::minmax_element(waveform->SamplesHigh.begin(), waveform->SamplesHigh.end());
	//float min = std::min(*lowMin, *midMin);	min = std::min(min, *highMin);
	//float max = std::max(*lowMax, *midMax);	max = std::max(max, *highMax);
	//mapSamples(waveform->SamplesLow, min, max);
	//mapSamples(waveform->SamplesMid, min, max);
	//mapSamples(waveform->SamplesHigh, min, max);

	return true;
}

bool OFS_Waveform::GenerateAndLoadFlac(const std::string& ffmpegPath, const std::string& videoPath, const std::string& output) noexcept
{
	generating = true;
	reproc::options options;
	options.redirect.parent = true;
	std::array<const char*, 9> args =
	{
		ffmpegPath.c_str(),
		"-y",
		"-i", videoPath.c_str(),
		"-vn",
		"-ac", "1",
		output.c_str(),
		nullptr
	};
	auto [status, ec] = reproc::run(args.data(), options);
	if (status != 0) { generating = false; return false; }

	if (!LoadFlac(this, output)) {
		generating = false;
		return false;
	}

	generating = false;
	return true;
}
