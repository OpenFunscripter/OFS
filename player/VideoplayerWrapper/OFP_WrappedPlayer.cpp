#include "OFP_WrappedPlayer.h"

#ifdef WIN32
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_events.h"

#include "OFS_Util.h"
#include "OFS_ImGui.h"
#include "EventSystem.h"

// HACK: used here for openFile, could fire an event to remove the necessity of this include
#include "OFP.h" 

#include "cluon.hpp"

void WhirligigPlayer::parseWhirligigMsg(std::string& msg) noexcept
{
    bool handled = false;
    for (int i = 0; i < msg.size(); i++)
    {
        char c = msg[i];
        switch (c)
        {
        case 'S':
        {
            // paused
            if (vars.playing) {
                vars.endTicks = SDL_GetTicks();
            }
            vars.playing = false;
            EventSystem::PushEvent(VideoEvents::PlayPauseChanged);
            handled = true;
            break;
        }
        case 'P':
        {
            // P 12.642
            float newTimeSeconds = strtof(&msg[i + 1], (char**)&msg[msg.size()]);
            if (newTimeSeconds != vars.currentTimeSeconds)
            {
                vars.currentTimeSeconds = newTimeSeconds;
                vars.startTicks = SDL_GetTicks();
                if (!vars.playing) {
                    vars.playing = true;
                    EventSystem::PushEvent(VideoEvents::PlayPauseChanged);
                }
            }
            else {
                if (vars.playing) {
                    vars.playing = false;
                    EventSystem::PushEvent(VideoEvents::PlayPauseChanged);
                }
            }

            handled = true;
            break;
        }
        case 'C':
        {
            //C "E:\funscript\VR\\BaDoinkVR - Blowing The Blues - Riley Reid.mp4"
            int startIndex = -1;
            int endIndex = msg.size();
            bool skippedDometype = false;
            for (i = i + 1; i < msg.size(); i++)
            {
                c = msg[i];
                if (c == '"')
                {
                    if (startIndex < 0) {
                        startIndex = i + 1;
                    }
                    else {
                        endIndex = i;
                        vars.videoPath = std::string(&msg[startIndex], endIndex - startIndex);
                        EventSystem::SingleShot([](void* ctx)
                            {
                                WhirligigThreadData* thread = (WhirligigThreadData*)ctx;
                                thread->ofp->openFile(thread->player->vars.videoPath);
                            }, &threadData);
                        SDL_Event ev;
                        ev.type = VideoEvents::MpvVideoLoaded;
                        ev.user.data1 = (void*)vars.videoPath.c_str();
                        SDL_PushEvent(&ev);
                        handled = true;
                    }
                }
            }
            break;
        }
        case 'd':
        {
            //    dometype = 6180
            //    duration = 2186.304
            for (i = i + 1; i < msg.size(); i++)
            {
                c = msg[i];
                if (c == 'o') { handled = true; break; } // dometype ignored
                else if (c == 'u') {
                    i += strlen("ration = ");
                    vars.totalDurationSeconds = strtof(&msg[i + 1], (char**)&msg[msg.size()]);
                    handled = true; 
                    break;
                }
            }
            break;
        }
        default:
        {
            LOGF_DEBUG("whirligig: unknown data: %s", msg);
            handled = true;
        }
        }
        if (handled) { break; }
    }
}

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
	return !vars.playing;
}

float WhirligigPlayer::getPosition() const noexcept
{
    if (vars.totalDurationSeconds == 0.f)
    {
        // assume 3 hours because we don't want to divide by 0
        return getCurrentPositionSecondsInterp() / (60.f*60.f*3.f);
    }
    else
    {
        return getCurrentPositionSecondsInterp() / vars.totalDurationSeconds;
    }
	return 0.0f;
}

float WhirligigPlayer::getDuration() const noexcept
{
	return vars.totalDurationSeconds;
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
    if (vars.playing) {
        return vars.currentTimeSeconds + ((SDL_GetTicks()-vars.startTicks)/1000.f);
    }
    else
    {
        return vars.currentTimeSeconds + ((vars.endTicks - vars.startTicks)/1000.f);
    }
}

const char* WhirligigPlayer::getVideoPath() const noexcept
{
	return vars.videoPath.empty() ? nullptr : vars.videoPath.c_str();
}

static constexpr const char* ConnectAddress = "127.0.0.1";
static constexpr uint16_t DefaultPort = 2000;

void WhirligigPlayer::DrawVideoPlayer(bool* open, bool* show_video) noexcept
{
	ImGui::Begin(PlayerId);
    if (!threadData.connected)
    {
	    ImGui::TextUnformatted("Whirligig.");
	    ImGui::Text("Trying to connect to %s:%hu", ConnectAddress, DefaultPort);
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
        data->connected = false;

        std::stringstream ss;
        while (!data->shouldExit)
        {
            LOG_INFO("Trying to connect to Whirligig");
            cluon::TCPConnection con(ConnectAddress, DefaultPort,
                [data, &ss](auto&& str, auto&& time) {
                    for (auto c : str) {
                        ss << c;
                        if (c == '\n')
                        {
                            auto msg = ss.str();
                            data->player->parseWhirligigMsg(msg);
                            ss.str(std::string());
                        }
                    }
                },
                []() {
                    LOG_INFO("Whirligig connection lost");
                });

            if (!con.isRunning()) { SDL_Delay(50); }
            else {
                data->connected = true;
                while (con.isRunning()) { SDL_Delay(10); if(data->shouldExit) { break; } }
            }
            data->connected = false;
        }

        // cleanup
        data->isRunning = false;
        data->shouldExit = false;
        return 0;
    };

    auto thread = SDL_CreateThread(threadLoop, "WhirligigLoop", &threadData);
    SDL_DetachThread(thread);
	return true;
}

#endif // WIN32