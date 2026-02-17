-- trace_reservation.lua
-- Traces reservation creation, block compilation, dispatch, and crash detection
-- for debugging the "1 reservation crashes first run" bug.
--
-- Logs to CSV file for analysis by analyze_trace.py

local LOG_PATH = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\reservation_trace.csv"
local logFile = io.open(LOG_PATH, "w")

-- Sentinel: write a marker to WRAM address $7FF0 to confirm script loaded
emu.write(0x7FF0, 0xAC, emu.memType.nesDebug)

-- ====================================================================
-- Key addresses from vicemap.map (CPU address space)
-- ====================================================================

-- Emulated CPU state (zero page)
local ADDR_PC         = 0x006A  -- uint16_t _pc
local ADDR_SP         = 0x006C  -- uint8_t  _sp
local ADDR_A          = 0x006D  -- uint8_t  _a
local ADDR_X          = 0x006E  -- uint8_t  _x
local ADDR_Y          = 0x006F  -- uint8_t  _y
local ADDR_STATUS     = 0x0070  -- uint8_t  _status
local ADDR_CACHE_IDX  = 0x0087  -- uint8_t  _cache_index
local ADDR_FASH_CIDX  = 0x0093  -- uint16_t _flash_cache_index (ZP)
local ADDR_CODE_IDX   = 0x00A5  -- uint8_t  _code_index

-- WRAM variables (data section at $6000+)
local ADDR_MAPPER_PRG = 0x60D0  -- uint8_t  _mapper_prg_bank
local ADDR_RES_COUNT  = 0x60D4  -- uint8_t  _reservation_count
local ADDR_RES_ENABLE = 0x60D5  -- uint8_t  _reservations_enabled
local ADDR_NEXT_FREE  = 0x61F1  -- uint16_t _next_free_block

-- Reservation arrays (WRAM)
local ADDR_RESV_PC    = 0x6403  -- uint16_t _reserved_pc[32]
local ADDR_RESV_BLK   = 0x6443  -- uint16_t _reserved_block[32]

-- Reserve result (WRAM)
local ADDR_RES_ADDR   = 0x6483  -- uint16_t _reserve_result_addr
local ADDR_RES_BANK   = 0x6485  -- uint8_t  _reserve_result_bank

-- Flash state (WRAM)
local ADDR_FLASH_ADDR = 0x6CFC  -- uint16_t _flash_code_address
local ADDR_FLASH_BANK = 0x6CFE  -- uint8_t  _flash_code_bank
local ADDR_SA_BLOCKS  = 0x63A1  -- uint16_t _sa_blocks_total

-- Code entry points (for exec callbacks)
local FUNC_SA_RUN           = 0xE7AF  -- _sa_run (fixed bank)
local FUNC_RUN_6502         = 0xCACA  -- _run_6502 (fixed bank)
local FUNC_DISPATCH         = 0x6205  -- _dispatch_on_pc (WRAM)
local FUNC_FLASH_RET        = 0x6274  -- _flash_dispatch_return (WRAM)
local FUNC_CROSS_BANK       = 0x61F9  -- _cross_bank_dispatch (WRAM)
local FUNC_RESERVE_SAFE     = 0xD24D  -- _reserve_block_for_pc_safe (fixed)
local FUNC_RESERVE          = 0xD022  -- _reserve_block_for_pc (fixed)
local FUNC_CONSUME          = 0xD187  -- _consume_reservation (fixed)
local FUNC_RECOMPILE        = 0xD4AF  -- _recompile_opcode (trampoline, fixed)
local FUNC_MAIN             = 0xC472  -- _main (fixed)

-- ====================================================================
-- Helpers
-- ====================================================================

local function readByte(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

local function readWord(addr)
    local lo = emu.read(addr, emu.memType.nesDebug)
    local hi = emu.read(addr + 1, emu.memType.nesDebug)
    return lo + (hi * 256)
end

local function log(msg)
    if logFile then
        logFile:write(msg .. "\n")
    end
end

local function flush()
    if logFile then
        logFile:flush()
    end
end

-- ====================================================================
-- State
-- ====================================================================

local frameCount = 0
local totalEvents = 0
local phase = "BOOT"        -- BOOT, SA_RUN, MAIN, DONE
local sa_run_entered = false
local main_loop_entered = false
local dispatch_count = 0
local reserve_call_count = 0
local consume_call_count = 0
local recompile_call_count = 0
local MAX_EVENTS = 50000    -- safety limit
local MAX_DISPATCHES = 200  -- capture first N dispatches after sa_run

-- Write CSV header
log("event,frame,nativePC,phase,emPC,emA,emX,emY,emSP,emStatus,mapperBank,resCount,resEnabled,nextFree,flashAddr,flashBank,flashCacheIdx,codeIdx,extra")

-- ====================================================================
-- Snapshot: capture full state as CSV row
-- ====================================================================

local function snapshot(event, extra, nativePC)
    totalEvents = totalEvents + 1
    if totalEvents > MAX_EVENTS then return end

    nativePC = nativePC or 0

    local emPC = readWord(ADDR_PC)
    local emA  = readByte(ADDR_A)
    local emX  = readByte(ADDR_X)
    local emY  = readByte(ADDR_Y)
    local emSP = readByte(ADDR_SP)
    local emSt = readByte(ADDR_STATUS)
    local mapBank = readByte(ADDR_MAPPER_PRG)
    local resCount = readByte(ADDR_RES_COUNT)
    local resEnable = readByte(ADDR_RES_ENABLE)
    local nextFree = readWord(ADDR_NEXT_FREE)
    local flashAddr = readWord(ADDR_FLASH_ADDR)
    local flashBank = readByte(ADDR_FLASH_BANK)
    local fci = readWord(ADDR_FASH_CIDX)
    local codeIdx = readByte(ADDR_CODE_IDX)

    local extraStr = extra or ""
    log(string.format("%s,%d,%04X,%s,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%d,%d,%d,%04X,%02X,%d,%d,%s",
        event, frameCount, nativePC, phase,
        emPC, emA, emX, emY, emSP, emSt,
        mapBank, resCount, resEnable, nextFree,
        flashAddr, flashBank, fci, codeIdx, extraStr))
end

-- ====================================================================
-- Callbacks on function entry (exec at function address)
-- ====================================================================

-- sa_run entry
emu.addMemoryCallback(function()
    phase = "SA_RUN"
    sa_run_entered = true
    snapshot("SA_RUN_ENTER", nil, FUNC_SA_RUN)
    flush()
end, emu.callbackType.exec, FUNC_SA_RUN)

-- reserve_block_for_pc_safe entry
emu.addMemoryCallback(function()
    reserve_call_count = reserve_call_count + 1
    -- Read A/X for target_pc argument (passed in r0/r1 = ZP $02/$03)
    local targetLo = readByte(0x02)
    local targetHi = readByte(0x03)
    local targetPC = targetLo + (targetHi * 256)
    snapshot("RESERVE_SAFE_ENTER", string.format("target=%04X call#%d", targetPC, reserve_call_count), FUNC_RESERVE_SAFE)
end, emu.callbackType.exec, FUNC_RESERVE_SAFE)

-- consume_reservation entry
emu.addMemoryCallback(function()
    consume_call_count = consume_call_count + 1
    local targetLo = readByte(0x02)
    local targetHi = readByte(0x03)
    local targetPC = targetLo + (targetHi * 256)
    snapshot("CONSUME_ENTER", string.format("target=%04X call#%d", targetPC, consume_call_count), FUNC_CONSUME)
end, emu.callbackType.exec, FUNC_CONSUME)

-- recompile_opcode entry (trampoline)
emu.addMemoryCallback(function()
    recompile_call_count = recompile_call_count + 1
    -- Only log periodically during SA or first few during main
    if phase == "SA_RUN" then
        -- Log every Nth during SA to keep volume manageable
        if recompile_call_count % 50 == 1 or recompile_call_count <= 5 then
            snapshot("RECOMPILE", string.format("call#%d", recompile_call_count), FUNC_RECOMPILE)
        end
    elseif phase == "MAIN" then
        if recompile_call_count <= 10 then
            snapshot("RECOMPILE_MAIN", string.format("call#%d", recompile_call_count), FUNC_RECOMPILE)
        end
    end
end, emu.callbackType.exec, FUNC_RECOMPILE)

-- dispatch_on_pc entry
emu.addMemoryCallback(function(addr)
    dispatch_count = dispatch_count + 1
    if phase == "SA_RUN" then
        -- Shouldn't happen during SA - log it!
        snapshot("DISPATCH_IN_SA", string.format("disp#%d nPC=%04X", dispatch_count, addr), addr)
    elseif phase == "MAIN" then
        if dispatch_count <= MAX_DISPATCHES then
            snapshot("DISPATCH", string.format("disp#%d nPC=%04X", dispatch_count, addr), addr)
        end
    end
end, emu.callbackType.exec, FUNC_DISPATCH)

-- flash_dispatch_return
emu.addMemoryCallback(function(addr)
    if phase == "MAIN" and dispatch_count <= MAX_DISPATCHES then
        snapshot("FLASH_RET", string.format("disp#%d nPC=%04X", dispatch_count, addr), addr)
    end
end, emu.callbackType.exec, FUNC_FLASH_RET)

-- cross_bank_dispatch
emu.addMemoryCallback(function(addr)
    if phase == "MAIN" and dispatch_count <= MAX_DISPATCHES then
        snapshot("CROSS_BANK", string.format("disp#%d nPC=%04X", dispatch_count, addr), addr)
    end
end, emu.callbackType.exec, FUNC_CROSS_BANK)

-- run_6502 entry
emu.addMemoryCallback(function()
    if not main_loop_entered then
        main_loop_entered = true
        phase = "MAIN"
        snapshot("MAIN_LOOP_ENTER", nil, FUNC_RUN_6502)
        flush()

        -- Dump reservation state at main loop entry
        local resCount = readByte(ADDR_RES_COUNT)
        if resCount > 0 then
            for i = 0, resCount - 1 do
                local rpc = readWord(ADDR_RESV_PC + i * 2)
                local rblk = readWord(ADDR_RESV_BLK + i * 2)
                snapshot("LEFTOVER_RESERVATION", string.format("idx=%d pc=%04X blk=%d", i, rpc, rblk), FUNC_RUN_6502)
            end
        end
    end
end, emu.callbackType.exec, FUNC_RUN_6502)

-- ====================================================================
-- Write watcher: detect writes to reservation_count
-- ====================================================================

emu.addMemoryCallback(function(addr, value)
    snapshot("RES_COUNT_WRITE", string.format("newval=%d", value), 0)
end, emu.callbackType.write, ADDR_RES_COUNT)

-- ====================================================================
-- Write watcher: reserved_pc[0] writes (first reservation slot)
-- ====================================================================

emu.addMemoryCallback(function(addr, value)
    -- Only care about lo byte write
    snapshot("RESV_PC0_WRITE", string.format("addr=%04X val=%02X", addr, value), 0)
end, emu.callbackType.write, ADDR_RESV_PC, ADDR_RESV_PC + 1)

-- ====================================================================
-- Frame counter + periodic state dump + crash detection
-- ====================================================================

emu.addEventCallback(function()
    frameCount = frameCount + 1

    -- Periodic state dump during SA_RUN
    if phase == "SA_RUN" and frameCount % 10 == 0 then
        snapshot("SA_PERIODIC", string.format("blocks=%d", readWord(ADDR_SA_BLOCKS)), 0)
        flush()
    end

    -- Periodic state dump during MAIN
    if phase == "MAIN" and frameCount <= 60 then
        snapshot("MAIN_PERIODIC", string.format("dispatches=%d frame=%d", dispatch_count, frameCount), 0)
        flush()
    end

    -- Stop after enough data collected
    if phase == "MAIN" and frameCount > 120 then
        snapshot("TRACE_DONE", string.format("total_dispatches=%d total_reserves=%d total_consumes=%d",
            dispatch_count, reserve_call_count, consume_call_count), 0)
        flush()
        if logFile then logFile:close() end
        logFile = nil
        emu.displayMessage("Trace", "Reservation trace complete")
        emu.breakExecution()
    end

    if totalEvents >= MAX_EVENTS then
        snapshot("EVENT_LIMIT_HIT", nil, 0)
        flush()
        if logFile then logFile:close() end
        logFile = nil
        emu.breakExecution()
    end
end, emu.eventType.endFrame)

-- ====================================================================
-- Crash detection: watch for execution in unexpected regions
-- During MAIN phase, code should only execute from:
--   $6000-$6FFF (WRAM helpers: dispatch, peek_bank_byte, flash_byte_program)
--   $8000-$BFFF (compiled code blocks)
--   $C000-$FFFF (fixed bank: C runtime, recompiler, etc.)
-- Execution at $0000-$5FFF or $7000-$7FFF is suspicious
-- ====================================================================

-- Watch for execution in zero page (common crash pattern: JMP ($0000))
emu.addMemoryCallback(function(addr, value)
    if phase == "MAIN" then
        snapshot("CRASH_ZP_EXEC", string.format("nativePC=%04X opcode=%02X", addr, value), addr)
        flush()
    end
end, emu.callbackType.exec, 0x0000, 0x00FF)

-- Watch for execution in $0100-$01FF (stack area - definitely a crash)
emu.addMemoryCallback(function(addr, value)
    if phase == "MAIN" then
        snapshot("CRASH_STACK_EXEC", string.format("nativePC=%04X opcode=%02X", addr, value), addr)
        flush()
    end
end, emu.callbackType.exec, 0x0100, 0x01FF)

-- Watch for BRK execution (opcode $00) - use a different approach
-- We watch writes to the IRQ vector area which BRK triggers
-- Actually, let's watch for specific crash signatures after dispatch

-- ====================================================================
-- Boot complete message
-- ====================================================================

emu.displayMessage("Trace", "Reservation trace ACTIVE - logging to CSV")
log("# Trace script loaded at " .. os.date())
flush()

-- Force a reset so sa_run is captured from the beginning.
-- emu.reset() can only be called from inside a callback, so we
-- defer it to fire on the very first exec callback we can catch.
local needReset = true
local resetRef
resetRef = emu.addEventCallback(function()
    if needReset then
        needReset = false
        emu.removeEventCallback(resetRef, emu.eventType.startFrame)
        -- Reset all tracking state so we start fresh
        frameCount = 0
        totalEvents = 0
        phase = "BOOT"
        sa_run_entered = false
        main_loop_entered = false
        dispatch_count = 0
        reserve_call_count = 0
        consume_call_count = 0
        recompile_call_count = 0
        log("# Triggering emu.reset() to capture sa_run from boot")
        flush()
        emu.reset()
    end
end, emu.eventType.startFrame)
