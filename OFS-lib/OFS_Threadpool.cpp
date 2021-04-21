#include "OFS_Threadpool.h"
#include "OFS_Util.h"
#include "stb_sprintf.h"

OFS_Threadpool::OFS_Threadpool() noexcept
{
	NewWorkCond = SDL_CreateCond();
}

OFS_Threadpool::~OFS_Threadpool() noexcept
{
	Shutdown();
}

struct WorkThreadInitData
{
	uint32_t Id;
	OFS_Threadpool* pool;
	SDL_mutex* mutex;
};

static int WorkThread(void* data) 
{
	OFS_ThreadpoolWork work{0};
	OFS_ThreadpoolThreadData tdata{0};
	WorkThreadInitData* init = (WorkThreadInitData*)data;
	OFS_Threadpool* pool = init->pool;
	SDL_mutex* mutex = SDL_CreateMutex();
	SDL_LockMutex(mutex);
	tdata.ThreadId = init->Id;
	tdata.Lock = SDL_CreateSemaphore(1);
	delete init; init = 0;
	
	for(;;) {
		if (pool->WorkQueue.empty() && pool->ShouldExit) {
			break;
		}
		if (pool->WorkQueue.empty() && !pool->ShouldExit) {
			SDL_CondWait(pool->NewWorkCond, mutex);
		}
		
		SDL_AtomicLock(&pool->WorkLock);
		if (!pool->WorkQueue.empty()) {
			work = pool->WorkQueue.front();
			pool->WorkQueue.pop();
		}
		else {
			SDL_AtomicUnlock(&pool->WorkLock);
			continue;
		}
		SDL_AtomicUnlock(&pool->WorkLock);
		tdata.SharedPoolData = pool->SharedMemory;
		tdata.User = work.user;
		work.func(&tdata);

		// work.func lock the semaphore
		// but also has to unlock it
		SDL_SemWait(tdata.Lock);
		SDL_SemPost(tdata.Lock);
	}
	SDL_DestroyMutex(mutex);
	SDL_DestroySemaphore(tdata.Lock);
	return 0;
}

void OFS_Threadpool::Init(int threadCount) noexcept
{
	FUN_ASSERT(Threads.empty(), "already initialized");
	ShouldExit = false;
	char nameBuf[32];

	for (int i = 0; i < threadCount; ++i) {
		auto init = new WorkThreadInitData;
		init->Id = i;
		init->pool = this;
		stbsp_snprintf(nameBuf, sizeof(nameBuf), "OFS_Threadpool%d", i);
		Threads.push_back(SDL_CreateThread(WorkThread, (const char*)nameBuf, init));
	}
}

void OFS_Threadpool::DoWork(OFS_ThreadFunc func, void* user) noexcept
{
	SDL_AtomicLock(&WorkLock);
	WorkQueue.emplace(OFS_ThreadpoolWork{ func, user });
	SDL_AtomicUnlock(&WorkLock);
	SDL_CondSignal(NewWorkCond);
}

void OFS_Threadpool::Shutdown() noexcept
{
	if (ShouldExit) return;
	ShouldExit = true;
	SDL_CondBroadcast(NewWorkCond);

	for (auto t : Threads) {
		int s;
		SDL_WaitThread(t, &s);
	}
	SDL_DestroyCond(NewWorkCond);
	Threads.clear();
	FUN_ASSERT(!SharedMemory, "shared memory not freed");
}
