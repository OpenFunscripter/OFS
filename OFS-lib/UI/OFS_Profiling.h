#pragma once

#include "OFS_Util.h"
#include <chrono>
#include <map>
#include <string>
#include <cstring>

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

#if OFS_BENCHMARK == 1
#define OFS_BENCHMARK(function) OFS_Benchmark OFS_CONCAT(xBenchmarkx_,__LINE__) ## (function, __FILENAME__, __LINE__)
#else
#define OFS_BENCHMARK(function)
#endif



class OFS_Codepath
{
public:
	std::string Name;
	int16_t Depth = 0;
	bool Done = false;
	std::chrono::high_resolution_clock::time_point Start;
	std::chrono::duration<float, std::milli> Duration;
	std::vector<OFS_Codepath> Children;
};

class OFS_Profiler
{
	static std::vector<OFS_Codepath> Stack;
	static std::vector<OFS_Codepath> Frame;
	static std::vector<OFS_Codepath> LastFrame;

	static bool RecordOnce;
	static bool Live;
public:
	inline OFS_Profiler(const std::string& Path) noexcept
	{
		if (!Live && !RecordOnce) return;
		OFS_Codepath newPath;
		newPath.Name = Path;
		newPath.Start = std::chrono::high_resolution_clock::now();
		if (!Stack.empty())
		{
			if (!Stack.back().Done) {
				newPath.Depth = Stack.back().Depth + 1;
			}
		}
		Stack.emplace_back(std::move(newPath));
	}

	inline ~OFS_Profiler() noexcept
	{
		if (!Live && !RecordOnce) return;
		auto p = std::move(Stack.back());
		Stack.pop_back();
		p.Duration = std::chrono::high_resolution_clock::now() - p.Start;
		p.Done = true;

		Frame.emplace_back(std::move(p));
	}

	static void ShowProfiler() noexcept;

	static void BeginProfiling() noexcept;
	static void EndProfiling() noexcept;
};


#if OFS_PROFILE == 1
#define OFS_PROFILEPATH(path) OFS_Profiler OFS_CONCAT(xProfilerx,__LINE__) ## ( path )
#define OFS_SHOWPROFILER() OFS_Profiler::ShowProfiler();
#define OFS_BEGINPROFILING() OFS_Profiler::BeginProfiling()
#define OFS_ENDPROFILING() OFS_Profiler::EndProfiling();
#else
#define OFS_PROFILEPATH(path)
#define OFS_SHOWPROFILER() 
#define OFS_BEGINPROFILING()
#define OFS_ENDPROFILING()
#endif

