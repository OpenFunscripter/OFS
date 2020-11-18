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

#include "stb_sprintf.h"
#include "stb_image.h"

// helper for FontAwesome. Version 4.7.0 2016 ttf
#define ICON_FOLDER_OPEN "\xef\x81\xbc"
#define ICON_VOLUME_UP "\xef\x80\xa8"
#define ICON_VOLUME_OFF "\xef\x80\xa6"
#define ICON_LONG_ARROW_UP "\xef\x85\xb6"
#define ICON_LONG_ARROW_DOWN "\xef\x85\xb5"
#define ICON_LONG_ARROW_RIGHT "\xef\x85\xb8"
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

//#ifndef NDEBUG
#define LOG_INFO(msg)  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_WARN(msg)  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_DEBUG(msg)  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, msg)
#define LOG_ERROR(msg)  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, msg)

#define LOGF_INFO( format, ...)	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_WARN( format, ...)	SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_DEBUG(format, ...)	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
#define LOGF_ERROR(format, ...)	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, format, __VA_ARGS__)
//#else
//#define LOG_INFO(...)
//#define LOG_WARN(...)
//#define LOG_DEBUG(...)
//#define LOG_ERROR(...)
//
//#define LOGF_INFO(...)
//#define LOGF_WARN(...)
//#define LOGF_DEBUG(...)
//#define LOGF_ERROR(...)
//#endif

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

#define FUN_ASSERT_X(expr, format, ...)
#define FUN_ASSERT(expr, msg)

#endif


class Util {
public:
	static bool LoadTextureFromFile(const char* filename, unsigned int* out_texture, int* out_width, int* out_height);

	template<typename T>
	inline static T Clamp(T val, T min, T max) noexcept {
		return std::max(std::min(val, max), min);
	}

	inline static auto LoadJson(const std::string& file, bool* success) { return LoadJson(file.c_str(), success); }
	inline static nlohmann::json LoadJson(const char* file, bool* success) {
		auto handle = SDL_RWFromFile(file, "rb");
		nlohmann::json j;
		if (handle != nullptr) {
			size_t size = handle->size(handle);
			char* buffer = new char[size+1];
			SDL_RWread(handle, buffer, sizeof(char), size);
			buffer[size] = '\0';
			if (size > 0) {
				j = nlohmann::json::parse(std::string(buffer, size), nullptr, false, false);
				*success = !j.is_discarded();
			}
			SDL_RWclose(handle);
			delete[] buffer;
		}
		else {
			LOGF_ERROR("Failed to load json: \"%s\"", file);
		}
		
		return j;
	}

	inline static void WriteJson(const nlohmann::json& json, const std::string& file, bool pretty = false) {
		return WriteJson(json, file.c_str(), pretty);
	}
	inline static void WriteJson(const nlohmann::json& json, const char* file, bool pretty = false) {
		auto handle = SDL_RWFromFile(file, "wb");
		if (handle != nullptr) {
			auto jsonText = json.dump((pretty) ? 4 : -1, ' ');
			SDL_RWwrite(handle, jsonText.data(), sizeof(char), jsonText.size());
			SDL_RWclose(handle);
		}
		else {
			LOGF_ERROR("Failed to save: \"%s\"\n%s", file, SDL_GetError());
		}
	}

	static inline size_t FormatTime(char* buffer, size_t buf_size, float time_seconds, bool with_ms) {
		if (std::isinf(time_seconds) || std::isnan(time_seconds)) time_seconds = 0.f;
		auto duration = std::chrono::duration<double>(time_seconds);
		std::time_t t = duration.count();
		std::tm timestamp = *std::gmtime(&t);

		size_t size = std::strftime(buffer, buf_size, "%H:%M:%S", &timestamp);
		if (!with_ms)
			return size;
		else {
			int32_t ms = (time_seconds - (int)time_seconds) * 1000.0;
			return stbsp_snprintf(buffer, buf_size, "%s.%03i", buffer, ms);
		}
	}

	static int OpenFileExplorer(const char* path);
	static int OpenUrl(const char* url);

	inline static std::filesystem::path Basepath() {
		char* base = SDL_GetBasePath();
		std::filesystem::path path(base);
		SDL_free(base);
		return path;
	}

	inline static std::string Filename(const std::string& path) {
		return std::filesystem::path(path)
			.replace_extension("")
			.filename()
			.string();
	}

	inline static bool FileExists(const std::string& file) { return FileExists(file.c_str()); }
	inline static bool FileExists(const char* file) {
		//std::filesystem::path file_path(file);
		//bool exists = std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
		
		// this sucks but unlike the code above works with unicode 
		// and std::filesystem::u8path keeps throwing ...
		// SDL2 uses utf-8 strings
		bool exists = false;
		auto handle = SDL_RWFromFile(file, "r");
		if (handle != nullptr) {
			SDL_RWclose(handle);
			exists = true;
		}
		else {
			LOGF_WARN("\"%s\" doesn't exist", file);
		}

		return exists;
	}
	inline static bool FileNamesMatch(std::filesystem::path path1, std::filesystem::path path2) {
		path1.replace_extension(""); path2.replace_extension();
		return path1.filename() == path2.filename();
	}

	static void Tooltip(const char* tip);

	// http://www.martinbroadhurst.com/how-to-trim-a-stdstring.html
	static std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ")
	{
		str.erase(0, str.find_first_not_of(chars));
		return str;
	}

	static std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ")
	{
		str.erase(str.find_last_not_of(chars) + 1);
		return str;
	}

	static std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ")
	{
		return ltrim(rtrim(str, chars), chars);
	}

	inline static bool ContainsInsensitive(const std::string& string1, const std::string& string2) {
		auto it = std::search(
			string1.begin(), string1.end(),
			string2.begin(), string2.end(),
			[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
		);
		return (it != string1.end());
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

	static std::string Resource(const std::string& path) noexcept;

	static std::string Prefpath(const std::string& path) {
		static const char* cachedPref = SDL_GetPrefPath("OFS", "OFS_data");
		static std::filesystem::path prefPath(cachedPref);
		std::filesystem::path rel(path);
		rel.make_preferred();
		return (prefPath / rel).string();
	}

	static bool CreateDirectories(const std::filesystem::path& dirs) {
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
};
