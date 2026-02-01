-- Monitor BOTH PC bytes to understand the full picture

local PC_LO = 0x0058
local PC_HI = 0x0059

local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\both_pc.log", "w")
logFile:write("Monitoring both PC_LO and PC_HI writes\n\n")
logFile:flush()

local lastFrame = 0
local history = {}
local transitionDetected = false

-- Track the last few writes
local function addToHistory(addr, value, frame)
    local name = (addr == PC_LO) and "LO" or "HI"
    local pcLo = emu.read(PC_LO, emu.memType.nesDebug)
    local pcHi = emu.read(PC_HI, emu.memType.nesDebug)
    
    local entry = {
        frame = frame,
        addr = name,
        value = value,
        fullPc = pcHi * 256 + pcLo
    }
    table.insert(history, entry)
    if #history > 100 then
        table.remove(history, 1)
    end
end

-- Watch PC_LO writes
emu.addMemoryCallback(function(addr, value, prevValue)
    local frame = emu.getState().ppu.frameCount
    addToHistory(PC_LO, value, frame)
    
    -- Check if value is suspicious (like $28 low byte of vector)
    if value == 0x00 and not transitionDetected then
        local pcHi = emu.read(PC_HI, emu.memType.nesDebug)
        if pcHi >= 0x28 and pcHi <= 0x2F then
            logFile:write(string.format("Frame %d: PC_LO set to $00, full PC = $%04X\n", 
                frame, pcHi * 256 + value))
            logFile:flush()
        end
    end
end, emu.memCallbackType.cpuWrite, PC_LO, PC_LO)

-- Watch PC_HI writes  
emu.addMemoryCallback(function(addr, value, prevValue)
    local frame = emu.getState().ppu.frameCount
    addToHistory(PC_HI, value, frame)
    
    -- Detect transition from $2A to $28
    if prevValue == 0x2A and value == 0x28 and frame > 20 then
        transitionDetected = true
        logFile:write(string.format("\n*** TRANSITION at Frame %d: PC_HI $%02X -> $%02X ***\n\n", 
            frame, prevValue, value))
        
        -- Dump entire history with detail
        logFile:write("Full history (last 100 writes):\n")
        for i, entry in ipairs(history) do
            logFile:write(string.format("  %d: Frame %d, PC_%s <- $%02X, full PC = $%04X\n",
                i, entry.frame, entry.addr, entry.value, entry.fullPc))
        end
        logFile:write("\n")
        logFile:flush()
        
        -- Also capture memory around interpreter state
        logFile:write("Zero page CPU state ($50-$60):\n")
        for addr = 0x50, 0x60 do
            logFile:write(string.format("  $%02X: $%02X\n", addr, emu.read(addr, emu.memType.nesDebug)))
        end
        logFile:write("\n")
        
        -- Check RAM at emulated addresses
        logFile:write("RAM at $6C00-$6C10:\n")
        for addr = 0x6C00, 0x6C10 do
            logFile:write(string.format("  $%04X: $%02X\n", addr, emu.read(addr, emu.memType.nesDebug)))
        end
        logFile:flush()
    end
end, emu.memCallbackType.cpuWrite, PC_HI, PC_HI)

logFile:write("Watching PC_LO ($0058) and PC_HI ($0059) writes...\n\n")
logFile:flush()
emu.log("Watching both PC bytes...")
