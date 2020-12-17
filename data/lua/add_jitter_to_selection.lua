 -- this needs to be called "Settings" and a global to work
 Settings = {}
 -- -10 to +10 ms jitter
 Settings.time_jitter_ms = 10
 -- -15 to +15 jitter 
 Settings.pos_jitter = 15 
 SetSettings(Settings)
 -- anything using Settings needs to use it after "SetSettings"


for idx, action in ipairs(CurrentScript.actions) do
   -- check if action is selected
   if action.selected then
      local time_jitter_value = math.random(-Settings.time_jitter_ms, Settings.time_jitter_ms) 
      local pos_jitter_value = math.random(-Settings.pos_jitter, Settings.pos_jitter)

      
      -- apply jitter
      action.at =  action.at + time_jitter_value
      action.pos = action.pos + pos_jitter_value

      -- make sure at doesn't become negative
      action.at = math.max(action.at, 0)

      -- clamp position between 0 and 100
      action.pos = clamp(action.pos, 0, 100)     
   end

   -- update progress bar every 100 actions
   if idx % 100 == 0 then
      -- SetProgress expects a number between 0 and 1
      SetProgress(idx / #CurrentScript.actions)
   end
end