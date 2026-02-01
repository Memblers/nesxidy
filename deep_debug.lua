-- Deep debug: capture NES state right before and during the transition

local PC_LO = 0x0058
local PC_HI = 0x0059
local SP = 0x005A
local A_REG = 0x005B
local X_REG = 0x005C
local Y_REG = 0x005D
local STATUS = 0x005E

local RAM_BASE = 0x6C00

local logFile = io.open("c:/proj/c/NES/nesxidy-co/nesxidy/deep_debug.log", "w")
logFile:write("Deep debug started\n\n")
logFile:flush()

local sampleCount = 0
local lastPcHi = 0

-- Take a snapshot every time PC_HI changes, but limit samples
emu.addMemoryCallback(function(addr, value, prevValue)
    sampleCount = sampleCount + 1
    
    if sampleCount < 5000 then return end  -- Skip early samples
    
    -- Only log when value changes or periodically
    if value ~= lastPcHi then
        lastPcHi = value
        
        local pcLo = emu.read(PC_LO, emu.memType.nesDebug)
        local fullPc = value * 256 + pcLo
        
        logFile:write(string.format("Sample %d: PC=$%04X\n", sampleCount, fullPc))
        
        -- If transition to $28xx
        if prevValue == 0x2A and value == 0x28 then
            logFile:write("\n*** TRANSITION DETECTED ***\n")
            logFile:write(string.format("  Previous PC_HI: $%02X\n", prevValue))
            logFile:write(string.format("  New PC_HI: $%02X\n", value))
            logFile:write(string.format("  Full PC: $%04X\n", fullPc))
            
            logFile:write("\nJIT CPU State:\n")
            logFile:write(string.format("  A:  $%02X\n", emu.read(A_REG, emu.memType.nesDebug)))
            logFile:write(string.format("  X:  $%02X\n", emu.read(X_REG, emu.memType.nesDebug)))
            logFile:write(string.format("  Y:  $%02X\n", emu.read(Y_REG, emu.memType.nesDebug)))
            logFile:write(string.format("  SP: $%02X\n", emu.read(SP, emu.memType.nesDebug)))
            logFile:write(string.format("  Status: $%02X\n", emu.read(STATUS, emu.memType.nesDebug)))
            
            -- Read emulated stack area
            logFile:write("\nEmulated Stack (top 16 bytes):\n")
            local sp = emu.read(SP, emu.memType.nesDebug)
            for i = 0, 15 do
                local stackAddr = RAM_BASE + 0x100 + ((sp + 1 + i) % 256)
                logFile:write(string.format("  $%04X: $%02X\n", stackAddr, emu.read(stackAddr, emu.memType.nesDebug)))
            end
            
            -- Read emulated ZP $00-$0F
            logFile:write("\nEmulated ZP $00-$0F:\n")
            for i = 0, 15 do
                logFile:write(string.format("  $%02X: $%02X\n", i, emu.read(RAM_BASE + i, emu.memType.nesDebug)))
            end
            
            logFile:flush()
        end
        
        if sampleCount >= 50000 then
            logFile:write("\nSample limit reached\n")
            logFile:flush()
        end
    end
end, emu.callbackType.write, PC_HI, PC_HI)

logFile:write("Waiting for frame 100+...\n\n")
logFile:flush()
emu.log("Deep debug started")
