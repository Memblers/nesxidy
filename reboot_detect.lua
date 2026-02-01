-- Debug script - watch for PC_HI becoming $28 AFTER startup (reboot detection)
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\reboot_detect.log", "w")
local frameCount = 0
local startupComplete = false
local lastPcHi = 0

logFile:write("Reboot detection started\n")
logFile:write("Will detect when PC_HI becomes $28 after startup\n\n")

-- Watch writes to PC high byte
emu.addMemoryCallback(function(addr, value)
    -- After frame 100, startup is definitely complete
    if frameCount > 100 then
        startupComplete = true
    end
    
    if startupComplete and value == 0x28 and lastPcHi ~= 0x28 then
        local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
        local fullPC = pcLo + (value * 256)
        logFile:write(string.format("\n*** REBOOT DETECTED at Frame %d ***\n", frameCount))
        logFile:write(string.format("PC_HI changed from $%02X to $%02X\n", lastPcHi, value))
        logFile:write(string.format("Full PC = $%04X\n", fullPC))
        
        -- Dump stack area
        logFile:write("\nStack dump ($0100-$01FF, emulated stack):\n")
        local sp = emu.read(0x005A, emu.memType.cpuMemory)  -- emulated SP
        logFile:write(string.format("Emulated SP = $%02X\n", sp))
        for i = 0, 15 do
            local stackAddr = 0x01FF - i
            local stackVal = emu.read(stackAddr + 0x6C00 - 0x0100, emu.memType.cpuMemory)  -- RAM_BASE offset
            logFile:write(string.format("  $%04X: $%02X\n", stackAddr, stackVal))
        end
        
        -- Dump zero page CPU state
        logFile:write("\nCPU state:\n")
        logFile:write(string.format("  A = $%02X\n", emu.read(0x005B, emu.memType.cpuMemory)))
        logFile:write(string.format("  X = $%02X\n", emu.read(0x005C, emu.memType.cpuMemory)))
        logFile:write(string.format("  Y = $%02X\n", emu.read(0x005D, emu.memType.cpuMemory)))
        logFile:write(string.format("  Status = $%02X\n", emu.read(0x005E, emu.memType.cpuMemory)))
        
        logFile:flush()
        emu.breakExecution()
    end
    
    lastPcHi = value
end, emu.callbackType.write, 0x0059, 0x0059)

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount % 100 == 0 then
        logFile:write(string.format("Frame %d...\n", frameCount))
        logFile:flush()
    end
    if frameCount > 1200 then
        logFile:write("=== 1200 frames reached without reboot ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching for reboot (PC_HI -> $28 after frame 100)...\n")
logFile:flush()
