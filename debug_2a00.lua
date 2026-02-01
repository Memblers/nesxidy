-- Debug the $2A00 instruction handling

local PC_LO = 0x0058
local PC_HI = 0x0059

local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\debug_2a00.log", "w")
logFile:write("Debugging $2A00 instruction\n\n")
logFile:flush()

local frameCount = 0
local callCount = 0
local lastFrame = -1

-- Watch when PC is set TO $2A00 (or $2A01, etc)
emu.addMemoryCallback(function(addr, value, prevValue)
    local frame = emu.getState().ppu.frameCount
    local otherByte = emu.read(PC_LO, emu.memType.nesDebug)
    local fullPc = value * 256 + otherByte
    
    if value == 0x2A then
        callCount = callCount + 1
        if callCount <= 100 then
            logFile:write(string.format("Frame %d: PC_HI set to $%02X, full PC = $%04X\n", 
                frame, value, fullPc))
            logFile:flush()
        end
    end
    
    -- Detect the bad transition
    if prevValue == 0x2A and value == 0x28 and frame > 20 then
        logFile:write(string.format("\n*** BAD TRANSITION at Frame %d: $2A -> $28 ***\n", frame))
        
        -- Dump CPU state
        logFile:write("\nCPU State:\n")
        logFile:write(string.format("  PC: $%04X\n", emu.read(PC_LO, emu.memType.nesDebug) + emu.read(PC_HI, emu.memType.nesDebug) * 256))
        logFile:write(string.format("  A:  $%02X\n", emu.read(0x5B, emu.memType.nesDebug)))
        logFile:write(string.format("  X:  $%02X\n", emu.read(0x5C, emu.memType.nesDebug)))
        logFile:write(string.format("  Y:  $%02X\n", emu.read(0x5D, emu.memType.nesDebug)))
        logFile:write(string.format("  SP: $%02X\n", emu.read(0x5A, emu.memType.nesDebug)))
        logFile:write(string.format("  Status: $%02X\n", emu.read(0x5E, emu.memType.nesDebug)))
        
        -- Read the emulated RAM at $00-$01 (where the INDX pointer is)
        logFile:write("\nEmulated ZP pointer at $00-$01:\n")
        local ram_base = 0x6C00
        logFile:write(string.format("  $6C00: $%02X\n", emu.read(ram_base + 0, emu.memType.nesDebug)))
        logFile:write(string.format("  $6C01: $%02X\n", emu.read(ram_base + 1, emu.memType.nesDebug)))
        
        logFile:flush()
    end
end, emu.memCallbackType.cpuWrite, PC_HI, PC_HI)

logFile:write("Watching PC_HI writes...\n\n")
logFile:flush()
emu.log("Debug $2A00 started")
