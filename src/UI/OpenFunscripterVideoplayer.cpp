#include "OpenFunscripterVideoplayer.h"

#include "OpenFunscripter.h"
#include "event/EventSystem.h"

#include "imgui_internal.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <cstdlib>
#include <filesystem>

#include "SDL.h"

#include "stb_sprintf.h"
#include "stb_image_write.h"

#include "portable-file-dialogs.h"

static void* get_proc_address_mpv(void* fn_ctx, const char* name)
{
	return SDL_GL_GetProcAddress(name);
}

static void on_mpv_events(void* ctx)
{
	SDL_Event event{ 0 };
	event.type = EventSystem::WakeupOnMpvEvents;
	SDL_PushEvent(&event);
}

static void on_mpv_render_update(void* ctx)
{
	SDL_Event event{ 0 };
	event.type = EventSystem::WakeupOnMpvRenderUpdate;
	SDL_PushEvent(&event);
}

void VideoplayerWindow::MpvEvents(SDL_Event& ev)
{
	while (1) {
		mpv_event* mp_event = mpv_wait_event(mpv, 0);
		if (mp_event->event_id == MPV_EVENT_NONE)
			break;
		switch (mp_event->event_id) {
		case MPV_EVENT_LOG_MESSAGE:
		{
			mpv_event_log_message* msg = (mpv_event_log_message*)mp_event->data;
			if (msg->log_level <= MPV_LOG_LEVEL_INFO) {
				switch (msg->log_level)
				{
				case MPV_LOG_LEVEL_INFO:
					LOGF_INFO("MPV (%s): %s", msg->prefix, msg->text);
					break;
				case MPV_LOG_LEVEL_FATAL:
					LOGF_ERROR("!!! MPV (%s): %s !!!", msg->prefix, msg->text);
					break;
				case MPV_LOG_LEVEL_ERROR:
					LOGF_ERROR("! MPV (%s): %s !", msg->prefix, msg->text);
					break;
				case MPV_LOG_LEVEL_WARN:
					LOGF_WARN("MPV (%s): %s", msg->prefix, msg->text);
					break;
				case MPV_LOG_LEVEL_DEBUG:
					LOGF_DEBUG("MPV (%s): %s", msg->prefix, msg->text);
					break;
				default:
					LOGF_INFO("MPV (%s): %s", msg->prefix, msg->text);
					break;
				}
			}
			continue;
		}
		case MPV_EVENT_COMMAND_REPLY:
		{
			// attach user_data to command
			// and handle it here when it finishes
			continue;
		}
		case MPV_EVENT_FILE_LOADED:
			// this is kind of useless because no frame has been decoded yet and we don't know the size
			// which is why I'm setting video_loaded = true when height & width have been updated
			//MpvData.video_loaded = true; 		
			continue;
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
				//	LOGF_DEBUG("Property \"%s\" has changed to %lf", prop->name, *(double*)prop->data);
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
				MpvData.video_width = *(int64_t*)prop->data;
				if (MpvData.video_height > 0) {
					updateRenderTexture();
					MpvData.video_loaded = true;
					notifyVideoLoaded();
				}
				break;
			}
			case MpvVideoHeight:
			{
				MpvData.video_height = *(int64_t*)prop->data;
				if (MpvData.video_width > 0) {
					updateRenderTexture();
					MpvData.video_loaded = true;
					notifyVideoLoaded();
				}
				break;
			}
			case MpvFramesPerSecond:
				MpvData.fps = *(double*)prop->data;
				MpvData.average_frame_time = (1.0 / MpvData.fps);
				break;
			case MpvDuration:
				MpvData.duration = *(double*)prop->data;
				break;
			case MpvTotalFrames:
				MpvData.total_num_frames = *(int64_t*)prop->data;
				break;
			case MpvPosition:
				MpvData.real_percent_pos = (*(double*)prop->data) / 100.0;
				if (!MpvData.paused) {
					MpvData.percent_pos = MpvData.real_percent_pos;
				}
				smooth_time = std::chrono::high_resolution_clock::now();
				break;
			case MpvSpeed:
				MpvData.current_speed = *(double*)prop->data;
				break;
			case MpvPauseState:
				MpvData.paused = *(int64_t*)prop->data;
				smooth_time = std::chrono::high_resolution_clock::now();
				break;
			case MpvFilePath:
				// I'm not sure if I own this memory :/
				// But I can't free it so I will assume I don't
				MpvData.file_path = *((const char**)(prop->data));
				break;
			}
			continue;
		}
		}
	}
}

void VideoplayerWindow::MpvRenderUpdate(SDL_Event& ev)
{
	uint64_t flags = mpv_render_context_update(mpv_gl);
	if (flags & MPV_RENDER_UPDATE_FRAME) {
		redraw_video = true;
	}
}

void VideoplayerWindow::observeProperties()
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
}

void VideoplayerWindow::renderToTexture()
{
	redraw_video = false;
	mpv_opengl_fbo fbo{ 0 };
	fbo.fbo = framebuffer_obj; fbo.w = MpvData.video_width; fbo.h = MpvData.video_height;
	int enable = 1;
	int disable = 0;
	mpv_render_param params[] = {
		{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
		{MPV_RENDER_PARAM_FLIP_Y, &enable},
		// without this the whole application slows down to the framerate of the video
		{MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &disable}, 
		mpv_render_param{}
	};
	mpv_render_context_render(mpv_gl, params);
}

void VideoplayerWindow::updateRenderTexture()
{
	if (framebuffer_obj == 0) {
		glGenFramebuffers(1, &framebuffer_obj);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_obj);

		glGenTextures(1, &render_texture);
		glBindTexture(GL_TEXTURE_2D, render_texture);
		const int default_width = 1920;
		const int default_height = 1080;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, default_width, default_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, render_texture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); 

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create framebuffer for video!");
		}
	}
	else {
		// update size of render texture based on video resolution
		glBindTexture(GL_TEXTURE_2D, render_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MpvData.video_width, MpvData.video_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	}
}

bool VideoplayerWindow::setup()
{
	auto app = OpenFunscripter::ptr;
	app->events->Subscribe(EventSystem::WakeupOnMpvEvents, EVENT_SYSTEM_BIND(this, &VideoplayerWindow::MpvEvents));
	app->events->Subscribe(EventSystem::WakeupOnMpvRenderUpdate, EVENT_SYSTEM_BIND(this, &VideoplayerWindow::MpvRenderUpdate));
	app->events->Subscribe(SDL_MOUSEWHEEL, EVENT_SYSTEM_BIND(this, &VideoplayerWindow::mouse_scroll));
	
	updateRenderTexture();

	mpv = mpv_create();
	if (mpv_initialize(mpv) < 0) {
		LOG_ERROR("mpv context init failed");
		return false;
	}

	bool suc;
	// hardware decoding. only important when running 5k vr footage
	if (OpenFunscripter::ptr->settings->data().force_hw_decoding) {
		suc = mpv_set_property_string(mpv, "hwdec", "auto-safe") == 0;
		if (!suc)
			LOG_WARN("failed to set mpv hardware decoding to \"auto-safe\"");
	}
	
	// without this the file gets closed when the end is reached
	suc = mpv_set_property_string(mpv, "keep-open", "yes") == 0;
	if (!suc)
		LOG_WARN("failed to set mpv: keep-open=yes");

	// looping
	suc = mpv_set_property_string(mpv, "loop-file", "inf") == 0;
	if (!suc)
		LOG_WARN("failed to set mpv: loop-file=inf");

	mpv_request_log_messages(mpv, "debug");

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

	// This makes mpv use the currently set GL context. It will use the callback
	// (passed via params) to resolve GL builtin functions, as well as extensions.
	if (mpv_render_context_create(&mpv_gl, mpv, params) < 0) {
		LOG_ERROR("failed to initialize mpv GL context");
		return false;
	}

	// When normal mpv events are available.
	mpv_set_wakeup_callback(mpv, on_mpv_events, NULL);

	// When there is a need to call mpv_render_context_update(), which can
	// request a new frame to be rendered.
	// (Separate from the normal event handling mechanism for the sake of
	//  users which run OpenGL on a different thread.)
	mpv_render_context_set_update_callback(mpv_gl, on_mpv_render_update, NULL);

	observeProperties();

	setup_vr_mode();
	return true;
}

VideoplayerWindow::~VideoplayerWindow()
{
	mpv_render_context_free(mpv_gl);
	mpv_detach_destroy(mpv);

	// TODO: free gl resources
}

void VideoplayerWindow::mouse_scroll(SDL_Event& ev)
{
	auto scroll = ev.wheel;
	if (videoHovered) {
		auto mouse_pos_in_vid = ImGui::GetMousePos() - viewport_pos - settings.video_pos;
		float zoom_point_x = (mouse_pos_in_vid.x - (video_draw_size.x/2.f)) / video_draw_size.x;
		float zoom_point_y = (mouse_pos_in_vid.y - (video_draw_size.y/2.f)) / video_draw_size.y;

		float vid_width = MpvData.video_width;
		float vid_height = MpvData.video_height;

		switch (settings.activeMode) {
		case VideoMode::LEFT_PANE:
		case VideoMode::RIGHT_PANE:
			vid_width /= 2.f;
			break;
		case VideoMode::TOP_PANE:
		case VideoMode::BOTTOM_PANE:
			vid_height /= 2.f;
			break;
		}
		zoom_point_x *= vid_width;
		zoom_point_y *= vid_height;

		const float old_scale = settings.zoom_factor;
		// apply zoom
		if (settings.activeMode == VideoMode::VR_MODE) {
			settings.vr_zoom *= ((1+(zoom_multi * scroll.y)));
			settings.vr_zoom = Util::Clamp(settings.vr_zoom, 0.05f, 2.0f);
			return;
		}

		settings.zoom_factor *= 1 + (zoom_multi * scroll.y);
		settings.zoom_factor = Util::Clamp(settings.zoom_factor, 0.0f, 10.f);

		const float scale_change = (settings.zoom_factor - old_scale) * base_scale_factor;
		const float offset_x = -(zoom_point_x * scale_change);
		const float offset_y = -(zoom_point_y * scale_change);

		settings.prev_translation.x += offset_x;
		settings.prev_translation.y += offset_y;


		if (!dragStarted) {
			settings.current_translation = settings.prev_translation;
		}
	}
}

void VideoplayerWindow::setup_vr_mode()
{
	// VR MODE
	// setup shader
	const char* vtx_shader = R"(
		#version 330 core
		uniform mat4 ProjMtx;
		in vec2 Position;
		in vec2 UV;
		in vec4 Color;
		out vec2 Frag_UV;
		out vec4 Frag_Color;
		void main()
		{
			Frag_UV = UV;
			Frag_Color = Color;
			gl_Position = ProjMtx * vec4(Position.xy,0,1);
		}
	)";

	// shader from https://www.shadertoy.com/view/4lK3DK
	const char* frag_shader = R"(

		#version 330 core
		uniform sampler2D Texture;
		uniform vec2 rotation;
		uniform float zoom;
		uniform float aspect_ratio;

		in vec2 Frag_UV;
		in vec4 Frag_Color;

		out vec4 Out_Color;
		#define PI 3.1415926535
		#define DEG2RAD 0.01745329251994329576923690768489
		
		float hfovDegrees = 75.0;
		float vfovDegrees = 59.0;

		vec3 rotateXY(vec3 p, vec2 angle) {
			vec2 c = cos(angle), s = sin(angle);
			p = vec3(p.x, c.x*p.y + s.x*p.z, -s.x*p.y + c.x*p.z);
			return vec3(c.y*p.x + s.y*p.z, p.y, -s.y*p.x + c.y*p.z);
		}

		void main()
		{
			float inverse_aspect = 1.f / aspect_ratio;
			float hfovRad = hfovDegrees * DEG2RAD;
			float vfovRad = -2.f * atan(tan(hfovRad/2.f)*inverse_aspect);

			vec2 uv = vec2(Frag_UV.s - 0.5, Frag_UV.t - 0.5);

			//to spherical
			vec3 camDir = normalize(vec3(uv.xy * vec2(tan(0.5 * hfovRad), tan(0.5 * vfovRad)), zoom));
			//camRot is angle vec in rad
			vec3 camRot = vec3( (rotation - 0.5) * vec2(2.0 * PI,  PI), 0.);

			//rotate
			vec3 rd = normalize(rotateXY(camDir, camRot.yx));

			//radial azmuth polar
			vec2 texCoord = vec2(atan(rd.z, rd.x) + PI, acos(-rd.y)) / vec2(2.0 * PI, PI);

			Out_Color = texture(Texture, texCoord);
		}
	)";

	unsigned int vertex, fragment;
	int success;
	char infoLog[512];
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vtx_shader, NULL);
	glCompileShader(vertex);

	// print compile errors if any
	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertex, 512, NULL, infoLog);
		LOGF_ERROR("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s", infoLog);
	};

	// similiar for Fragment Shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &frag_shader, NULL);
	glCompileShader(fragment);

	// print compile errors if any
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragment, 512, NULL, infoLog);
		LOGF_ERROR("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s", infoLog);
	};

	// shader Program
	vr_shader = glCreateProgram();
	glAttachShader(vr_shader, vertex);
	glAttachShader(vr_shader, fragment);
	glLinkProgram(vr_shader);
	// print linking errors if any
	glGetProgramiv(vr_shader, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(vr_shader, 512, NULL, infoLog);
		LOGF_ERROR("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s", infoLog);
	}

	glUseProgram(vr_shader);
	glUniform1i(glGetUniformLocation(vr_shader, "Texture"), GL_TEXTURE0);

	// delete the shaders as they're linked into our program now and no longer necessary
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void VideoplayerWindow::notifyVideoLoaded()
{
	SDL_Event ev;
	ev.type = EventSystem::MpvVideoLoaded;
	SDL_PushEvent(&ev);
}


void VideoplayerWindow::DrawVideoPlayer(bool* open)
{
	if (MpvData.video_loaded) {
		ImGui::Begin(PlayerId, open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

		// this redraw has to happen even if the video isn't actually shown in the gui
		if (redraw_video) { renderToTexture(); }

		if (OpenFunscripter::ptr->settings->data().draw_video) {
			viewport_pos = ImGui::GetWindowViewport()->Pos;

			ImVec2 videoSize(MpvData.video_width, MpvData.video_height);
			ImVec2 dst = ImGui::GetContentRegionAvail();
			base_scale_factor = std::min(dst.x / videoSize.x, dst.y / videoSize.y);
			videoSize.x *= base_scale_factor;
			videoSize.y *= base_scale_factor;

			ImVec2 uv0(0.f, 1.f);
			ImVec2 uv1(1.f, 0.f);

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

			if (settings.activeMode == VideoMode::VR_MODE) {
				if (videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
					dragStarted = true;
				}
				// apply drag to translation
				else if (dragStarted && videoHovered)
				{
					settings.current_vr_rotation =
						settings.prev_vr_rotation
						+ (ImGui::GetMouseDragDelta(ImGuiMouseButton_Left) 
							/ ImVec2((10000.f * settings.vr_zoom), (video_draw_size.y / video_draw_size.x) * 10000.f * settings.vr_zoom));
				}

				player_viewport = ImGui::GetCurrentWindowRead()->Viewport;
				ImGui::GetWindowDrawList()->AddCallback(
					[](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
						auto& ctx = *(VideoplayerWindow*)cmd->UserCallbackData;

						auto draw_data = ctx.player_viewport->DrawData;
						glUseProgram(ctx.vr_shader);

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
						glUniformMatrix4fv(glGetUniformLocation(ctx.vr_shader, "ProjMtx"), 1, GL_FALSE, &ortho_projection[0][0]);
						glUniform2fv(glGetUniformLocation(ctx.vr_shader, "rotation"), 1, &ctx.settings.current_vr_rotation.x);
						glUniform1f(glGetUniformLocation(ctx.vr_shader, "zoom"), ctx.settings.vr_zoom);
						glUniform1f(glGetUniformLocation(ctx.vr_shader, "aspect_ratio"), ctx.video_draw_size.x / ctx.video_draw_size.y);
					}, this);
				ImGui::Image((void*)(intptr_t)render_texture, ImGui::GetContentRegionAvail(), uv0, uv1);
				video_draw_size = ImGui::GetItemRectSize();
				// TODO: ImDrawCallback_ResetRenderState unnecessary. instead just reset the shader
				ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
			}
			else {
				videoSize = videoSize * ImVec2(settings.zoom_factor, settings.zoom_factor);
				settings.video_pos = (ImGui::GetWindowSize() - videoSize) * 0.5f + settings.current_translation;
				ImGui::SetCursorPos(settings.video_pos);
				// the videoHovered is one frame old but moving this up prevents flicker while dragging and zooming at the same time
				// start video dragging
				if (videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
					dragStarted = true;
				}
				// apply drag to translation
				else if(dragStarted && videoHovered)
				{
					settings.current_translation = settings.prev_translation + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
				}
				ImGui::Image((void*)(intptr_t)render_texture, videoSize, uv0, uv1);
			}
			videoHovered = ImGui::IsItemHovered();
			video_draw_size = ImGui::GetItemRectSize();

			// cancel drag
			if ((dragStarted && !videoHovered) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				dragStarted = false;
				settings.prev_translation = settings.current_translation;
				settings.prev_vr_rotation = settings.current_vr_rotation;
			}

			// recenter
			if (videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
				resetTranslationAndZoom();
		}
		else {
			if (ImGui::Button("Click to enable video")) {
				OpenFunscripter::ptr->settings->data().draw_video = true;
				OpenFunscripter::ptr->settings->saveSettings();
			}
		}
		ImGui::End();
	}
}

void VideoplayerWindow::setSpeed(float speed) noexcept
{
	speed = Util::Clamp<float>(speed, minPlaybackSpeed, maxPlaybackSpeed);
	if (settings.playback_speed != speed) {
		settings.playback_speed = speed;
		stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.3f", speed);
		const char* cmd[]{ "set", "speed", tmp_buf, NULL };
		mpv_command_async(mpv, 0, cmd);
	}
}

void VideoplayerWindow::addSpeed(float speed) noexcept
{
	settings.playback_speed += speed;
	settings.playback_speed = Util::Clamp<float>(settings.playback_speed, minPlaybackSpeed, maxPlaybackSpeed);
	setSpeed(settings.playback_speed);
	//stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.3f", speed);
	//const char* cmd[]{ "add", "speed", tmp_buf, NULL };
	//mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::openVideo(const std::string& file)
{
	LOGF_INFO("Opening video: \"%s\"", file.c_str());
	MpvData.video_loaded = false;

	closeVideo();

	const char* cmd[] = { "loadfile", file.c_str(), NULL };
	mpv_command_async(mpv, 0, cmd);
	MpvData.video_width = 0;
	MpvData.video_height = 0;

	setPaused(true);
	setVolume(settings.volume);
	setSpeed(settings.playback_speed);
	resetTranslationAndZoom();
}

void VideoplayerWindow::saveFrameToImage(const std::string& directory)
{
	// there's probably a builtin screenshot function in libmpv 
	// I should probably use that instead
	static ScreenshotSavingThreadData threadData{0};
	if (threadData.dataBuffer != nullptr) return; // saving in progress

	std::error_code ec;
	std::filesystem::create_directories(directory, ec);
	if (ec) {
		LOGF_ERROR("Failed to create screenshot. Couldn't create \"%s\" directory", directory.c_str());
		return;
	}

	std::stringstream ss;
	std::filesystem::path currentFile(getVideoPath());
	std::string filename = currentFile.filename().replace_extension("").string();
	std::array<char, 15> tmp;
	double time = getCurrentPositionSeconds();
	Util::FormatTime(tmp.data(), tmp.size(), time, true);
	std::replace(tmp.begin(), tmp.end(), ':', '_');
	ss << filename << '_' << tmp.data() << '-' << ".png";
	
	GLint drawFboId = 0, readFboId = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);

	int rowPack;
	glGetIntegerv(GL_PACK_ALIGNMENT, &rowPack);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	const size_t buff_size = MpvData.video_width * MpvData.video_height * 3;
	threadData.dataBuffer = new uint8_t[buff_size];
	threadData.w = MpvData.video_width;
	threadData.h = MpvData.video_height;
	threadData.filename = (std::filesystem::path(directory) / ss.str()).string();

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_obj);
	glReadnPixels(0, 0, MpvData.video_width, MpvData.video_height, GL_RGB, GL_UNSIGNED_BYTE, buff_size, threadData.dataBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, drawFboId);

	glPixelStorei(GL_PACK_ALIGNMENT, rowPack);

	auto saveThread = [](void* user) -> int {
		auto ctx = (ScreenshotSavingThreadData*)user;
		stbi_flip_vertically_on_write(true);
		stbi_write_png(ctx->filename.c_str(),
			ctx->w, ctx->h,
			3, ctx->dataBuffer, 0
		);

		delete[] ctx->dataBuffer;
		ctx->dataBuffer = nullptr;
		return 0;
	};


	auto handle = SDL_CreateThread(saveThread, "SaveVideoFrameThread", &threadData);
	SDL_DetachThread(handle);
}

void VideoplayerWindow::setVolume(float volume) noexcept
{
	stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.2f", (float)(volume*100.f));
	const char* cmd[]{"set", "volume", tmp_buf, NULL};
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::setPosition(float pos, bool pausesVideo) noexcept
{
	MpvData.percent_pos = pos;
	stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.08f%", (float)(pos * 100.0f));
	const char* cmd[]{ "seek", tmp_buf, "absolute-percent+exact", NULL };
	if (pausesVideo) {
		setPaused(true);
	}
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::setPaused(bool paused) noexcept
{
	MpvData.paused = paused;
	mpv_set_property_async(mpv, 0, "pause", MPV_FORMAT_FLAG, &MpvData.paused);
}

void VideoplayerWindow::nextFrame() noexcept
{
	if (isPaused()) {
		// use same method as previousFrame for consistency
		float relSeek = ((getFrameTimeMs() * 1.001f) / 1000.f);
		MpvData.percent_pos += (relSeek / MpvData.duration);
		MpvData.percent_pos = Util::Clamp(MpvData.percent_pos, 0.0, 1.0);

		stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.08f", relSeek);
		const char* cmd[]{ "seek", tmp_buf, "exact", NULL };
		//const char* cmd[]{ "frame-step", NULL };
		mpv_command_async(mpv, 0, cmd);
	}
}

void VideoplayerWindow::previousFrame() noexcept
{
	if (isPaused()) {
		// this seeks much faster
		// https://github.com/mpv-player/mpv/issues/4019#issuecomment-358641908
		float relSeek = ((getFrameTimeMs() * 1.001f) / 1000.f);
		MpvData.percent_pos -= (relSeek / MpvData.duration);
		MpvData.percent_pos = Util::Clamp(MpvData.percent_pos, 0.0, 1.0);

		stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "-%.08f", relSeek);
		const char* cmd[]{ "seek", tmp_buf, "exact", NULL };
		//const char* cmd[]{ "frame-back-step", NULL };
		mpv_command_async(mpv, 0, cmd);
	}
}

void VideoplayerWindow::relativeFrameSeek(int32_t seek) noexcept
{
	if (isPaused()) {
		float relSeek = ((getFrameTimeMs() * 1.001f) / 1000.f) * seek;
		MpvData.percent_pos += (relSeek / MpvData.duration);
		MpvData.percent_pos = Util::Clamp(MpvData.percent_pos, 0.0, 1.0);

		stbsp_snprintf(tmp_buf, sizeof(tmp_buf), "%.08f", relSeek);
		const char* cmd[]{ "seek", tmp_buf, "exact", NULL };
		mpv_command_async(mpv, 0, cmd);
	}
}

void VideoplayerWindow::togglePlay() noexcept
{
	const char* cmd[]{ "cycle", "pause", NULL };
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::cycleSubtitles() noexcept
{
	const char* cmd[]{ "cycle", "sub", NULL};
	mpv_command_async(mpv, 0, cmd);
}

void VideoplayerWindow::closeVideo() noexcept
{
	const char* cmd[] = { "stop", NULL };
	mpv_command_async(mpv, 0, cmd);
	MpvData.video_loaded = false;
}
