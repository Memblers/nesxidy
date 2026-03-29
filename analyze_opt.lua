-- analyze_opt.lua - Read optimizer state directly from memory
-- Mesen 2 API doesn't reliably fire callbacks, so we sample state manually

local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\opt_analysis.log", "w")

local function log(msg)
    if logFile then
        logFile:write(msg .. "\n")
        logFile:flush()
    end
end

local function hexWord(val)
    return string.format("$%04X", val)
end

local function hexByte(val)
    return string.format("$%02X", val)
end

local function readWord(addr)
    local lo = emu.read(addr, emu.memType.nesMemory)
    local hi = emu.read(addr + 1, emu.memType.nesMemory)
    return lo + (hi * 256)
end

-- From vicemap.map:
-- opt_state is in fixed bank at some address
-- Let me search for it by pattern

local ADDR_PC = 0x02           -- Emulated PC
local ADDR_CACHE_INDEX = 0x14  -- cache_index
local ADDR_CODE_INDEX = 0x12   -- code_index
local ADDR_OPT_DEBUG_BLOCK = 0x621E   -- From vicemap.map
local ADDR_OPT_DEBUG_DISPATCH = 0x621F -- From vicemap.map
local ADDR_OPT_DEBUG_TRIGGER = 0x6220  -- From vicemap.map

log("=== Optimizer State Analysis ===")
log("Timestamp: " .. os.date())

local frameCount = 0
local lastPC = 0
local lastUniqueBlocks = 0
local sampleCount = 0

emu.addEventCallback(function()
    frameCount = frameCount + 1
    
    -- Sample every 30 frames
    if frameCount % 30 == 0 then
        sampleCount = sampleCount + 1
        local pc = readWord(ADDR_PC)
        local cacheIdx = emu.read(ADDR_CACHE_INDEX, emu.memType.nesMemory)
        local codeIdx = readWord(ADDR_CODE_INDEX)
        
        -- Debug variables in fixed RAM
        local optDebugBlockCount = emu.read(ADDR_OPT_DEBUG_BLOCK, emu.memType.nesMemory)
        local optDebugDispatchCount = emu.read(ADDR_OPT_DEBUG_DISPATCH, emu.memType.nesMemory)
        local optDebugTriggerFired = emu.read(ADDR_OPT_DEBUG_TRIGGER, emu.memType.nesMemory)
        
        local msg = string.format("SAMPLE %3d (frame %4d): emuPC=%s cacheIdx=%s codeIdx=%s blockCnt=%3d dispCnt=%3d trigger=%s",
            sampleCount, frameCount, hexWord(pc), hexByte(cacheIdx), hexWord(codeIdx), 
            optDebugBlockCount, optDebugDispatchCount, hexByte(optDebugTriggerFired))
        
        log(msg)
    end
    
end, emu.eventType.endFrame)

print("Optimizer analysis script loaded - checking memory each frame")
