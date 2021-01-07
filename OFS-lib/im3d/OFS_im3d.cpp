#include "OFS_im3d.h"
#include "OFS_Util.h"

#include "SDL_keycode.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "glad/glad.h"

#include "im3d_math.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <string>
#include <sstream>

static constexpr const char* im3d_shader = R"(
#if !defined(POINTS) && !defined(LINES) && !defined(TRIANGLES)
	#error No primitive type defined
#endif
#if !defined(VERTEX_SHADER) && !defined(GEOMETRY_SHADER) && !defined(FRAGMENT_SHADER)
	#error No shader stage defined
#endif

#define VertexData \
	_VertexData { \
		noperspective float m_edgeDistance; \
		noperspective float m_size; \
		smooth vec4 m_color; \
	}

#define kAntialiasing 2.0

#ifdef VERTEX_SHADER
	uniform mat4 uViewProjMatrix;
	
	layout(location=0) in vec4 aPositionSize;
	layout(location=1) in vec4 aColor;
	
	out VertexData vData;
	
	void main() 
	{
		vData.m_color = aColor.abgr; // swizzle to correct endianness
		#if !defined(TRIANGLES)
			vData.m_color.a *= smoothstep(0.0, 1.0, aPositionSize.w / kAntialiasing);
		#endif
		vData.m_size = max(aPositionSize.w, kAntialiasing);
		gl_Position = uViewProjMatrix * vec4(aPositionSize.xyz, 1.0);
		#if defined(POINTS)
			gl_PointSize = vData.m_size;
		#endif
	}
#endif

#ifdef GEOMETRY_SHADER
 // expand line -> triangle strip
	layout(lines) in;
	layout(triangle_strip, max_vertices = 4) out;
	
	uniform vec2 uViewport;
	
	in  VertexData vData[];
	out VertexData vDataOut;
	
	void main() 
	{
		vec2 pos0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
		vec2 pos1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;
		
		vec2 dir = pos0 - pos1;
		dir = normalize(vec2(dir.x, dir.y * uViewport.y / uViewport.x)); // correct for aspect ratio
		vec2 tng0 = vec2(-dir.y, dir.x);
		vec2 tng1 = tng0 * vData[1].m_size / uViewport;
		tng0 = tng0 * vData[0].m_size / uViewport;
		
	 // line start
		gl_Position = vec4((pos0 - tng0) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw); 
		vDataOut.m_edgeDistance = -vData[0].m_size;
		vDataOut.m_size = vData[0].m_size;
		vDataOut.m_color = vData[0].m_color;
		EmitVertex();
		
		gl_Position = vec4((pos0 + tng0) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw);
		vDataOut.m_color = vData[0].m_color;
		vDataOut.m_edgeDistance = vData[0].m_size;
		vDataOut.m_size = vData[0].m_size;
		EmitVertex();
		
	 // line end
		gl_Position = vec4((pos1 - tng1) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
		vDataOut.m_edgeDistance = -vData[1].m_size;
		vDataOut.m_size = vData[1].m_size;
		vDataOut.m_color = vData[1].m_color;
		EmitVertex();
		
		gl_Position = vec4((pos1 + tng1) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
		vDataOut.m_color = vData[1].m_color;
		vDataOut.m_size = vData[1].m_size;
		vDataOut.m_edgeDistance = vData[1].m_size;
		EmitVertex();
	}
#endif

#ifdef FRAGMENT_SHADER
	in VertexData vData;
	
	layout(location=0) out vec4 fResult;
	
	void main() 
	{
		fResult = vData.m_color;
		
		#if   defined(LINES)
			float d = abs(vData.m_edgeDistance) / vData.m_size;
			d = smoothstep(1.0, 1.0 - (kAntialiasing / vData.m_size), d);
			fResult.a *= d;
			
		#elif defined(POINTS)
			float d = length(gl_PointCoord.xy - vec2(0.5));
			d = smoothstep(0.5, 0.5 - (kAntialiasing / vData.m_size), d);
			fResult.a *= d;
			
		#endif		
	}
#endif
)";

static bool LoadShader(const char* shader, const char* _defines, std::stringstream& ss)
{
	if (_defines)
	{
		while (*_defines != '\0')
		{
			ss << "#define ";
			ss << _defines << "\n";
			_defines = strchr(_defines, 0);
			IM3D_ASSERT(_defines);
			++_defines;
		}
	}
	ss << shader;
	return true;
}

static GLuint LoadCompileShader(GLenum _stage, const char* shader, const char* _defines)
{
	std::string src;
	{
		std::stringstream ss;
		ss << "#version 330 core\n";
		if (!LoadShader(shader, _defines, ss))
		{
			return 0;
		}
		src = ss.str();
	}

	GLuint ret = 0;
	ret = glCreateShader(_stage);
	const GLchar* pd = src.c_str();
	GLint ps = src.size();
	glShaderSource(ret, 1, &pd, &ps);

	glCompileShader(ret);
	GLint compileStatus = GL_FALSE;
	glGetShaderiv(ret, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE)
	{
		GLint len;
		glGetShaderiv(ret, GL_INFO_LOG_LENGTH, &len);
		char* log = new GLchar[len];
		glGetShaderInfoLog(ret, len, 0, log);
		LOG_ERROR(log);
		delete[] log;

		glDeleteShader(ret);

		return 0;
	}

	return ret;
}

static bool LinkShaderProgram(GLuint _handle)
{
	IM3D_ASSERT(_handle != 0);

	glLinkProgram(_handle);
	GLint linkStatus = GL_FALSE;
	glGetProgramiv(_handle, GL_LINK_STATUS, &linkStatus);
	if (linkStatus == GL_FALSE)
	{
		LOG_ERROR("Error linking program:\n\n");
		GLint len;
		glGetProgramiv(_handle, GL_INFO_LOG_LENGTH, &len);
		GLchar* log = new GLchar[len];
		glGetProgramInfoLog(_handle, len, 0, log);
		LOG_ERROR(log);
		delete[] log;

		return false;
	}

	return true;
}

using namespace Im3d;

struct OFS_Im3d_Context
{
	//bool init(int _width, int _height, const char* _title);
	//void shutdown();
	inline void update(float width, float height) noexcept {
		m_camWorld = LookAt(m_camPos, m_camPos - m_camDir);
		m_camView = Inverse(m_camWorld);

		m_camFovRad = Im3d::Radians(m_camFovDeg);
		float n = 0.1f;
		float f = 500.0f;
		float a = width / height;
		float scale = tanf(m_camFovRad * 0.5f) * n;
		float viewZ = -1.0f;

		if (m_camOrtho)
		{
			// ortho proj
			scale = 3.0f;
			float r = scale * a;
			float l = -scale * a;
			float t = scale;
			float b = -scale;
			m_camProj = Mat4(
				2.0f / (r - l), 0.0f, 0.0f, (r + l) / (l - r),
				0.0f, 2.0f / (t - b), 0.0f, (t + b) / (b - t),
				0.0f, 0.0f, 2.0f / (n - f), (n + f) / (n - f),
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}
		else
		{
			// infinite perspective proj
			float r = a * scale;
			float l = -r;
			float t = scale;
			float b = -t;

			m_camProj = Mat4(
				2.0f * n / (r - l), 0.0f, -viewZ * (r + l) / (r - l), 0.0f,
				0.0f, 2.0f * n / (t - b), -viewZ * (t + b) / (t - b), 0.0f,
				0.0f, 0.0f, viewZ, -2.0f * n,
				0.0f, 0.0f, viewZ, 0.0f
			);
		}

		m_camWorld = LookAt(m_camPos, m_camPos + m_camDir * viewZ);
		m_camView = Inverse(m_camWorld);
		m_camViewProj = m_camProj * m_camView;
	}
	//void draw();

	// window 
	//int m_width, m_height;
	//const char* m_title;
	//Vec2  m_prevCursorPos;

	OFS_Im3d_Context() {
		m_camDir = glm::vec3(0.f, 0.f, -1.f);
		m_camViewProj = glm::mat4(1.f);
		m_camWorld = glm::mat4(1.f);
		m_camView = glm::mat4(1.f);
		m_camFovDeg = 60.f;
		m_camFovRad = glm::radians(m_camFovDeg);
		m_camPos = glm::vec3(0.f,0.f, 0.f);
		m_camOrtho = true;
	}

	//bool hasFocus() const;
	inline Vec2 getWindowRelativeCursor() const {
		auto& io = ImGui::GetIO();
		auto viewport = ImGui::GetMainViewport();
		auto relPos = io.MousePos - viewport->Pos;
		return Vec2(relPos.x, relPos.y);
	}

	// 3d camera
	bool  m_camOrtho;
	Vec3  m_camPos;
	Vec3  m_camDir;
	float m_camFovDeg;
	float m_camFovRad;
	Mat4  m_camWorld;
	Mat4  m_camView;
	Mat4  m_camProj;
	Mat4  m_camViewProj;

	float m_deltaTime;

	// text rendering
	inline void drawTextDrawListsImGui(const Im3d::TextDrawList _textDrawLists[], U32 _count) {
		
		auto viewport = ImGui::GetMainViewport();
		// Using ImGui here as a simple means of rendering text draw lists, however as with primitives the application is free to draw text in any conceivable  manner.
		// Invisible ImGui window which covers the screen.
		ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::Begin("Invisible", nullptr, 0
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoInputs
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoBringToFrontOnFocus
		);

		ImDrawList* imDrawList = ImGui::GetWindowDrawList();
		const Mat4 viewProj = m_camViewProj;
		for (U32 i = 0; i < _count; ++i)
		{
			const TextDrawList& textDrawList = Im3d::GetTextDrawLists()[i];

			if (textDrawList.m_layerId == Im3d::MakeId("NamedLayer"))
			{
				// The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
			}

			for (U32 j = 0; j < textDrawList.m_textDataCount; ++j)
			{
				const Im3d::TextData& textData = textDrawList.m_textData[j];
				if (textData.m_positionSize.w == 0.0f || textData.m_color.getA() == 0.0f)
				{
					continue;
				}

				// Project world -> screen space.
				Vec4 clip = viewProj * Vec4(textData.m_positionSize.x, textData.m_positionSize.y, textData.m_positionSize.z, 1.0f);
				ImVec2 screen = ImVec2(clip.x / clip.w, clip.y / clip.w);

				// Cull text which falls offscreen. Note that this doesn't take into account text size but works well enough in practice.
				if (clip.w < 0.0f || screen.x >= 1.0f || screen.y >= 1.0f)
				{
					continue;
				}

				// Pixel coordinates for the ImGuiWindow ImGui.
				screen = screen * ImVec2(0.5f, 0.5f) + ImVec2(0.5f, 0.5f);
				screen.y = 1.0f - screen.y; // screen space origin is reversed by the projection.
				screen = screen * ImVec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);

				// All text data is stored in a single buffer; each textData instance has an offset into this buffer.
				const char* text = textDrawList.m_textBuffer + textData.m_textBufferOffset;

				// Calculate the final text size in pixels to apply alignment flags correctly.
				ImGui::SetWindowFontScale(textData.m_positionSize.w); // NB no CalcTextSize API which takes a font/size directly...
				ImVec2 textSize = ImGui::CalcTextSize(text, text + textData.m_textLength);
				ImGui::SetWindowFontScale(1.0f);

				// Generate a pixel offset based on text flags.
				ImVec2 textOffset = ImVec2(-textSize.x * 0.5f, -textSize.y * 0.5f); // default to center
				if ((textData.m_flags & Im3d::TextFlags_AlignLeft) != 0)
				{
					textOffset.x = -textSize.x;
				}
				else if ((textData.m_flags & Im3d::TextFlags_AlignRight) != 0)
				{
					textOffset.x = 0.0f;
				}

				if ((textData.m_flags & Im3d::TextFlags_AlignTop) != 0)
				{
					textOffset.y = -textSize.y;
				}
				else if ((textData.m_flags & Im3d::TextFlags_AlignBottom) != 0)
				{
					textOffset.y = 0.0f;
				}

				// Add text to the window draw list.
				screen = screen + textOffset;
				imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(), screen, textData.m_color.getABGR(), text, text + textData.m_textLength);
			}
		}

		ImGui::End();
		ImGui::PopStyleColor(1);
	}
};

static OFS_Im3d_Context* g_Example;


static GLuint g_Im3dVertexArray;
static GLuint g_Im3dVertexBuffer;
static GLuint g_Im3dShaderPoints;
static GLuint g_Im3dShaderLines;
static GLuint g_Im3dShaderTriangles;

bool OFS::Im3d_Init() noexcept
{
	{	
		GLuint vs = LoadCompileShader(GL_VERTEX_SHADER, im3d_shader, "VERTEX_SHADER\0POINTS\0");
		GLuint fs = LoadCompileShader(GL_FRAGMENT_SHADER, im3d_shader, "FRAGMENT_SHADER\0POINTS\0");
		if (vs && fs) {
			g_Im3dShaderPoints = glCreateProgram();
			glAttachShader(g_Im3dShaderPoints, vs);
			glAttachShader(g_Im3dShaderPoints, fs);
			bool ret = LinkShaderProgram(g_Im3dShaderPoints);
			glDeleteShader(vs);
			glDeleteShader(fs);
			if (!ret)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	{	
		GLuint vs = LoadCompileShader(GL_VERTEX_SHADER, im3d_shader, "VERTEX_SHADER\0LINES\0");
		GLuint gs = LoadCompileShader(GL_GEOMETRY_SHADER, im3d_shader, "GEOMETRY_SHADER\0LINES\0");
		GLuint fs = LoadCompileShader(GL_FRAGMENT_SHADER, im3d_shader, "FRAGMENT_SHADER\0LINES\0");
		if (vs && gs && fs)
		{
			g_Im3dShaderLines = glCreateProgram();
			glAttachShader(g_Im3dShaderLines, vs);
			glAttachShader(g_Im3dShaderLines, gs);
			glAttachShader(g_Im3dShaderLines, fs);
			bool ret = LinkShaderProgram(g_Im3dShaderLines);
			glDeleteShader(vs);
			glDeleteShader(gs);
			glDeleteShader(fs);
			if (!ret)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	{	
		GLuint vs = LoadCompileShader(GL_VERTEX_SHADER, im3d_shader, "VERTEX_SHADER\0TRIANGLES\0");
		GLuint fs = LoadCompileShader(GL_FRAGMENT_SHADER, im3d_shader, "FRAGMENT_SHADER\0TRIANGLES\0");
		if (vs && fs)
		{
			g_Im3dShaderTriangles = glCreateProgram();
			glAttachShader(g_Im3dShaderTriangles, vs);
			glAttachShader(g_Im3dShaderTriangles, fs);
			bool ret = LinkShaderProgram(g_Im3dShaderTriangles);
			glDeleteShader(vs);
			glDeleteShader(fs);
			if (!ret)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	glGenBuffers(1, &g_Im3dVertexBuffer);
	glGenVertexArrays(1, &g_Im3dVertexArray);
	glBindVertexArray(g_Im3dVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, g_Im3dVertexBuffer);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Im3d::VertexData), (GLvoid*)offsetof(Im3d::VertexData, m_positionSize));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Im3d::VertexData), (GLvoid*)offsetof(Im3d::VertexData, m_color));
	glBindVertexArray(0);

	g_Example = new OFS_Im3d_Context();
	return false;
}

void OFS::Im3d_NewFrame() noexcept
{
	auto& ad = Im3d::GetAppData();
	auto& io = ImGui::GetIO();
	auto viewport = ImGui::GetMainViewport();

	g_Example->update(viewport->Size.x, viewport->Size.y);
	

	ad.m_deltaTime = g_Example->m_deltaTime;
	ad.m_viewportSize = Vec2(viewport->Size.x, viewport->Size.y);
	ad.m_viewOrigin = g_Example->m_camPos; // for VR use the head position
	ad.m_viewDirection = g_Example->m_camDir;
	ad.m_worldUp = Vec3(0.0f, 1.0f, 0.0f); // used internally for generating orthonormal bases
	ad.m_projOrtho = g_Example->m_camOrtho;

	// m_projScaleY controls how gizmos are scaled in world space to maintain a constant screen height
	ad.m_projScaleY = g_Example->m_camOrtho
		? 2.0f / g_Example->m_camProj(1, 1) // use far plane height for an ortho projection
		: tanf(g_Example->m_camFovRad * 0.5f) * 2.0f // or vertical fov for a perspective projection
		;

	// World space cursor ray from mouse position; for VR this might be the position/orientation of the HMD or a tracked controller.
	Vec2 cursorPos = g_Example->getWindowRelativeCursor();
	{
		cursorPos = (cursorPos / ad.m_viewportSize) * 2.0f;
		cursorPos.x -= 1.f;
		cursorPos.y -= 1.f;
	}
	cursorPos.y = -cursorPos.y; // window origin is top-left, ndc is bottom-left
	Vec3 rayOrigin, rayDirection;
	if (g_Example->m_camOrtho)
	{
		rayOrigin.x = cursorPos.x / g_Example->m_camProj(0, 0);
		rayOrigin.y = cursorPos.y / g_Example->m_camProj(1, 1);
		rayOrigin.z = 0.0f;
		rayOrigin = g_Example->m_camWorld * Vec4(rayOrigin, 1.0f);
		rayDirection = g_Example->m_camWorld * Vec4(0.0f, 0.0f, -1.0f, 0.0f);

	}
	else
	{
		rayOrigin = ad.m_viewOrigin;
		rayDirection.x = cursorPos.x / g_Example->m_camProj(0, 0);
		rayDirection.y = cursorPos.y / g_Example->m_camProj(1, 1);
		rayDirection.z = -1.0f;
		rayDirection = g_Example->m_camWorld * Vec4(Normalize(rayDirection), 0.0f);
	}
	ad.m_cursorRayOrigin = rayOrigin;
	ad.m_cursorRayDirection = rayDirection;

	// Set cull frustum planes. This is only required if IM3D_CULL_GIZMOS or IM3D_CULL_PRIMTIIVES is enable via
	// im3d_config.h, or if any of the IsVisible() functions are called.
	ad.setCullFrustum(g_Example->m_camViewProj, true);

	// Fill the key state array; using GetAsyncKeyState here but this could equally well be done via the window proc.
	// All key states have an equivalent (and more descriptive) 'Action_' enum.
	ad.m_keyDown[Im3d::Mouse_Left/*Im3d::Action_Select*/] = ImGui::IsMouseDown(ImGuiMouseButton_Left);

	// The following key states control which gizmo to use for the generic Gizmo() function. Here using the left ctrl
	// key as an additional predicate.
	bool ctrlDown = ImGui::GetMergedKeyModFlags() & ImGuiKeyModFlags_Ctrl;
	ad.m_keyDown[Im3d::Key_L/*Action_GizmoLocal*/] = ctrlDown && ImGui::IsKeyDown(SDLK_l);;
	ad.m_keyDown[Im3d::Key_T/*Action_GizmoTranslation*/] = ctrlDown && ImGui::IsKeyDown(SDLK_t);
	ad.m_keyDown[Im3d::Key_R/*Action_GizmoRotation*/] = ctrlDown && ImGui::IsKeyDown(SDLK_r);
	ad.m_keyDown[Im3d::Key_S/*Action_GizmoScale*/] = ctrlDown && ImGui::IsKeyDown(SDLK_s);

	// Enable gizmo snapping by setting the translation/rotation/scale increments to be > 0
	ad.m_snapTranslation = ctrlDown ? 0.5f : 0.0f;
	ad.m_snapRotation = ctrlDown ? Im3d::Radians(30.0f) : 0.0f;
	ad.m_snapScale = ctrlDown ? 0.5f : 0.0f;

	Im3d::NewFrame();
}

void OFS::Im3d_EndFrame() noexcept
{
	Im3d::EndFrame();

	auto& io = ImGui::GetIO();
	auto viewport = ImGui::GetMainViewport();

	// Primitive rendering.

	glViewport(0, 0, (GLsizei)viewport->Size.x, (GLsizei)viewport->Size.y);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_PROGRAM_POINT_SIZE);

	for (U32 i = 0, n = Im3d::GetDrawListCount(); i < n; ++i)
	{
		const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];

		if (drawList.m_layerId == Im3d::MakeId("NamedLayer"))
		{
			// The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
		}

		GLenum prim;
		GLuint sh;
		switch (drawList.m_primType)
		{
		case Im3d::DrawPrimitive_Points:
			prim = GL_POINTS;
			sh = g_Im3dShaderPoints;
			glDisable(GL_CULL_FACE); // points are view-aligned
			break;
		case Im3d::DrawPrimitive_Lines:
			prim = GL_LINES;
			sh = g_Im3dShaderLines;
			glDisable(GL_CULL_FACE); // lines are view-aligned
			break;
		case Im3d::DrawPrimitive_Triangles:
			prim = GL_TRIANGLES;
			sh = g_Im3dShaderTriangles;
			//glEnable(GL_CULL_FACE); // culling valid for triangles, but optional
			break;
		default:
			IM3D_ASSERT(false);
			return;
		};

		glBindVertexArray(g_Im3dVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, g_Im3dVertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)drawList.m_vertexCount * sizeof(Im3d::VertexData), (GLvoid*)drawList.m_vertexData, GL_STREAM_DRAW);

		AppData& ad = GetAppData();
		glUseProgram(sh);
		glUniform2f(glGetUniformLocation(sh, "uViewport"), ad.m_viewportSize.x, ad.m_viewportSize.y);
		glUniformMatrix4fv(glGetUniformLocation(sh, "uViewProjMatrix"), 1, false, (const GLfloat*)g_Example->m_camViewProj);
		glDrawArrays(prim, 0, (GLsizei)drawList.m_vertexCount);
	}

	// Text rendering.
	// This is common to all examples since we're using ImGui to draw the text lists, see im3d_example.cpp.
	//g_Example->drawTextDrawListsImGui(Im3d::GetTextDrawLists(), Im3d::GetTextDrawListCount());
}

void OFS::Im3d_Shutdown() noexcept
{
	glDeleteVertexArrays(1, &g_Im3dVertexArray);
	glDeleteBuffers(1, &g_Im3dVertexBuffer);
	glDeleteProgram(g_Im3dShaderPoints);
	glDeleteProgram(g_Im3dShaderLines);
	glDeleteProgram(g_Im3dShaderTriangles);
}
