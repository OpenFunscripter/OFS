-- title contains the filename wihtout \".funscript\" extension
-- title is a readonly property changing it does nothing
print("CurrentScript.title: " .. CurrentScript.title)

-- alternative way of getting the CurrentScript
print("LoadedScripts[CurrentScriptIdx].title: " .. LoadedScripts[CurrentScriptIdx].title)

-- LoadedScripts contains all loaded scripts
for scriptIdx, script in ipairs(LoadedScripts) do
    print(script.title)
    for actionIdx, action in ipairs(script.actions) do
        action.at = action.at + FrameTimeMs
        action.pos = action.pos + 1
    end
end