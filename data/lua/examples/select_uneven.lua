-- a script which selects all actions with an uneven index
-- indexing starts at 1 in lua

CurrentScript:DeselectAll()

for idx,action in ipairs(CurrentScript.actions) do	
    if idx % 2 == 1 then
        action.selected = true
    end
end