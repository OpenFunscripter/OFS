-- length
local MinStrokeLen = 30
local MaxStrokeLen = 100

-- duration in ms
local MinStrokeDuration = 100
local MaxStrokeDuration = 500


local LastTimeMs = 0
local LastPos = 0

CurrentScript:Clear()


local goingUp = true

while LastTimeMs < TotalTimeMs do
	CurrentScript:AddActionUnordered(LastTimeMs, LastPos)
	
	if goingUp then
		LastPos = LastPos + math.random(30, 100)
	else
		LastPos = LastPos - math.random(30, 100)
	end
	goingUp = not goingUp
	LastPos = clamp(LastPos, 0, 100)
	LastTimeMs = LastTimeMs + math.random(100, 500)

	SetProgress(LastTimeMs / TotalTimeMs)
end