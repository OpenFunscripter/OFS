#pragma once
#if OFS_PROFILE_ENABLED == 1
#include "Tracy.hpp"
#endif

#if OFS_PROFILE_ENABLED == 1
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
#endif

#if OFS_PROFILE_ENABLED == 1
#define OFS_PROFILE(name) ZoneScopedN(name)
#define OFS_BEGINPROFILING() OFS_Profiler::BeginProfiling()
#define OFS_ENDPROFILING() OFS_Profiler::EndProfiling();
#else
#define OFS_PROFILE(name)
#define OFS_BEGINPROFILING()
#define OFS_ENDPROFILING()
#endif

