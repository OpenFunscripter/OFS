#pragma once

#include <functional>
#include <queue>
#include <string>

#include "SDL_thread.h"
#include "SDL_mutex.h"

/*
* I wrote this because spinning up thread to save takes longer than you'd think (like 1ms :/)
*/
class OFS_AsyncIO
{
public:
	struct Write
	{
		std::string Path;
		uint8_t* Buffer = nullptr;
		size_t Size = 0;
		void* Userdata = nullptr;
		std::function<void(Write&)> Callback = [](Write&) {};
	};

	SDL_SpinLock QueueLock;

	SDL_cond* WakeThreadCondition = nullptr;

	std::queue<Write> Writes;

	SDL_Thread* IO_Thread = nullptr;
	bool ShouldExit = false;

	inline void PushWrite(Write&& write) noexcept
	{
		SDL_AtomicLock(&QueueLock);
		Writes.emplace(std::move(write));
		SDL_AtomicUnlock(&QueueLock);

		SDL_CondSignal(WakeThreadCondition);
	}

	void Init() noexcept;
	void Shutdown() noexcept;
};