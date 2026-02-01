-- Break when SP ($005A) is written with $FF
-- Shows the NES CPU's PC (not emulated Exidy PC)
-- Revision 6: Dump flash_cache_pc lookup for Exidy PC (no bit32)

local EXIDY_SP_ADDR = 0x005A

local logPath = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\sp_break.log"
local logFile = io.open(logPath, "w")
if logFile then
    logFile:write("Script starting - Revision 6\n")
    logFile:flush()
else
    emu.log("ERROR: Could not open log file: " .. logPath)
end

local frameCount = 0
local lastSP = 0
local breakCount = 0
local writeCount = 0

local BANK_PC = 0x13  -- from dynamos-asm.s (19 decimal)
local BANK_PC_FLAGS = 0x1B  -- from dynamos-asm.s (27 decimal)

-- Helper to get NES CPU PC from state
local function getNesPc()
    local state = emu.getState()
    if not state then return 0 end
    return state["cpu.pc"] or 0
end

local function readByte(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

emu.addMemoryCallback(function(addr, value)
    writeCount = writeCount + 1
    
    local nesPc = getNesPc()
    local oldSP = lastSP
    lastSP = value
    
    -- Detect F7 -> FF transition
    if oldSP == 0xF7 and value == 0xFF then
        breakCount = breakCount + 1
        
        local msg = string.format("*** F7->FF #%d frame=%d NES_PC=$%04X\n", breakCount, frameCount, nesPc)
        logFile:write(msg)
        
        -- Dump Exidy PC
        local exidyPcLo = readByte(0x58)
        local exidyPcHi = readByte(0x59)
        local exidyPc = exidyPcLo + exidyPcHi * 256
        logFile:write(string.format("Exidy PC: $%04X\n", exidyPc))
        
        -- Read current bank from mapper state
        local state = emu.getState()
        local prgBank = state["mapper.prgBank"] or -1
        logFile:write(string.format("Current PRG bank: $%02X\n", prgBank))
        
        -- Calculate lookup addresses
        local pcShifted = exidyPc * 2
        local pcLookupOffset = pcShifted % 0x4000
        local pcLookupAddr = pcLookupOffset + 0x8000
        local pcLookupBank = (math.floor(exidyPc / 8192) % 8) + BANK_PC
        
        -- Calculate flag lookup
        local pcFlagOffset = math.floor(exidyPc / 64) % 0x4000
        local pcFlagAddr = pcFlagOffset + 0x8000
        local pcFlagBank = (math.floor(exidyPc / 16384) % 4) + BANK_PC_FLAGS
        
        logFile:write(string.format("PC lookup: bank=$%02X addr=$%04X\n", pcLookupBank, pcLookupAddr))
        logFile:write(string.format("Flag lookup: bank=$%02X addr=$%04X\n", pcFlagBank, pcFlagAddr))
        
        -- Dump flash cache lookup values from PRG ROM
        -- Bank 0x11 ($30AF lookup) - we need to use prgRom memory type
        -- $30AF << 1 = $615E, bank = $11-$10 = 1, so PRG ROM offset = bank*16KB + offset
        local prgOffset = ((pcLookupBank - BANK_PC) * 0x4000) + pcLookupOffset
        logFile:write(string.format("PRG ROM offset for code lookup: $%05X\n", prgOffset))
        
        -- Read 4 bytes at lookup location using prgRom
        logFile:write("Code address stored at lookup: ")
        local addrLo = emu.read(prgOffset, emu.memType.prgRom)
        local addrHi = emu.read(prgOffset + 1, emu.memType.prgRom)
        logFile:write(string.format("$%02X%02X\n", addrHi, addrLo))
        
        -- Read flag byte from the correct flash bank in PRG ROM
        -- The flag is at bank $1B (27), offset = low byte of (exidyPc >> 6) within the bank
        local realFlagBank = 0x1B  -- BANK_PC_FLAGS = 27
        local realFlagOffset = exidyPc % 256  -- Low byte of PC is used as offset
        local realFlagPrgOffset = (realFlagBank * 0x4000) + realFlagOffset
        logFile:write(string.format("Correct PRG ROM offset for flag: $%05X (bank $%02X + $%02X)\n", 
            realFlagPrgOffset, realFlagBank, realFlagOffset))
        
        -- Read from PRG ROM (original ROM file) at the CORRECT offset
        local flagFromCorrectBank = emu.read(realFlagPrgOffset, emu.memType.prgRom)
        logFile:write(string.format("Flag from original PRG ROM: $%02X\n", flagFromCorrectBank))
        
        -- Try reading from the LIVE flash state using debugRead with the bank address
        -- We need to read what the CPU would see when bank 27 is selected
        -- Write to $C000 to select the bank, then read from $8000+offset
        local savedBank = state["mapper.prgBank"] or 0
        
        -- Can't actually switch banks from Lua, so let's try a different approach
        -- Use prgFlash if available, or try other memory types
        local memTypes = {"prgRom", "nesDebug", "cpuMemory"}
        logFile:write("Trying different memory types to find live flash:\n")
        for _, mt in ipairs(memTypes) do
            local mtEnum = emu.memType[mt]
            if mtEnum then
                local val = emu.read(realFlagPrgOffset, mtEnum)
                logFile:write(string.format("  %s at offset $%05X: $%02X\n", mt, realFlagPrgOffset, val))
            end
        end
        
        -- Also verify the dispatch_on_pc code in wram has the BEQ instruction
        logFile:write("Verifying dispatcher code in wram:\n")
        local dispatchAddr = 0x60E4  -- from vicemap.map
        local beqOffset = 32  -- BEQ is 32 bytes into the function
        logFile:write(string.format("  dispatch_on_pc at $%04X\n", dispatchAddr))
        logFile:write("  Bytes at dispatch_on_pc+28 to +36:\n    ")
        for i = 28, 36 do
            local b = readByte(dispatchAddr + i)
            if i == 32 then
                logFile:write(string.format("[%02X] ", b))
            else
                logFile:write(string.format("%02X ", b))
            end
        end
        logFile:write("\n")
        local beqByte = readByte(dispatchAddr + beqOffset)
        local bmiOffset = beqOffset + 2
        local bmiByte = readByte(dispatchAddr + bmiOffset)
        logFile:write(string.format("  Byte at +32: $%02X (expected F0 for BEQ)\n", beqByte))
        logFile:write(string.format("  Byte at +34: $%02X (expected 30 for BMI)\n", bmiByte))
        if beqByte ~= 0xF0 then
            logFile:write("  *** BEQ IS MISSING! This is the bug! ***\n")
        end
        
        -- Check zero page variables used by dispatcher
        local addrLo = readByte(0x96)
        local addrHi = readByte(0x97)
        local targetBank = readByte(0x9A)  -- from the map
        local temp = readByte(0x98)
        local temp2 = readByte(0x99)
        logFile:write(string.format("Dispatcher zpage vars: addr=$%02X%02X temp=$%02X temp2=$%02X target_bank=$%02X\n",
            addrHi, addrLo, temp, temp2, targetBank))
        
        -- Read the debug flag value stored by dispatcher at $6BFF
        local debugFlag = readByte(0x6BFF)
        logFile:write(string.format("DEBUG: Flag value stored by dispatcher at $6BFF: $%02X\n", debugFlag))
        if debugFlag == 0x00 then
            logFile:write("  Flag is $00 - BEQ should have branched!\n")
        elseif debugFlag >= 0x80 then
            logFile:write("  Flag has bit 7 set - BMI should have branched!\n")
        else
            logFile:write(string.format("  Flag $%02X - bit7 clear, non-zero - will execute as bank %d\n", 
                debugFlag, debugFlag % 32))
        end
        
        -- Read the flag value that would be read from (addr_lo),Y with Y=0
        local flagAtAddr = readByte(addrLo + addrHi * 256)
        logFile:write(string.format("Flag at ($%02X%02X),0 = $%02X\n", addrHi, addrLo, flagAtAddr))
        
        local flagByte = flagAtAddr
        logFile:write(string.format("Flag byte: $%02X (bit7=%d = %s)\n", flagByte, 
            (flagByte >= 128) and 1 or 0,
            (flagByte >= 128) and "NOT recompiled" or "recompiled"))
        
        -- Read code addresses around the lookup
        logFile:write("Code addresses at nearby PCs:\n")
        for offset = -4, 4, 2 do
            local lo = emu.read(prgOffset + offset, emu.memType.prgRom)
            local hi = emu.read(prgOffset + offset + 1, emu.memType.prgRom)
            local marker = (offset == 0) and " <--" or ""
            logFile:write(string.format("  offset %+d: $%02X%02X%s\n", offset, hi, lo, marker))
        end
        
        -- Dump code at $8000-$8040
        logFile:write("Code at $8000 (current bank):\n")
        local line = "  "
        for i = 0, 63 do
            local b = readByte(0x8000 + i)
            line = line .. string.format("%02X ", b)
            if (i + 1) % 16 == 0 then
                logFile:write(line .. "\n")
                line = "  "
            end
        end
        
        -- Dump NES stack ($1F0-$1FF) to see return addresses
        logFile:write("NES stack ($1F0-$1FF):\n  ")
        for i = 0x1F0, 0x1FF do
            logFile:write(string.format("%02X ", readByte(i)))
        end
        logFile:write("\n")
        
        logFile:flush()
    end
    
    -- Log every write where value is $FF
    if value == 0xFF then
        local msg = string.format("SP=$FF #%d frame=%d NES_PC=$%04X old=$%02X\n", breakCount, frameCount, nesPc, oldSP)
        logFile:write(msg)
        logFile:flush()
    end
end, emu.callbackType.write, EXIDY_SP_ADDR, EXIDY_SP_ADDR)

emu.addEventCallback(function()
    frameCount = frameCount + 1
end, emu.eventType.endFrame)

emu.log("SP break script loaded - Revision 6")
if logFile then
    logFile:write("SP break script loaded - Revision 6\n")
    logFile:flush()
end
if logFile then
    logFile:write("SP break script loaded - Revision 2\n")
    logFile:flush()
end
