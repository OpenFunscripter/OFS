#pragma once

#include <cstdint>
#include <string>

class OFS_Videoplayer
{
    private:
    // Implementation data
    void* ctx = nullptr;
    // A OpenGL 2D_TEXTURE expected to contain the current video frame.
    uint32_t frameTexture = 0;
    // The position which was last requested via any of the seeking functions.
    float logicalPosition = 0.f;
    // Helper for Mute/Unmute
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
    
    // All seeking functions must update logicalPosition
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
    void SaveFrameToImage(const std::string& directory) noexcept;
    void NotifySwap() noexcept;

    inline void Mute() noexcept {
        lastVolume = Volume();
        SetVolume(0.f);
    }
    inline void Unmute() noexcept {
        SetVolume(lastVolume);
    }
    inline void SyncWithPlayerTime() noexcept { SetPositionExact(CurrentPlayerTime()); }
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
    
    inline float CurrentPercentPosition() const noexcept { return logicalPosition; }
    inline double CurrentTime() const noexcept { return CurrentPercentPosition() * Duration(); }
    double CurrentTimeInterp() const noexcept; // interpolated time
    double CurrentPlayerPosition() const noexcept; // the "actual" position reported by the player
    double CurrentPlayerTime() const noexcept { return CurrentPlayerPosition() * Duration(); }

    const char* VideoPath() const noexcept;
    inline uint32_t FrameTexture() const noexcept { return frameTexture; }
};