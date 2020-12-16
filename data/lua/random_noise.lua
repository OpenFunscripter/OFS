-- length
local MinStrokeLen = 30
local MaxStrokeLen = 100

-- duration in ms
local MinStrokeDuration = 150
local MaxStrokeDuration = 600


local LastTimeMs = 0
local LastPos = 0

CurrentScript:Clear()


local goingUp = true

while LastTimeMs < TotalTimeMs do
	CurrentScript:AddActionUnordered(LastTimeMs, LastPos)
	
	if goingUp then
		LastPos = LastPos + math.random(MinStrokeLen, MaxStrokeLen)
	else
		LastPos = LastPos - math.random(MinStrokeLen, MaxStrokeLen)
	end
	goingUp = not goingUp
	LastPos = clamp(LastPos, 0, 100)
	LastTimeMs = LastTimeMs + math.random(MinStrokeDuration, MaxStrokeDuration)

	SetProgress(LastTimeMs / TotalTimeMs)
end