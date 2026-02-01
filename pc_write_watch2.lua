-- Debug script - watch ALL writes to emulated PC to understand pattern
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\pc_write2.log", "w")
local frameCount = 0
local ignoreFrames = 5
local maxLogs = 500  -- Cap output
local logCount = 0
local lastPcLo = 0
local lastPcHi = 0

logFile:write("PC write watch started - logging ALL writes\n")
logFile:write("Watching for writes to $0058-$0059 (emulated PC)\n\n")

-- Watch writes to PC low byte
emu.addMemoryCallback(function(addr, value)
    if frameCount > ignoreFrames and logCount < maxLogs then
        if value ~= lastPcLo then  -- Only log if value changed
            local pcHi = emu.read(0x0059, emu.memType.cpuMemory)
            local fullPC = value + (pcHi * 256)
            logFile:write(string.format("Frame %d: PC_LO = $%02X, Full PC = $%04X\n", 
                frameCount, value, fullPC))
            lastPcLo = value
            logCount = logCount + 1
            logFile:flush()
        end
    end
end, emu.callbackType.write, 0x0058, 0x0058)

-- Watch writes to PC high byte
emu.addMemoryCallback(function(addr, value)
    if frameCount > ignoreFrames and logCount < maxLogs then
        if value ~= lastPcHi then  -- Only log if value changed
            local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
            local fullPC = pcLo + (value * 256)
            logFile:write(string.format("Frame %d: PC_HI = $%02X, Full PC = $%04X\n", 
                frameCount, value, fullPC))
            lastPcHi = value
            logCount = logCount + 1
            
            if value == 0x28 then
                logFile:write("*** POTENTIAL REBOOT: PC_HI = $28 ***\n")
            end
            logFile:flush()
        end
    end
end, emu.callbackType.write, 0x0059, 0x0059)

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount > 1200 then  -- 20 seconds
        logFile:write(string.format("=== Done, logCount=%d ===\n", logCount))
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching for PC writes (capped at 500)...\n")
logFile:flush()
