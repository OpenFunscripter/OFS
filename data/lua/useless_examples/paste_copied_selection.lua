-- a useless script
-- showing the Clipboard context variable

-- check if something is in the clipboard
if Clipboard.actions[1] then

    -- calculate paste offset based on the current time
    -- and the timestamp of the first action in the clipboard
    pasteOffset = CurrentTimeMs - Clipboard.actions[1].at -- indexing starts at 1 in lua ...

    -- paste clipboard actions into the script with the offset applied
    for idx, copiedAction in ipairs(Clipboard.actions) do
        CurrentScript:AddAction(copiedAction.at + pasteOffset, copiedAction.pos)
    end

else
    print("Nothing is in the copy buffer.")
end