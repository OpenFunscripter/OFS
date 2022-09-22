#pragma once
#include "ScriptPositionsOverlayMode.h"
#include "OFS_Localization.h"
#include <cstdint>


// ATTENTION: no reordering
enum ScriptingOverlayModes : int32_t {
	FRAME,
	TEMPO,
	EMPTY,
};


class TempoOverlay : public BaseOverlay {
private:
	static constexpr std::array<float, 10> beatMultiples{
		4.f * 1.f,
		4.f * (1.f / 2.f),
		4.f * (1.f / 4.f),
		4.f * (1.f / 8.f),
		4.f * (1.f / 12.f),
		4.f * (1.f / 16.f),
		4.f * (1.f / 24.f),
		4.f * (1.f / 32.f),
		4.f * (1.f / 48.f),
		4.f * (1.f / 64.f),
	};
	static constexpr std::array<uint32_t, 10> beatMultipleColor{
		IM_COL32(0xbb, 0xbe, 0xbc, 0xFF), // 1st ???

		IM_COL32(0x53, 0xd3, 0xdf, 0xFF), // 2nds
		IM_COL32(0xc1, 0x65, 0x77, 0xFF), // 4ths
		IM_COL32(0x24, 0x54, 0x99, 0xFF), // 8ths
		IM_COL32(0xc8, 0x86, 0xee, 0xFF), // 12ths
		IM_COL32(0xd2, 0xcc, 0x23, 0xFF), // 16ths
		IM_COL32(0xea, 0x8d, 0xe0, 0xFF), // 24ths
		IM_COL32(0xe7, 0x97, 0x5c, 0xFF), // 32nds
		IM_COL32(0xeb, 0x38, 0x99, 0xFF), // 48ths
		IM_COL32(0x23, 0xd2, 0x54, 0xFF), // 64ths
	};
	static constexpr std::array<Tr, 10> beatMultiplesStrings{
		Tr::TEMPO_WHOLE_MEASURES,
		Tr::TEMPO_2ND_MEASURES,
		Tr::TEMPO_4TH_MEASURES,
		Tr::TEMPO_8TH_MEASURES,
		Tr::TEMPO_12TH_MEASURES,
		Tr::TEMPO_16TH_MEASURES,
		Tr::TEMPO_24TH_MEASURES,
		Tr::TEMPO_32ND_MEASURES,
		Tr::TEMPO_48TH_MEASURES,
		Tr::TEMPO_64TH_MEASURES,
	};
public:
	TempoOverlay(class ScriptTimeline* timeline)
		: BaseOverlay(timeline) {}
	virtual void DrawSettings() noexcept override;
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
	virtual void nextFrame() noexcept override;
	virtual void previousFrame() noexcept override;

	virtual float steppingIntervalForward(float fromMs) noexcept override;
	virtual float steppingIntervalBackward(float fromMs) noexcept override;
};


class FrameOverlay : public BaseOverlay {
private:
	float framerateOverride = 0.f;
	bool enableFramerateOverride = false;
public:
	FrameOverlay(class ScriptTimeline* timeline)
		: BaseOverlay(timeline) {}
	virtual void DrawScriptPositionContent(const OverlayDrawingCtx& ctx) noexcept override;
	virtual void DrawSettings() noexcept override;
	virtual void nextFrame() noexcept override;
	virtual void previousFrame() noexcept override;

	virtual float steppingIntervalForward(float fromMs) noexcept override;
	virtual float steppingIntervalBackward(float fromMs) noexcept override;
};