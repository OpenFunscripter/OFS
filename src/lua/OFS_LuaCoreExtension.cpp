#include "OFS_LuaCoreExtension.h"
#include "OFS_LuaExtensions.h"
#include "OFS_Util.h"

#include <filesystem>

static constexpr const char* Source = R"(
-- globals
Jitter = {}
Jitter.TimeMs = 10
Jitter.Position = 15

Random = {}
Random.MinStrokeLen = 30
Random.MaxStrokeLen = 100
Random.MinStrokeDuration = 150
Random.MaxStrokeDuration = 600

Spline = {}
Spline.PointEveryMs = 100

function init()
    -- this runs once at when loading the extension
    print("initialized")
end

function update(delta)
    -- this runs even when the gui is closed
end

function gui()
    -- this only runs when the window is open
    ofs.Text("Jitter settings")
    Jitter.TimeMs, changedTime = ofs.Drag("Time ms", Jitter.TimeMs)
    Jitter.Position, changedPos = ofs.Drag("Position", Jitter.Position)   

    -- prevent negative or 0
    Jitter.TimeMs = math.max(Jitter.TimeMs, 1)
    Jitter.Position = math.max(Jitter.Position, 1)

    if changedPos or changedTime then
        ofs.Undo() -- ofs.Undo() only undoes changes by Lua scripts never anything else
        jitter()
    end

    ofs.Separator()
    ofs.Text("Random noise")
    Random.MinStrokeLen = ofs.Input("Min length", Random.MinStrokeLen)
    Random.MaxStrokeLen = ofs.Input("Max length", Random.MaxStrokeLen)
    Random.MinStrokeDuration = ofs.Input("Min duration", Random.MinStrokeDuration)
    Random.MaxStrokeDuration = ofs.Input("Max duration", Random.MaxStrokeDuration)
    if ofs.Button("Add random noise") then
        binding.random_noise()
    end

    if ofs.Button("Select all") then
        select_all()
    end

    if ofs.Button("Select none") then
        select_none()
    end

    ofs.Separator()
    ofs.Text("Spline smooth")
    Spline.PointEveryMs, splineChanged = ofs.Slider("Point per ms", Spline.PointEveryMs, 20, 500)
    if splineChanged then
        ofs.Undo()
        binding.spline_smooth()
    end
end

function binding.spline_smooth()
    local catmullRom = function(v1, v2, v3, v4, s)
        s2 = s*s
        s3 = s*s*s
    
        f1 = -s3 + 2.0 * s2 - s;
        f2 = 3.0 * s3 - 5.0 * s2 + 2.0;
        f3 = -3.0 * s3 + 4.0 * s2 + s;
        f4 = s3 - s2;
    
        return (f1 * v1 + f2 * v2 + f3 * v3 + f4 * v4) / 2.0;
    end
    local script = ofs.Script(ofs.ActiveIdx())
    local actionCount = #script.actions

    if not script:hasSelection() then
        return
    end

    local smoothedActions = {}

    for idx, action in ipairs(script.actions) do
        local i0 = clamp(idx - 1, 1, actionCount);
        local i1 = clamp(idx, 1, actionCount);
        local i2 = clamp(idx + 1, 1, actionCount);
        local i3 = clamp(idx + 2, 1, actionCount);
    
        local action1 = script.actions[i0]
        local action2 = script.actions[i1]
        local action3 = script.actions[i2]
        local action4 = script.actions[i3]
    
        -- skip all actions which aren't selected
        -- since lua doesn't have a continue,
        -- we use a goto + a label at the bottom of the loop
        if not action2.selected or not action3.selected then goto continue end


        local startTime = action2.at
        local endTime = action3.at
        local duration = endTime - startTime;
    
        local pointEverySecond = Spline.PointEveryMs / 1000.0

        pointCount = duration/pointEverySecond
   
        for i=1, pointCount-1, 1 do
            s = (i*pointEverySecond) / duration
            time_ms = action2.at + (i*pointEverySecond)
            spline_pos = catmullRom(action1.pos, action2.pos, action3.pos, action4.pos, s)
            spline_pos = clamp(spline_pos, 0, 100)
            table.insert( smoothedActions, {at=time_ms, pos=spline_pos} )
        end
        ::continue::
    end

    for idx, action in ipairs(smoothedActions) do
        script.actions:add(Action.new(action.at, action.pos))
    end
    script:commit()
end

function binding.random_noise()
    local script = ofs.Script(ofs.ActiveIdx())
    script.actions:clear()

    local LastTimeMs = 0
    local LastPos = 0
    local TotalTimeMs = player.Duration() * 1000.0

    local goingUp = true
    while LastTimeMs < TotalTimeMs do
        script.actions:add(Action.new(LastTimeMs/1000.0, LastPos))
       
        if goingUp then
            LastPos = LastPos + math.random(Random.MinStrokeLen, Random.MaxStrokeLen)
        else
            LastPos = LastPos - math.random(Random.MinStrokeLen, Random.MaxStrokeLen)
        end
        goingUp = not goingUp
        LastPos = clamp(LastPos, 0, 100)
        LastTimeMs = LastTimeMs + math.random(Random.MinStrokeDuration, Random.MaxStrokeDuration)
    end

    script:commit()
end

function select_all() 
    local script = ofs.Script(ofs.ActiveIdx())
    for idx, action in ipairs(script.actions) do
        action.selected = true
    end
    script:commit()
end

function select_none()
    local script = ofs.Script(ofs.ActiveIdx())
    for idx, action in ipairs(script.actions) do
        action.selected = false
    end
    script:commit()
end

function binding.jitter()
    local script = ofs.Script(ofs.ActiveIdx())

    if not script:hasSelection() then
        return
    end
    
    for idx, action in ipairs(script.actions) do
        -- check if action is selected
        if action.selected then
            local time_jitter_value = math.random(-Jitter.TimeMs, Jitter.TimeMs) 
            local pos_jitter_value = math.random(-Jitter.Position, Jitter.Position)

            -- ATTENTION
            -- there's a bug here
            -- the timestamp change can reorder actions when that happens
            -- the position jitter will be applied to the wrong action

            -- A more sophisticated function would check the next and the previous action
            -- to ensure that the new time is still in between them.
            action.at = action.at + time_jitter_value
            action.pos = action.pos + pos_jitter_value
        end
    end
    script:commit()
end
)";

void OFS_CoreExtension::setup() noexcept
{
    std::error_code ec;
    auto path = Util::PathFromString(Util::Prefpath(OFS_LuaExtensions::ExtensionDir)) / "Core";
    #if 1
    if(true) {
    #else
    if (!std::filesystem::exists(path, ec)) {
    #endif
        Util::CreateDirectories(path);
        path /= "main.lua";
        auto pString = path.u8string();
        auto handle = Util::OpenFile(pString.c_str(), "wb", pString.size());
        if (handle) {
            auto size = strlen(Source);
            SDL_RWwrite(handle, Source, size, sizeof(char));
            SDL_RWclose(handle);
        }
    }
}
