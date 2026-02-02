-- Quick test script - auto-presses START to skip waiting for demo mode
-- Usage: Mesen.exe exidy.nes --lua quicktest.lua

local frame_count = 0
local start_pressed = false

function onFrame()
    frame_count = frame_count + 1
    
    -- Wait a few frames for the game to initialize, then press START
    if frame_count >= 60 and frame_count < 90 then
        -- Press START for 30 frames
        emu.setInput(0, { start = true })
    elseif frame_count >= 90 and frame_count < 120 then
        -- Release for a bit
        emu.setInput(0, { })
    elseif frame_count >= 120 and frame_count < 150 then
        -- Press START again to begin game
        emu.setInput(0, { start = true })
    elseif frame_count >= 150 then
        -- Done with auto-start, let user play
        if not start_pressed then
            emu.log("Quick test: Auto-start complete at frame " .. frame_count)
            start_pressed = true
        end
    end
end

emu.addEventCallback(onFrame, emu.eventType.endFrame)
emu.log("Quick test script loaded - will auto-press START")
