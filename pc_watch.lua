-- Debug script to catch when emulated PC ($0058-$0059) becomes $2800
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\pc_watch.log", "w")
local frameCount = 0
local ignoreFrames = 5  -- Ignore first 5 frames

logFile:write("PC watch debug started\n")
logFile:write("Watching emulated PC at $0058-$0059 for value $2800\n\n")

-- Track previous PC values
local prevPCs = {}
local pcIndex = 0

emu.addEventCallback(function()
    frameCount = frameCount + 1
    
    -- Check emulated PC value
    local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
    local pcHi = emu.read(0x0059, emu.memType.cpuMemory)
    local emulatedPC = pcLo + (pcHi * 256)
    
    -- Store in circular buffer
    pcIndex = (pcIndex % 20) + 1
    prevPCs[pcIndex] = emulatedPC
    
    if frameCount > ignoreFrames and emulatedPC == 0x2800 then
        logFile:write("=== REBOOT DETECTED at frame " .. frameCount .. " ===\n")
        logFile:write("Emulated PC = $2800\n")
        logFile:write("Previous PC values:\n")
        for i = 1, 20 do
            local idx = ((pcIndex - i) % 20) + 1
            if prevPCs[idx] then
                logFile:write(string.format("  -%d: $%04X\n", i, prevPCs[idx]))
            end
        end
        
        -- Dump emulated stack
        local sp = emu.read(0x005A, emu.memType.cpuMemory)
        logFile:write(string.format("\nEmulated SP = $%02X\n", sp))
        logFile:write("Emulated stack contents ($6C00 + $100 + SP):\n  ")
        -- Stack is at RAM_BASE ($6C00) + $100
        local stackBase = 0x6C00 + 0x100
        for i = sp + 1, 0xFF do
            local val = emu.read(stackBase + i, emu.memType.cpuMemory)
            logFile:write(string.format("%02X ", val))
            if (i - sp) % 16 == 0 then
                logFile:write("\n  ")
            end
        end
        logFile:write("\n\n")
        
        -- Also dump the run_count variable
        local runCountLo = emu.read(0x60DF, emu.memType.cpuMemory)  -- Check map for actual address
        local runCountHi = emu.read(0x60E0, emu.memType.cpuMemory)
        logFile:write(string.format("run_count = %d\n", runCountLo + (runCountHi * 256)))
        
        logFile:flush()
        emu.breakExecution()
    end
    
    if frameCount > 3600 then
        logFile:write("=== 3600 frames reached, no reboot detected ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching started...\n")
logFile:flush()
