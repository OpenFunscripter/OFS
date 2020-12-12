-- a script which generates a 1 minute sine wave

print("CurrentTimeMs: "..CurrentTimeMs)
print("FrameTimeMs: "..FrameTimeMs)

function wave(time, frequency)       
    return 0.5*(1.0 + math.sin(2.0 * math.pi * frequency * time));
end

-- CurrentScript:Clear() -- clears all actions

-- you can read variables from the console but it's pretty awkward
-- print("enter wave frequency: ")
-- freq = io.read("*n")

freq = 0.75 -- wave frequency

-- CurrentTimeMs is the starting value
-- CurrentTimeMs + 1 minute the end value
-- FrameTimeMs is the step size for example 16,666 ms in a 60 fps video
for ms=CurrentTimeMs,CurrentTimeMs+(60*1000), FrameTimeMs do
	pos = round(wave(ms/1000.0, freq) * 100.0);
	CurrentScript:AddAction(round(ms), pos, false)
end

print("action count:" .. #CurrentScript.actions )