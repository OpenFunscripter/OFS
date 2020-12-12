-- a useless script
-- showing the Clipboard context variable

if Clipboard.actions[1] then

    pasteOffset = CurrentTimeMs - Clipboard.actions[1].at -- indexing starts at 1 in lua ...
    print(pasteOffset)
    for idx, copiedAction in ipairs(Clipboard.actions) do
        CurrentScript:AddAction(copiedAction.at + pasteOffset, copiedAction.pos)
    end

else
    print("Nothing is in the copy buffer.")
end