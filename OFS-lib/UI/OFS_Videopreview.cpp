#include "OFS_Videopreview.h"

#include "OFS_Profiling.h"

VideoPreview::VideoPreview(const char* playerName, bool hwAccel) noexcept
{
	player = std::make_unique<OFS_Videoplayer>(playerName);
	player->Init(hwAccel);
}

VideoPreview::~VideoPreview() noexcept
{
}

void VideoPreview::Init() noexcept
{
	player->SetVolume(0.f);
}

void VideoPreview::Update(float delta) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	player->Update(delta);
}

void VideoPreview::SetPosition(float pos) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	player->SetPositionPercent(pos);
}

void VideoPreview::PreviewVideo(const std::string& path, float pos) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	player->OpenVideo(path);
	player->SetVolume(0.f);
}

void VideoPreview::Play() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	player->SetPaused(false);
}

void VideoPreview::Pause() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	player->SetPaused(true);
}

void VideoPreview::CloseVideo() noexcept
{
	player->CloseVideo();
}
