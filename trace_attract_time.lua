-- trace_attract_time.lua
-- Mesen 2 script: measure how long the attract delay takes
-- Breakpoint at $2E58 (entry) and $2E6D (RTS at end of delay)
-- Reports frames elapsed

local start_frame = nil
local started = false

emu.addEventCallback(function()
    local pc = emu.getState()["cpu.pc"]
    
    if pc == 0x2E58 and not started then
        start_frame = emu.getState()["ppu.frameCount"]
        started = true
        emu.log("Attract delay START at frame " .. start_frame)
    end
    
    if pc == 0x2E6D and started then
        local end_frame = emu.getState()["ppu.frameCount"]
        local elapsed = end_frame - start_frame
        emu.log("Attract delay END at frame " .. end_frame .. " elapsed=" .. elapsed .. " frames (" .. string.format("%.1f", elapsed/60.0) .. " sec at 60fps)")
        started = false
    end
    
    -- Also report every 600 frames (~10 seconds) if still in delay
    if started then
        local cur = emu.getState()["ppu.frameCount"]
        if (cur - start_frame) % 600 == 0 and cur ~= start_frame then
            emu.log("Still in delay at frame " .. cur .. " elapsed=" .. (cur - start_frame) .. " frames")
        end
    end
end, emu.eventType.startFrame)
