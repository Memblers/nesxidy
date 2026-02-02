-- debug_optimizer.lua - Comprehensive optimizer tracing for Mesen
-- Logs state before/after optimizer, PC table accesses, flash writes

local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\opt_debug.log", "w")
local logging = false
local logCount = 0
local maxLogs = 50000

-- Key addresses from vicemap.map
local ADDR_PC = 0x02           -- Emulated PC (2 bytes, zero page)
local ADDR_CACHE_INDEX = 0x14  -- cache_index
local ADDR_CODE_INDEX = 0x12   -- code_index (2 bytes)
local ADDR_OPT_STATE = nil     -- Will find from map

-- Debug port
local DEBUG_PORT = 0x4020

-- Flash banks
local BANK_CODE = 4
local BANK_PC = 19
local BANK_PC_FLAGS = 27

-- State tracking
local lastDebugValue = 0
local optimizerRunning = false
local stateBeforeOpt = {}
local writeLog = {}

local function log(msg)
    if logFile and logCount < maxLogs then
        logFile:write(msg .. "\n")
        logCount = logCount + 1
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

local function captureState(label)
    local pc = readWord(ADDR_PC)
    local cacheIdx = emu.read(ADDR_CACHE_INDEX, emu.memType.nesMemory)
    local codeIdx = readWord(ADDR_CODE_INDEX)
    local state = emu.getState()
    local cpuPC = 0
    local cpuA = 0
    local cpuX = 0
    local cpuY = 0
    local cpuS = 0
    
    -- Mesen 2 uses string indexing for CPU state
    if state then
        cpuPC = state["cpu.pc"] or 0
        cpuA = state["cpu.a"] or 0
        cpuX = state["cpu.x"] or 0
        cpuY = state["cpu.y"] or 0
        cpuS = state["cpu.sp"] or 0
    end
    
    return {
        label = label,
        emulatedPC = pc,
        cacheIndex = cacheIdx,
        codeIndex = codeIdx,
        cpuPC = cpuPC,
        cpuA = cpuA,
        cpuX = cpuX,
        cpuY = cpuY,
        cpuS = cpuS
    }
end

local function logState(state)
    log(string.format("%s: emuPC=%s cacheIdx=%s codeIdx=%s CPU: PC=%s A=%s X=%s Y=%s S=%s",
        state.label,
        hexWord(state.emulatedPC),
        hexByte(state.cacheIndex),
        hexWord(state.codeIndex),
        hexWord(state.cpuPC),
        hexByte(state.cpuA),
        hexByte(state.cpuX),
        hexByte(state.cpuY),
        hexByte(state.cpuS)
    ))
end

-- Helper to get CPU PC
local function getCpuPC()
    local state = emu.getState()
    if state then
        return state["cpu.pc"] or 0
    end
    return 0
end

-- Watch debug port writes
local function onDebugWrite(addr, value)
    if value ~= lastDebugValue then
        log(string.format("DEBUG_PORT: %s (was %s) at CPU PC=%s", 
            hexByte(value), hexByte(lastDebugValue), hexWord(getCpuPC())))
        lastDebugValue = value
        
        -- Track optimizer lifecycle
        if value == 0xAA then
            log("=== OPTIMIZER STARTING ===")
            optimizerRunning = true
            stateBeforeOpt = captureState("BEFORE_OPT")
            logState(stateBeforeOpt)
        elseif value == 0xBB then
            log("=== OPTIMIZER ENDING ===")
            local stateAfterOpt = captureState("AFTER_OPT")
            logState(stateAfterOpt)
            
            -- Compare
            if stateBeforeOpt.emulatedPC ~= stateAfterOpt.emulatedPC then
                log(string.format("!!! PC CHANGED: %s -> %s", 
                    hexWord(stateBeforeOpt.emulatedPC), hexWord(stateAfterOpt.emulatedPC)))
            end
            if stateBeforeOpt.cacheIndex ~= stateAfterOpt.cacheIndex then
                log(string.format("!!! cacheIndex CHANGED: %s -> %s",
                    hexByte(stateBeforeOpt.cacheIndex), hexByte(stateAfterOpt.cacheIndex)))
            end
            if stateBeforeOpt.codeIndex ~= stateAfterOpt.codeIndex then
                log(string.format("!!! codeIndex CHANGED: %s -> %s",
                    hexWord(stateBeforeOpt.codeIndex), hexWord(stateAfterOpt.codeIndex)))
            end
            
            optimizerRunning = false
        elseif value == 0xFF then
            log("=== OPTIMIZER TRIGGER (opt_check_trigger) ===")
            logState(captureState("TRIGGER"))
        elseif value == 0xD0 then
            log("--- opt_do_recompile entered ---")
        elseif value == 0xDF then
            log("--- opt_do_recompile exiting ---")
        elseif value >= 0x10 and value <= 0x1F then
            log(string.format("--- evacuating sector %d ---", value - 0x10))
        elseif value == 0x50 then
            log("--- recompilation phase done ---")
        elseif value == 0x60 then
            log("--- PC updates done ---")
        elseif value == 0xEE then
            log("!!! PC entry update SKIPPED (not $FFFF) !!!")
        elseif value == 0xEF then
            log("!!! Flag entry update SKIPPED !!!")
        end
    end
end

-- Track PRG bank switches
local currentBank = 0
local function onBankSwitch(addr, value)
    if addr == 0xC000 then
        local newBank = value & 0x1F
        if newBank ~= currentBank then
            if optimizerRunning then
                log(string.format("BANK SWITCH: %d -> %d at PC=%s", 
                    currentBank, newBank, hexWord(getCpuPC())))
            end
            currentBank = newBank
        end
    end
end

-- Track flash writes during optimization
local flashWriteCount = 0
local function onFlashWrite(addr, value)
    if optimizerRunning and flashWriteCount < 1000 then
        log(string.format("FLASH WRITE: [%s] bank=%d <- %s at PC=%s",
            hexWord(addr), currentBank, hexByte(value), hexWord(getCpuPC())))
        flashWriteCount = flashWriteCount + 1
    end
end

-- Track execution in specific ranges
local function onExec(addr)
    if optimizerRunning then
        -- Log key function entries
        -- These addresses would need to be updated from vicemap.map
    end
end

-- Periodic state dump
local frameCount = 0
local function onFrame()
    frameCount = frameCount + 1
    if frameCount % 60 == 0 then  -- Every ~1 second
        logState(captureState(string.format("FRAME_%d", frameCount)))
    end
end

-- Register callbacks
emu.addMemoryCallback(onDebugWrite, emu.callbackType.cpuWrite, DEBUG_PORT, DEBUG_PORT)
emu.addMemoryCallback(onBankSwitch, emu.callbackType.cpuWrite, 0xC000, 0xC000)

-- Watch for writes to flash program sequence (unlock writes)
-- $5555 and $2AAA are the flash unlock addresses
emu.addMemoryCallback(function(addr, value)
    if optimizerRunning then
        log(string.format("FLASH UNLOCK: [%s] <- %s", hexWord(addr), hexByte(value)))
    end
end, emu.callbackType.cpuWrite, 0x9555, 0x9555)

emu.addMemoryCallback(function(addr, value)
    if optimizerRunning then
        log(string.format("FLASH UNLOCK: [%s] <- %s", hexWord(addr), hexByte(value)))
    end
end, emu.callbackType.cpuWrite, 0xAAAA, 0xAAAA)

emu.addEventCallback(onFrame, emu.eventType.endFrame)

log("=== Optimizer Debug Script Started ===")
log(string.format("Timestamp: %s", os.date()))
logState(captureState("INITIAL"))

-- Cleanup on script end
emu.addEventCallback(function()
    log("=== Script ending, closing log ===")
    if logFile then
        logFile:close()
    end
end, emu.eventType.scriptEnded)

print("Optimizer debug script loaded - logging to opt_debug.log")
