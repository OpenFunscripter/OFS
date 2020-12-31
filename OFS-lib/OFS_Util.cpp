#include "OFS_Util.h"

#include "EventSystem.h"

#include <filesystem>
#include  "SDL.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if WIN32
#define STBI_WINDOWS_UTF8
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#include "glad/glad.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "tinyfiledialogs.h"


// we pretend whre on a lower standard because the cpp11 header wants to throw desperately and doesn't compile with -fno-exceptions
#define UTF_CPP_CPLUSPLUS 199711L 
#include "utf8.h"

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
	stbsp_snprintf(tmp, sizeof(tmp), "start \"\" \"%s\"", url);
	return std::system(tmp);
#elif __APPLE__
	LOG_ERROR("Not implemented for this platform.");
#else
	stbsp_snprintf(tmp, sizeof(tmp), "xdg-open \"%s\"", url);
	return std::system(tmp);
#endif
	return 1;
}

void Util::Tooltip(const char* tip) noexcept
{
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(tip);
		ImGui::EndTooltip();
	}
}

void Util::ForceMinumumWindowSize(ImGuiWindow* window) noexcept
{
	auto expectedSize = ImGui::CalcWindowExpectedSize(window);
	auto actualSize = ImGui::GetWindowSize();
	if (expectedSize.x > actualSize.x) {
		actualSize.x = expectedSize.x;
	}
	if (expectedSize.y > actualSize.y || expectedSize.y < actualSize.y) {
		actualSize.y = expectedSize.y;
	}
	ImGui::SetWindowSize(actualSize, ImGuiCond_Always);
}

void Util::OpenFileDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler, bool multiple, const std::vector<const char*>& filters, const std::string& filterText) noexcept
{
	struct FileDialogThreadData {
		bool multiple = false;
		std::string title;
		std::string path;
		std::vector<const char*> filters;
		std::string filterText;
		EventSystem::SingleShotEventHandler handler;
	};
	auto thread = [](void* ctx) {
		auto data = (FileDialogThreadData*)ctx;
		if (!std::filesystem::exists(data->path)) {
			data->path = "";
		}

#ifdef WIN32
		std::wstring wtitle(tinyfd_utf8to16(data->title.c_str()));
		std::wstring wpath(tinyfd_utf8to16(data->path.c_str()));
		std::wstring wfilterText(tinyfd_utf8to16(data->filterText.c_str()));
		std::vector<std::wstring> wfilters;
		std::vector<const wchar_t*> wc_str;
		wfilters.reserve(data->filters.size());
		wc_str.reserve(data->filters.size());
		for (auto&& filter : data->filters) {
			wfilters.emplace_back(tinyfd_utf8to16(filter));
			wc_str.push_back(wfilters.back().c_str());
		}
		auto result = tinyfd_utf16to8(tinyfd_openFileDialogW(wtitle.c_str(), wpath.c_str(), wc_str.size(), wc_str.data(), wfilterText.empty() ? NULL : wfilterText.c_str(), data->multiple));
#else
		auto result = tinyfd_openFileDialog(data->title.c_str(), data->path.c_str(), data->filters.size(), data->filters.data(), data->filterText.empty() ? NULL : data->filterText.c_str(), data->multiple);
#endif
		auto dialogResult = new FileDialogResult;
		if (result != nullptr) {
			if (data->multiple) {
				int last = 0;
				int index = 0;
				for (char c : std::string(result)) {
					if (c == '|') {
						dialogResult->files.emplace_back(std::string(result + last, index - last));
						last = index+1;
					}
					index++;
				}
				dialogResult->files.emplace_back(std::string(result + last, index - last));
			}
			else {
				dialogResult->files.emplace_back(result);
			}
		}

		auto eventData = new EventSystem::SingleShotEventData;
		eventData->ctx = dialogResult;
		eventData->handler = std::move(data->handler);

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
	threadData->filterText = filterText;
	threadData->multiple = multiple;
	threadData->path = path;
	threadData->title = title;
	auto handle = SDL_CreateThread(thread, "OpenFileDialog", threadData);
	SDL_DetachThread(handle);
}

void Util::SaveFileDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler, const std::vector<const char*>& filters, const std::string& filterText) noexcept
{
	struct SaveFileDialogThreadData {
		std::string title;
		std::string path;
		std::vector<const char*> filters;
		std::string filterText;
		EventSystem::SingleShotEventHandler handler;
	};
	auto thread = [](void* ctx) -> int32_t {
		auto data = (SaveFileDialogThreadData*)ctx;

		auto dialogPath = Util::PathFromString(data->path);
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

		auto result = tinyfd_saveFileDialog(data->title.c_str(), data->path.c_str(), data->filters.size(), data->filters.data(), !data->filterText.empty() ? data->filterText.c_str() : NULL);

		FUN_ASSERT(result, "Ignore this if you pressed cancel.");
		auto saveDialogResult = new FileDialogResult;
		if (result != nullptr) {
			saveDialogResult->files.emplace_back(result);
		}
		auto eventData = new EventSystem::SingleShotEventData;
		eventData->ctx = saveDialogResult;
		eventData->handler = std::move(data->handler);

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
	threadData->filterText = filterText;
	threadData->handler = [handler](void* ctx) {
		auto result = (FileDialogResult*)ctx;
		handler(*result);
		delete result;
	};
	auto handle = SDL_CreateThread(thread, "SaveFileDialog", threadData);
}

void Util::OpenDirectoryDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler) noexcept
{
	struct OpenDirectoryDialogThreadData {
		std::string title;
		std::string path;
		EventSystem::SingleShotEventHandler handler;
	};
	auto thread = [](void* ctx) -> int32_t {
		auto data = (OpenDirectoryDialogThreadData*)ctx;

		auto dialogPath = Util::PathFromString(data->path);
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

		auto result = tinyfd_selectFolderDialog(data->title.c_str(), data->path.c_str());

		FUN_ASSERT(result, "Ignore this if you pressed cancel.");
		auto directoryDialogResult = new FileDialogResult;
		if (result != nullptr) {
			directoryDialogResult->files.emplace_back(result);
		}
	
		auto eventData = new EventSystem::SingleShotEventData;
		eventData->ctx = directoryDialogResult;
		eventData->handler = std::move(data->handler);

		SDL_Event ev{ 0 };
		ev.type = EventSystem::SingleShotEvent;
		ev.user.data1 = eventData;
		SDL_PushEvent(&ev);
		delete data;
		return 0;
	};
	auto threadData = new OpenDirectoryDialogThreadData;
	threadData->title = title;
	threadData->path = path;
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

std::wstring Util::Utf8ToUtf16(const std::string& str) noexcept
{
	std::wstring result;

	if (!utf8::is_valid(str.begin(), str.end())) {
		LOGF_ERROR("%s is not valid utf8", str.c_str());
		return result;
	}

	result.reserve(utf8::unchecked::distance(str.begin(), str.end()));
	utf8::unchecked::utf8to16(str.begin(), str.end(), std::back_inserter(result));
	return result;
}

std::string Util::Utf16ToUtf8(const std::wstring& str) noexcept
{
	FUN_ASSERT(false, "this is untested but also unused");
	std::string result;
	result.reserve(utf8::unchecked::distance(str.begin(), str.end()));
	utf8::unchecked::utf16to8(str.begin(), str.end(), std::back_inserter(result));
	return result;
}

int32_t Util::Utf8Length(const std::string& str) noexcept
{
	if (!utf8::is_valid(str.begin(), str.end())) {
		LOGF_ERROR("%s is not valid utf8", str.c_str());
		return 0;
	}

	return utf8::unchecked::distance(str.begin(), str.end());
}

std::filesystem::path Util::PathFromString(const std::string& str) noexcept
{
	std::filesystem::path result;
#if WIN32
	auto wideString = Util::Utf8ToUtf16(str);
	result = std::filesystem::path(wideString);
#else
	result = std::filesystem::path(str);
#endif
	result.make_preferred();
	return result;
}

void Util::ConcatPathSafe(std::filesystem::path& path, const std::string& element) noexcept
{
	// I don't know if this is safe lol
#if WIN32
	path /= Util::Utf8ToUtf16(element);
#else
	path /= element;
#endif
}

bool Util::SavePNG(const std::string& path, void* buffer, int32_t width, int32_t height, int32_t channels, bool flipVertical) noexcept
{
	stbi_flip_vertically_on_write(flipVertical);
	bool success = stbi_write_png(path.c_str(),
		width, height,
		channels, buffer, 0
	);
	return success;
}

std::filesystem::path Util::FfmpegPath() noexcept
{
	auto base_path = Util::Basepath();
#if WIN32
	auto ffmpeg_path = base_path / "ffmpeg.exe";
#else
	auto ffmpeg_path = std::filesystem::path("ffmpeg");
#endif
	return ffmpeg_path;
}
