--- Core functions
--
-- @module ofs


--- Print to the extension log.
--
-- Takes a variable amount of arguments strings and numbers.
-- @example 
--   print("Number", 42) --  "Number 42"
-- @scope .
function print(...) end

--- Clamp a value
-- @tparam number val
-- @tparam number min
-- @tparam number max
-- @treturn number Result
-- @scope .
function clamp(val, min, max) end

--- Get the API version
-- @treturn number Version
function ofs.Version() end

--- Get the extension directory path
-- @treturn string Path
function ofs.ExtensionDir() end

--- Get the currently loaded script count
-- @treturn number Count
function ofs.ScriptCount() end

--- Get the script name
-- @tparam number scriptIdx
-- @treturn String Name
function ofs.ScriptName(scriptIdx) end

--- Funscript.
-- @section funscript

--- Gets the index of the currently active script
-- @treturn number scriptIdx
function ofs.ActiveIdx() end

--- Get a currently loaded script
-- @treturn Funscript funscript
-- @example 
--   local script = ofs.Script(ofs.ActiveIdx())
function ofs.Script(scriptIdx) end

--- Get a read-only version of the clipboard
-- @treturn Funscript clipboard
function ofs.Clipboard() end

--- Undo the last modification
-- @treturn bool hasUndoneSomething
-- @note Note
--   This function can only undo modifications done by a Lua extension.
function ofs.Undo() end


--- GUI.
-- @note Important
--   All of these functions must be called from within the `gui()` function.
-- @section gui

--- Display text
-- @tparam string txt
-- @treturn nil
function ofs.Text(txt) end

--- Create a button
-- @tparam string txt
-- @treturn bool clicked
-- @example 
--   if ofs.Button("Click me") then
--     print("I was clicked!")
--   end
function ofs.Button(txt) end

--- Create an input field
-- @tparam string txt
-- @tparam string|number value
-- @tparam number|nil stepSize only applies to numeric inputs,
-- @treturn string|number value
-- @treturn bool valueChanged
-- @example
--   -- global variables
--   text = ""
--   number = 5
--   function gui()
--     text, textChanged = ofs.Input("Text", text)
--     number, valueChanged = ofs.Input("Number", number, 2)
--   end
function ofs.Input(txt, value, stepSize) end

--- Create an input field
-- @tparam string txt
-- @tparam string|number value
-- @tparam number|nil stepSize only applies to numeric inputs,
-- @treturn number value
-- @treturn bool valueChanged
function ofs.InputInt(txt, value, stepSize) end

--- Create a numeric drag input
-- @tparam string txt
-- @tparam number value
-- @tparam number|nil stepSize
-- @treturn number value
-- @treturn bool valueChanged
function ofs.Drag(txt, value, stepSize) end

--- Create a numeric integer drag input
-- @tparam string txt
-- @tparam number value
-- @tparam number|nil stepSize
-- @treturn number value
-- @treturn bool valueChanged
function ofs.DragInt(txt, value, stepSize) end

--- Create a numeric slider
-- @tparam string txt
-- @tparam number value
-- @tparam number min 
-- @tparam number max
-- @treturn number value
-- @treturn bool valueChanged
function ofs.Slider(txt, value, min, max) end


--- Create a numeric integer slider
-- @tparam string txt
-- @tparam number value
-- @tparam number min 
-- @tparam number max
-- @treturn number value
-- @treturn bool valueChanged
function ofs.SliderInt(txt, value, min, max) end

--- Create a checkbox
-- @tparam string txt
-- @tparam bool checked
-- @treturn bool checked
-- @treturn bool checkChanged
function ofs.Checkbox(txt, checked) end

--- Create a combobox
-- @tparam string txt
-- @tparam number currentIdx
-- @tparam string[] items
-- @treturn number currentIdx
-- @treturn bool selectionChanged
function ofs.Combo(txt, currentIdx, items) end

--- Create a collapsable header
-- @example
--   function gui()
--     if ofs.CollapsingHeader("abc") then
--        ofs.Text("This text is only visible when the header is opened")
--     end
--   end
-- @tparam string txt
-- @treturn bool headerOpened
function ofs.CollapsingHeader(txt) end

--- Put next control on the same line as the previous
-- @example
--   ofs.Button("Button 1")
--   ofs.SameLine()
--   ofs.Button("Button 2")
-- @treturn nil
function ofs.SameLine() end

--- Insert a separator
-- @treturn nil
function ofs.Separator() end

--- Insert a new line between controls
-- @example
--   ofs.Button("Button 1")
--   ofs.NewLine()
--   ofs.Button("Button 2")
-- @treturn nil
function ofs.NewLine() end

--- Create a tooltip
-- @tparam string txt
-- @example 
--   ofs.Button("...")
--   ofs.Tooltip("The button does X") -- displayed when hovering the button
-- @treturn nil
function ofs.Tooltip(txt) end

--- Begin disabled area
-- @tparam bool disabled
-- @example 
--   ofs.BeginDisabled(true)
--   ofs.Button("...") -- button is forever disabled
--   ofs.EndDisabled()
-- @treturn nil
function ofs.BeginDisabled(disabled) end

--- End disabled area
-- @treturn nil
function ofs.EndDisabled(disabled) end

--- Player functions
--
-- @module player

--- Get the current time in the video
-- @treturn number Time in seconds
function player.CurrentTime() end

--- Seek to time
-- @tparam number time Time in seconds
-- @treturn nil
function player.Seek(time) end

--- Get the duration
-- @treturn number Time in seconds
function player.Duration() end

--- Control playback
-- @tparam bool|nil shouldPlay Toggles when no value is passed.
-- @treturn nil
function player.Play(shouldPlay) end

--- Gets if the player is playing
-- @treturn bool isPlaying
function player.IsPlaying() end

--- Gets the path to the current video
-- @treturn string Path
function player.CurrentVideo() end

--- Get the FPS of the video
-- @treturn number FPS
function player.FPS() end

--- Get the width of the video
-- @treturn number Pixels
function player.Width() end

--- Get the height of the video
-- @treturn number Pixels
function player.Height() end

--- Control playback speed
--
-- The value is automatically clamped between 0.05 minimum speed and 3.0 maximum speed
-- @meta read/write
-- @type number
playbackSpeed = 1.0

--- Funscript returned by `ofs.Script()`
-- @see funscript
-- @display Funscript
-- @class Funscript

--- Array of actions
-- @meta read/write
-- @type Action[]
actions = {}

--- Default save path
-- @meta read-only
-- @type string
path = ""

--- Name of the script
-- @meta read-only
-- @type string
name = ""

--- Gets if the script has a selection
-- @treturn bool hasSelection
function Funscript:hasSelection() end

--- Commit the changes
-- @treturn nil
function Funscript:commit() end

--- Sort the actions array
-- @treturn nil
function Funscript:sort() end

--- Get the closest action to a given time
-- @tparam number time Time in seconds
-- @treturn Action closest
-- @treturn number index
function Funscript:closestAction(time) end

--- Get the closest action after a given time
-- @tparam number time Time in seconds
-- @treturn Action closest
-- @treturn number index
function Funscript:closestActionAfter(time) end

--- Get the closest action before a given time
-- @tparam number time Time in seconds
-- @treturn Action closest
-- @treturn number index
function Funscript:closestActionBefore(time) end

--- Get an array of selected indices into the actions array
-- @treturn number[] indices
function Funscript:selectedIndices() end

--- Mark an action for removal
-- @tparam number actionIdx
-- @treturn nil
-- @example
--   for idx, action in ipairs(script.actions) do
--     if action.pos > 50 then
--       script:markForRemoval(idx)
--     end
--   end
--   script:removeMarked()
function Funscript:markForRemoval(actionIdx) end

--- Remove all marked actions
-- @treturn number removedCount
function Funscript:removeMarked() end


--- Action creation
-- @module action

--- Create a new action
-- @tparam number at
-- @tparam number pos
-- @tparam bool|nil selected
-- @treturn Action action
function Action.new(at, pos, selected) end

--- Action returned by `Action.new()`
-- @see action
-- @display Action
-- @class Action

--- Time in seconds
-- @type number
-- @meta read/write
at = 0

--- Position (0 - 100)
-- @type number
-- @meta read/write
pos = 0

--- Is selected
-- @type bool
-- @meta read/write
selected = false


--- Process creation
-- @module process

--- Create a new process
-- @display Process.new
-- @treturn Process|nil Returns a process on success or nil
function Process.new(program, ...) end

--- Process handle returned by `Process.new()`
--
-- If the handle goes out of scope the process may get killed. (This is not guaranteed)
-- @see process
-- @display Process
-- @class Process
-- @example 
--  function notepad()
--    local p = Process.new("notepad.exe", "file.txt")
--    if p:alive() then
--      print("it's alive")
--      local code = p:join()
--      print("Process returned with", code)
--    end
--  end

--- Is the process alive
-- @treturn bool isAlive
function Process:alive() end

--- Join the process
-- (blocking)
-- @treturn number Return code
function Process:join() end

--- Detach the process letting it run freely
-- @treturn nil
function Process:detach() end

--- Kill the process
-- @treturn nil
function Process:kill() end
