-- Simple comprehensive PC trace script
-- Logs EVERY PC change and SP change to find reset pattern

local PC_LO_ADDR = 0x0058
local PC_HI_ADDR = 0x0059
local EXIDY_SP_ADDR = 0x005A

local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\pc_trace.csv", "w")
logFile:write("frame,type,value,pc,sp\n")

local frameCount = 0
local lastPC = 0
local lastSP = 0

local function readByte(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

local function getFullPC()
    local pcLo = readByte(PC_LO_ADDR)
    local pcHi = readByte(PC_HI_ADDR)
    return pcLo + (pcHi * 256)
end

-- Watch writes to PC high byte
emu.addMemoryCallback(function(addr, value)
    local fullPC = getFullPC()
    local sp = readByte(EXIDY_SP_ADDR)
    logFile:write(string.format("%d,PCH,%02X,%04X,%02X\n", frameCount, value, fullPC, sp))
end, emu.callbackType.write, PC_HI_ADDR, PC_HI_ADDR)

-- Watch writes to PC low byte
emu.addMemoryCallback(function(addr, value)
    local fullPC = getFullPC()
    local sp = readByte(EXIDY_SP_ADDR)
    logFile:write(string.format("%d,PCL,%02X,%04X,%02X\n", frameCount, value, fullPC, sp))
end, emu.callbackType.write, PC_LO_ADDR, PC_LO_ADDR)

-- Watch writes to SP - log ALL of them with full context
emu.addMemoryCallback(function(addr, value)
    local fullPC = getFullPC()
    local oldSP = lastSP
    lastSP = value
    -- Log every SP write with full context
    logFile:write(string.format("%d,SP=,%02X,%04X,%02X\n", frameCount, value, fullPC, oldSP))
end, emu.callbackType.write, EXIDY_SP_ADDR, EXIDY_SP_ADDR)

-- Count frames
emu.addEventCallback(function()
    frameCount = frameCount + 1
    -- Flush every 100 frames
    if frameCount % 100 == 0 then
        logFile:flush()
    end
    -- Stop after 1000 frames to catch the first reset
    if frameCount >= 1000 then
        logFile:close()
        emu.displayMessage("Trace", "PC trace complete - 1000 frames captured")
        emu.breakExecution()
    end
end, emu.eventType.endFrame)

emu.displayMessage("Trace", "PC trace started - will capture 1000 frames")
