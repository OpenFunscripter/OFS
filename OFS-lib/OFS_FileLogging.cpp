#include "OFS_FileLogging.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#include "SDL_log.h"
#include "SDL_rwops.h"
#include "SDL_thread.h"
#include "SDL_timer.h"

#include "stb_sprintf.h"
#include <vector>


SDL_RWops* OFS_FileLogger::LogFileHandle = nullptr;

struct OFS_LogThread
{
    SDL_SpinLock lock;
    SDL_mutex* WaitMut = nullptr;
    SDL_cond* WaitMsg = nullptr;
    std::vector<char> LogMsgBuffer;

    volatile bool ShouldExit = false;
    volatile bool Exited = false;

    void Init() noexcept
    {
        WaitMsg = SDL_CreateCond();
        LogMsgBuffer.reserve(4096);
    }

    void Shutdown() noexcept
    {
        ShouldExit = true;
        SDL_CondSignal(WaitMsg);
        while(!Exited) { SDL_Delay(1); }
        SDL_DestroyCond(WaitMsg);
    }
};

OFS_LogThread Thread;

static int LogThreadFunction(void* threadData) noexcept
{
    auto& thread = *(OFS_LogThread*)threadData;
    auto& msg = thread.LogMsgBuffer;
    thread.WaitMut = SDL_CreateMutex();

    while(!thread.ShouldExit && OFS_FileLogger::LogFileHandle) {
        if(SDL_CondWait(thread.WaitMsg, thread.WaitMut) == 0) {
            SDL_AtomicLock(&thread.lock);
            SDL_RWwrite(OFS_FileLogger::LogFileHandle, msg.data(), 1, msg.size());
            msg.clear();
            SDL_AtomicUnlock(&thread.lock);
        }
    }

    SDL_DestroyMutex(thread.WaitMut);
    thread.Exited = true;

    return 0;
}

void OFS_FileLogger::Init() noexcept
{
    if(LogFileHandle) return;
    #ifndef NDEBUG
        SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    #endif
    auto LogFilePath = Util::Prefpath("OFS.log");
    LogFileHandle = SDL_RWFromFile(LogFilePath.c_str(), "w");

    Thread.Init();
    auto t = SDL_CreateThread(LogThreadFunction, "MessageLogging", &Thread);
    SDL_DetachThread(t);
}

void OFS_FileLogger::Shutdown() noexcept
{
    if(!LogFileHandle) return;
    Thread.Shutdown();
    SDL_RWclose(LogFileHandle);
}

inline static void LogToConsole(OFS_LogLevel level, const char* msg) noexcept
{
    switch(level) {
        case OFS_LogLevel::OFS_LOG_INFO: SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, msg); break;
        case OFS_LogLevel::OFS_LOG_WARN: SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, msg); break;
        case OFS_LogLevel::OFS_LOG_DEBUG: SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, msg); break;
        case OFS_LogLevel::OFS_LOG_ERROR: SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, msg); break;
    }
}

void OFS_FileLogger::LogToFileR(OFS_LogLevel level, const char* msg, uint32_t size) noexcept
{
    LogToConsole(level, msg);
    SDL_AtomicLock(&Thread.lock);
    const char* foo = Thread.LogMsgBuffer.data();
    auto currentSize = Thread.LogMsgBuffer.size();
    size = size == 0 ? strlen(msg) : size;
    Thread.LogMsgBuffer.resize(currentSize + size + 1);
    memcpy(Thread.LogMsgBuffer.data() + currentSize, msg, size);
    Thread.LogMsgBuffer.back() = '\n';
    SDL_AtomicUnlock(&Thread.lock);
}

void OFS_FileLogger::Flush() noexcept
{
    SDL_CondSignal(Thread.WaitMsg);
}

void OFS_FileLogger::LogToFileF(OFS_LogLevel level, const char* fmt, ...) noexcept
{
    char FormatBuffer[4096];
    va_list args;
    va_start(args, fmt);
    stbsp_vsnprintf(FormatBuffer, sizeof(FormatBuffer), fmt, args);
    va_end(args);
    LogToFileR(level, FormatBuffer);
}