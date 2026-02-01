-- Debug script - catch the EXACT moment PC_HI goes from $2A to $28
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\transition.log", "w")
local frameCount = 0
local lastPcHi = 0xFF  -- Unknown

logFile:write("Transition watch started\n")
logFile:write("Watching for PC_HI transition from $2A to $28\n\n")

-- Watch writes to PC high byte
emu.addMemoryCallback(function(addr, value)
    if frameCount > 5 then
        if value == 0x28 and lastPcHi == 0x2A then
            local pcLo = emu.read(0x0058, emu.memType.cpuMemory)
            local fullPC = pcLo + (value * 256)
            
            logFile:write(string.format("\n*** CAUGHT TRANSITION at Frame %d ***\n", frameCount))
            logFile:write(string.format("PC_HI: $2A -> $28, Full PC = $%04X\n", fullPC))
            
            -- Get NES CPU registers (the JIT's host CPU)
            local state = emu.getState()
            logFile:write(string.format("\nNES CPU state:\n"))
            logFile:write(string.format("  NES PC: $%04X\n", state.cpu.pc))
            logFile:write(string.format("  NES A: $%02X  X: $%02X  Y: $%02X\n", 
                state.cpu.a, state.cpu.x, state.cpu.y))
            logFile:write(string.format("  NES SP: $%02X  Status: $%02X\n", 
                state.cpu.sp, state.cpu.ps))
            
            -- Dump the area around NES PC to see what instruction just executed
            logFile:write(string.format("\nCode around NES PC:\n"))
            for i = -6, 6 do
                local addr = state.cpu.pc + i
                if addr >= 0 and addr <= 0xFFFF then
                    local byte = emu.read(addr, emu.memType.cpuMemory)
                    local marker = ""
                    if i == 0 then marker = " <-- PC" end
                    logFile:write(string.format("  $%04X: $%02X%s\n", addr, byte, marker))
                end
            end
            
            -- Dump emulated CPU state
            logFile:write(string.format("\nEmulated CPU state:\n"))
            logFile:write(string.format("  SP: $%02X\n", emu.read(0x005A, emu.memType.cpuMemory)))
            logFile:write(string.format("  A: $%02X\n", emu.read(0x005B, emu.memType.cpuMemory)))
            logFile:write(string.format("  X: $%02X\n", emu.read(0x005C, emu.memType.cpuMemory)))
            logFile:write(string.format("  Y: $%02X\n", emu.read(0x005D, emu.memType.cpuMemory)))
            logFile:write(string.format("  Status: $%02X\n", emu.read(0x005E, emu.memType.cpuMemory)))
            
            logFile:flush()
            emu.breakExecution()
        end
        
        -- Track non-startup $28 -> other transitions
        if lastPcHi == 0x28 and value ~= 0x28 and frameCount > 20 then
            logFile:write(string.format("Frame %d: PC_HI $28 -> $%02X (leaving reset)\n", frameCount, value))
        end
    end
    
    lastPcHi = value
end, emu.callbackType.write, 0x0059, 0x0059)

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount % 50 == 0 then
        logFile:write(string.format("Frame %d...\n", frameCount))
        logFile:flush()
    end
    if frameCount > 300 then
        logFile:write("=== 300 frames reached ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching...\n")
logFile:flush()
