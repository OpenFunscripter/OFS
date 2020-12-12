-- a script which selects all actions with an uneven index and deletes them

CurrentScript:DeselectAll()

for idx,action in ipairs(CurrentScript.actions) do	
    if idx % 2 == 1 then
        action.selected = true -- select uneven
    end
end

CurrentScript:RemoveSelected()