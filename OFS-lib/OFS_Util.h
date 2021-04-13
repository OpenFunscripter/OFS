#pragma once

#include "SDL_rwops.h"
#include "SDL_log.h"
#include "SDL_filesystem.h"
#include "nlohmann/json.hpp"

#include <memory>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <functional>
#include <vector>
#include <sstream>

#include "stb_sprintf.h"
#include "stb_image.h"

#include "emmintrin.h" // for _mm_pause

#define OFS_PAUSE_INTRIN _mm_pause

// helper for FontAwesome. Version 4.7.0 2016 ttf
#define ICON_FOLDER_OPEN "\xef\x81\xbc"
#define ICON_VOLUME_UP "\xef\x80\xa8"
#define ICON_VOLUME_OFF "\xef\x80\xa6"
#define ICON_LONG_ARROW_UP "\xef\x85\xb6"
#define ICON_LONG_ARROW_DOWN "\xef\x85\xb5"
#define ICON_LONG_ARROW_RIGHT "\xef\x85\xb8"
#define ICON_ARROW_RIGHT "\xef\x81\xa1"
#define ICON_PLAY "\xef\x81\x8b"
#define ICON_PAUSE "\xef\x81\x8c"
#define ICON_GAMEPAD "\xef\x84\x9b"
#define ICON_HAND_RIGHT "\xef\x82\xa4"
#define ICON_BACKWARD "\xef\x81\x8a"
#define ICON_FORWARD "\xef\x81\x8e"
#define ICON_STEP_BACKWARD "\xef\x81\x88"
#define ICON_STEP_FORWARD "\xef\x81\x91"
#define ICON_GITHUB "\xef\x82\x9b"
#define ICON_SHARE "\xef\x81\x85"
#define ICON_EXCLAMATION "\xef\x84\xaa"
#define ICON_REFRESH "\xef\x80\xa1"
#define ICON_TRASH "\xef\x87\xb8"
#define ICON_RANDOM "\xef\x81\xb4"
#define ICON_WARNING_SIGN "\xef\x81\xb1"
#define ICON_LINK "\xef\x83\x81"
#define ICON_UNLINK "\xef\x84\xa7"

#ifndef NDEBUG
#define LOG_INFO(msg)  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_WARN(msg)  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_DEBUG(msg)  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_ERROR(msg)  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, msg)

#define LOGF_INFO( format, ...)	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_WARN( format, ...)	SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_DEBUG(format, ...)	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_ERROR(format, ...)	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#else
#define LOG_INFO(msg)  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_WARN(msg)  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_DEBUG(msg)
#define LOG_ERROR(msg)  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, msg)

#define LOGF_INFO( format, ...)	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_WARN( format, ...)	SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_DEBUG(format, ...)
#define LOGF_ERROR(format, ...)	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#endif

#if defined(WIN32)
#define FUN_DEBUG_BREAK() __debugbreak()
#else
#define FUN_DEBUG_BREAK()
#endif

#ifndef NDEBUG
#define FUN_ASSERT_F(expr, format, ...) \
		if(expr) {} \
		else \
		{ \
			LOG_ERROR("============== ASSERTION FAILED ==============");\
			LOGF_ERROR("in file: \"%s\" line: %d", __FILE__, __LINE__); \
			LOGF_ERROR(format,__VA_ARGS__);\
			FUN_DEBUG_BREAK(); \
		}


// assertion without error message
#define FUN_ASSERT(expr, msg) \
		if(expr) {} \
		else \
		{ \
			LOG_ERROR("============== ASSERTION FAILED ==============");\
			LOGF_ERROR("in file: \"%s\" line: %d", __FILE__, __LINE__); \
			LOG_ERROR(msg); \
			FUN_DEBUG_BREAK();\
		}
#else

#define FUN_ASSERT_F(expr, format, ...)
#define FUN_ASSERT(expr, msg)

#endif

class Util {
public:
	static bool LoadTextureFromFile(const char* filename, unsigned int* out_texture, int* out_width, int* out_height) noexcept;
	static bool LoadTextureFromBuffer(const char* buffer, size_t buffsize, unsigned int* out_texture, int* out_width, int* out_height) noexcept;

	template<typename T>
	inline static T Clamp(T v, T mn, T mx) noexcept {
		return (v < mn) ? mn : (v > mx) ? mx : v;
	}

	template<typename T>
	inline static T MapRange(T val, T a1, T a2, T b1, T b2) noexcept {
		return b1 + (val - a1) * (b2 - b1) / (a2 - a1);
	}

	template<typename T>
	inline static T Lerp(T startVal, T endVal, float t) noexcept {
		return startVal + ((endVal - startVal) * t);
	}

	inline static auto LoadJson(const std::string& file, bool* success) noexcept { return LoadJson(file.c_str(), success, file.size()); }
	inline static nlohmann::json LoadJson(const char* file, bool* success, int32_t path_len = 0) noexcept {
		auto handle = OpenFile(file, "rb", path_len == 0 ? strlen(file) : path_len);
		nlohmann::json j;
		if (handle != nullptr) {
			size_t size = handle->size(handle);
			std::string buffer;
			buffer.resize(size + 1);
			SDL_RWread(handle, buffer.data(), sizeof(char), size);
			buffer[size] = '\0';
			if (size > 0) {
				j = nlohmann::json::parse(buffer, nullptr, false, true);
				*success = !j.is_discarded();
			}
			SDL_RWclose(handle);
		}
		else {
			LOGF_ERROR("Failed to load json: \"%s\"", file);
			*success = false;
		}
		
		return j;
	}

	inline static SDL_RWops* OpenFile(const char* path, const char* mode, int32_t path_len) noexcept
	{
#ifdef WIN32
		SDL_RWops* handle = nullptr;
		if (path_len >= _MAX_PATH) {
			std::stringstream ss;
			ss << "\\\\?\\" << path;
			handle = SDL_RWFromFile(ss.str().c_str(), mode);
		}
		else {
			handle = SDL_RWFromFile(path, mode);
		}
#else
		auto handle = SDL_RWFromFile(path, mode);
#endif
		return handle;
	}

	inline static size_t AppendToFile(const char* path, const char* buffer, size_t size, bool newLine) noexcept
	{
		size_t written = 0;
		auto file = OpenFile(path, "a", strlen(path));
		if (file) {
			written = SDL_RWwrite(file, buffer, sizeof(char), size);
			if (newLine) {
				SDL_RWwrite(file, "\n", 1, 1);
			}
			SDL_RWclose(file);
		}
		return written;
	}

	inline static size_t ReadFile(const char* path, std::vector<uint8_t>& buffer) noexcept
	{
		auto file = OpenFile(path, "r", strlen(path));
		if (file) {
			buffer.clear();
			buffer.resize(SDL_RWsize(file));
			SDL_RWread(file, buffer.data(), sizeof(uint8_t), buffer.size());
			SDL_RWclose(file);
			return buffer.size();
		}
		return 0;
	}

	inline static size_t WriteFile(const char* path, uint8_t* buffer, size_t size) noexcept
	{
		auto file = OpenFile(path, "wb", strlen(path));
		if (file) {
			auto written = SDL_RWwrite(file, buffer, sizeof(uint8_t), size);
			SDL_RWclose(file);
			return written;
		}
		return 0;
	}

	inline static void WriteJson(const nlohmann::json& json, const std::string& file, bool pretty = false) noexcept {
		return WriteJson(json, file.c_str(), pretty, file.size());
	}
	inline static void WriteJson(const nlohmann::json& json, const char* file, bool pretty = false, int32_t path_len = 0) noexcept {
		auto handle = OpenFile(file, "wb", path_len == 0 ? strlen(file) : path_len);
		if (handle != nullptr) {
			auto jsonText = json.dump((pretty) ? 4 : -1, ' ');
			SDL_RWwrite(handle, jsonText.data(), sizeof(char), jsonText.size());
			SDL_RWclose(handle);
		}
		else {
			LOGF_ERROR("Failed to save: \"%s\"\n%s", file, SDL_GetError());
		}
	}

	static inline size_t FormatTime(char* buffer, size_t buf_size, float time_seconds, bool with_ms) noexcept {
		if (std::isinf(time_seconds) || std::isnan(time_seconds)) time_seconds = 0.f;
		auto duration = std::chrono::duration<double>(time_seconds);
		std::time_t t = duration.count();
		std::tm& timestamp = *std::gmtime(&t);

		size_t size = std::strftime(buffer, buf_size, "%H:%M:%S", &timestamp);
		if (!with_ms)
			return size;
		else {
			int32_t ms = (time_seconds - (int)time_seconds) * 1000.f;
			return stbsp_snprintf(buffer, buf_size, "%s.%03i", buffer, ms);
		}
	}

	static int OpenFileExplorer(const std::string& path);
	static int OpenUrl(const std::string& url);

	inline static std::filesystem::path Basepath() noexcept {
		char* base = SDL_GetBasePath();
		std::filesystem::path path(base);
		SDL_free(base);
		return path;
	}

	inline static std::string Filename(const std::string& path) noexcept {
		return std::filesystem::path(path)
			.replace_extension("")
			.filename()
			.string();
	}

	inline static bool FileExists(const std::string& file) noexcept {
		bool exists = false;
#if WIN32
		std::wstring wfile = Util::Utf8ToUtf16(file);
		struct _stati64 s;
		exists = _wstati64(wfile.c_str(), &s) == 0;
#else
		auto handle = OpenFile(file.c_str(), "r", file.size());
		if (handle != nullptr) {
			SDL_RWclose(handle);
			exists = true;
		}
		else {
			LOGF_WARN("\"%s\" doesn't exist", file.c_str());
		}
#endif
		return exists;
	}
	inline static bool FileNamesMatch(std::filesystem::path path1, std::filesystem::path path2) noexcept {
		path1.replace_extension(""); path2.replace_extension();
		return path1.filename() == path2.filename();
	}

	static void ForceMinumumWindowSize(class ImGuiWindow* window) noexcept;

	// http://www.martinbroadhurst.com/how-to-trim-a-stdstring.html
	static std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") noexcept {
		str.erase(0, str.find_first_not_of(chars));
		return str;
	}

	static std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") noexcept {
		str.erase(str.find_last_not_of(chars) + 1);
		return str;
	}

	static std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") noexcept {
		return ltrim(rtrim(str, chars), chars);
	}

	inline static bool StringEqualsInsensitive(const std::string& string1, const std::string string2) noexcept {
		if (string1.length() != string2.length()) return false;
		return ContainsInsensitive(string1, string2);
	}

	inline static bool ContainsInsensitive(const std::string& string1, const std::string& string2) noexcept {
		auto it = std::search(
			string1.begin(), string1.end(),
			string2.begin(), string2.end(),
			[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
		);
		return (it != string1.end());
	}

	inline static bool StringEndsWith(const std::string& string, const std::string& ending) noexcept {
		if (string.length() >= ending.length()) {
			return (0 == string.compare(string.length() - ending.length(), ending.length(), ending));
		}
		return false;
	}

	inline static bool StringStartsWith(const std::string& string, const std::string& start) noexcept
	{
		if (string.length() >= start.length()) {
			for (int i = 0; i < start.size(); i++)
			{
				if (string[i] != start[i]) return false;
			}
			return true;
		}
		return false;
	}

	struct FileDialogResult {
		std::vector<std::string> files;
	};
	using FileDialogResultHandler = std::function<void(FileDialogResult&)>;
	static void OpenFileDialog(const std::string& title,
		const std::string& path,
		FileDialogResultHandler&& handler,
		bool multiple = false,
		const std::vector<const char*>& filters = {},
		const std::string& filterText = "") noexcept;
	static void SaveFileDialog(const std::string& title, 
		const std::string& path, 
		FileDialogResultHandler&& handler, 
		const std::vector<const char*>& filters = { },
		const std::string& filterText = "") noexcept;
	static void OpenDirectoryDialog(const std::string& title,
		const std::string& path,
		FileDialogResultHandler&& handler) noexcept;

	enum class YesNoCancel
	{
		Yes,
		No,
		Cancel
	};

	using YesNoDialogResultHandler = std::function<void(YesNoCancel)>;
	static void YesNoCancelDialog(const std::string& title, const std::string& message,
		YesNoDialogResultHandler&& handler);

	static void MessageBoxAlert(const std::string& title, const std::string& message) noexcept;

	static std::string Resource(const std::string& path) noexcept;

	static std::string Prefpath(const std::string& path) noexcept {
		static const char* cachedPref = SDL_GetPrefPath("OFS", "OFS_data");
		static std::filesystem::path prefPath(cachedPref);
		std::filesystem::path rel(path);
		rel.make_preferred();
		return (prefPath / rel).string();
	}

	static std::string PrefpathOFP(const std::string& path) noexcept {
		static const char* cachedPref = SDL_GetPrefPath("OFS", "OFP_data");
		static std::filesystem::path prefPath(cachedPref);
		std::filesystem::path rel(path);
		rel.make_preferred();
		return (prefPath / rel).string();
	}

	static bool CreateDirectories(const std::filesystem::path& dirs) noexcept {
		std::error_code ec;
		std::filesystem::create_directories(dirs, ec);
		if (ec) {
			LOGF_ERROR("Failed to create directory: %s", ec.message().c_str());
			return false;
		}
		return true;
	}

	static std::wstring Utf8ToUtf16(const std::string& str) noexcept;
	static std::string Utf16ToUtf8(const std::wstring& str) noexcept;
	static int32_t Utf8Length(const std::string& str) noexcept;

	static std::filesystem::path PathFromString(const std::string& str) noexcept;
	static void ConcatPathSafe(std::filesystem::path& path, const std::string& element) noexcept;

	static bool SavePNG(const std::string& path, void* buffer, int32_t width, int32_t height, int32_t channels = 3, bool flipVertical = true) noexcept;

	static std::filesystem::path FfmpegPath() noexcept;


	static char FormatBuffer[4096];
	inline static const char* Format(const char* fmt, ...) noexcept
	{
		va_list argp;
		va_start(argp, fmt);
		stbsp_vsnprintf(FormatBuffer, sizeof(FormatBuffer), fmt, argp);
		return FormatBuffer;
	}
};