#pragma once
#include "SDL_thread.h"

#include <vector>
#include <queue>

// insane c-style function pointer
typedef int (*OFS_ThreadFunc)(void*);

struct OFS_ThreadpoolWork
{
	OFS_ThreadFunc func = 0;
	void* user = 0;
};

struct OFS_ThreadpoolThreadData
{
	uint32_t ThreadId = 0;
	volatile void* SharedPoolData = 0;
	SDL_semaphore* Lock = 0;
	void* User = 0;
};

class OFS_Threadpool
{
public:
	SDL_cond* NewWorkCond = 0;
	SDL_SpinLock WorkLock = 0;
	volatile void* SharedMemory = nullptr;
	std::vector<SDL_Thread*> Threads;
	std::queue<OFS_ThreadpoolWork> WorkQueue;
	volatile bool ShouldExit = false;
	
	OFS_Threadpool() noexcept;
	~OFS_Threadpool() noexcept;

	void Init(int threadCount) noexcept;
	void DoWork(OFS_ThreadFunc func, void* user) noexcept;
	void Shutdown() noexcept;
};
