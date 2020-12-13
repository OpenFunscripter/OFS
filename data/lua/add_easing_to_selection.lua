-- frequency every n frames
-- round(100/FrameTimeMs) makes it roughly so that one point every 100 ms gets placed
local point_frequency = 100 / FrameTimeMs

-- create a container to hold added actions temporarily
-- since we can't add actions as we are iterating the script
local TmpScript = Funscript:new()


-- https://easings.net/#easeInOutCubic
-- applies easing to the linear interpolation
function easeInOutCubic(start_val, end_val, x)
    if x < 0.5 then
        x = 4 * x * x * x
    else
        x = 1-((-2 * x + 2)^3) / 2
    end
    return lerp(start_val, end_val, x)
end


-- variable to hold the previous action
-- initialized with nil
local previous_action = nil

for idx, action in ipairs(CurrentScript.actions) do
    -- skip all actions which aren't selected
    -- since lua doesn't have a continue,
    -- we use a goto + a label at the bottom of the loop
    if not action.selected then goto continue end

    -- check if previous_action is set (not nil)
    if previous_action then
        -- calculate duration between previous_action and action
        duration = action.at - previous_action.at

        -- amount of points to add between previous_action and action
        point_count = round(duration / (FrameTimeMs * point_frequency))-1
        
        -- generate points between previous_action and action       
        for i=1, point_count, 1 do
            progress = i/(point_count+1)
            
            time_ms = round(previous_action.at + (i*FrameTimeMs * point_frequency))
            interpolated_pos = round(easeInOutCubic(previous_action.pos, action.pos, progress))

            -- store new action in TmpScript
            -- this is faster than "AddAction"
            -- since we don't have to care about the order in which actions are stored
            -- in this case
            TmpScript:AddActionUnordered(time_ms, interpolated_pos)
            -- TmpScript:AddAction(time_ms, interpolated_pos) -- terrible performance. use only when temporal order matters
        end
    end

    -- update progress bar every 100 actions
    if idx % 100 == 0 then
        -- SetProgress expects a number between 0 and 1
        SetProgress(idx / #CurrentScript.actions)
    end
    previous_action = action

    ::continue::
end

-- copy actions from the temporary container to the CurrentScript
print("adding easing points")
for idx, action in ipairs(TmpScript.actions) do

    -- AddAction defined in funscript.lua searches all the actions to find the place where to insert to keep the actions ordered by time
    -- but we don't actually need to keep the order which is why AddActionUnordered
    -- when OFS loads the data back it will reorder them
    CurrentScript:AddActionUnordered(action.at, action.pos, true) -- mark all added actions as selected
    -- CurrentScript:AddAction(action.at, action.pos, true) -- terrible performance. use only when temporal order matters


    if idx % 100 == 0 then
        -- SetProgress expects a number between 0 and 1
        SetProgress(idx / #TmpScript.actions)
    end
end