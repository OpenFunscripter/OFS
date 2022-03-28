#include "OFS_Videoplayer.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include "stb_sprintf.h"
#include "stb_image_write.h"
#include "OFS_GL.h"

#include "EventSystem.h"
#include "OFS_ImGui.h"
#include "OFS_Profiling.h"
#include "OFS_Shader.h"

#define OFS_MPV_LOADER_MACROS
#include "OFS_MpvLoader.h"


static void* getProcAddressMpv(void* fn_ctx, const char* name) noexcept
{
	return SDL_GL_GetProcAddress(name);
}

static void onMpvEvents(void* ctx) noexcept
{
	EventSystem::PushEvent(VideoEvents::WakeupOnMpvEvents, ctx);
}

static void onMpvRenderUpdate(void* ctx) noexcept
{
	EventSystem::PushEvent(VideoEvents::WakeupOnMpvRenderUpdate, ctx);
}

void VideoplayerWindow::MpvEvents(SDL_Event& ev) noexcept
{
	if (ev.user.data1 != this) return;
	OFS_PROFILE(__FUNCTION__);
	for(;;) {
		mpv_event* mp_event = mpv_wait_event(mpv, 0.);
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
			MpvData.videoLoaded = true; 	
			continue;
		}
		case MPV_EVENT_PROPERTY_CHANGE:
		{
			mpv_event_property* prop = (mpv_event_property*)mp_event->data;
			if (prop->data == nullptr) break;
			
#ifndef NDEBUG
			switch (prop->format)
			{
			case MPV_FORMAT_NONE:
				continue;
			case MPV_FORMAT_DOUBLE:
				//if (mp_event->reply_userdata != MpvPosition) {
					//LOGF_DEBUG("Property \"%s\" has changed to %lf", prop->name, *(double*)prop->data);
				//}
				break;
			case MPV_FORMAT_FLAG:
			case MPV_FORMAT_INT64:
				LOGF_DEBUG("Property \"%s\" has changed to %ld", prop->name, *(int64_t*)prop->data);
				break;
			case MPV_FORMAT_STRING:
				LOGF_DEBUG("Property \"%s\" has changed to %s", prop->name, *(char**)prop->data);
				break;
			}
#endif
			switch (mp_event->reply_userdata) {
			case MpvHwDecoder:
			{
				LOGF_INFO("Active hardware decoder: %s", *(char**)prop->data);
				break;
			}
			case MpvVideoWidth:
			{
				MpvData.videoWidth = *(int64_t*)prop->data;
				if (MpvData.videoHeight > 0.f) {
					updateRenderTexture();
					MpvData.videoLoaded = true;
				}
				break;
			}
			case MpvVideoHeight:
			{
				MpvData.videoHeight = *(int64_t*)prop->data;
				if (MpvData.videoWidth > 0.f) {
					updateRenderTexture();
					MpvData.videoLoaded = true;
				}
				break;
			}
			case MpvFramesPerSecond:
				MpvData.fps = *(double*)prop->data;
				MpvData.averageFrameTime = (1.0 / MpvData.fps);
				break;
			case MpvDuration:
				MpvData.duration = *(double*)prop->data;
				notifyVideoLoaded();
				clearLoop();
				break;
			case MpvTotalFrames:
				MpvData.totalNumFrames = *(int64_t*)prop->data;
				break;
			case MpvPosition:
			{
				auto newPercentPos = (*(double*)prop->data) / 100.0;
				MpvData.realPercentPos = newPercentPos;
				if (!MpvData.paused) {
					lastVideoStep = ((MpvData.realPercentPos - MpvData.percentPos) * MpvData.duration);
					if (lastVideoStep > .0f) {
						smoothTime -= lastVideoStep;
					}
					else {
						// fix for looping correctly
						smoothTime = 0.f;
						lastVideoStep = 0.f;
					}
					MpvData.percentPos = MpvData.realPercentPos;
				}
				break;
			}
			case MpvSpeed:
				MpvData.currentSpeed = *(double*)prop->data;
				break;
			case MpvPauseState:
			{
				bool paused = *(int64_t*)prop->data;
				if (paused) {
					float actualTime = getRealCurrentPositionSeconds();
					float estimateTime = getCurrentPositionSecondsInterp();
					smoothTime += actualTime - estimateTime;
				}
				MpvData.paused = paused;
				EventSystem::PushEvent(VideoEvents::PlayPauseChanged, (void*)(intptr_t)MpvData.paused);
				break;
			}
			case MpvFilePath:
                // Copy string to ensure we own the memory and control the lifetime
				MpvData.filePath = std::string(*((const char**)(prop->data)));
				notifyVideoLoaded();
				break;
			case MpvAbLoopA:
			{
				MpvData.abLoopA = *(double*)prop->data;
				showText("Loop A set.");
				LoopState = LoopEnum::A_set;
				break;
			}
			case MpvAbLoopB:
			{
				MpvData.abLoopB = *(double*)prop->data;
				showText("Loop B set.");
				LoopState = LoopEnum::B_set;
				break;
			}
			}
			continue;
		}
		}
	}
}

void VideoplayerWindow::MpvRenderUpdate(SDL_Event& ev) noexcept
{
	if (ev.user.data1 != this) return;
	OFS_PROFILE(__FUNCTION__);
	uint64_t flags = mpv_render_context_update(mpv_gl);
	if (flags & MPV_RENDER_UPDATE_FRAME) {
		redrawVideo = true;
	}
}

void VideoplayerWindow::observeProperties() noexcept
{
	mpv_observe_property(mpv, MpvVideoHeight, "height", MPV_FORMAT_INT64);
	mpv_observe_property(mpv, MpvVideoWidth, "width", MPV_FORMAT_INT64);
	mpv_observe_property(mpv, MpvDuration, "duration", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, MpvPosition, "percent-pos", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, MpvTotalFrames, "estimated-frame-count", MPV_FORMAT_INT64);
	mpv_observe_property(mpv, MpvSpeed, "speed", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, MpvPauseState, "pause", MPV_FORMAT_FLAG);
	mpv_observe_property(mpv, MpvFilePath, "path", MPV_FORMAT_STRING);
	mpv_observe_property(mpv, MpvHwDecoder, "hwdec-current", MPV_FORMAT_STRING);
	mpv_observe_property(mpv, MpvFramesPerSecond, "estimated-vf-fps", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, MpvAbLoopA, "ab-loop-a", MPV_FORMAT_DOUBLE);
	mpv_observe_property(mpv, MpvAbLoopB, "ab-loop-b", MPV_FORMAT_DOUBLE);
}

void VideoplayerWindow::renderToTexture() noexcept
{
	OFS_PROFILE(__FUNCTION__);
	redrawVideo = false;
	mpv_opengl_fbo fbo = {0};
	fbo.fbo = framebufferObj; 
	fbo.w = MpvData.videoWidth;
	fbo.h = MpvData.videoHeight;
	fbo.internal_format = OFS_InternalTexFormat;

	uint32_t disable = 0;
	mpv_render_param params[] = {
		{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
		// without this the whole application slows down to the framerate of the video
		{MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &disable}, 
		mpv_render_param{}
	};
	mpv_render_context_render(mpv_gl, params);
}

void VideoplayerWindow::updateRenderTexture() noexcept
{
	if (!framebufferObj) {
		glGenFramebuffers(1, &framebufferObj);
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferObj);

		glGenTextures(1, &renderTexture);
		glBindTexture(GL_TEXTURE_2D, renderTexture);
		const int defaultWidth = 1920;
		const int defaultHeight = 1080;
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, defaultWidth, defaultHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); 

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create framebuffer for video!");
			FUN_ASSERT(false, "framebuffer not setup");
		}
	}
	else if(MpvData.videoHeight > 0 && MpvData.videoWidth > 0) {
		// update size of render texture based on video resolution
		glBindTexture(GL_TEXTURE_2D, renderTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, MpvData.videoWidth, MpvData.videoHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
	}
	else {
		FUN_ASSERT(false, "Video height/width was 0");
	}
}

bool VideoplayerWindow::setup(bool force_hw_decoding)
{
	EventSystem::ev().Subscribe(VideoEvents::WakeupOnMpvEvents, EVENT_SYSTEM_BIND(this, &VideoplayerWindow::MpvEvents));
	EventSystem::ev().Subscribe(VideoEvents::WakeupOnMpvRenderUpdate, EVENT_SYSTEM_BIND(this, &VideoplayerWindow::MpvRenderUpdate));
	EventSystem::ev().Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &VideoplayerWindow::mouseScroll));

	updateRenderTexture();
	mpv = mpv_create();
	auto confPath = Util::Prefpath();
	bool suc;
	
	suc = mpv_set_option_string(mpv, "config", "yes") == 0;
	if(!suc) LOG_WARN("failed to set mpv: config=yes");
	suc = mpv_set_option_string(mpv, "config-dir", confPath.c_str()) == 0;
	if (!suc) LOGF_WARN("failed to set mpv: config-dir=%s", confPath.c_str());

	if (mpv_initialize(mpv) < 0) {
		LOG_ERROR("mpv context init failed");
		return false;
	}

	// hardware decoding. only important when running 5k vr footage
	if (force_hw_decoding) {
		suc = mpv_set_property_string(mpv, "profile", "gpu-hq") == 0;
		if (!suc)
			LOG_WARN("failed to set mpv: profile=gpu-hq");
		suc = mpv_set_property_string(mpv, "hwdec", "auto-safe") == 0;
		if (!suc)
			LOG_WARN("failed to set mpv hardware decoding to \"auto-safe\"");
	}
	else {
		suc = mpv_set_property_string(mpv, "hwdec", "no") == 0;
	}

	// without this the file gets closed when the end is reached
	suc = mpv_set_property_string(mpv, "keep-open", "yes") == 0;
	if (!suc)
		LOG_WARN("failed to set mpv: keep-open=yes");

	// looping
	suc = mpv_set_property_string(mpv, "loop-file", "inf") == 0;
	if (!suc)
		LOG_WARN("failed to set mpv: loop-file=inf");

	if (!suc)
		LOG_WARN("failed to set mpv: config-dir");

#ifndef NDEBUG
	mpv_request_log_messages(mpv, "debug");
#else 
	mpv_request_log_messages(mpv, "info");
#endif

	mpv_opengl_init_params init_params = {0};
	init_params.get_proc_address = getProcAddressMpv;

	uint32_t enable = 1;
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
		mpv_render_param{MPV_RENDER_PARAM_ADVANCED_CONTROL, &enable },
		mpv_render_param{}
	};

	// This makes mpv use the currently set GL context. It will use the callback
	// (passed via params) to resolve GL builtin functions, as well as extensions.
	if (mpv_render_context_create(&mpv_gl, mpv, params) < 0) {
		LOG_ERROR("failed to initialize mpv GL context");
		return false;
	}

	// When normal mpv events are available.
	mpv_set_wakeup_callback(mpv, onMpvEvents, this);

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	mpv_render_context_set_update_callback(mpv_gl, onMpvRenderUpdate, this);

	observeProperties();

	setupVrMode();
	setPaused(true);
	
	// this may be bad :/
	// normally ids get generated with ImGui::GetID which seeds using previous ids
	videoImageId = ImGui::GetIDWithSeed("videoImage", 0, rand());
	return true;
}

VideoplayerWindow::~VideoplayerWindow()
{
	mpv_render_context_free(mpv_gl);
	mpv_destroy(mpv);
	glDeleteTextures(1, &renderTexture);
	glDeleteFramebuffers(1, &framebufferObj);
	EventSystem::ev().UnsubscribeAll(this);
}

void VideoplayerWindow::mouseScroll(SDL_Event& ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (settings.LockedPosition) return;

	auto& scroll = ev.wheel;
	if (videoHovered) {
		auto mousePosInVid = ImGui::GetMousePos() - viewportPos - windowPos - settings.videoPos;
		float zoomPointX = (mousePosInVid.x - (videoDrawSize.x/2.f)) / videoDrawSize.x;
		float zoomPointY = (mousePosInVid.y - (videoDrawSize.y/2.f)) / videoDrawSize.y;

		float vidWidth = MpvData.videoWidth;
		float vidHeight = MpvData.videoHeight;

		switch (settings.activeMode) {
		case VideoMode::LEFT_PANE:
		case VideoMode::RIGHT_PANE:
			vidWidth /= 2.f;
			break;
		case VideoMode::TOP_PANE:
		case VideoMode::BOTTOM_PANE:
			vidHeight /= 2.f;
			break;
		}
		zoomPointX *= vidWidth;
		zoomPointY *= vidHeight;

		const float oldScale = settings.zoomFactor;
		// apply zoom
		if (settings.activeMode == VideoMode::VR_MODE) {
			settings.vrZoom *= ((1+(ZoomMulti * scroll.y)));
			settings.vrZoom = Util::Clamp(settings.vrZoom, 0.05f, 2.0f);
			return;
		}

		settings.zoomFactor *= 1 + (ZoomMulti * scroll.y);
		settings.zoomFactor = Util::Clamp(settings.zoomFactor, 0.0f, 10.f);

		const float scaleChange = (settings.zoomFactor - oldScale) * baseScaleFactor;
		const float offsetX = -(zoomPointX * scaleChange);
		const float offsetY = -(zoomPointY * scaleChange);

		settings.prevTranslation.x += offsetX;
		settings.prevTranslation.y += offsetY;

		if (!dragStarted) {
			settings.currentTranslation = settings.prevTranslation;
		}
	}
}

void VideoplayerWindow::setupVrMode() noexcept
{
	// VR MODE
	// setup shader
	vrShader = std::make_unique<VrShader>();
}

void VideoplayerWindow::notifyVideoLoaded() noexcept
{
	EventSystem::PushEvent(VideoEvents::MpvVideoLoaded, (void*)MpvData.filePath.c_str());
}

void VideoplayerWindow::drawVrVideo(ImDrawList* draw_list) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (!settings.LockedPosition && videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
		dragStarted = true;
	}
	// apply drag to translation
	else if (dragStarted && videoHovered)
	{
		settings.currentVrRotation =
			settings.prevVrRotation
			+ (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left) 
				/ ImVec2((10000.f * settings.vrZoom), (videoDrawSize.y / videoDrawSize.x) * 10000.f * settings.vrZoom));
	}

	playerViewport = ImGui::GetCurrentWindowRead()->Viewport;
	draw_list->AddCallback(
		[](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
			auto& ctx = *(VideoplayerWindow*)cmd->UserCallbackData;

			auto draw_data = ctx.playerViewport->DrawData;
			ctx.vrShader->use();

			float L = draw_data->DisplayPos.x;
			float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
			float T = draw_data->DisplayPos.y;
			float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
			const float ortho_projection[4][4] =
			{
				{ 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
				{ 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
				{ 0.0f, 0.0f, -1.0f, 0.0f },
				{ (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
			};
			ctx.vrShader->ProjMtx(&ortho_projection[0][0]);
			ctx.vrShader->Rotation(&ctx.settings.currentVrRotation.x);
			ctx.vrShader->Zoom(ctx.settings.vrZoom);
			ctx.vrShader->AspectRatio(ctx.videoDrawSize.x / ctx.videoDrawSize.y);
			// TODO: set this somewhere else get rid of the branch
			if (ctx.MpvData.videoHeight > 0.f) {
				ctx.vrShader->VideoAspectRatio(ctx.MpvData.videoWidth /(float)ctx.MpvData.videoHeight);
			}
		}, this);
	//ImGui::Image((void*)(intptr_t)renderTexture, ImGui::GetContentRegionAvail(), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));
	OFS::ImageWithId(videoImageId, (void*)(intptr_t)renderTexture, ImGui::GetContentRegionAvail(), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));
	videoRightClickMenu();
	videoDrawSize = ImGui::GetItemRectSize();
}

void VideoplayerWindow::draw2dVideo(ImDrawList* draw_list) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	ImVec2 videoSize(MpvData.videoWidth, MpvData.videoHeight);
	ImVec2 dst = ImGui::GetContentRegionAvail();
	baseScaleFactor = std::min(dst.x / videoSize.x, dst.y / videoSize.y);
	videoSize.x *= baseScaleFactor;
	videoSize.y *= baseScaleFactor;

	ImVec2 uv0(0.f, 0.f);
	ImVec2 uv1(1.f, 1.f);

	switch (settings.activeMode) {
	case VideoMode::LEFT_PANE:
		videoSize.x /= 2.f;
		uv1.x = 0.5f;
		break;
	case VideoMode::RIGHT_PANE:
		videoSize.x /= 2.f;
		uv0.x = 0.5f;
		break;
	case VideoMode::TOP_PANE:
		videoSize.y /= 2.f;
		uv1.y = 0.5;
		break;
	case VideoMode::BOTTOM_PANE:
		videoSize.y /= 2.f;
		uv0.y = 0.5f;
		break;
	case VideoMode::VR_MODE:
	case VideoMode::FULL:
	default:
		// nothing
		break;
	}

	videoSize = videoSize * ImVec2(settings.zoomFactor, settings.zoomFactor);
	settings.videoPos = (ImGui::GetWindowSize() - videoSize) * 0.5f + settings.currentTranslation;
	ImGui::SetCursorPos(settings.videoPos);
	// the videoHovered is one frame old but moving this up prevents flicker while dragging and zooming at the same time
	// start video dragging
	if (!settings.LockedPosition && videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
		dragStarted = true;
	}
	// apply drag to translation
	else if(dragStarted && videoHovered)
	{
		settings.currentTranslation = settings.prevTranslation + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
	}

	playerViewport = ImGui::GetCurrentWindowRead()->Viewport;
	OFS::ImageWithId(videoImageId, (void*)(intptr_t)renderTexture, videoSize, uv0, uv1);
	videoRightClickMenu();
}

void VideoplayerWindow::videoRightClickMenu() noexcept
{
	if (ImGui::BeginPopupContextItem())
	{
		ImGui::MenuItem("Lock", NULL, &settings.LockedPosition);


#ifndef NDEBUG
		if (ImGui::BeginMenu("Empty")) {
			ImGui::TextDisabled("it really do be empty");
			ImGui::EndMenu();
		}
#endif
		ImGui::EndPopup();
	}
}

void VideoplayerWindow::showText(const char* text) noexcept
{
	const char* cmd[] = { "show_text", text, NULL };
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::clearLoop() noexcept
{
	if (LoopState == LoopEnum::A_set)
	{
		// call twice
		cycleLoopAB(); cycleLoopAB();
	}
	else if (LoopState == LoopEnum::B_set)
	{
		cycleLoopAB();
	}
	else { /*loop already clear*/ }
}

void VideoplayerWindow::DrawVideoPlayer(bool* open, bool* draw_video) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	// this redraw has to happen even if the video isn't actually shown in the gui
	if (redrawVideo) { renderToTexture(); }
	if (open != nullptr && !*open) return;
	
	ImGui::Begin(PlayerId, open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

	if (!MpvData.videoLoaded) {
		ImGui::End();
		return;
	}

	if (*draw_video) {
		viewportPos = ImGui::GetWindowViewport()->Pos;
		auto draw_list = ImGui::GetWindowDrawList();
		if (settings.activeMode != VideoMode::VR_MODE) {
			draw2dVideo(draw_list);
		}
		else if(*draw_video) {
			drawVrVideo(draw_list);
		}
		if (OnRenderCallback != nullptr) { draw_list->AddCallback(OnRenderCallback, this); }
		// this reset is for the simulator 3d, vr mode or both
		draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	
		videoHovered = ImGui::IsItemHovered() && ImGui::IsWindowHovered();
		videoDrawSize = ImGui::GetItemRectSize();
	
		// cancel drag
		if ((dragStarted && !videoHovered) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			dragStarted = false;
			settings.prevTranslation = settings.currentTranslation;
			settings.prevVrRotation = settings.currentVrRotation;
		}
	
		// recenter
		if (videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			resetTranslationAndZoom();
		}
	}
	else
	{
		if (ImGui::Button("Click to enable video")) {
			*draw_video = true;
		}
	}
	windowPos = ImGui::GetWindowPos() - viewportPos;
	ImGui::End();
}

void VideoplayerWindow::setSpeed(float speed) noexcept
{
	speed = Util::Clamp<float>(speed, MinPlaybackSpeed, MaxPlaybackSpeed);
	if (getSpeed() != speed) {
		settings.playbackSpeed = speed;
		stbsp_snprintf(tmpBuf, sizeof(tmpBuf), "%.3f", speed);
		const char* cmd[]{ "set", "speed", tmpBuf, NULL };
		mpv_command_async(mpv, 0, cmd);
	}
}

void VideoplayerWindow::addSpeed(float speed) noexcept
{
	settings.playbackSpeed += speed;
	settings.playbackSpeed = Util::Clamp<float>(settings.playbackSpeed, MinPlaybackSpeed, MaxPlaybackSpeed);
	setSpeed(settings.playbackSpeed);
}

void VideoplayerWindow::openVideo(const std::string& file)
{
	LOGF_INFO("Opening video: \"%s\"", file.c_str());
	closeVideo();

	const char* cmd[] = { "loadfile", file.c_str(), NULL };
	mpv_command_async(mpv, 0, cmd);
	
	MpvDataCache newCache;
	// some variables shouldn't get reset
	newCache.currentSpeed = MpvData.currentSpeed;
	newCache.paused = MpvData.paused;
	MpvData = newCache;

	setPaused(true);
	setVolume(settings.volume);
	setSpeed(settings.playbackSpeed);
	resetTranslationAndZoom();
}

void VideoplayerWindow::saveFrameToImage(const std::string& directory)
{
	std::stringstream ss;
	auto currentFile = Util::PathFromString(getVideoPath());
	std::string filename = currentFile.filename().replace_extension("").string();
	std::array<char, 15> tmp;
	double time = getCurrentPositionSeconds();
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
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::setVolume(float volume) noexcept
{
	stbsp_snprintf(tmpBuf, sizeof(tmpBuf), "%.2f", (float)(volume*100.f));
	const char* cmd[]{"set", "volume", tmpBuf, NULL};
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::setPositionPercent(float pos, bool pausesVideo) noexcept
{
	MpvData.percentPos = pos;
	stbsp_snprintf(tmpBuf, sizeof(tmpBuf), "%.08f", (float)(pos * 100.0f));
	const char* cmd[]{ "seek", tmpBuf, "absolute-percent+exact", NULL };
	if (pausesVideo) {
		setPaused(true);
	}
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::seekRelative(float time) noexcept
{
	auto seekTo = getCurrentPositionSecondsInterp() + time;
	seekTo = std::max(seekTo, 0.0);
	setPositionExact(seekTo);
}

void VideoplayerWindow::setPaused(bool paused) noexcept
{
	if (!paused && !isLoaded()) return;
	MpvData.paused = paused;
	mpv_set_property_async(mpv, 0, "pause", MPV_FORMAT_FLAG, &MpvData.paused);
}

void VideoplayerWindow::nextFrame() noexcept
{
	if (isPaused()) {
		// use same method as previousFrame for consistency
		double relSeek = getFrameTime() * 1.000001;
		MpvData.percentPos += (relSeek / MpvData.duration);
		MpvData.percentPos = Util::Clamp(MpvData.percentPos, 0.0, 1.0);
		setPositionPercent(MpvData.percentPos, false);
	}
}

void VideoplayerWindow::previousFrame() noexcept
{
	if (isPaused()) {
		// this seeks much faster
		// https://github.com/mpv-player/mpv/issues/4019#issuecomment-358641908
		double relSeek = getFrameTime() * 1.000001;
		MpvData.percentPos -= (relSeek / MpvData.duration);
		MpvData.percentPos = Util::Clamp(MpvData.percentPos, 0.0, 1.0);
		setPositionPercent(MpvData.percentPos, false);
	}
}

void VideoplayerWindow::relativeFrameSeek(int32_t seek) noexcept
{
	if (isPaused()) {
		float relSeek = (getFrameTime() * 1.000001f) * seek;
		MpvData.percentPos += (relSeek / MpvData.duration);
		MpvData.percentPos = Util::Clamp(MpvData.percentPos, 0.0, 1.0);
		setPositionPercent(MpvData.percentPos, false);
	}
}

void VideoplayerWindow::togglePlay() noexcept
{
	if (!isLoaded()) return;
	const char* cmd[]{ "cycle", "pause", NULL };
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::cycleSubtitles() noexcept
{
	const char* cmd[]{ "cycle", "sub", NULL};
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::cycleLoopAB() noexcept
{
	const char* cmd[]{ "ab-loop", NULL };
	mpv_command_async(mpv, 0, cmd);
	if (LoopState == LoopEnum::B_set) {
		MpvData.abLoopA = 0.f;
		MpvData.abLoopB = 0.f;
		showText("Loop cleared.");
		LoopState = LoopEnum::Clear;
	}
}

void VideoplayerWindow::closeVideo() noexcept
{
	const char* cmd[] = { "stop", NULL };
	mpv_command_async(mpv, 0, cmd);
	MpvData.videoLoaded = false;
	setPaused(true);
}

void VideoplayerWindow::NotifySwap() const noexcept
{
	mpv_render_context_report_swap(mpv_gl);
}

int32_t VideoEvents::MpvVideoLoaded = 0;
int32_t VideoEvents::WakeupOnMpvEvents = 0;
int32_t VideoEvents::WakeupOnMpvRenderUpdate = 0;
int32_t VideoEvents::PlayPauseChanged = 0;

void VideoEvents::RegisterEvents() noexcept
{
	MpvVideoLoaded = SDL_RegisterEvents(1);
	WakeupOnMpvEvents = SDL_RegisterEvents(1);
	WakeupOnMpvRenderUpdate = SDL_RegisterEvents(1);
	PlayPauseChanged = SDL_RegisterEvents(1);
}
