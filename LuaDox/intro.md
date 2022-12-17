# How to create an extension

- [How to create an extension](#how-to-create-an-extension)
- [Extension Structure](#extension-structure)
- [Keybindings](#keybindings)
- [Custom UI](#custom-ui)

# Extension Structure

Extension live in the `%appdata%/OFS3_data/extensions/` directory.  
An extension must have it's own directory with a `main.lua` in it.  
By default there is a `Core` extension which serves as a demo.

```
OFS3_data
|-- extensions
   |-- Core
      |-- main.lua
```

In OFS extensions become available trough the main menu bar `Extensions` and must be enabled before they can be used.  

The `main.lua` must define three functions otherwise it just won't work.

```lua
function init()
    -- this runs once when enabling the extension
end

function update(delta)
    -- this runs every OFS frame
    -- delta is the time since the last call in seconds
    -- doing heavy computation here will lag OFS
end

function gui()
    -- this only runs when the window is open
    -- this is the place where a custom gui can be created
    -- doing heavy computation here will lag OFS
end
```

A new optional function which can be defined is `scriptChange(scriptIdx)`.
```lua
function scriptChange(scriptIdx) 
    -- is called when a funscript gets changed in some way
    -- this can be used for validation (or other creative ways?)
    local s = ofs.Script(scriptIdx)
end
```

# Keybindings

Keybindings are created by adding them to a global `binding` table **before** `init()` is called.   
This must be done in global scope.

```lua
-- In both cases you can still call these functions as binding.keybinding1() & binding.keybinding2()
-- clean way
function binding.keybinding1()
-- do something
end

-- OR

-- savage way
binding.keybinding2 = function()
-- do something
end
```     
    
    
# Custom UI
GUI functions must be called from within the *gui()* function.  
A list of functions can be found [here](/module/ofs.html#gui).
You can't pass values by reference in Lua which is why things like `ofs.Slider` have multiple return values.

```Lua
x = 0 -- global value x

function gui()
    x, valueChanged = ofs.Slider("x", x, 0, 100) -- slider min: 0 & max: 100
    if valueChanged then
        print("The value changed!")
    end
end
```

**When using multiple controls with the same name you have to give them a unique id.**
<br>This applies to all controls not just buttons.
```Lua
function gui()
    -- THIS WON'T WORK
    if ofs.Button("Apply") then
        -- do something
    end
    if ofs.Button("Apply") then
        -- do something
    end

    -- Do this instead
    -- unique id "ApplyBtn1"
    if ofs.Button("Apply##ApplyBtn1") then
        -- do something
    end
    -- unique id "ApplyBtn2"
    if ofs.Button("Apply##ApplyBtn2") then
        -- do something
    end
end
```