script = CurrentScript

function wave(time, frequency)       
    return 0.5*(1.0 + math.sin(2.0 * math.pi * frequency * time));
end


script:Clear()

print("enter wave frequency: ")
freq = io.read("*n")

for ms=0,60*1000, 100 do
	pos = wave(ms/1000.0, freq) * 100;
	script:AddAction(ms, pos)
end

print("action count:" .. #script.actions )
for i,v in ipairs(script.actions) do	
	v.pos = v.pos + 1
end