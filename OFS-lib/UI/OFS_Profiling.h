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
	//std::string Name;
	std::chrono::high_resolution_clock::time_point Start;
	std::chrono::duration<float, std::milli> LastDuration;
};

class OFS_Profiler
{
	static std::map<std::string, OFS_Codepath> Paths;
	std::string Path;
public:
	inline OFS_Profiler(const std::string& Path) noexcept
	{
		this->Path = Path;
		auto it = Paths.find(Path);
		if (it == Paths.end())
		{
			OFS_Codepath newPath;
			newPath.Start = std::chrono::high_resolution_clock::now();
			auto p = Paths.insert(std::move(std::make_pair(Path, std::move(newPath))));
		}
		else
		{			
			it->second.Start = std::chrono::high_resolution_clock::now();
		}
	}

	inline ~OFS_Profiler() noexcept
	{
		auto it = Paths.find(Path);
		it->second.LastDuration = std::chrono::high_resolution_clock::now() - it->second.Start;
	}

	static void ShowProfiler() noexcept;
};


#if OFS_PROFILE == 1
#define OFS_PROFILEPATH(path) OFS_Profiler OFS_CONCAT(xProfilerx,__LINE__) ## ( path )
#define OFS_SHOWPROFILER() OFS_Profiler::ShowProfiler();
#else
#define OFS_PROFILEPATH(path)
#define OFS_SHOWPROFILER() 
#endif

