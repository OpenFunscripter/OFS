
for idx, action in ipairs(CurrentScript.actions) do
    if action.selected then
        action.tag = 1
        action.selected = false
    else
        if action.tag == 1 then
            action.selected = true
        end
        action.tag = 0
    end
end