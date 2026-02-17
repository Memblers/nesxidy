--============================================================================
-- trace_crash.lua v8 — COMPREHENSIVE circular-buffer crash catcher
--
-- v8 CHANGES:
--   - $91EE breakpoint now BANK-AWARE: only triggers on bank 0 (exec6502).
--     When bank 2 is mapped, $91EE is inside recompile_opcode_b2 — NOT a crash.
--   - $C000 write logging filtered to reduce flash-programming noise.
--   - False-positive $91EE hits logged as FP_91EE without stopping.
--
-- Strategy:
--   1. Keep last RING_SIZE events in a circular buffer (in memory).
--   2. Also stream ALL events to a file (up to MAX_LINES).
--   3. On crash detection, dump the ring buffer to a separate file.
--   4. Crash detection:
--      a. NES CPU executes $91EE (the known crash address)
--      b. Guest PC stuck on same value for STUCK_THRESHOLD frames
--      c. Script runs for MAX_FRAMES without crash (dump final state)
--   5. Monitor EVERYTHING:
--      - dispatch, JMP, return, NJSR, XBANK, XFAST
--      - exec6502, irq6502, nmi6502, step6502, interpret
--      - run_6502, PC update, sector alloc, sweep, scan
--      - NES hardware IRQ/NMI events (emu.eventType.irq/nmi)
--      - All writes to $C000 (mapper bank register)
--      - Frame heartbeats with full state
--
-- Sentinel: trace_signal.txt = SCRIPT_READY / TRACE_DONE reason
--============================================================================

local LOG_PATH    = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\full_trace.txt"
local RING_PATH   = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\crash_ring.txt"
local SIGNAL_PATH = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\trace_signal.txt"

---------- tunables ----------
local RING_SIZE       = 4000
local MAX_LINES       = 2000000
local MAX_FRAMES      = 200000
local STUCK_THRESHOLD = 2000

---------- addresses (from vicemap.map) ----------
local DISPATCH_ON_PC     = 0x620D
local DISPATCH_JMP       = 0x6279
local FLASH_DISP_RETURN  = 0x627C
local FLASH_DISP_RET_NR  = 0x6280
local NJSR_TRAMPOLINE    = 0x628F
local CROSS_BANK_DISP    = 0x61F8
local XBANK_TRAMPOLINE   = 0x6204
local PC_UPDATE          = 0xD45C
local RUN_6502           = 0xCACF
local SWEEP_PENDING      = 0xDE81
local SCAN_EPILOGUES     = 0xDE9A
local FLASH_SECTOR_ALLOC = 0xD21C
local EXEC6502           = 0x9102
local INTERPRET_6502     = 0x92BF
local STEP6502           = 0x921A
local NMI6502            = 0x9062
local IRQ6502            = 0x90B3
local CRASH_ADDR         = 0x91EE
local NES_IRQ_HANDLER    = 0xC140
local NES_NMI_HANDLER    = 0xC192

---------- guest ZP ----------
local PC_LO         = 0x6A
local PC_HI         = 0x6B
local SP_REG        = 0x6C
local A_REG         = 0x6D
local X_REG         = 0x6E
local Y_REG         = 0x6F
local STATUS_REG    = 0x70
local TARGET_BANK   = 0xAB
local NJSR_SAVED_SP = 0xB3
local MAPPER_PRG    = 0x60D0
local MAPPER_CHR    = 0x60D1
local FCA           = 0x6CE4
local FCB           = 0x6CE6
local PJA           = 0x6CDE
local PJB           = 0x6CE0
local PJFA          = 0x6CE1
local PJFB          = 0x6CE3
local SFO_BASE      = 0x6486

---------- helpers ----------
local M = emu.memType.nesDebug
local function rd(a)    return emu.read(a, M) end
local function rd16(a)  return rd(a) + rd(a+1) * 256 end

local function gs()
    return string.format("gPC=%04X gA=%02X gX=%02X gY=%02X gSP=%02X gST=%02X",
        rd16(PC_LO), rd(A_REG), rd(X_REG), rd(Y_REG), rd(SP_REG), rd(STATUS_REG))
end

local function bankInfo()
    return string.format("prg=%d chr=%d", rd(MAPPER_PRG), rd(MAPPER_CHR))
end

local function stackDump()
    local t = {}
    for i = 0x01F0, 0x01FF do t[#t+1] = string.format("%02X", rd(i)) end
    return "STK[" .. table.concat(t, " ") .. "]"
end

local function sfoSnap()
    local t = {}
    for i = 0, 14 do t[#t+1] = string.format("%04X", rd16(SFO_BASE + i*2)) end
    return "sfo=[" .. table.concat(t, " ") .. "]"
end

---------- ring buffer ----------
local ring = {}
local ringIdx = 0
local eventCount = 0

local function ringPut(msg)
    eventCount = eventCount + 1
    local s = string.format("[%07d] %s", eventCount, msg)
    ringIdx = (ringIdx % RING_SIZE) + 1
    ring[ringIdx] = s
    return s
end

---------- file log ----------
local logFile = io.open(LOG_PATH, "w")
if not logFile then return end
local lineCount = 0
local flushN = 0
local stopped = false
local frameCount = 0

local function filePut(s)
    if stopped or not logFile then return end
    logFile:write(s .. "\n")
    lineCount = lineCount + 1
    flushN = flushN + 1
    if flushN >= 500 then logFile:flush(); flushN = 0 end
    if lineCount >= MAX_LINES then
        logFile:write("=== MAX_LINES HIT ===\n")
        logFile:flush()
        logFile:close()
        logFile = nil
    end
end

local function log(msg)
    if stopped then return end
    local s = ringPut(msg)
    filePut(s)
end

---------- crash dump ----------
local function dumpRing(reason)
    if stopped then return end
    stopped = true
    -- flush main log
    if logFile then
        pcall(function()
            logFile:write("=== CRASH: " .. reason .. " ===\n")
            logFile:flush()
            logFile:close()
        end)
        logFile = nil
    end
    -- dump ring to separate file
    local rf = io.open(RING_PATH, "w")
    if rf then
        rf:write("=== CRASH RING DUMP: " .. reason .. " ===\n")
        rf:write("=== Total events: " .. tostring(eventCount) .. " ===\n")
        rf:write("=== Frame: " .. tostring(frameCount) .. " ===\n\n")
        local start = (ringIdx % RING_SIZE) + 1
        for i = 0, RING_SIZE - 1 do
            local idx = ((start - 1 + i) % RING_SIZE) + 1
            if ring[idx] then rf:write(ring[idx] .. "\n") end
        end
        rf:write("\n=== FINAL STATE ===\n")
        rf:write("  " .. gs() .. " " .. bankInfo() .. " " .. stackDump() .. "\n")
        for i = 0, 59 do
            rf:write(string.format("  sector[%02d] free=%04X\n", i, rd16(SFO_BASE + i*2)))
        end
        rf:write("  mapper_prg_bank=" .. tostring(rd(MAPPER_PRG)) .. "\n")
        rf:write("  mapper_chr_bank=" .. tostring(rd(MAPPER_CHR)) .. "\n")
        rf:write("  irq_count=" .. tostring(rd(0x003C)) .. "\n")
        rf:write("\n=== NES STACK $0100-$01FF ===\n")
        for row = 0, 15 do
            local p = {}
            for col = 0, 15 do p[#p+1] = string.format("%02X", rd(0x0100 + row*16 + col)) end
            rf:write(string.format("  $%04X: %s\n", 0x0100 + row*16, table.concat(p, " ")))
        end
        rf:write("\n=== WRAM ZP $6000-$60FF ===\n")
        for row = 0, 15 do
            local p = {}
            for col = 0, 15 do p[#p+1] = string.format("%02X", rd(0x6000 + row*16 + col)) end
            rf:write(string.format("  $%04X: %s\n", 0x6000 + row*16, table.concat(p, " ")))
        end
        rf:write("\n=== NES ZP $00-$FF ===\n")
        for row = 0, 15 do
            local p = {}
            for col = 0, 15 do p[#p+1] = string.format("%02X", rd(row*16 + col)) end
            rf:write(string.format("  $%04X: %s\n", row*16, table.concat(p, " ")))
        end
        rf:flush()
        rf:close()
    end
    local s = io.open(SIGNAL_PATH, "w")
    if s then s:write("TRACE_DONE " .. reason .. "\n"); s:flush(); s:close() end
end

---------- state tracking ----------
local lastGPC = -1
local stuckCount = 0
local stepCount = 0

---------- EXEC CALLBACKS ----------

-- *** THE CRASH ADDRESS — only trigger when bank 0 is mapped ***
-- $91EE is inside exec6502 in bank 0.  When bank 2 is mapped, $91EE is
-- inside recompile_opcode_b2 or another bank-2 function — NOT a crash.
emu.addMemoryCallback(function()
    local prg = rd(MAPPER_PRG)
    if prg == 0 then
        -- REAL crash: exec6502 code at $91EE reached with bank 0 mapped
        log(string.format("*** CRASH $91EE HIT (bank0) *** %s %s %s", gs(), bankInfo(), stackDump()))
        dumpRing("EXEC HIT $91EE (bank0)")
    else
        -- False positive: bank 2 (or other) code passes through $91EE normally
        -- Log it but DON'T stop — just note it
        log(string.format("FP_91EE prg=%d %s %s", prg, gs(), bankInfo()))
    end
end, emu.callbackType.exec, CRASH_ADDR)

-- dispatch_on_pc
emu.addMemoryCallback(function()
    log(string.format("DISP %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, DISPATCH_ON_PC)

-- dispatch JMP
emu.addMemoryCallback(function()
    log(string.format("  JMP>%04X tbank=%d %s", rd16(0x627A), rd(TARGET_BANK), gs()))
end, emu.callbackType.exec, DISPATCH_JMP)

-- flash_dispatch_return
emu.addMemoryCallback(function()
    log(string.format("  RET %s %s", gs(), bankInfo()))
end, emu.callbackType.exec, FLASH_DISP_RETURN)

-- flash_dispatch_return_no_regs
emu.addMemoryCallback(function()
    log(string.format("  RET_NR %s %s", gs(), bankInfo()))
end, emu.callbackType.exec, FLASH_DISP_RET_NR)

-- NJSR trampoline
emu.addMemoryCallback(function()
    log(string.format("  NJSR sp=%02X %s %s", rd(NJSR_SAVED_SP), gs(), stackDump()))
end, emu.callbackType.exec, NJSR_TRAMPOLINE)

-- cross_bank_dispatch
emu.addMemoryCallback(function()
    log(string.format("  XBANK %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, CROSS_BANK_DISP)

-- xbank_trampoline fast
emu.addMemoryCallback(function()
    log(string.format("  XFAST tgt=%04X %s", rd16(0x620B), gs()))
end, emu.callbackType.exec, XBANK_TRAMPOLINE)

-- PC update
emu.addMemoryCallback(function()
    log(string.format("PCUPD %s fca=%04X:b%d pj=%04X:b%d pjf=%04X:b%d",
        gs(), rd16(FCA), rd(FCB), rd16(PJA), rd(PJB), rd16(PJFA), rd(PJFB)))
end, emu.callbackType.exec, PC_UPDATE)

-- run_6502
emu.addMemoryCallback(function()
    log(string.format("RUN6502 %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, RUN_6502)

-- sector alloc
emu.addMemoryCallback(function()
    log(string.format("ALLOC %s fca=%04X:b%d %s", gs(), rd16(FCA), rd(FCB), sfoSnap()))
end, emu.callbackType.exec, FLASH_SECTOR_ALLOC)

-- sweep
emu.addMemoryCallback(function()
    log(string.format("SWEEP %s", gs()))
end, emu.callbackType.exec, SWEEP_PENDING)

-- scan
emu.addMemoryCallback(function()
    log(string.format("SCAN %s", gs()))
end, emu.callbackType.exec, SCAN_EPILOGUES)

-- exec6502
emu.addMemoryCallback(function()
    log(string.format("EXEC6502 %s %s", gs(), bankInfo()))
end, emu.callbackType.exec, EXEC6502)

-- interpret_6502
emu.addMemoryCallback(function()
    log(string.format("INTERP %s %s", gs(), bankInfo()))
end, emu.callbackType.exec, INTERPRET_6502)

-- step6502
emu.addMemoryCallback(function()
    stepCount = stepCount + 1
    if stepCount <= 5 or stepCount % 100 == 0 then
        log(string.format("STEP#%d %s %s", stepCount, gs(), bankInfo()))
    end
end, emu.callbackType.exec, STEP6502)

-- nmi6502
emu.addMemoryCallback(function()
    log(string.format("NMI6502 %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, NMI6502)

-- irq6502
emu.addMemoryCallback(function()
    log(string.format("IRQ6502 %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, IRQ6502)

-- NES HW IRQ handler
emu.addMemoryCallback(function()
    log(string.format("HW_IRQ %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, NES_IRQ_HANDLER)

-- NES HW NMI handler
emu.addMemoryCallback(function()
    log(string.format("HW_NMI %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.callbackType.exec, NES_NMI_HANDLER)

---------- BANK WRITES ($C000) — only log "interesting" ones ----------
-- Flash byte-program sequences do 01→00→01→bank→restore
-- Filter out the flood: only log when the bank changes to something
-- other than the immediate flash-seq values, or when prg >= 2
local lastBankVal = -1
emu.addMemoryCallback(function(addr, val)
    if val ~= lastBankVal then
        -- Always log bank switches to 2+ (render/compile bank switches)
        -- and transitions that aren't part of 00↔01 flash sequences
        if val >= 2 or (lastBankVal >= 2) or (val ~= 0 and val ~= 1) then
            log(string.format("BANKW $C000<=%02X %s %s", val, gs(), bankInfo()))
        end
        lastBankVal = val
    end
end, emu.callbackType.write, 0xC000)

---------- NES HW IRQ/NMI EVENTS ----------
emu.addEventCallback(function()
    log(string.format("EVT:IRQ %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.eventType.irq)

emu.addEventCallback(function()
    log(string.format("EVT:NMI %s %s %s", gs(), bankInfo(), stackDump()))
end, emu.eventType.nmi)

---------- PER-FRAME ----------
emu.addEventCallback(function()
    if stopped then return end
    frameCount = frameCount + 1

    local gpc = rd16(PC_LO)

    -- stuck detection
    if gpc == lastGPC then
        stuckCount = stuckCount + 1
        if stuckCount >= STUCK_THRESHOLD then
            log(string.format("*** STUCK %d frames at gPC=%04X *** %s %s %s",
                stuckCount, gpc, gs(), bankInfo(), stackDump()))
            dumpRing(string.format("STUCK gPC=%04X %d frames", gpc, stuckCount))
            return
        end
    else
        if stuckCount > 5 then
            log(string.format("(gPC was %04X for %d frames, now %04X)", lastGPC, stuckCount, gpc))
        end
        stuckCount = 0
        lastGPC = gpc
    end

    -- heartbeat
    if frameCount <= 100 or frameCount % 50 == 0 then
        log(string.format("--- FRAME %d --- %s %s", frameCount, gs(), bankInfo()))
    end

    -- periodic SFO
    if frameCount % 1000 == 0 then
        log(string.format("  SFO@f%d: %s", frameCount, sfoSnap()))
    end

    -- frame limit
    if frameCount >= MAX_FRAMES then
        log(string.format("=== FINAL STATE at frame %d ===", frameCount))
        log(string.format("  %s %s %s", gs(), bankInfo(), stackDump()))
        log(string.format("  %s", sfoSnap()))
        log(string.format("  events=%d steps=%d", eventCount, stepCount))
        dumpRing("FRAME LIMIT 200000")
    end
end, emu.eventType.endFrame)

---------- startup ----------
log("=== trace_crash.lua v7 started ===")
log(string.format("  %s %s", gs(), bankInfo()))
if logFile then logFile:flush() end

local sig = io.open(SIGNAL_PATH, "w")
if sig then sig:write("SCRIPT_READY\n"); sig:flush(); sig:close() end
