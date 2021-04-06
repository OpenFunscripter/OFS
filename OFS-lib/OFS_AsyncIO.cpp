#include "OFS_AsyncIO.h"
#include "OFS_Util.h"

static int AsyncIO_Thread(void* data) noexcept
{
	OFS_AsyncIO* io = (OFS_AsyncIO*)data;
	for (;;)
	{
		if (io->ShouldExit && io->Writes.empty()) {
			break;
		}
		else {
			SDL_CondWait(io->WakeThreadCondition, io->ThreadMutex);
			if (io->Writes.empty()) break;
		}
		SDL_AtomicLock(&io->QueueLock);
		auto write = std::move(io->Writes.back());
		io->Writes.pop_back();
		SDL_AtomicUnlock(&io->QueueLock);

		size_t written = Util::WriteFile(write.Path.c_str(), write.Buffer, write.Size);
		FUN_ASSERT(written == write.Size, "fuck");
		write.Callback(write);
	}
	return 0;
}

void OFS_AsyncIO::Init() noexcept
{
	FUN_ASSERT(IO_Thread == nullptr, "thread already running");
	WakeThreadCondition = SDL_CreateCond();
	ThreadMutex = SDL_CreateMutex();
	IO_Thread = SDL_CreateThread(AsyncIO_Thread, "OFS_AsyncIO", this);
}

void OFS_AsyncIO::Shutdown() noexcept
{
	FUN_ASSERT(Writes.empty(), "Writes not empty!!!");
	ShouldExit = true;
	SDL_CondBroadcast(WakeThreadCondition);
	int result;
	SDL_WaitThread(IO_Thread, &result);
	SDL_DestroyCond(WakeThreadCondition);
	SDL_DestroyMutex(ThreadMutex);
}
