#include "OpenFunscripterUtil.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "glad/glad.h"

#include "imgui.h"

bool Util::LoadTextureFromFile(const char* filename, unsigned int* out_texture, int* out_width, int* out_height)
{
	// Load from file
	int image_width = 0;
	int image_height = 0;
	unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
		return false;

	// Create a OpenGL texture identifier
	GLuint image_texture;
	glGenTextures(1, &image_texture);
	glBindTexture(GL_TEXTURE_2D, image_texture);

	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Upload pixels into texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
	stbi_image_free(image_data);

	*out_texture = image_texture;
	*out_width = image_width;
	*out_height = image_height;

	return true;
}

int Util::OpenFileExplorer(const char* path)
{
	char tmp[1024];
#if WIN32
	stbsp_snprintf(tmp, sizeof(tmp), "explorer %s", path);
	std::system(tmp);
#elif __APPLE__
	LOG_ERROR("Not implemented for this platform.");
#else
	OpenUrl(path);
#endif
	return 0;
}

int Util::OpenUrl(const char* url)
{
	char tmp[1024];
#if WIN32
	stbsp_snprintf(tmp, sizeof(tmp), "start %s", url);
	std::system(tmp);
#elif __APPLE__
	LOG_ERROR("Not implemented for this platform.");
#else
	stbsp_snprintf(tmp, sizeof(tmp), "xdg-open %s", url);
	std::system(tmp);
#endif
	return 0;
}

void Util::Tooltip(const char* tip)
{
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("%s", tip);
		ImGui::EndTooltip();
	}
}
