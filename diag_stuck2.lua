-- diag_stuck2.lua — Focused diagnostic for "game stuck" issue
-- Uses only documented Mesen 2 API (no emu.getState())
-- Logs emulator state every frame for 600 frames (10 sec), writes CSV + summary.
-- Run in Mesen's script window.

local LOG_PATH = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\diag_stuck2.csv"
local logFile = io.open(LOG_PATH, "w")
if not logFile then
    emu.log("ERROR: could not open " .. LOG_PATH)
    return
end

-- sentinel: prove script loaded
emu.displayMessage("Diag2", "diag_stuck2 LOADED - v3")
emu.log("diag_stuck2 loaded OK")

-- ====================================================================
-- Addresses from vicemap.map  (NES CPU address space)
-- ====================================================================

-- Emulated 6502 state (zero page)
local ADDR_PC_LO        = 0x005B   -- uint16_t _pc (little-endian)
local ADDR_PC_HI        = 0x005C
local ADDR_SP            = 0x005D   -- uint8_t  _sp
local ADDR_A             = 0x005E   -- uint8_t  _a
local ADDR_X             = 0x005F   -- uint8_t  _x
local ADDR_Y             = 0x0060   -- uint8_t  _y
local ADDR_STATUS        = 0x0061   -- uint8_t  _status

-- Frame / timing
local ADDR_NMI_COUNTER   = 0x0026   -- lazyNES NMI frame counter (ZP $26)
local ADDR_LAST_NMI      = 0x003D   -- uint8_t  _last_nmi_frame
local ADDR_IRQ_COUNT     = 0x003C   -- uint8_t  _irq_count
local ADDR_INT_COND      = 0x0044   -- uint8_t  _interrupt_condition

-- Flash / JIT
local ADDR_CACHE_IDX     = 0x0088   -- uint8_t  _cache_index
local ADDR_MAPPER_PRG    = 0x60D0   -- uint8_t  _mapper_prg_bank

-- Key NES code entry points
local FUNC_DISPATCH      = 0x620D   -- _dispatch_on_pc
local FUNC_RUN_6502      = 0xCB1A   -- _run_6502
local FUNC_RENDER_VIDEO  = 0xC951   -- _render_video
local FUNC_IRQ6502       = 0x92D5   -- _irq6502
local FUNC_NMI_HANDLER   = 0xC192   -- lazyNES NMI handler

-- ====================================================================
-- Helpers — all reads use nesDebug to avoid side-effects
-- ====================================================================
local MEM = emu.memType.nesDebug

local function rb(addr)
    return emu.read(addr, MEM)
end

local function rw(addr)
    return rb(addr) + rb(addr + 1) * 256
end

local function hexb(v) return string.format("%02X", v) end
local function hexw(v) return string.format("%04X", v) end

-- ====================================================================
-- State
-- ====================================================================
local frameCount = 0
local sampleCount = 0
local lastIrqCount = 0
local lastNMI = 0
local renderCount = 0
local irq6502Count = 0
local dispatchCount = 0
local run6502Count = 0

-- Histogram of emulated PCs seen at frame boundaries
local pcHist = {}

-- Track last 40 dispatch PCs
local dispRing = {}
local dispRingIdx = 0
local DISP_RING_SIZE = 40

-- ====================================================================
-- CSV header
-- ====================================================================
logFile:write("sample,frame,type,emu_pc,emu_a,emu_x,emu_y,emu_sp,emu_status,")
logFile:write("nmi_ctr,last_nmi,irq_count,int_cond,mapper_bank,cache_idx,")
logFile:write("info\n")

local function logState(eventType, info)
    sampleCount = sampleCount + 1
    local emuPC = rw(ADDR_PC_LO)
    local emuA  = rb(ADDR_A)
    local emuX  = rb(ADDR_X)
    local emuY  = rb(ADDR_Y)
    local emuSP = rb(ADDR_SP)
    local emuST = rb(ADDR_STATUS)
    local nmiCtr = rb(ADDR_NMI_COUNTER)
    local lastNmi = rb(ADDR_LAST_NMI)
    local irqCnt = rb(ADDR_IRQ_COUNT)
    local intCond = rb(ADDR_INT_COND)
    local mapBank = rb(ADDR_MAPPER_PRG)
    local cacheIdx = rb(ADDR_CACHE_IDX)

    logFile:write(string.format("%d,%d,%s,%s,%s,%s,%s,%s,%s,",
        sampleCount, frameCount, eventType,
        hexw(emuPC),
        hexb(emuA), hexb(emuX), hexb(emuY), hexb(emuSP),
        hexb(emuST)))
    logFile:write(string.format("%s,%s,%s,%s,%s,%s,",
        hexb(nmiCtr), hexb(lastNmi),
        hexb(irqCnt), hexb(intCond),
        hexb(mapBank), hexb(cacheIdx)))
    logFile:write((info or "") .. "\n")
end

-- ====================================================================
-- Frame callback — log state at end of every frame
-- ====================================================================
emu.addEventCallback(function()
    frameCount = frameCount + 1

    local emuPC = rw(ADDR_PC_LO)
    local nmiCtr = rb(ADDR_NMI_COUNTER)
    local irqCnt = rb(ADDR_IRQ_COUNT)
    local emuST = rb(ADDR_STATUS)
    local intCond = rb(ADDR_INT_COND)
    local lastNmi = rb(ADDR_LAST_NMI)

    -- Track IRQ firing
    local irqDelta = (irqCnt - lastIrqCount) % 256
    lastIrqCount = irqCnt

    -- Track NMI changes
    local nmiDelta = (nmiCtr - lastNMI) % 256
    lastNMI = nmiCtr

    -- I-flag status
    local iFlag = (emuST & 0x04) ~= 0

    -- Build info string
    local info = string.format("irq_delta=%d nmi_delta=%d I=%d intcond=%02X last_nmi=%02X nmi_ctr=%02X disp=%d run=%d render=%d irq6502=%d",
        irqDelta, nmiDelta, iFlag and 1 or 0, intCond, lastNmi, nmiCtr,
        dispatchCount, run6502Count, renderCount, irq6502Count)

    -- Count emulated PC in histogram
    pcHist[emuPC] = (pcHist[emuPC] or 0) + 1

    logState("FRAME", info)

    -- Display on screen every 30 frames
    if frameCount % 30 == 0 then
        local statusStr = string.format("F%d PC:%04X I:%d IRQ#%d NMI:%02X/%02X IC:%02X",
            frameCount, emuPC, iFlag and 1 or 0, irqCnt, nmiCtr, lastNmi, intCond)
        emu.displayMessage("Diag2", statusStr)
    end

    -- Flush periodically
    if frameCount % 60 == 0 then
        logFile:flush()
    end

    -- After 600 frames (10 sec), dump summary and stop
    if frameCount >= 600 then
        -- Write summary
        logFile:write("\n\n--- EMULATED PC HISTOGRAM (top 30) ---\n")
        local sorted = {}
        for pc, count in pairs(pcHist) do
            table.insert(sorted, {pc=pc, count=count})
        end
        table.sort(sorted, function(a,b) return a.count > b.count end)
        for i = 1, math.min(30, #sorted) do
            logFile:write(string.format("  $%04X : %d times\n", sorted[i].pc, sorted[i].count))
        end

        logFile:write("\n--- DISPATCH PC RING (last " .. DISP_RING_SIZE .. ") ---\n")
        for i = 1, DISP_RING_SIZE do
            local idx = ((dispRingIdx - DISP_RING_SIZE + i - 1) % DISP_RING_SIZE) + 1
            if dispRing[idx] then
                logFile:write(string.format("  $%04X\n", dispRing[idx]))
            end
        end

        logFile:write(string.format("\nTotal frames: %d\n", frameCount))
        logFile:write(string.format("Total IRQs fired (irq_count): %d\n", rb(ADDR_IRQ_COUNT)))
        logFile:write(string.format("Total render_video calls: %d\n", renderCount))
        logFile:write(string.format("Total dispatch_on_pc calls: %d\n", dispatchCount))
        logFile:write(string.format("Total run_6502 calls: %d\n", run6502Count))
        logFile:write(string.format("Total irq6502 calls: %d\n", irq6502Count))
        logFile:write(string.format("Dispatches per frame: %.1f\n", dispatchCount / frameCount))
        logFile:write(string.format("IRQs per frame: %.2f\n", rb(ADDR_IRQ_COUNT) / frameCount))
        logFile:write(string.format("Renders per frame: %.2f\n", renderCount / frameCount))

        logFile:close()
        emu.displayMessage("Diag2", "diag_stuck2.csv written - " .. frameCount .. " frames")
        emu.log("diag_stuck2 complete: " .. frameCount .. " frames, " .. sampleCount .. " samples")
        emu.breakExecution()
    end
end, emu.eventType.endFrame)

-- ====================================================================
-- Track dispatch_on_pc calls
-- ====================================================================
emu.addMemoryCallback(function(addr, value)
    dispatchCount = dispatchCount + 1
    local emuPC = rw(ADDR_PC_LO)

    -- Record in ring buffer
    dispRingIdx = (dispRingIdx % DISP_RING_SIZE) + 1
    dispRing[dispRingIdx] = emuPC

    -- Log every 500th dispatch
    if dispatchCount % 500 == 0 then
        logState("DISPATCH", string.format("emu_pc=%04X count=%d", emuPC, dispatchCount))
    end
end, emu.callbackType.exec, FUNC_DISPATCH, FUNC_DISPATCH)

-- ====================================================================
-- Track run_6502 calls
-- ====================================================================
emu.addMemoryCallback(function(addr, value)
    run6502Count = run6502Count + 1
end, emu.callbackType.exec, FUNC_RUN_6502, FUNC_RUN_6502)

-- ====================================================================
-- Track render_video calls
-- ====================================================================
emu.addMemoryCallback(function(addr, value)
    renderCount = renderCount + 1
    logState("RENDER", string.format("count=%d", renderCount))
end, emu.callbackType.exec, FUNC_RENDER_VIDEO, FUNC_RENDER_VIDEO)

-- ====================================================================
-- Track irq6502 calls
-- ====================================================================
emu.addMemoryCallback(function(addr, value)
    irq6502Count = irq6502Count + 1
    local emuPC = rw(ADDR_PC_LO)
    local emuST = rb(ADDR_STATUS)
    logState("IRQ6502", string.format("emu_pc=%04X status=%02X count=%d", emuPC, emuST, irq6502Count))
end, emu.callbackType.exec, FUNC_IRQ6502, FUNC_IRQ6502)

-- ====================================================================
-- Watch _status changes (I flag transitions)
-- ====================================================================
local lastStatusVal = 0
emu.addMemoryCallback(function(addr, value)
    if value ~= lastStatusVal then
        local oldI = (lastStatusVal & 0x04) ~= 0
        local newI = (value & 0x04) ~= 0
        if oldI ~= newI then
            local emuPC = rw(ADDR_PC_LO)
            logState("STATUS_I", string.format("old=%02X new=%02X I:%d->%d emu_pc=%04X",
                lastStatusVal, value, oldI and 1 or 0, newI and 1 or 0, emuPC))
        end
        lastStatusVal = value
    end
end, emu.callbackType.write, ADDR_STATUS, ADDR_STATUS)

-- ====================================================================
-- Watch _interrupt_condition changes
-- ====================================================================
local lastIntCond = 0
emu.addMemoryCallback(function(addr, value)
    if value ~= lastIntCond then
        local emuPC = rw(ADDR_PC_LO)
        logState("INT_COND", string.format("old=%02X new=%02X emu_pc=%04X", lastIntCond, value, emuPC))
        lastIntCond = value
    end
end, emu.callbackType.write, ADDR_INT_COND, ADDR_INT_COND)

-- ====================================================================
-- Watch _last_nmi_frame changes
-- ====================================================================
emu.addMemoryCallback(function(addr, value)
    local nmiCtr = rb(ADDR_NMI_COUNTER)
    logState("LAST_NMI_W", string.format("val=%02X nmi_ctr=%02X", value, nmiCtr))
end, emu.callbackType.write, ADDR_LAST_NMI, ADDR_LAST_NMI)

-- ====================================================================
-- Startup
-- ====================================================================
emu.displayMessage("Diag2", "diag_stuck2 capturing 600 frames (10 sec)")
emu.log("diag_stuck2: capturing 600 frames")
