#include "OFP_WrappedPlayer.h"

#ifdef WIN32
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_events.h"

#include "OFS_Util.h"
#include "OFS_ImGui.h"
#include "EventSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WS2tcpip.h>
//#include <ws2tcpip.h>
#include <winsock2.h>
//#include <winsock.h>

#include <stdlib.h>
#include <stdio.h>

#include "OFP.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

WhirligigPlayer::~WhirligigPlayer()
{
    threadData.shouldExit = true;
    while (threadData.isRunning) { SDL_Delay(1); }
    WSACleanup();
}


void WhirligigPlayer::togglePlay() noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::setPositionExact(int32_t ms, bool pause) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::setPositionRelative(float pos, bool pause) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::seekRelative(int32_t ms) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::previousFrame() noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::nextFrame() noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::setVolume(float volume) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::setPaused(bool paused) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::setSpeed(float speed) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::addSpeed(float val) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::closeVideo() noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

void WhirligigPlayer::openVideo(const std::string& path) noexcept
{
    LOG_INFO("This does nothing on Whirligig");
}

float WhirligigPlayer::getFrameTimeMs() const noexcept
{
	return (1.f/60.f)*1000.f;
}

bool WhirligigPlayer::isPaused() const noexcept
{
	return !threadData.vars.playing;
}

float WhirligigPlayer::getPosition() const noexcept
{
    if (threadData.vars.totalDurationSeconds == 0.f)
    {
        // assume 3 hours because we don't want to divide by 0
        return getCurrentPositionSecondsInterp() / (60.f*60.f*3.f);
    }
    else
    {
        return getCurrentPositionSecondsInterp() / threadData.vars.totalDurationSeconds;
    }
	return 0.0f;
}

float WhirligigPlayer::getDuration() const noexcept
{
	return threadData.vars.totalDurationSeconds;
}

float WhirligigPlayer::getSpeed() const noexcept
{
	return 1.0f;
}

float WhirligigPlayer::getCurrentPositionMsInterp() const noexcept
{
	return getCurrentPositionSecondsInterp()*1000.f;
}

float WhirligigPlayer::getCurrentPositionSecondsInterp() const noexcept
{
    if (threadData.vars.playing) {
        return threadData.vars.currentTimeSeconds + ((SDL_GetTicks()-threadData.vars.startTicks)/1000.f);
    }
    else
    {
        return threadData.vars.currentTimeSeconds + ((threadData.vars.endTicks - threadData.vars.startTicks)/1000.f);
    }
}

const char* WhirligigPlayer::getVideoPath() const noexcept
{
	return threadData.vars.videoPath.empty() ? nullptr : threadData.vars.videoPath.c_str();
}

static constexpr const char* ConnectAddress = "127.0.0.1";
static constexpr const char* DefaultPort = "2000";
static constexpr size_t DefaultBufLen = 512;

void WhirligigPlayer::DrawVideoPlayer(bool* open, bool* show_video) noexcept
{
	ImGui::Begin(PlayerId);
    if (!threadData.connected)
    {
	    ImGui::TextUnformatted("Whirligig.");
	    ImGui::Text("Trying to connect to %s:%s", ConnectAddress, DefaultPort);
	    ImGui::SameLine(); OFS::Spinner("IDoBeWaitingForAConnection", ImGui::GetFontSize() / 2.f, ImGui::GetFontSize() / 4.f, IM_COL32(66, 150, 250, 255));
    }
    else
    {
        ImGui::TextUnformatted("Connected!");
    }
	ImGui::End();
}

bool WhirligigPlayer::setup()
{
    threadData.player = this;
    threadData.isRunning = true;

    auto threadLoop = [](void* ctx) -> int
    {

        WhirligigThreadData* data = (WhirligigThreadData*)ctx;

        WSADATA wsaData;
        struct addrinfo* result = NULL,
            * ptr = NULL,
            hints;
        
        char recvbuf[DefaultBufLen];
        int iResult;
        int recvbuflen = DefaultBufLen;
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);


        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        // Resolve the server address and port
        iResult = getaddrinfo(ConnectAddress, DefaultPort, &hints, &result);
        if (iResult != 0) {
            LOGF_ERROR("getaddrinfo failed with error: %d", iResult);
        }

        while (!data->shouldExit)
        {
            while (!data->connected && !data->shouldExit)
            {
                // Attempt to connect to an address until one succeeds
                for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

                    // Create a SOCKET for connecting to server
                    data->player->ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
                        ptr->ai_protocol);
                    if (data->player->ConnectSocket == INVALID_SOCKET) {
                        LOGF_ERROR("socket failed with error: %ld", WSAGetLastError());
                    }

                    // Connect to server.
                    iResult = connect(data->player->ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
                    if (iResult == SOCKET_ERROR) {
                        closesocket(data->player->ConnectSocket);
                        data->player->ConnectSocket = INVALID_SOCKET;
                        continue;
                    }
                    break;
                }

                if (data->player->ConnectSocket == INVALID_SOCKET) {
                    LOG_ERROR("Unable to connect to Whirligig.");
                    SDL_Delay(100);
                    continue;
                }
                data->connected = true;
            }
            if (data->shouldExit) { break; }

            // Send an initial buffer
            //iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
            //if (iResult == SOCKET_ERROR) {
            //    printf("send failed with error: %d\n", WSAGetLastError());
            //    closesocket(ConnectSocket);
            //    WSACleanup();
            //    return 1;
            //}
            //printf("Bytes Sent: %ld\n", iResult);

            // shutdown the connection since no more data will be sent
            //iResult = shutdown(data->player->ConnectSocket, SD_SEND);
            //if (iResult == SOCKET_ERROR) {
            //    LOGF_ERROR("shutdown failed with error: %d\n", WSAGetLastError());
            //    closesocket(data->player->ConnectSocket);
            //    return 1;
            //}

            auto parseWhirligigMsg = [](WhirligigThreadData* data, const char* msg, int len)
            {
                bool handled = false;
                for (int i = 0; i < len; i++)
                {
                    char c = msg[i];
                    switch (c)
                    {
                        case 'S':
                        {
                            // paused
                            if (data->vars.playing) {
                                data->vars.endTicks = SDL_GetTicks();
                            }
                            data->vars.playing = false;
                            EventSystem::PushEvent(VideoEvents::PlayPauseChanged);
                            handled = true;
                            break;
                        }
                        case 'P':
                        {
                            // P 12.642
                            float newTimeSeconds = strtof(&msg[i+1], (char**)&msg[len]);
                            if (newTimeSeconds != data->vars.currentTimeSeconds)
                            {
                                data->vars.currentTimeSeconds = newTimeSeconds;
                                data->vars.startTicks = SDL_GetTicks();
                                if (!data->vars.playing) {
                                    data->vars.playing = true;
                                    EventSystem::PushEvent(VideoEvents::PlayPauseChanged);
                                }
                            }
                            else {
                                if (data->vars.playing) {
                                    data->vars.playing = false;
                                    EventSystem::PushEvent(VideoEvents::PlayPauseChanged);
                                }
                            }
                            
                            handled = true;
                            break;
                        }
                        case 'C':
                        {
                            //C "E:\funscript\VR\\BaDoinkVR - Blowing The Blues - Riley Reid.mp4"
                            //    dometype = 6180
                            //    duration = 2186.304
                            int startIndex = -1;
                            int endIndex = len;
                            bool skippedDometype = false;
                            for (int x = i+1; x < len; x++)
                            {
                                c = msg[x];
                                switch (c)
                                {
                                    case '"':
                                    {
                                        if (startIndex < 0) { 
                                            startIndex = x+1;
                                        }
                                        else { 
                                            endIndex = x; 
                                            data->vars.videoPath = std::string(&msg[startIndex], endIndex - startIndex);
                                            EventSystem::SingleShot([](void* ctx)
                                                {
                                                    WhirligigThreadData* data = (WhirligigThreadData*)ctx;
                                                    data->ofp->openFile(data->vars.videoPath);
                                            }, data);
                                            SDL_Event ev;
                                            ev.type = VideoEvents::MpvVideoLoaded;
                                            ev.user.data1 = (void*)data->vars.videoPath.c_str();
                                            SDL_PushEvent(&ev);
                                        }
                                        break;
                                    }
                                    case '\n':
                                    {
                                        if (skippedDometype)
                                        {
                                            // parse duration
                                            x += strlen("duration = ") + 1;
                                            data->vars.totalDurationSeconds = strtof(&msg[x], (char**)&msg[len]);
                                            handled = true;
                                        }
                                        skippedDometype = true;
                                        break;
                                    }
                                }
                                if (handled) { break; }
                            }

                            break;
                        }
                        default:
                        {
                            LOGF_DEBUG("received %s", msg);
                            handled = true;
                        }
                    }
                    if (handled) { break; }
                }
            };

            // Receive until the peer closes the connection
            do {

                iResult = recv(data->player->ConnectSocket, recvbuf, recvbuflen, 0);
                FUN_ASSERT(iResult <= DefaultBufLen, "whoa this is to big");

                if (iResult > 0 && !data->shouldExit) {
                    recvbuf[iResult] = '\0';
                    parseWhirligigMsg(data, recvbuf, iResult);
                }
                else if (iResult == 0) {
                    LOG_INFO("Whirligig connection closed.");
                }
                else if (data->shouldExit) {
                    break;
                }
                else {
                    LOGF_ERROR("recv failed with error: %d", WSAGetLastError());
                }

            } while (iResult > 0);

            data->connected = false;
        }

        // cleanup
        freeaddrinfo(result);
        closesocket(data->player->ConnectSocket);
        data->isRunning = false;
        data->shouldExit = false;
        return 0;
    };

    auto thread = SDL_CreateThread(threadLoop, "WhirligigLoop", &threadData);
    SDL_DetachThread(thread);
	return true;
}

#endif // WIN32