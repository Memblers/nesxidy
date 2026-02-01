-- Debug script - read initial values and monitor state
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\state_monitor.log", "w")
local frameCount = 0

logFile:write("State monitor started\n\n")

-- Check state every 10 frames
emu.addEventCallback(function()
    frameCount = frameCount + 1
    
    if frameCount <= 200 and frameCount % 10 == 0 then
        local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
        local pcHi = emu.read(0x0059, emu.memType.cpuMemory)
        local sp = emu.read(0x005A, emu.memType.cpuMemory)
        local a = emu.read(0x005B, emu.memType.cpuMemory)
        local x = emu.read(0x005C, emu.memType.cpuMemory)
        local y = emu.read(0x005D, emu.memType.cpuMemory)
        local status = emu.read(0x005E, emu.memType.cpuMemory)
        
        logFile:write(string.format("Frame %3d: PC=$%04X SP=$%02X A=$%02X X=$%02X Y=$%02X S=$%02X\n",
            frameCount, pcLo + pcHi*256, sp, a, x, y, status))
        
        -- Check for suspicious state
        if sp == 0x00 and pcHi == 0x28 and frameCount > 100 then
            logFile:write("  *** SUSPICIOUS - SP=0 and PC=$28xx ***\n")
        end
        
        logFile:flush()
    end
    
    if frameCount > 200 then
        logFile:write("\n=== Done ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)
