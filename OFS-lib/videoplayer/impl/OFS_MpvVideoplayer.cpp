#include "OFS_Videoplayer.h"
#include "OFS_Util.h"

#include "OFS_VideoplayerEvents.h"
#include "EventSystem.h"

#define OFS_MPV_LOADER_MACROS
#include "OFS_MpvLoader.h"

#include "OFS_Localization.h"
#include "OFS_GL.h"

#include <sstream>

#include "SDL_atomic.h"

enum MpvPropertyGet : uint64_t {
    MpvDuration,
    MpvPosition,
    MpvTotalFrames,
    MpvSpeed,
    MpvVideoWidth,
    MpvVideoHeight,
    MpvPauseState,
    MpvFilePath,
    MpvHwDecoder,
    MpvFramesPerSecond,
    MpvAbLoopA,
    MpvAbLoopB,
};

struct MpvDataCache {
    double duration = 1.0;
    double percentPos = 0.0;
    double currentSpeed = 1.0;
    double fps = 30.0;
    double averageFrameTime = 1.0/fps;
    
    double abLoopA = 0;
    double abLoopB = 0;

    int64_t totalNumFrames = 0;
    int64_t paused = true;
    int64_t videoWidth = 0;
    int64_t videoHeight = 0;

    float currentVolume = .5f;

    bool videoLoaded = false;
    std::string filePath = "";
};

struct MpvPlayerContext
{
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpvGL = nullptr;
    uint32_t framebuffer = 0;
    MpvDataCache data = MpvDataCache();
    std::array<char, 32> tmpBuf;
    SDL_atomic_t renderUpdate = {0};
    SDL_atomic_t hasEvents = {0};

    uint32_t* frameTexture = nullptr;
    float* logicalPosition = nullptr;
    OFS_Videoplayer::LoopEnum* loopState = nullptr;
};

#define CTX ((MpvPlayerContext*)ctx)

static void OnMpvEvents(void* ctx) noexcept
{
    SDL_AtomicIncRef(&CTX->hasEvents);
}

static void OnMpvRenderUpdate(void* ctx) noexcept
{
    SDL_AtomicIncRef(&CTX->renderUpdate);
}

inline static void notifyVideoLoaded(const char* path) noexcept
{
    EventSystem::PushEvent(VideoEvents::VideoLoaded, (void*)path);
}

inline static void notifyPaused(bool paused) noexcept
{
    EventSystem::PushEvent(VideoEvents::PlayPauseChanged, (void*)(intptr_t)paused);
}

inline static void updateRenderTexture(MpvPlayerContext* ctx) noexcept
{
    if (!ctx->framebuffer) {
		glGenFramebuffers(1, &ctx->framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->framebuffer);

		glGenTextures(1, ctx->frameTexture);
		glBindTexture(GL_TEXTURE_2D, *ctx->frameTexture);
		
        int initialWidth = ctx->data.videoWidth > 0 ? ctx->data.videoWidth : 1920;
		int initialHeight = ctx->data.videoHeight > 0 ? ctx->data.videoHeight : 1080;
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, initialWidth, initialHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *ctx->frameTexture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); 

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create framebuffer for video!");
		}
	}
	else if(ctx->data.videoHeight > 0 && ctx->data.videoWidth > 0) {
		// update size of render texture based on video resolution
		glBindTexture(GL_TEXTURE_2D, *ctx->frameTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, ctx->data.videoWidth, ctx->data.videoHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
	}
}

inline static void showText(MpvPlayerContext* ctx, const char* text) noexcept
{
    const char* cmd[] = { "show_text", text, NULL };
    mpv_command_async(ctx->mpv, 0, cmd);
}

OFS_Videoplayer::~OFS_Videoplayer() noexcept
{
    mpv_render_context_free(CTX->mpvGL);
	mpv_destroy(CTX->mpv);
    delete ctx;
    ctx = nullptr;
}

OFS_Videoplayer::OFS_Videoplayer() noexcept
{
    ctx = new MpvPlayerContext();
    CTX->frameTexture = &this->frameTexture;
    CTX->logicalPosition = &this->logicalPosition;
    CTX->loopState = &this->LoopState;
}

bool OFS_Videoplayer::Init(bool hwAccel) noexcept
{
    CTX->mpv = mpv_create();
    if(!CTX->mpv) {
        return false;
    }
    auto confPath = Util::Prefpath();

    int error = 0;
    error = mpv_set_option_string(CTX->mpv, "config", "yes");
    if(error != 0) {
        LOG_WARN("Failed to set mpv: config=yes");
    }
    error = mpv_set_option_string(CTX->mpv, "config-dir", confPath.c_str());
    if(error != 0) {
        LOGF_WARN("Failed to set mpv: config-dir=%s", confPath.c_str());
    }

    if(mpv_initialize(CTX->mpv) != 0) {
        return false;
    }

    error = mpv_set_property_string(CTX->mpv, "loop-file", "inf");
    if(error != 0) {
        LOG_WARN("Failed to set mpv: loop-file=inf");
    }

    if(hwAccel) {
        error = mpv_set_property_string(CTX->mpv, "profile", "gpu-hq");
        if(error != 0) {
            LOG_WARN("Failed to set mpv: profile=gpu-hq");
        }
        error = mpv_set_property_string(CTX->mpv, "hwdec", "auto-safe");
        if(error != 0) {
            LOG_WARN("Failed to set mpv: hwdec=auto-safe");
        }
    }
    else {
        error = mpv_set_property_string(CTX->mpv, "hwdec", "no");
        if(error != 0) {
            LOG_WARN("Failed to set mpv: hwdec=no");
        }
    }

#ifndef NDEBUG
    mpv_request_log_messages(CTX->mpv, "debug");
#else
    mpv_request_log_messages(CTX->mpv, "info");
#endif

    mpv_opengl_init_params init_params = {0};
	init_params.get_proc_address = [](void* mpvContext, const char* fnName) noexcept -> void*
    {
        return SDL_GL_GetProcAddress(fnName);
    };
    
    uint32_t enable = 1;
	mpv_render_param renderParams[] = {
		mpv_render_param{MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
		mpv_render_param{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init_params},
		mpv_render_param{MPV_RENDER_PARAM_ADVANCED_CONTROL, &enable },
		mpv_render_param{}
	};

    if (mpv_render_context_create(&CTX->mpvGL, CTX->mpv, renderParams) < 0) {
		LOG_ERROR("Failed to initialize mpv GL context");
		return false;
	}

    mpv_set_wakeup_callback(CTX->mpv, OnMpvEvents, ctx);
    mpv_render_context_set_update_callback(CTX->mpvGL, OnMpvRenderUpdate, ctx);

	mpv_observe_property(CTX->mpv, MpvVideoHeight, "height", MPV_FORMAT_INT64);
	mpv_observe_property(CTX->mpv, MpvVideoWidth, "width", MPV_FORMAT_INT64);
	mpv_observe_property(CTX->mpv, MpvDuration, "duration", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvPosition, "percent-pos", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvTotalFrames, "estimated-frame-count", MPV_FORMAT_INT64);
	mpv_observe_property(CTX->mpv, MpvSpeed, "speed", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvPauseState, "pause", MPV_FORMAT_FLAG);
	mpv_observe_property(CTX->mpv, MpvFilePath, "path", MPV_FORMAT_STRING);
	mpv_observe_property(CTX->mpv, MpvHwDecoder, "hwdec-current", MPV_FORMAT_STRING);
	mpv_observe_property(CTX->mpv, MpvFramesPerSecond, "estimated-vf-fps", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvAbLoopA, "ab-loop-a", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvAbLoopB, "ab-loop-b", MPV_FORMAT_DOUBLE);

    return true;
}

inline static void ProcessEvents(MpvPlayerContext* ctx) noexcept
{
    for(;;) {
		mpv_event* mp_event = mpv_wait_event(ctx->mpv, 0.);
		if (mp_event->event_id == MPV_EVENT_NONE)
			break;
			
		switch (mp_event->event_id) {
            case MPV_EVENT_LOG_MESSAGE:
            {
                mpv_event_log_message* msg = (mpv_event_log_message*)mp_event->data;
                char MpvLogPrefix[48];
                int len = stbsp_snprintf(MpvLogPrefix, sizeof(MpvLogPrefix), "[%s][MPV] (%s): ", msg->level, msg->prefix);
                FUN_ASSERT(len <= sizeof(MpvLogPrefix), "buffer to small");
                OFS_FileLogger::LogToFileR(MpvLogPrefix, msg->text);
                continue;
            }
            case MPV_EVENT_COMMAND_REPLY:
            {
                // attach user_data to command
                // and handle it here when it finishes
                continue;
            }
            case MPV_EVENT_FILE_LOADED:
            {
                ctx->data.videoLoaded = true; 	
                continue;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
            {
                mpv_event_property* prop = (mpv_event_property*)mp_event->data;
                if (prop->data == nullptr) break;
                switch (mp_event->reply_userdata) {
                    case MpvHwDecoder:
                    {
                        LOGF_INFO("Active hardware decoder: %s", *(char**)prop->data);
                        break;
                    }
                    case MpvVideoWidth:
                    {
                        ctx->data.videoWidth = *(int64_t*)prop->data;
                        if (ctx->data.videoHeight > 0.f) {
                            updateRenderTexture(ctx);
                            ctx->data.videoLoaded = true;
                        }
                        break;
                    }
                    case MpvVideoHeight:
                    {
                        ctx->data.videoHeight = *(int64_t*)prop->data;
                        if (ctx->data.videoWidth > 0.f) {
                            updateRenderTexture(ctx);
                            ctx->data.videoLoaded = true;
                        }
                        break;
                    }
                    case MpvFramesPerSecond:
                        ctx->data.fps = *(double*)prop->data;
                        ctx->data.averageFrameTime = (1.0 / ctx->data.fps);
                        break;
                    case MpvDuration:
                        ctx->data.duration = *(double*)prop->data;
                        notifyVideoLoaded(ctx->data.filePath.c_str());
                        break;
                    case MpvTotalFrames:
                        ctx->data.totalNumFrames = *(int64_t*)prop->data;
                        break;
                    case MpvPosition:
                    {
                        auto newPercentPos = (*(double*)prop->data) / 100.0;
                        ctx->data.percentPos = newPercentPos;
                        if(!ctx->data.paused) {
                            *ctx->logicalPosition = newPercentPos;
                        }
                        break;
                    }
                    case MpvSpeed:
                        ctx->data.currentSpeed = *(double*)prop->data;
                        break;
                    case MpvPauseState:
                    {
                        bool paused = *(int64_t*)prop->data;
                        if (paused) {
                            *ctx->logicalPosition = ctx->data.percentPos;
                        }
                        ctx->data.paused = paused;
                        notifyPaused(ctx->data.paused);
                        break;
                    }
                    case MpvFilePath:
                        ctx->data.filePath = *((const char**)(prop->data));
                        notifyVideoLoaded(ctx->data.filePath.c_str());
                        break;
                    case MpvAbLoopA:
                    {
                        ctx->data.abLoopA = *(double*)prop->data;
                        showText(ctx, TR(LOOP_A_SET));
                        *CTX->loopState = OFS_Videoplayer::LoopEnum::A_set;
                        break;
                    }
                    case MpvAbLoopB:
                    {
                        ctx->data.abLoopB = *(double*)prop->data;
                        showText(ctx, TR(LOOP_B_SET));
                        *CTX->loopState = OFS_Videoplayer::LoopEnum::B_set;
                        break;
                    }
                }
                continue;
            }
		}
	}
}

inline static void RenderFrameToTexture(MpvPlayerContext* ctx) noexcept
{
    mpv_opengl_fbo fbo = {0};
	fbo.fbo = ctx->framebuffer; 
	fbo.w = ctx->data.videoWidth;
	fbo.h = ctx->data.videoHeight;
	fbo.internal_format = OFS_InternalTexFormat;

	uint32_t disable = 0;
	mpv_render_param params[] = {
		{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
		{MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &disable}, 
		mpv_render_param{}
	};
	mpv_render_context_render(ctx->mpvGL, params);
}

void OFS_Videoplayer::Update(float delta) noexcept
{
    while(SDL_AtomicGet(&CTX->hasEvents) > 0) {
        ProcessEvents(CTX);
        SDL_AtomicDecRef(&CTX->hasEvents);
    }

    while(SDL_AtomicGet(&CTX->renderUpdate) > 0)
    {
        uint64_t flags = mpv_render_context_update(CTX->mpvGL);
	    if (flags & MPV_RENDER_UPDATE_FRAME) {
            RenderFrameToTexture(CTX);
        }
        SDL_AtomicDecRef(&CTX->renderUpdate);
    }
}

void OFS_Videoplayer::SetVolume(float volume) noexcept
{
    CTX->data.currentVolume = volume;
    stbsp_snprintf(CTX->tmpBuf.data(), CTX->tmpBuf.size(), "%.2f", (float)(volume*100.f));
    const char* cmd[]{"set", "volume", CTX->tmpBuf.data(), NULL};
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::NextFrame() noexcept
{
    if (IsPaused()) {
        // use same method as previousFrame for consistency
        double relSeek = FrameTime() * 1.000001;
        CTX->data.percentPos += (relSeek / CTX->data.duration);
        CTX->data.percentPos = Util::Clamp(CTX->data.percentPos, 0.0, 1.0);
        SetPositionPercent(CTX->data.percentPos, false);
    }
}

void OFS_Videoplayer::PreviousFrame() noexcept
{
    if (IsPaused()) {
        // this seeks much faster
        // https://github.com/mpv-player/mpv/issues/4019#issuecomment-358641908
        double relSeek = FrameTime() * 1.000001;
        CTX->data.percentPos -= (relSeek / CTX->data.duration);
        CTX->data.percentPos = Util::Clamp(CTX->data.percentPos, 0.0, 1.0);
        SetPositionPercent(CTX->data.percentPos, false);
    }
}

void OFS_Videoplayer::OpenVideo(const std::string& path) noexcept
{
    LOGF_INFO("Opening video: \"%s\"", path.c_str());
    CloseVideo();
    
    const char* cmd[] = { "loadfile", path.c_str(), NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
    
    MpvDataCache newCache;
    newCache.currentSpeed = CTX->data.currentSpeed;
    newCache.paused = CTX->data.paused;
    CTX->data = newCache;

    SetPaused(true);
    SetVolume(CTX->data.currentVolume);
    SetSpeed(CTX->data.currentSpeed);
}

void OFS_Videoplayer::SetSpeed(float speed) noexcept
{
    speed = Util::Clamp<float>(speed, MinPlaybackSpeed, MaxPlaybackSpeed);
    if (CurrentSpeed() != speed) {
        stbsp_snprintf(CTX->tmpBuf.data(), CTX->tmpBuf.size(), "%.3f", speed);
        const char* cmd[]{ "set", "speed", CTX->tmpBuf.data(), NULL };
        mpv_command_async(CTX->mpv, 0, cmd);
    }
}

void OFS_Videoplayer::AddSpeed(float speed) noexcept
{
    speed += CTX->data.currentSpeed;
    speed = Util::Clamp<float>(speed, MinPlaybackSpeed, MaxPlaybackSpeed);
    SetSpeed(speed);
}

void OFS_Videoplayer::SetPositionPercent(float percentPos, bool pausesVideo) noexcept
{
    logicalPosition = percentPos;
    CTX->data.percentPos = percentPos;
    stbsp_snprintf(CTX->tmpBuf.data(), CTX->tmpBuf.size(), "%.08f", (float)(percentPos * 100.0f));
    const char* cmd[]{ "seek", CTX->tmpBuf.data(), "absolute-percent+exact", NULL };
    if (pausesVideo) {
        SetPaused(true);
    }
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::SetPositionExact(float timeSeconds, bool pausesVideo) noexcept
{
    // this updates logicalPosition in SetPositionPercent
    timeSeconds = Util::Clamp<float>(timeSeconds, 0.f, Duration());
    float relPos = ((float)timeSeconds) / Duration();
    SetPositionPercent(relPos, pausesVideo);
}

void OFS_Videoplayer::SeekRelative(float timeSeconds) noexcept
{
    // this updates logicalPosition in SetPositionPercent
    auto seekTo = CurrentTimeInterp() + timeSeconds;
    seekTo = std::max(seekTo, 0.0);
    SetPositionExact(seekTo);
}

void OFS_Videoplayer::SeekFrames(int32_t offset) noexcept
{
    // this updates logicalPosition in SetPositionPercent
    if (IsPaused()) {
        float relSeek = (FrameTime() * 1.000001f) * offset;
        CTX->data.percentPos += (relSeek / CTX->data.duration);
        CTX->data.percentPos = Util::Clamp(CTX->data.percentPos, 0.0, 1.0);
        SetPositionPercent(CTX->data.percentPos, false);
    }
}

void OFS_Videoplayer::SetPaused(bool paused) noexcept
{
    if (!paused && !VideoLoaded()) return;
    CTX->data.paused = paused;
    mpv_set_property_async(CTX->mpv, 0, "pause", MPV_FORMAT_FLAG, &CTX->data.paused);
}

void OFS_Videoplayer::TogglePlay() noexcept
{
    if (!VideoLoaded()) return;
    const char* cmd[]{ "cycle", "pause", NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::CycleSubtitles() noexcept
{
    const char* cmd[]{ "cycle", "sub", NULL};
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::CycleLoopAB() noexcept
{
    const char* cmd[]{ "ab-loop", NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
    if (LoopState == LoopEnum::B_set) {
        CTX->data.abLoopA = 0.f;
        CTX->data.abLoopB = 0.f;
        showText(CTX, TR(LOOP_CLEARED));
        LoopState = LoopEnum::Clear;
    }
}

void OFS_Videoplayer::ClearLoop() noexcept
{
    if (LoopState == LoopEnum::A_set)
    {
        // call twice
        CycleLoopAB(); CycleLoopAB();
    }
    else if (LoopState == LoopEnum::B_set)
    {
        CycleLoopAB();
    }
    else { /*loop already clear*/ }
}

void OFS_Videoplayer::CloseVideo() noexcept
{
    CTX->data.videoLoaded = false;
    const char* cmd[] = { "stop", NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
    SetPaused(true);
}

void OFS_Videoplayer::NotifySwap() noexcept
{
    mpv_render_context_report_swap(CTX->mpvGL);
}

void OFS_Videoplayer::SaveFrameToImage(const std::string& directory) noexcept
{
    std::stringstream ss;
    auto currentFile = Util::PathFromString(VideoPath());
    std::string filename = currentFile.filename().replace_extension("").string();
    std::array<char, 15> tmp;
    double time = CurrentTime();
    Util::FormatTime(tmp.data(), tmp.size(), time, true);
    std::replace(tmp.begin(), tmp.end(), ':', '_');
    ss << filename << '_' << tmp.data() << ".png";
    if(!Util::CreateDirectories(directory)) {
        return;
    }
    auto dir = Util::PathFromString(directory);
    dir.make_preferred();
    std::string finalPath = (dir / ss.str()).string();
    const char* cmd[]{ "screenshot-to-file", finalPath.c_str(), NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
}

// ==================== Getter ==================== 

uint16_t OFS_Videoplayer::VideoWidth() const noexcept
{
    return CTX->data.videoWidth;
}

uint16_t OFS_Videoplayer::VideoHeight() const noexcept
{
    return CTX->data.videoHeight;
}

float OFS_Videoplayer::FrameTime() const noexcept
{
    return CTX->data.averageFrameTime;
}

float OFS_Videoplayer::CurrentSpeed() const noexcept
{
    return CTX->data.currentSpeed;
}

float OFS_Videoplayer::Volume() const noexcept
{
    return CTX->data.currentVolume;
}

double OFS_Videoplayer::Duration() const noexcept
{
    return CTX->data.duration;
}

bool OFS_Videoplayer::IsPaused() const noexcept
{
    return CTX->data.paused;
}

float OFS_Videoplayer::Fps() const noexcept
{
    return CTX->data.fps;
}

bool OFS_Videoplayer::VideoLoaded() const noexcept
{
    return CTX->data.videoLoaded;
}

double OFS_Videoplayer::CurrentTimeInterp() const noexcept
{
    // no interpolation yet
    return CurrentTime();
}

double OFS_Videoplayer::CurrentPlayerPosition() const noexcept
{
    return CTX->data.percentPos;
}

const char* OFS_Videoplayer::VideoPath() const noexcept
{
    return CTX->data.filePath.c_str();
}
