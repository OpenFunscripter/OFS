#pragma once
#include "OFS_Event.h"
#include "Funscript.h"
#include <cstdint>

class FunscriptActionClickedEvent : public OFS_Event<FunscriptActionClickedEvent>
{
    public:
    FunscriptAction action;
    std::weak_ptr<Funscript> script;

    FunscriptActionClickedEvent(FunscriptAction action, std::weak_ptr<Funscript> script) noexcept
        : action(action), script(script) {}    
};

class FunscriptActionShouldMoveEvent : public OFS_Event<FunscriptActionShouldMoveEvent>
{
    public:
    FunscriptAction action;
    std::weak_ptr<Funscript> script;
    bool moveStarted = false;

    FunscriptActionShouldMoveEvent(FunscriptAction action, std::weak_ptr<Funscript> script, bool hasStarted) noexcept
        : action(action), script(script), moveStarted(hasStarted) {}
};

class FunscriptActionShouldCreateEvent : public OFS_Event<FunscriptActionShouldCreateEvent>
{
    public:
    FunscriptAction newAction;
    std::weak_ptr<Funscript> script;
    FunscriptActionShouldCreateEvent(FunscriptAction newAction, std::weak_ptr<Funscript> script) noexcept
        : newAction(newAction), script(script) {}
};

class ShouldSetTimeEvent : public OFS_Event<ShouldSetTimeEvent>
{
    public:
    float newTime = 0.f;
    ShouldSetTimeEvent(float newTime) noexcept
        : newTime(newTime) {}
};

class ShouldChangeActiveScriptEvent : public OFS_Event<ShouldChangeActiveScriptEvent>
{
    public:
    uint32_t activeIdx = 0;
    ShouldChangeActiveScriptEvent(uint32_t activeIdx) noexcept
        : activeIdx(activeIdx) {}
};

class WaveformProcessingFinishedEvent : public OFS_Event<WaveformProcessingFinishedEvent>
{
    public:
};

class FunscriptShouldSelectTimeEvent : public OFS_Event<FunscriptShouldSelectTimeEvent>
{
    public:
    float startTime;
    float endTime;
    bool clearSelection;
    std::weak_ptr<Funscript> script;
    FunscriptShouldSelectTimeEvent(float startTime, float endTime, bool clear, std::weak_ptr<Funscript> script) noexcept
        : startTime(startTime), endTime(endTime), clearSelection(clear), script(script) {}
};
