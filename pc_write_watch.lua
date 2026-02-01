-- Debug script - watch for writes to emulated PC that set it to $2800
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\pc_write.log", "w")
local frameCount = 0
local ignoreFrames = 5

logFile:write("PC write watch started\n")
logFile:write("Watching for writes to $0058-$0059 (emulated PC)\n\n")

-- Watch writes to PC low byte
emu.addMemoryCallback(function(addr, value)
    if frameCount > ignoreFrames then
        local pcHi = emu.read(0x0059, emu.memType.cpuMemory)
        local fullPC = value + (pcHi * 256)
        if fullPC == 0x2800 or value == 0x00 then  -- $00 is low byte of $2800
            logFile:write(string.format("Frame %d: Write to PC_LO ($0058) = $%02X, PC_HI = $%02X, Full PC = $%04X\n", 
                frameCount, value, pcHi, fullPC))
            if fullPC == 0x2800 then
                logFile:write("*** REBOOT DETECTED ***\n")
                logFile:flush()
                emu.breakExecution()
            end
        end
    end
end, emu.callbackType.write, 0x0058, 0x0058)

-- Watch writes to PC high byte
emu.addMemoryCallback(function(addr, value)
    if frameCount > ignoreFrames then
        local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
        local fullPC = pcLo + (value * 256)
        if value == 0x28 then  -- $28 is high byte of $2800
            logFile:write(string.format("Frame %d: Write to PC_HI ($0059) = $%02X, PC_LO = $%02X, Full PC = $%04X\n", 
                frameCount, value, pcLo, fullPC))
            if fullPC == 0x2800 then
                logFile:write("*** REBOOT DETECTED ***\n")
                logFile:flush()
                emu.breakExecution()
            end
        end
    end
end, emu.callbackType.write, 0x0059, 0x0059)

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount > 1200 then  -- 20 seconds
        logFile:write("=== 1200 frames reached ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching for PC writes...\n")
logFile:flush()
