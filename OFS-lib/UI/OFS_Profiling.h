#pragma once

#include "OFS_Util.h"
#include <chrono>
#include <map>
#include <string>
#include <cstring>

#if OFS_PROFILE_ENABLED == 1
#include "Tracy.hpp"
#endif

/*
* This is not useful in hot code paths.
*/
class OFS_Benchmark
{
private:
	const char* Function = nullptr;
	int Line = 0;
	const char* File = nullptr;
	std::chrono::high_resolution_clock::time_point start;
public:
	inline OFS_Benchmark(const char* function, const char* file, int line) noexcept
		: Function(function), Line(line), File(file)
	{
		start = std::chrono::high_resolution_clock::now();
	}

	inline ~OFS_Benchmark() noexcept
	{
		std::chrono::duration<float, std::milli> delta = std::chrono::high_resolution_clock::now() - start;
		LOGF_INFO("Benchmark: %s:%d\n\t\"%s\" took %.3f ms to exceute.", File, Line, Function, delta.count());
	}
};

#if WIN32
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define OFS_CONCAT_(x,y) x##y
#define OFS_CONCAT(x,y) OFS_CONCAT_(x,y)

#if OFS_BENCHMARK_ENABLED == 1
#define OFS_BENCHMARK(function) OFS_Benchmark OFS_CONCAT(xBenchmarkx_,__LINE__) ## (function, __FILENAME__, __LINE__)
#else
#define OFS_BENCHMARK(function)
#endif

class OFS_Profiler
{
public:
	inline static void BeginProfiling() noexcept
	{
		//FrameMark;
	}
	inline static void EndProfiling() noexcept
	{
		FrameMark;
		//FrameMarkEnd(nullptr);
	}
};


#if OFS_PROFILE_ENABLED == 1
#define OFS_PROFILE(name) ZoneScopedN(name)
#define OFS_BEGINPROFILING() OFS_Profiler::BeginProfiling()
#define OFS_ENDPROFILING() OFS_Profiler::EndProfiling();
#else
#define OFS_PROFILE(name)
#define OFS_BEGINPROFILING()
#define OFS_ENDPROFILING()
#endif

