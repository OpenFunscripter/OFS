#include "OFS_AsyncIO.h"
#include "OFS_Util.h"

static int AsyncIO_Thread(void* data) noexcept
{
	OFS_AsyncIO* io = (OFS_AsyncIO*)data;
	SDL_mutex* mutex = SDL_CreateMutex();
	SDL_LockMutex(mutex);
	for (;;)
	{
		if (io->ShouldExit && io->Writes.empty()) {
			break;
		}
		else if(io->Writes.empty()) {
			SDL_CondWait(io->WakeThreadCondition, mutex);
			if (io->Writes.empty()) break;
		}
		SDL_AtomicLock(&io->QueueLock);
		auto write = std::move(io->Writes.front());
		io->Writes.pop();
		SDL_AtomicUnlock(&io->QueueLock);

		size_t written = Util::WriteFile(write.Path.c_str(), write.Buffer, write.Size);
		FUN_ASSERT(written == write.Size, "fuck");
		write.Callback(write);
	}
	SDL_DestroyMutex(mutex);
	return 0;
}

void OFS_AsyncIO::Init() noexcept
{
	FUN_ASSERT(IO_Thread == nullptr, "thread already running");
	WakeThreadCondition = SDL_CreateCond();
	IO_Thread = SDL_CreateThread(AsyncIO_Thread, "OFS_AsyncIO", this);
}

void OFS_AsyncIO::Shutdown() noexcept
{
	ShouldExit = true;
	SDL_CondBroadcast(WakeThreadCondition);
	int result;
	SDL_WaitThread(IO_Thread, &result);
	FUN_ASSERT(Writes.empty(), "Writes not empty!!!");
	SDL_DestroyCond(WakeThreadCondition);
}
