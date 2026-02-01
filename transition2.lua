-- Debug script - catch PC_HI transition and dump NES PC
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\transition2.log", "w")
local frameCount = 0
local lastPcHi = 0xFF

logFile:write("Transition watch started\n\n")

-- Watch writes to PC high byte
emu.addMemoryCallback(function(addr, value)
    if frameCount > 100 then  -- After startup
        if value == 0x28 and lastPcHi ~= 0x28 then
            local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
            
            logFile:write(string.format("*** Frame %d: PC_HI $%02X -> $28 ***\n", frameCount, lastPcHi))
            logFile:write(string.format("Full emulated PC = $%04X\n", pcLo + 0x2800))
            
            -- Try to get NES PC
            local nesPC = emu.read(0xFFFC, emu.memType.cpuMemory)  -- Just as a test
            
            -- Dump zero page around emulated registers
            logFile:write("Zero page $50-$60:\n")
            for i = 0x50, 0x60 do
                logFile:write(string.format("  $%02X: $%02X\n", i, emu.read(i, emu.memType.cpuMemory)))
            end
            
            logFile:flush()
            emu.breakExecution()
        end
    end
    
    lastPcHi = value
end, emu.callbackType.write, 0x0059, 0x0059)

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount > 300 then
        logFile:write("=== 300 frames reached ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching...\n")
logFile:flush()
