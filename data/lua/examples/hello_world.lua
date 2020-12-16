-- a useless hello world script

print("hello world!")

for i,v in ipairs(CurrentScript.actions) do	
    print(v)
end

print("frametime: " .. FrameTimeMs)
print("current time: " .. CurrentTimeMs)
print("action count:" .. #CurrentScript.actions )