 -- this needs to be called "Settings" and a global to work
 Settings = {}
-- length
Settings.MinStrokeLen = 30
Settings.MaxStrokeLen = 100
-- duration in ms
Settings.MinStrokeDuration = 150
Settings.MaxStrokeDuration = 600
SetSettings(Settings)
-- anything using Settings needs to use it after "SetSettings"

local LastTimeMs = 0
local LastPos = 0

CurrentScript:Clear()


local goingUp = true

while LastTimeMs < TotalTimeMs do
	CurrentScript:AddActionUnordered(LastTimeMs, LastPos)
	
	if goingUp then
		LastPos = LastPos + math.random(Settings.MinStrokeLen, Settings.MaxStrokeLen)
	else
		LastPos = LastPos - math.random(Settings.MinStrokeLen, Settings.MaxStrokeLen)
	end
	goingUp = not goingUp
	LastPos = clamp(LastPos, 0, 100)
	LastTimeMs = LastTimeMs + math.random(Settings.MinStrokeDuration, Settings.MaxStrokeDuration)

	SetProgress(LastTimeMs / TotalTimeMs)
end