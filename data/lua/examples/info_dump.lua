-- this script shows dumping of information into a seperate file

function disp_time(time)
    local hours = math.floor(math.fmod(time, 86400)/3600)
    local minutes = math.floor(math.fmod(time,3600)/60)
    local seconds = math.floor(math.fmod(time,60))
    return string.format("%02d:%02d:%02d",hours,minutes,seconds)
end

info_dump_path = VideoFileDirectory .. CurrentScript.title .. ".log"

file = io.open(info_dump_path, "w")

io.output(file)

io.write("video duration: " .. disp_time(TotalTimeMs/1000) .. "\n")

stroke_count = 0
last_action = nil
direction = nil

-- This stroke counting may be wrong
-- I havn't verified it
for i, action in ipairs(CurrentScript.actions) do
    if last_action then
        if direction then
            if direction and action.pos < last_action.pos then
                direction = false
                stroke_count = stroke_count + 1
            elseif not direction and action.pos > last_action.pos then
                direction = true
                stroke_count = stroke_count + 1
            end
        else
            if action.pos ~= last_action.pos then
                direction = action.pos > last_action.pos
            else
                direction = nil
            end
        end
    end
    last_action = action
end

io.write("stroke count: " .. stroke_count .. "\n")
io.write("average strokes per second: " .. stroke_count / (TotalTimeMs/1000) .. "\n")

io.close(file)

print(info_dump_path)
os.execute("\"" .. info_dump_path .. "\"")