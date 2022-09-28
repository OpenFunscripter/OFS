#pragma once

#include <cstdint>
#include <string>

class OFS_Videoplayer
{
    private:
    void* ctx = nullptr;
    float lastVolume = 0.f;

    public:
    OFS_Videoplayer() noexcept;
    ~OFS_Videoplayer() noexcept;

    enum class LoopEnum : int8_t
	{
		A_set,
		B_set,
		Clear,
	};
	private: LoopEnum LoopState = LoopEnum::Clear;
    public:

	static constexpr float MinPlaybackSpeed = 0.05f;
	static constexpr float MaxPlaybackSpeed = 3.0f;

    bool Init(bool hwAccel) noexcept;

    void OpenVideo(const std::string& path) noexcept;
    void SetSpeed(float speed) noexcept;
	void AddSpeed(float speed) noexcept;
    void SetVolume(float volume) noexcept;
    void SetPositionExact(float timeSeconds, bool pausesVideo = false) noexcept;
    void SetPositionPercent(float percentPos, bool pausesVideo = false) noexcept;
    void SeekRelative(float timeSeconds) noexcept;
    void SeekFrames(int32_t offset) noexcept;
    void SetPaused(bool paused) noexcept;
    void TogglePlay() noexcept;
    void CycleSubtitles() noexcept;
    void CycleLoopAB() noexcept;
    void ClearLoop() noexcept;
    void CloseVideo() noexcept;
    void NotifySwap() noexcept;
    void SaveFrameToImage(const std::string& directory) noexcept;

    void Mute() noexcept {
        lastVolume = Volume();
        SetVolume(0.f);
    }

    void UnMute() noexcept {
        SetVolume(lastVolume);
    }

    void Update(float delta) noexcept;

    uint16_t VideoWidth() const noexcept;
    uint16_t VideoHeight() const noexcept;
    float FrameTime() const noexcept;
    float CurrentSpeed() const noexcept;
    float Volume() const noexcept;
    double Duration() const noexcept;
    bool IsPaused() const noexcept;
    float Fps() const noexcept;
    bool VideoLoaded() const noexcept;
    void NextFrame() noexcept;
    void PreviousFrame() noexcept;
    float CurrentPercentPosition() const noexcept;
    double CurrentTime() const noexcept;
    double CurrentTimeInterp() const noexcept;
    uint32_t FrameTexture() const noexcept;
    const char* VideoPath() const noexcept;
};