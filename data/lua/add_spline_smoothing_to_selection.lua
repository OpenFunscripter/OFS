 -- this needs to be called "Settings" and a global to work
 Settings = {}
 Settings.PointEveryMs = 100
 SetSettings(Settings)
 -- anything using Settings needs to use it after "SetSettings"
 
 function catmullRom(v1, v2, v3, v4, s)
    s2 = s*s
    s3 = s*s*s
   
    f1 = -s3 + 2.0 * s2 - s;
    f2 = 3.0 * s3 - 5.0 * s2 + 2.0;
    f3 = -3.0 * s3 + 4.0 * s2 + s;
    f4 = s3 - s2;
   
    return (f1 * v1 + f2 * v2 + f3 * v3 + f4 * v4) / 2.0;
 end

 -- create a container to hold added actions temporarily
 -- since we can't add actions as we are iterating the script
 local TmpScript = Funscript:new()
 
 local actionCount = #CurrentScript.actions
 
 for idx, action in ipairs(CurrentScript.actions) do
    local i0 = clamp(idx - 1, 1, actionCount);
    local i1 = clamp(idx, 1, actionCount);
    local i2 = clamp(idx + 1, 1, actionCount);
    local i3 = clamp(idx + 2, 1, actionCount);

    local action1 = CurrentScript.actions[i0]
    local action2 = CurrentScript.actions[i1]
    local action3 = CurrentScript.actions[i2]
    local action4 = CurrentScript.actions[i3]

    -- skip all actions which aren't selected
    -- since lua doesn't have a continue,
    -- we use a goto + a label at the bottom of the loop
    if not action2.selected or not action3.selected then goto continue end

    local startTime = action2.at
    local endTime = action3.at
    local duration = endTime - startTime;

    pointCount = duration/Settings.PointEveryMs;

    for i=1, pointCount, 1 do
        s = (i*Settings.PointEveryMs) / duration
        time_ms = math.floor(action2.at + (i*Settings.PointEveryMs))
        spline_pos = catmullRom(action1.pos, action2.pos, action3.pos, action4.pos, s)
        TmpScript:AddActionUnordered(time_ms, spline_pos)
    end
 
     -- update progress bar every 100 actions
     if idx % 100 == 0 then
         -- SetProgress expects a number between 0 and 1
         SetProgress(idx / #CurrentScript.actions)
     end

     ::continue::
 end
 
 -- copy actions from the temporary container to the CurrentScript
 print("adding easing points")
 for idx, action in ipairs(TmpScript.actions) do
     CurrentScript:AddActionUnordered(action.at, action.pos, true) -- mark all added actions as selected
 
     if idx % 100 == 0 then
         -- SetProgress expects a number between 0 and 1
         SetProgress(idx / #TmpScript.actions)
     end
 end