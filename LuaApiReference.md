# Extension API Reference

Here I'm trying to document all the API calls of the new Extension API.

- [Extension API Reference](#extension-api-reference)
- [Extension Structure](#extension-structure)
- [Core API](#core-api)
- [Video player API](#video-player-api)
- [Funscript API](#funscript-api)
  - [Some pitfalls](#some-pitfalls)
- [GUI API](#gui-api)
- [Process API](#process-api)
    - [Example usage](#example-usage)

# Extension Structure

Extension live in the `%appdata%/OFS_data/extensions/` directory.  
An extension must have it's own directory with a `main.lua` in it.  
By default there is a `Core` extension which serves as a demo.

```
OFS_data
├─ extensions
   ├─ Core
      ├─ main.lua
```

In OFS extensions become available trough the main menu bar "Extensions" and must be enabled before they can be used.  

The `main.lua` must define 3 functions otherwise things will explode.

```lua
function init()
    -- this runs once at when loading the extension
    -- custom keybindings are registered here with "ofs.Bind"
end

function update(delta)
    -- this runs every OFS frame
    -- delta is the time since the last call in seconds
    -- doing heavy computation here will lag OFS
end

function gui()
    -- this only runs when the window is open
    -- and is the place where a custom gui can be created
    -- doing heavy computation here will lag OFS
end
```

One of the biggest differences to the older Lua scripting API is that Extensions can have their own state.  
When an Extension gets enabled it gets it's own Lua VM seperate from other extensions.

# Core API
| Call        |Params| Returns | Description |
| ----------- |------| ------- |----------- |
| `print(msg)`|String| nil | Will print to the "Extension Log Output".<br/>Accessible through "Extensions"->"Show logs"|
| `clamp(val, min, max)`| Number, Number, Number | Number | Clamps a value.<br/>This is a Lua function which I placed in global scope. |
| `ofs.Bind(functionName, description)` |String, String| nil | Will create a "Dynamic" key binding.<br/>Must be called from within `init()`.<br/>Bindings will always run in another thread. |
| `ofs.Task(functionName)` | String | nil |Will run a function in another thread. Use cautiously. |
| `ofs.ExtensionDir()` | None | String | Path to extension directory. |


# Video player API

| Call        | Params | Returns | Description |
| ----------- | ------ | ------- | ----------- |
| `player.Play(shouldPlay)` | Optional bool | nil | If no bool is provided it will toggle.<br/>Otherwise true/false will play/pause the video. |
| `player.Seek(time)` | Number | nil | Seeks to a given time. Time must be passed in seconds. |
| `player.CurrentTime()`| None | Number | Returns the current position in the video in seconds. |
| `player.Duration()`| None | Number | Returns the total duration of the video in seconds. |
| `player.IsPlaying()`| None | bool | Returns a boolean if the player is playing or not. |
| `player.CurrentVideo()`| None | String | Returns a path to the currently playing video. |
| `player.FPS()`| None | Number | Returns the fps of the video. |

# Funscript API

`ofs.Script(index)` will copy the current state of the funscript on the C++ side and store it with the Lua table returned.  
The table returned isn't an ordinary table it contains some hidden data to make stuff work. 

## Some pitfalls

The actions in the Funscript table are also not ordinary Lua tables.  
All actions rely on a special meta table with the Lua metamethods `__index` & `_newindex` which call into C++.  
Which is why **you can not add/remove actions in a raw Lua way.**

```Lua
function BAD()
    -- ! THIS IS WHAT NOT TO DO !
    local script = ofs.Script(ofs.ActiveIdx())
    local action;
    action.at = 123
    action.pos = 0
    action.selected = false
    table.remove(script.actions, 1) -- THIS WON'T WORK
    table.insert(script.actions, action) -- THIS WON'T WORK
    ofs.Commit(script)
    -- ! THIS IS WHAT NOT TO DO !
end

function GOOD()
    -- this is the correct way to do it
    local script = ofs.Script(ofs.ActiveIdx())
     -- correct way of removing actions
    ofs.RemoveAction(script, script.actions[1])
     -- correct way of adding actions
    ofs.AddAction(script, 123, 0, false)
    ofs.Commit(script) -- save changes
end
```
Only use `ofs.AddAction` & `ofs.RemoveAction` to add & remove actions.  
Modifying actions can be done like this. 
```Lua
function iterate() 
    local script = ofs.Script(ofs.ActiveIdx())
    for idx, action in ipairs(script.actions) do
        -- action.at is in milliseconds
        action.at = idx * 1000 -- it may not look like it but this calls code on the C++ side
        action.pos = 0 
        action.selected = false
    end
    ofs.Commit(script) -- save changes
end
```

Actions/Funscripts can not be easily copied.  
Because their meta table still holds a reference to the Funscript table.
```Lua
function copyingActions()
    local script = ofs.Script(ofs.ActiveIdx())
    local action = script.actions[1]
    -- action still references the script
    -- changes made to `action` will go into `script`
    action.at = 1000
    action.pos = 0

    -- you can copy into a new lua table
    -- this table is detached from `script`
    local copied = { at = action.at, pos = action.pos, selected = action.selected }
    copied.at = 1234
    copied.pos = 0
end
```

| Call        | Params | Returns | Description |
| ----------- | ------ | ------- | ----------- |
| `ofs.Script(index)` | Number | Funscript | Given an index this returns a copy of a currently loaded Funscript.<br/>Following the Lua convention the index starts at 1. |
|`ofs.ActiveIdx()`| None | Number| Returns index of the currently active script.<br/>A common pattern would be `ofs.Script(ofs.ActiveIdx())` |
|`ofs.AddAction(script, time, pos, selected)`| Funscript, Number, Number, [Optional bool] | nil | Adds an action to the script. The selected bool is optional. <br/>**This is the only way to add actions.** |
|`ofs.RemoveAction(script, action)`| Funscript, Action | nil | Removes an action from the script.|
|`ofs.RemoveSelected(script)`| Funscript | nil | Removes all selected actions. | 
|`ofs.ClearScript(script)`| Funscript | nil | Removes all actions. |
|`ofs.HasSelection(script)`| Funscript | bool | Returns if the script has a selection. |
|`ofs.Commit(script)`| Funscript | nil | This creates an undo snapshot and saves changes back to OFS.<br/>If you forget to call commit nothing will change in OFS. |
|`ofs.Undo()`| None | nil | Will undo the last modification done by a Lua script.<br/>It will do nothing if the last modification wasn't done by a script.<br/>Essentially you only undo modifications by `ofs.Commit`. |
|`ofs.ScriptTitle(index)`| Number | String | Returns title of a funscript. |
|`ofs.ClosestAction(script, time)`| Funscript, Number | Number | Given a funscript and a time in seconds it returns an action index or nil. |
|`ofs.ClosestActionAfter(script, time)`| Funscript, Number | Number | Given a funscript and a time in seconds it returns an action index or nil. |
|`ofs.ClosestActionBefore(script, time)`| Funscript, Number | Number | Given a funscript and a time in seconds it returns an action index or nil. |
|`ofs.ScriptPath(idx)`| Number | String | Given an index it returns the funscript path. |
|`ofs.SaveScript(idx, path)`| Number, [Optional String] | bool | Given an index it saves the funscript.<br/>If no path is provided the default is used. |
|`ofs.SelectedIndices(script)`| Funscript | Array of Numbers | Returns an array of the selected action indices | 


# GUI API
This is a subset of ImGui functions exposed through Lua.  
These functions must be called from within `gui()`.  
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



| Call        | Params | Returns | Description |
| ----------- | ------ | ------- | ----------- |
| `ofs.Text(txt)` | String | nil | Can be used to display text. |
| `clicked = ofs.Button(txt)` | String | bool | Creates a button. Returns if the button was clicked. |
| `newValue, valueChanged = ofs.Input(txt, value, stepSize)` | String, (String \| Number), [optional Number] | (String \| Number), bool | Creates an Input field for either strings or numbers. |
| `newValue, valueChanged = ofs.Drag(txt, value, stepSize)` | String, Number, [optional Number] | Number, bool | Creates a drag button.|
| `newValue, valueChanged = ofs.Slider(txt, value, min, max)` | String, Number, Number, Number | Number, bool | Creates a slider. |
| `newValue, valueChanged = ofs.Checkbox(txt, value)` | String, Number, Number, Number | Number, bool | Creates a slider. |
| `ofs.SameLine()` | None | nil | Can be used to put things on the same line.<br/> For example if you want two buttons next two each other. |
| `ofs.Separator()` | None | nil | Inserts a separator. |
| `ofs.Spacing()` | None | nil | Controls after this will be a bit further spaced away. |
| `ofs.NewLine()` | None | nil | Controls after this will go into the next line. |

# Process API
| Call        |Params| Returns | Description |
| ----------- |------| ------- |----------- |
| `ofs.CreateProcess(program, args...)`| String, [variable amount of Strings]| ProcessHandle | Will create a process or return nil.|
| `ofs.IsProcessAlive(process)`| ProcessHandle | bool | Returns if the process is still running. |
| `ofs.JoinProcess(process)`| ProcessHandle | Number | Waits for the process to finish.<br/> Returns the exit code. |

### Example usage
```Lua
process_handle = nil

function start_process()
    -- store process handle globally so it doesn't get garbage collected
    process_handle = ofs.CreateProcess("notepad.exe", "arg1", "arg2", "arg3")
end

function terminate_process()
    -- to terminate the process let it be garbage collected like this
    process_handle = nil
end

function update(dt)
    -- detecting if the processs has finished could work like this
    if process_handle and not ofs.IsProcessAlive(process_handle) then
        print("process has terminated")
        process_handle = nil
    end
end
```