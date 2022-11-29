#pragma once
#include <cstdint>
#include <string>
#include <memory>

#include "OFS_Videoplayer.h"

class VideoPreview {
private:
	std::unique_ptr<OFS_Videoplayer> player;
public:
	VideoPreview(bool hwAccel) noexcept;
	~VideoPreview() noexcept;

	void Init() noexcept;
	void Update(float delta) noexcept;

	void SetPosition(float pos) noexcept;
	void PreviewVideo(const std::string& path, float pos) noexcept;
	void Play() noexcept;
	void Pause() noexcept;
	void CloseVideo() noexcept;

	inline uint32_t FrameTex() const noexcept { return player->FrameTexture(); }
};