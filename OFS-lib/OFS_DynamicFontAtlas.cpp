#include "OFS_DynamicFontAtlas.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"
#include "OFS_GL.h"

#include "imgui.h"
#include "SDL_rwops.h"
#include "SDL_timer.h"

#include <vector>

OFS_DynFontAtlas* OFS_DynFontAtlas::ptr = nullptr;

ImFont* OFS_DynFontAtlas::DefaultFont = nullptr;
ImFont* OFS_DynFontAtlas::DefaultFont2 = nullptr;

std::string OFS_DynFontAtlas::FontOverride;

OFS_DynFontAtlas::OFS_DynFontAtlas() noexcept
{
	config.OversampleH = 2;
	config.FontDataOwnedByAtlas = false;
	checkIfRebuildNeeded = true;
	
	auto& io = ImGui::GetIO();
	builder.AddRanges(io.Fonts->GetGlyphRangesDefault());

	builder.AddText(ICON_FOLDER_OPEN );
	builder.AddText(ICON_VOLUME_UP );
	builder.AddText(ICON_VOLUME_OFF );
	builder.AddText(ICON_LONG_ARROW_UP );
	builder.AddText(ICON_LONG_ARROW_DOWN );
	builder.AddText(ICON_LONG_ARROW_RIGHT );
	builder.AddText(ICON_ARROW_RIGHT );
	builder.AddText(ICON_PLAY );
	builder.AddText(ICON_PAUSE );
	builder.AddText(ICON_GAMEPAD );
	builder.AddText(ICON_HAND_RIGHT );
	builder.AddText(ICON_BACKWARD );
	builder.AddText(ICON_FORWARD );
	builder.AddText(ICON_STEP_BACKWARD );
	builder.AddText(ICON_STEP_FORWARD );
	builder.AddText(ICON_GITHUB );
	builder.AddText(ICON_SHARE );
	builder.AddText(ICON_EXCLAMATION );
	builder.AddText(ICON_REFRESH );
	builder.AddText(ICON_TRASH );
	builder.AddText(ICON_RANDOM );
	builder.AddText(ICON_WARNING_SIGN );
	builder.AddText(ICON_LINK );
	builder.AddText(ICON_UNLINK );
	builder.AddText(ICON_COPY );

	LastUsedChars.resize(builder.UsedChars.Size);
}

static ImFont* AddFontFromFile(OFS_DynFontAtlas* builder, const char* path, float fontSize, bool merge) noexcept
{
	auto& io = ImGui::GetIO();
	builder->config.MergeMode = merge;
	return io.Fonts->AddFontFromFileTTF(path, fontSize, &builder->config, builder->UsedRanges.Data);
	return nullptr;
}

void OFS_DynFontAtlas::RebuildFont(float fontSize) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	assert(ptr->builder.UsedChars.size_in_bytes() == ptr->LastUsedChars.size_in_bytes());

	if(ptr->forceRebuild || memcmp(ptr->builder.UsedChars.Data, ptr->LastUsedChars.Data, ptr->builder.UsedChars.size_in_bytes()) != 0) {
		// rebuild atlas
		memcpy(ptr->LastUsedChars.Data, ptr->builder.UsedChars.Data, ptr->builder.UsedChars.size_in_bytes());
		ptr->checkIfRebuildNeeded = false;
		ptr->forceRebuild = false;

		ptr->UsedRanges.clear();
		ptr->builder.BuildRanges(&ptr->UsedRanges);

		auto& io = ImGui::GetIO();
		GLuint fontTexture = (GLuint)(intptr_t)io.Fonts->TexID;
		io.Fonts->Clear();

		auto roboto = Util::Resource("fonts/RobotoMono-Regular.ttf");
		auto mainFont = FontOverride.empty() ? roboto : FontOverride;    
		auto fontawesome = Util::Resource("fonts/fontawesome-webfont.ttf");
		auto notoCJK = Util::Resource("fonts/NotoSansCJKjp-Regular.otf");

		ImFont* font = nullptr;
		{
			OFS_PROFILE("Main font");
			font = AddFontFromFile(ptr, mainFont.c_str(), fontSize, false);
			if(!font) {
				LOGF_ERROR("Failed to load \"%s\"", mainFont.c_str());
				font = AddFontFromFile(ptr, roboto.c_str(), fontSize, false);
				if(!font) {
					LOGF_ERROR("Failed to load \"%s\"", roboto.c_str());
					// fallback to default font
					io.Fonts->Clear();
					io.Fonts->AddFontDefault();
					goto default_font_end;
				}
			} 
			io.FontDefault = font;
			ptr->DefaultFont = font;
		}
		{
			OFS_PROFILE("Load fontawesome font");
			font = AddFontFromFile(ptr, fontawesome.c_str(), fontSize, true);
			if(!font) {
				LOGF_ERROR("Failed to load \"%s\"", fontawesome.c_str());
			}
		}
		{
			OFS_PROFILE("Load NotoSansCJK");
			font = AddFontFromFile(ptr, notoCJK.c_str(), fontSize, true);
			if(!font) {
				LOGF_ERROR("Failed to load \"%s\"", notoCJK.c_str());
			}
		}
		{
			OFS_PROFILE("Load main font (2x)");
			font = AddFontFromFile(ptr, mainFont.c_str(), fontSize * 2.f, false);
			ptr->DefaultFont2 = font;
		}

		default_font_end:
		unsigned char* pixels;
		int width, height;
		double fontBuildDuration;
		{
			double startCount = SDL_GetPerformanceCounter();
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
			double endCount = SDL_GetPerformanceCounter();
			fontBuildDuration = (endCount - startCount) / (double)SDL_GetPerformanceFrequency();
		}
		
		// Upload texture to graphics system
		if (!fontTexture) {
			glGenTextures(1, &fontTexture);
		}
		glBindTexture(GL_TEXTURE_2D, fontTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, width, height, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, pixels);
		
		io.Fonts->ClearTexData();
		io.Fonts->TexID = (void*)(intptr_t)fontTexture;
		
		LOGF_INFO("Font atlas was rebuilt. Took %0.3lf seconds.", fontBuildDuration);
		LOGF_INFO("New font atlas size: %dx%d", width, height);
	}
}