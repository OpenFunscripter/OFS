#include "FunscriptHeatmap.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#include "OFS_ImGui.h"
#include "OFS_Shader.h"
#include "OFS_GL.h"

#include <chrono>
#include <memory>
#include <array>

ImGradient FunscriptHeatmap::Colors;
ImGradient FunscriptHeatmap::LineColors;

static constexpr auto SpeedTextureResolution = 2048;

class HeatmapShader : public ShaderBase
{
private:
	int32_t ProjMtxLoc = 0;
    int32_t SpeedLoc = 0;

	static constexpr const char* vtx_shader = OFS_SHADER_VERSION R"(
		precision highp float;

		uniform mat4 ProjMtx;
		in vec2 Position;
		in vec2 UV;
		in vec4 Color;
		out vec2 Frag_UV;
		out vec4 Frag_Color;
		
		void main()	{
			Frag_UV = UV;
			Frag_Color = Color;
			gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
		}
	)";

	static constexpr const char* frag_shader = OFS_SHADER_VERSION R"(
		precision highp float;
        uniform sampler2D speedTex;

		in vec2 Frag_UV;
		in vec4 Frag_Color;
		out vec4 Out_Color;

        const vec3 colors[] = vec3[](
            vec3(0.f, 0.f, 0.f),
            vec3(30.f, 144.f, 255.f) / 255.f,
            vec3(0.f, 255.f, 255.f) / 255.f,
            vec3(0.f, 255.f, 0.f) / 255.f,
            vec3(255.f, 255.f, 0.f) / 255.f,
            vec3(255.f, 0.f, 0.f) / 255.f
        );

        vec3 RAMP(vec3 cols[6], float x) {
            x *= float(cols.length() - 1);
            return mix(cols[int(x)], cols[int(x) + 1], smoothstep(0.0, 1.0, fract(x)));
        }

		void main()	{
            float speed = texture(speedTex, vec2(Frag_UV.x, 0.f)).r;
            vec3 color = RAMP(colors, speed);
            color = mix(vec3(0.f, 0.f, 0.f), color, Frag_UV.y);
            Out_Color = vec4(color, 1.f);
		}
	)";

	void initUniformLocations() noexcept;
public:
	HeatmapShader()
		: ShaderBase(vtx_shader, frag_shader)
	{
		initUniformLocations();
	}

	void ProjMtx(const float* mat4) noexcept;
    void SpeedTex(uint32_t unit) noexcept;
};

static std::unique_ptr<HeatmapShader> Shader;


void HeatmapShader::initUniformLocations() noexcept
{
	ProjMtxLoc = glGetUniformLocation(program, "ProjMtx");
    SpeedLoc = glGetUniformLocation(program, "speedTex");
}

void HeatmapShader::ProjMtx(const float* mat4) noexcept
{
	glUniformMatrix4fv(ProjMtxLoc, 1, GL_FALSE, mat4);
}

void HeatmapShader::SpeedTex(uint32_t unit) noexcept
{
    glUniform1i(SpeedLoc, unit);
}

void FunscriptHeatmap::Init() noexcept
{
    if (Colors.getMarks().empty()) 
    {
        std::array<ImColor, 6> heatColor{
            IM_COL32(0x00, 0x00, 0x00, 0xFF),
            IM_COL32(0x1E, 0x90, 0xFF, 0xFF),
            IM_COL32(0x00, 0xFF, 0xFF, 0xFF),
            IM_COL32(0x00, 0xFF, 0x00, 0xFF),
            IM_COL32(0xFF, 0xFF, 0x00, 0xFF),
            IM_COL32(0xFF, 0x00, 0x00, 0xFF),
        };
        Colors.addMark(0.f, heatColor[0]);
        Colors.addMark(std::nextafterf(0.f, 1.f), heatColor[1]);
        Colors.addMark(2.f/(heatColor.size()-1), heatColor[2]);
        Colors.addMark(3.f/(heatColor.size()-1), heatColor[3]);
        Colors.addMark(4.f/(heatColor.size()-1), heatColor[4]);
        Colors.addMark(5.f/(heatColor.size()-1), heatColor[5]);
        Colors.refreshCache();
    }
    if(LineColors.getMarks().empty())
    {
        std::array<ImColor, 4> heatColor {
            IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
            IM_COL32(0x66, 0xff, 0x00, 0xFF),
            IM_COL32(0xFF, 0xff, 0x00, 0xFF),
            IM_COL32(0xFF, 0x00, 0x00, 0xFF),
        };
        float pos = 0.0f;
        for (auto& col : heatColor) {
            LineColors.addMark(pos, col);
            pos += (1.f / (heatColor.size() - 1));
        }
        LineColors.refreshCache();
    }
    Shader = std::make_unique<HeatmapShader>();
}

FunscriptHeatmap::FunscriptHeatmap() noexcept
{
    glGenTextures(1, &speedTexture);
    glBindTexture(GL_TEXTURE_2D, speedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, SpeedTextureResolution, 1, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FunscriptHeatmap::Update(float totalDuration, const FunscriptArray& actions) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto startTime = std::chrono::high_resolution_clock::now();    
    std::vector<float> speedBuffer; 
    speedBuffer.resize(SpeedTextureResolution, 0.f);
    std::vector<uint16_t> sampleCountBuffer;
    sampleCountBuffer.resize(SpeedTextureResolution, 0);

    float timeStep = totalDuration / SpeedTextureResolution;


    for(uint32_t i = 0, j = 1, size = actions.size(); j < size; i = j++)
    {
        auto prev = actions[i];
        auto next = actions[j];

        float strokeDuration = next.atS - prev.atS;
        float speed = std::abs(prev.pos - next.pos) / strokeDuration;
    
        uint32_t prevSampleIdx = prev.atS / timeStep;
        uint32_t nextSampleIdx = next.atS / timeStep;
        if(prevSampleIdx == nextSampleIdx)
        {
            if(prevSampleIdx < SpeedTextureResolution)
            {
                sampleCountBuffer[prevSampleIdx] += 1;
                speedBuffer[prevSampleIdx] += speed;
            }
        }
        else
        {
            if(prevSampleIdx < SpeedTextureResolution)
            {
                sampleCountBuffer[prevSampleIdx] += 1;
                speedBuffer[prevSampleIdx] += speed;
            }

            if(nextSampleIdx < SpeedTextureResolution)
            {
                sampleCountBuffer[nextSampleIdx] += 1;
                speedBuffer[nextSampleIdx] += speed;
            }
        }
    }

    for(uint32_t i=0; i < SpeedTextureResolution; i += 1)
    {
        speedBuffer[i] /= sampleCountBuffer[i] > 0 ? (float)sampleCountBuffer[i] : 1.f;
        speedBuffer[i] /= MaxSpeedPerSecond;
        speedBuffer[i] = Util::Clamp(speedBuffer[i], 0.f, 1.f);
    }

    glBindTexture(GL_TEXTURE_2D, speedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, SpeedTextureResolution, 1, 0, GL_RED, GL_FLOAT, speedBuffer.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> duration = endTime - startTime;
    LOGF_INFO("Heatmap update took: %f", duration.count());
}

void FunscriptHeatmap::DrawHeatmap(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) noexcept
{
    drawList->AddCallback([](const ImDrawList* parentList, const ImDrawCmd* cmd) noexcept
    {
        auto self = (FunscriptHeatmap*)cmd->UserCallbackData;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, self->speedTexture);
        glActiveTexture(GL_TEXTURE0);

        auto drawData = OFS_ImGui::CurrentlyRenderedViewport->DrawData;
        float L = drawData->DisplayPos.x;
        float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
        float T = drawData->DisplayPos.y;
        float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
        const float orthoProjection[4][4] =
        {
            { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
            { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f, 0.0f },
            { (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
        };
        Shader->Use();
        Shader->ProjMtx(&orthoProjection[0][0]);
        Shader->SpeedTex(1);

    }, this);
    drawList->AddImage(0, min, max);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, 0);
}

#include "imgui_impl/imgui_impl_opengl3.h"

std::vector<uint8_t> FunscriptHeatmap::RenderToBitmap(int16_t width, int16_t height) noexcept
{
    width = Util::Min(width, FunscriptHeatmap::MaxResolution);
    height = Util::Min(height, FunscriptHeatmap::MaxResolution);

    // Prepare temporary framebuffer
    uint32_t tmpFramebuffer = 0;
    uint32_t tmpColorTex = 0;

    glGenFramebuffers(1, &tmpFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, tmpFramebuffer);

    glGenTextures(1, &tmpColorTex);
    glBindTexture(GL_TEXTURE_2D, tmpColorTex);

    glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, width, height, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmpColorTex, 0);
    GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, DrawBuffers);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("Failed to create framebuffer for video!");
    }

    // Backup out main ImGuiContext
    auto prevContext = ImGui::GetCurrentContext();

    // Create a temporary ImGuiContext
    auto tmpContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(tmpContext);
    ImGui_ImplOpenGL3_Init(OFS_SHADER_VERSION);

    // Prepare drawing a single image
    auto& io = ImGui::GetIO();
    io.DisplaySize.x = width;
    io.DisplaySize.y = height;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Draw calls
    {
        auto drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
        DrawHeatmap(drawList, ImVec2(0.f, 0.f), ImVec2(width, height));
    }

    // Render image
    ImGui::Render();
    OFS_ImGui::CurrentlyRenderedViewport = ImGui::GetMainViewport();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    OFS_ImGui::CurrentlyRenderedViewport = nullptr;


    // Grab the bitmap
    std::vector<uint8_t> bitmap;
    bitmap.resize((size_t)width * (size_t)height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.data());

    // Destroy everything
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext(tmpContext);

    glDeleteTextures(1, &tmpColorTex);
    glDeleteFramebuffers(1, &tmpFramebuffer);

    // Reset to default framebuffer and main ImGuiContext    
    ImGui::SetCurrentContext(prevContext);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return bitmap;
}