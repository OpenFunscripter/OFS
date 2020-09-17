#pragma once

#include "SDL_log.h"
#include "nlohmann/json.hpp"

#include <memory>
#include <fstream>
#include <iomanip>
#include <filesystem>


// helper for FontAwesome. Version 4.7.0 2016 ttf
#define ICON_FOLDER_OPEN "\xef\x81\xbc"
#define ICON_VOLUME_UP "\xef\x80\xa8"
#define ICON_VOLUME_OFF "\xef\x80\xa6"
#define ICON_LONG_ARROW_UP "\xef\x85\xb6"
#define ICON_LONG_ARROW_DOWN "\xef\x85\xb5"
#define ICON_LONG_ARROW_RIGHT "\xef\x85\xb8"
#define ICON_PLAY "\xef\x81\x8b"
#define ICON_PAUSE "\xef\x81\x8c"




// this functionality seems to be available in c++20
template<class T, class... Args>
void overwrite_shared_ptr_content(std::shared_ptr<T>& ptr, Args&&... args) {
	if (ptr) { // nullptr check
		ptr->~T(); // destroy object held by pointer
		new(ptr.get()) T(std::forward<Args>(args)...); // construct new object in place 
	}
}


class Util {
public:
	static bool LoadTextureFromFile(const char* filename, unsigned int* out_texture, int* out_width, int* out_height);

	template<typename T>
	inline static T Clamp(T val, T min, T max) noexcept {
		return std::max(std::min(val, max), min);
	}

	inline static auto LoadJson(const std::string& file) { return LoadJson(file.c_str()); }
	inline static nlohmann::json LoadJson(const char* file) {
		std::ifstream i(file);
		nlohmann::json j;
		i >> j;
		return j;
	}

	inline static void WriteJson(const nlohmann::json& json, const std::string& file, bool pretty = false) {
		return WriteJson(json, file.c_str(), pretty);
	}
	inline static void WriteJson(const nlohmann::json& json, const char* file, bool pretty = false) {
		std::ofstream o(file);
		if(pretty)
			o << std::setw(4);
		o << json << std::endl;
	}

	inline static bool FileExists(const std::string& file) { return FileExists(file.c_str()); }
	inline static bool FileExists(const char* file) {
		std::filesystem::path file_path(file);
		return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
	}

};

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