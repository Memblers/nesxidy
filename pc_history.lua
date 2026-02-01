-- Debug script - log emulated PC history leading up to the transition
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\pc_history.log", "w")
local frameCount = 0
local pcHistory = {}
local historySize = 50
local historyIndex = 1
local lastPcHi = 0xFF

logFile:write("PC history watch started\n\n")

-- Watch writes to PC high byte to track PC changes
emu.addMemoryCallback(function(addr, value)
    if frameCount > 5 then
        local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
        local fullPC = pcLo + (value * 256)
        
        -- Record to circular buffer
        pcHistory[historyIndex] = {frame = frameCount, pc = fullPC, hi = value}
        historyIndex = (historyIndex % historySize) + 1
        
        -- Check for transition after startup
        if frameCount > 100 and value == 0x28 and lastPcHi ~= 0x28 then
            logFile:write(string.format("*** TRANSITION at Frame %d: PC_HI $%02X -> $28 ***\n\n", frameCount, lastPcHi))
            logFile:write("PC history (last 50 PC_HI writes):\n")
            
            -- Dump history
            for i = 1, historySize do
                local idx = ((historyIndex - 2 + i) % historySize) + 1
                local entry = pcHistory[idx]
                if entry then
                    logFile:write(string.format("  Frame %3d: PC=$%04X (HI=$%02X)\n", 
                        entry.frame, entry.pc, entry.hi))
                end
            end
            
            logFile:flush()
            emu.breakExecution()
        end
        
        lastPcHi = value
    end
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
