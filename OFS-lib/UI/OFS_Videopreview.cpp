#include "OFS_Videopreview.h"

#include "mpv/client.h"
#include "mpv/render_gl.h"

#include "OFS_GL.h"
#include "EventSystem.h"
#include "OFS_MpvLoader.h"

int32_t VideoPreviewEvents::PreviewWakeUpMpvEvents = 0;
int32_t VideoPreviewEvents::PreviewWakeUpMpvRender = 0;

void VideoPreviewEvents::RegisterEvents() noexcept
{
	if (PreviewWakeUpMpvEvents != 0) return;
	PreviewWakeUpMpvEvents = SDL_RegisterEvents(1);
	PreviewWakeUpMpvRender = SDL_RegisterEvents(1);
}


static void* get_proc_address_mpv(void* fn_ctx, const char* name)
{
	return SDL_GL_GetProcAddress(name);
}

static void on_mpv_events(void* ctx)
{
	EventSystem::PushEvent(VideoPreviewEvents::PreviewWakeUpMpvEvents, ctx);
}

static void on_mpv_render_update(void* ctx)
{
	EventSystem::PushEvent(VideoPreviewEvents::PreviewWakeUpMpvRender, ctx);
}

void VideoPreview::updateRenderTexture() noexcept
{
	if (framebufferObj == 0) {
		glGenFramebuffers(1, &framebufferObj);
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferObj);

		glGenTextures(1, &renderTexture);
		glBindTexture(GL_TEXTURE_2D, renderTexture);
		
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, 360, 200, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderTexture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create framebuffer for video!");
		}
	}
	else {
		// update size of render texture based on video resolution
		glBindTexture(GL_TEXTURE_2D, renderTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, videoWidth, videoHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
	}
}

void VideoPreview::observeProperties() noexcept
{
	mpv_observe_property(mpv, VideoHeightProp, "height", MPV_FORMAT_INT64);
	mpv_observe_property(mpv, VideoWidthProp, "width", MPV_FORMAT_INT64);
	mpv_observe_property(mpv, VideoPosProp, "percent-pos", MPV_FORMAT_DOUBLE);
}

void VideoPreview::redraw() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	needsRedraw = false;
	mpv_opengl_fbo fbo{ 0 };
	fbo.fbo = framebufferObj;
	fbo.w = videoWidth;
	fbo.h = videoHeight;
	fbo.internal_format = OFS_InternalTexFormat;

	uint32_t disable = 0;
	mpv_render_param params[] = {
		{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
		// without this the whole application slows down to the framerate of the video
		{MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &disable},
		mpv_render_param{}
	};
	mpv_render_context_render(mpv_gl, params);
	if (videoPos >= seek_to) {
		if (renderComplete) {
			ready = true;
		}
		renderComplete = true;
	}
}

VideoPreview::~VideoPreview()
{
	mpv_render_context_free(mpv_gl);
	mpv_detach_destroy(mpv);
	EventSystem::ev().UnsubscribeAll(this);
	
	glDeleteFramebuffers(1, &framebufferObj);
	glDeleteTextures(1, &renderTexture);
}

void VideoPreview::setup(bool autoplay) noexcept
{
	VideoPreviewEvents::RegisterEvents();
	EventSystem::ev().Subscribe(VideoPreviewEvents::PreviewWakeUpMpvEvents, EVENT_SYSTEM_BIND(this, &VideoPreview::MpvEvents));
	EventSystem::ev().Subscribe(VideoPreviewEvents::PreviewWakeUpMpvRender, EVENT_SYSTEM_BIND(this, &VideoPreview::MpvRenderUpdate));

	updateRenderTexture();
	mpv = mpv_create();
	if (mpv_initialize(mpv) < 0) {
		LOG_ERROR("mpv context init failed");
		return;
	}

	observeProperties();
	bool suc = mpv_set_property_string(mpv, "keep-open", "yes") == 0;
	suc = mpv_set_property_string(mpv, "loop-file", "inf") == 0;
	
	mpv_opengl_init_params init_params{ 0 };
	init_params.get_proc_address = get_proc_address_mpv;

	const int enable = 1;
	mpv_render_param params[] = {
		mpv_render_param{MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
		mpv_render_param{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init_params},

		// Tell libmpv that you will call mpv_render_context_update() on render
		// context update callbacks, and that you will _not_ block on the core
		// ever (see <libmpv/render.h> "Threading" section for what libmpv
		// functions you can call at all when this is active).
		// In particular, this means you must call e.g. mpv_command_async()
		// instead of mpv_command().
		// If you want to use synchronous calls, either make them on a separate
		// thread, or remove the option below (this will disable features like
		// DR and is not recommended anyway).
		mpv_render_param{MPV_RENDER_PARAM_ADVANCED_CONTROL, (void*)&enable },
		mpv_render_param{}
	};
	if (mpv_render_context_create(&mpv_gl, mpv, params) < 0) {
		LOG_ERROR("failed to initialize mpv GL context");
		return;
	}

	mpv_set_wakeup_callback(mpv, on_mpv_events, this);
	mpv_render_context_set_update_callback(mpv_gl, on_mpv_render_update, this);

	if (autoplay)
	{
		const char* play_cmd[]{ "play",  NULL };
		mpv_command_async(mpv, 0, play_cmd);
	}

	// mute
	stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f", (float)(0.f * 100.f));
	const char* cmd[]{ "set", "volume", tmp_buf, NULL };
	mpv_command_async(mpv, 0, cmd);

}

void VideoPreview::update() noexcept
{
	if (needsRedraw) redraw();
}

void VideoPreview::MpvEvents(SDL_Event& ev) noexcept
{
	if (ev.user.data1 != this) return;
	OFS_PROFILE(__FUNCTION__);
	while (1) {
		mpv_event* mp_event = mpv_wait_event(mpv, 0);
		if (mp_event->event_id == MPV_EVENT_NONE) break;
		switch (mp_event->event_id) {
		case MPV_EVENT_LOG_MESSAGE:
		{
			//mpv_event_log_message* msg = (mpv_event_log_message*)mp_event->data;
			//if (msg->log_level <= MPV_LOG_LEVEL_INFO) {
			//	switch (msg->log_level)
			//	{
			//	case MPV_LOG_LEVEL_INFO:
			//		LOGF_INFO("MPV (%s): %s", msg->prefix, msg->text);
			//		break;
			//	case MPV_LOG_LEVEL_FATAL:
			//		LOGF_ERROR("!!! MPV (%s): %s !!!", msg->prefix, msg->text);
			//		break;
			//	case MPV_LOG_LEVEL_ERROR:
			//		LOGF_ERROR("! MPV (%s): %s !", msg->prefix, msg->text);
			//		break;
			//	case MPV_LOG_LEVEL_WARN:
			//		LOGF_WARN("MPV (%s): %s", msg->prefix, msg->text);
			//		break;
			//	case MPV_LOG_LEVEL_DEBUG:
			//		LOGF_DEBUG("MPV (%s): %s", msg->prefix, msg->text);
			//		break;
			//	default:
			//		LOGF_INFO("MPV (%s): %s", msg->prefix, msg->text);
			//		break;
			//	}
			//}
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
			setPosition(seek_to);
			continue;
		}
		case MPV_EVENT_PROPERTY_CHANGE:
		{
			mpv_event_property* prop = (mpv_event_property*)mp_event->data;
			if (prop->data == nullptr) break;
			switch (mp_event->reply_userdata) {
			case VideoHeightProp:
			{
				videoHeight = *(int64_t*)prop->data;
				if (videoWidth > 0) {
					updateRenderTexture();
				}
				break;
			}
			case VideoWidthProp:
			{
				videoWidth = *(int64_t*)prop->data;
				if (videoHeight > 0) {
					updateRenderTexture();
				}
				break;
			}
			case VideoPosProp:
			{
				videoPos = *(double*)prop->data;
				break;
			}
			}
		}
		}
	}
}

void VideoPreview::MpvRenderUpdate(SDL_Event& ev) noexcept
{
	if (ev.user.data1 != this) return;
	OFS_PROFILE(__FUNCTION__);
	uint64_t flags = mpv_render_context_update(mpv_gl);
	if (flags & MPV_RENDER_UPDATE_FRAME) {
		needsRedraw = true;
	}
}


void VideoPreview::setPosition(float pos) noexcept
{
	stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.08f", (float)(pos * 100.0f));
	const char* cmd[]{ "seek", tmp_buf, "absolute-percent+exact", NULL };
	mpv_command_async(mpv, 0, cmd);
}

void VideoPreview::previewVideo(const std::string& path, float pos) noexcept
{
	loading = true;
	ready = false;
	renderComplete = false;
	seek_to = pos;
	videoPos = 0.f;
	videoHeight = -1;
	videoWidth = -1;
	const char* cmd[] = { "loadfile", path.c_str(), NULL };
	mpv_command_async(mpv, 0, cmd);
	LOG_DEBUG("start preview");
}

void VideoPreview::play() noexcept
{
	if (!paused) return;
	paused = false;
	mpv_set_property_async(mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
}

void VideoPreview::pause() noexcept
{
	if (paused) return;
	paused = true;
	mpv_set_property_async(mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
}

void VideoPreview::closeVideo() noexcept
{
	ready = false;
	loading = false;
	videoPos = 0.f;
	videoHeight = -1;
	videoWidth = -1;
	const char* cmd[] = { "stop", NULL };
	mpv_command_async(mpv, 0, cmd);
	LOG_DEBUG("stop preview");
}
