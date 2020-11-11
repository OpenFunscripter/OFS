#include "OpenFunscripterUtil.h"

#include "EventSystem.h"

#include <filesystem>
#include  "SDL.h"
//#include "portable-file-dialogs.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "glad/glad.h"

#include "imgui.h"

#define NOC_FILE_DIALOG_IMPLEMENTATION
#if WIN32
#define NOC_FILE_DIALOG_WIN32
#elif __APPLE__
#define NOC_FILE_DIALOG_OSX
#else
#define NOC_FILE_DIALOG_GTK
#endif
#include "noc_file_dialog.h"

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
#if WIN32
	char tmp[1024];
	stbsp_snprintf(tmp, sizeof(tmp), "explorer %s", path);
	return std::system(tmp);
#elif __APPLE__
	LOG_ERROR("Not implemented for this platform.");
#else
	return OpenUrl(path);
#endif
	return 1;
}

int Util::OpenUrl(const char* url)
{
	char tmp[1024];
#if WIN32
	stbsp_snprintf(tmp, sizeof(tmp), "start %s", url);
	return std::system(tmp);
#elif __APPLE__
	LOG_ERROR("Not implemented for this platform.");
#else
	stbsp_snprintf(tmp, sizeof(tmp), "xdg-open %s", url);
	return std::system(tmp);
#endif
	return 1;
}

void Util::Tooltip(const char* tip)
{
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("%s", tip);
		ImGui::EndTooltip();
	}
}

void Util::OpenFileDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler, bool multiple, const std::vector<std::string>& filters) noexcept
{
	struct FileDialogThreadData {
		bool multiple = false;
		std::string title;
		std::string path;
		std::vector<std::string> filters;
		EventSystem::SingleShotEventHandler handler;
	};
	auto thread = [](void* ctx) {
		auto data = (FileDialogThreadData*)ctx;
		if (!std::filesystem::exists(data->path)) {
			data->path = "";
		}

		auto result = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "All Files\0*.*\0", data->path.c_str(), NULL);
		//pfd::open_file fileDialog(data->title, data->path, data->filters, (data->multiple) ? pfd::opt::multiselect : pfd::opt::none);
		
		auto dialogResult = new FileDialogResult;
		if (result != nullptr) {
			dialogResult->files.emplace_back(result);
		}
		//dialogResult->files = fileDialog.result();

		auto eventData = new EventSystem::SingleShotEventData;
		eventData->ctx = dialogResult;
		eventData->handler = data->handler;

		SDL_Event ev{ 0 };
		ev.type = EventSystem::SingleShotEvent;
		ev.user.data1 = eventData;
		SDL_PushEvent(&ev);
		delete data;
		return 0;
	};
	auto threadData = new FileDialogThreadData;
	threadData->handler = [handler](void* ctx) {
		auto result = (FileDialogResult*)ctx;
		handler(*result);
		delete result;
	};
	threadData->filters = filters;
	threadData->multiple = multiple;
	threadData->path = path;
	threadData->title = title;
	auto handle = SDL_CreateThread(thread, "OpenFileDialog", threadData);
	SDL_DetachThread(handle);
}

void Util::SaveFileDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler, const std::vector<std::string>& filters) noexcept
{
	struct SaveFileDialogThreadData {
		std::string title;
		std::string path;
		std::vector<std::string> filters;
		EventSystem::SingleShotEventHandler handler;
	};
	auto thread = [](void* ctx) -> int32_t {
		auto data = (SaveFileDialogThreadData*)ctx;

		auto dialogPath = std::filesystem::path(data->path);
		if (std::filesystem::is_directory(dialogPath) && !std::filesystem::exists(dialogPath)) {
			data->path = "";
		}
		else {
			auto directory = dialogPath;
			directory.replace_filename("");
			if (!std::filesystem::exists(directory)) {
				data->path = "";
			}
		}


		//pfd::save_file saveFileDialog(data->title, data->path, data->filters, pfd::opt::none);
		auto result = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "All Files\0*.*\0", data->path.c_str(), NULL);
		auto saveDialogResult = new FileDialogResult;
		//saveDialogResult->files.emplace_back(saveFileDialog.result());
		saveDialogResult->files.emplace_back(result);
		auto eventData = new EventSystem::SingleShotEventData;
		eventData->ctx = saveDialogResult;
		eventData->handler = data->handler;

		SDL_Event ev{ 0 };
		ev.type = EventSystem::SingleShotEvent;
		ev.user.data1 = eventData;
		SDL_PushEvent(&ev);
		delete data;
		return 0;
	};
	auto threadData = new SaveFileDialogThreadData;
	threadData->title = title;
	threadData->path = path;
	threadData->filters = filters;
	threadData->handler = [handler](void* ctx) {
		auto result = (FileDialogResult*)ctx;
		handler(*result);
		delete result;
	};
	auto handle = SDL_CreateThread(thread, "SaveFileDialog", threadData);
}

std::string Util::Resource(const std::string& path) noexcept
{
	auto rel = std::filesystem::path(path);
	rel.make_preferred();
	auto base = Util::Basepath();
	return (base / "data" / rel).string();
}
