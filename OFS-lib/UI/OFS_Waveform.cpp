#include "OFS_Waveform.h"
#include "OFS_Util.h"

//#define MINIMP3_ONLY_SIMD
//#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

bool OFS_Waveform::LoadMP3(const std::string& path) noexcept
{
	static mp3dec_t mp3d;
	mp3dec_file_info_t info;

	SamplesLow.clear();
	SamplesMid.clear();
	SamplesHigh.clear();

	mp3dec_init(&mp3d);

	mp3dec_iterate(path.c_str(),
		[](void* user_data, const uint8_t* frame,
			int frame_size, int free_format_bytes,
			size_t buf_size, uint64_t offset,
			mp3dec_frame_info_t* info) -> int 
		{
			constexpr float LowRangeMin = 0.f; constexpr float LowRangeMax = 500.f;
			constexpr float MidRangeMin = 501.f; constexpr float MidRangeMax = 6000.f;
			constexpr float HighRangeMin = 6001.f; constexpr float HighRangeMax = 20000.f;
			OFS_Waveform* ctx = (OFS_Waveform*)user_data;
			float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
			auto samples = mp3dec_decode_frame(&mp3d, frame, buf_size, pcm, info);

			FUN_ASSERT(samples <= 1152, "got more samples than expected");
			// 1152 is the sample count per frame
			constexpr int SamplesPerLine = 1152/24;

			auto floatToInt = [](float pcmVal) -> int32_t
			{
				pcmVal = pcmVal * 32768;
				//if (pcmVal > 32767) pcmVal = 32767;
				//if (pcmVal < -32768) pcmVal = -32768;
				return pcmVal;
			};

			for(int sampleIdx=0; sampleIdx < samples; sampleIdx += SamplesPerLine)
			{
				float lowPeak = 0.f, midPeak = 0.f, highPeak = 0.f;
				for (int i = 0; i < SamplesPerLine; i++) { 
					float sample = pcm[sampleIdx + i];
					//if (sample < 0.f) continue;
					sample = std::abs(sample);

					if (floatToInt(sample) <= LowRangeMax) {
						// low range
						//lowPeak = std::max(lowPeak, sample);
						lowPeak += sample;
					}
					else if (floatToInt(sample) <= MidRangeMax) {
						// mid range
						//midPeak = std::max(midPeak, sample);
						midPeak += sample;
					}
					else if (floatToInt(sample) <= HighRangeMax) {
						// high range
						//highPeak = std::max(highPeak, sample);
						highPeak += sample;
					}
				}
				lowPeak /= (float)SamplesPerLine;
				midPeak /= (float)SamplesPerLine;
				highPeak /= (float)SamplesPerLine;

				ctx->SamplesLow.push_back(lowPeak);
				ctx->SamplesMid.push_back(midPeak);
				ctx->SamplesHigh.push_back(highPeak);
			}

			return 0;
	}, this);

	SamplesLow.shrink_to_fit();
	SamplesMid.shrink_to_fit();
	SamplesHigh.shrink_to_fit();

	auto mapSamples = [](std::vector<float>& samples, float min, float max) noexcept
	{
		if (samples.size() <= 1) return;
		if (min != 0.f || max != 1.f) {
			for (auto& val : samples) {
				val = Util::MapRange<float>(val, min, max, 0.f, 1.f);
			}
		}
	};
	
	auto [lowMin, lowMax] = std::minmax_element(SamplesLow.begin(), SamplesLow.end());
	LowMax = *lowMax;
	auto [midMin, midMax] = std::minmax_element(SamplesMid.begin(), SamplesMid.end());
	MidMax = *midMax;

	auto [highMin, highMax] = std::minmax_element(SamplesHigh.begin(), SamplesHigh.end());
	float min = std::min(*lowMin, *midMin);	min = std::min(min, *highMin);
	float max = std::max(*lowMax, *midMax);	max = std::max(max, *highMax);
	mapSamples(SamplesLow, min, max);
	mapSamples(SamplesMid, min, max);
	mapSamples(SamplesHigh, min, max);

	LowMax = Util::MapRange(LowMax, min, max, 0.f, 1.f);
	MidMax = Util::MapRange(MidMax, min, max, 0.f, 1.f);

	return true;
}

bool OFS_Waveform::GenerateMP3(const std::string& ffmpegPath, const std::string& videoPath, const std::string& output) noexcept
{
	generating = true;
	reproc::options options;
	options.redirect.parent = true;
	std::array<const char*, 10> args =
	{
		ffmpegPath.c_str(),
		"-y",
		"-i", videoPath.c_str(),
		"-b:a", "320k",
		"-ac", "1",
		output.c_str(),
		nullptr
	};
	auto[status, ec] = reproc::run(args.data(), options);

	LOGF_WARN("OFS_Waveform::GenerateMP3: %s", ec.message().c_str());
	generating = false;
	return status == 0;
}
