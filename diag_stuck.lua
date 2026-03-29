-- diag_stuck.lua  —  Comprehensive diagnostic: what is the emulator doing?
-- Samples state every frame AND every N NES instructions, logs to CSV.
-- Run in Mesen's script window.

local LOG_PATH = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\diag_stuck.csv"
local logFile = io.open(LOG_PATH, "w")

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
local ADDR_FRAME_TIME_LO = 0x0040   -- uint32_t _frame_time (4 bytes, LE)
local ADDR_FRAME_TIME_HI = 0x0042

-- Video
local ADDR_SCR_UPD       = 0x0033   -- uint8_t  _screen_ram_updated
local ADDR_CHR_UPD       = 0x0032   -- uint8_t  _character_ram_updated
local ADDR_SPRITENO      = 0x003A   -- uint8_t  _spriteno
local ADDR_SPR_ENABLE    = 0x003B   -- uint8_t  _sprite_enable

-- Flash / JIT
local ADDR_CACHE_IDX     = 0x0088   -- uint8_t  _cache_index
local ADDR_MAPPER_PRG    = 0x60D0   -- uint8_t  _mapper_prg_bank
local ADDR_FLASH_ADDR    = 0x6CE4   -- uint16_t _flash_code_address
local ADDR_FLASH_BANK    = 0x6CE6   -- uint8_t  _flash_code_bank

-- Key NES code entry points
local FUNC_DISPATCH      = 0x620D   -- _dispatch_on_pc
local FUNC_RUN_6502      = 0xCA97   -- _run_6502
local FUNC_RENDER_VIDEO  = 0xC8CE   -- _render_video
local FUNC_IRQ6502       = 0x90B3   -- _irq6502

-- Guest (Exidy) game state (emulated RAM at NES $6800+ = WRAM)
-- Guest ZP $00-$FF maps to WRAM. Need to know where WRAM data section puts it.
-- The emulated RAM base: guest addr $0000 maps to NES WRAM.
-- Let's read it via the emulated memory. We'll read the emulated ZP from the 
-- perspective of the emulator's read6502.

-- ====================================================================
-- Helpers
-- ====================================================================

local function rb(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

local function rw(addr)
    return rb(addr) + rb(addr + 1) * 256
end

local function r32(addr)
    return rb(addr) + rb(addr+1)*256 + rb(addr+2)*65536 + rb(addr+3)*16777216
end

local function hexb(v) return string.format("%02X", v) end
local function hexw(v) return string.format("%04X", v) end

-- ====================================================================
-- State
-- ====================================================================
local frameCount = 0
local sampleCount = 0
local lastEmuPC = 0
local lastNESPC = 0
local lastIrqCount = 0
local lastNMI = 0
local renderCount = 0
local irq6502Count = 0
local dispatchCount = 0

-- Histogram of emulated PCs seen
local pcHist = {}
local pcHistNES = {}

-- Ring buffer of last 20 NES PCs
local nesRing = {}
local nesRingIdx = 0
local NES_RING_SIZE = 40

-- ====================================================================
-- CSV header
-- ====================================================================
logFile:write("sample,frame,type,nes_pc,emu_pc,emu_a,emu_x,emu_y,emu_sp,emu_status,")
logFile:write("nmi_ctr,last_nmi,irq_count,int_cond,mapper_bank,cache_idx,")
logFile:write("scr_upd,chr_upd,flash_addr,flash_bank,")
logFile:write("info\n")

local function logState(eventType, info)
    sampleCount = sampleCount + 1
    local nesPC = emu.getState().cpu.pc
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
    local scrUpd = rb(ADDR_SCR_UPD)
    local chrUpd = rb(ADDR_CHR_UPD)
    local flashAddr = rw(ADDR_FLASH_ADDR)
    local flashBank = rb(ADDR_FLASH_BANK)
    
    logFile:write(string.format("%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,",
        sampleCount, frameCount, eventType,
        hexw(nesPC), hexw(emuPC),
        hexb(emuA), hexb(emuX), hexb(emuY), hexb(emuSP),
        hexb(emuST)))
    logFile:write(string.format("%s,%s,%s,%s,%s,%s,",
        hexb(nmiCtr), hexb(lastNmi),
        hexb(irqCnt), hexb(intCond),
        hexb(mapBank), hexb(cacheIdx)))
    logFile:write(string.format("%d,%d,%s,%s,",
        scrUpd, chrUpd,
        hexw(flashAddr), hexb(flashBank)))
    logFile:write((info or "") .. "\n")
end

-- ====================================================================
-- Frame callback — log state at start of every frame
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
    local info = string.format("irq_delta=%d nmi_delta=%d I=%d intcond=%02X last_nmi=%02X nmi_ctr=%02X",
        irqDelta, nmiDelta, iFlag and 1 or 0, intCond, lastNmi, nmiCtr)
    
    -- Count emulated PC in histogram
    pcHist[emuPC] = (pcHist[emuPC] or 0) + 1
    
    logState("FRAME", info)
    
    -- Display on screen
    if frameCount % 30 == 0 then
        local statusStr = string.format("F%d PC:%04X I:%d IRQ#%d NMI:%02X/%02X IC:%02X",
            frameCount, emuPC, iFlag and 1 or 0, irqCnt, nmiCtr, lastNmi, intCond)
        emu.displayMessage("Diag", statusStr)
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
        
        logFile:write("\n--- NES PC HISTOGRAM (top 30) ---\n")
        sorted = {}
        for pc, count in pairs(pcHistNES) do
            table.insert(sorted, {pc=pc, count=count})
        end
        table.sort(sorted, function(a,b) return a.count > b.count end)
        for i = 1, math.min(30, #sorted) do
            logFile:write(string.format("  $%04X : %d times\n", sorted[i].pc, sorted[i].count))
        end
        
        logFile:write("\n--- NES PC RING (last " .. NES_RING_SIZE .. " samples) ---\n")
        for i = 1, NES_RING_SIZE do
            local idx = ((nesRingIdx - NES_RING_SIZE + i - 1) % NES_RING_SIZE) + 1
            if nesRing[idx] then
                logFile:write(string.format("  $%04X\n", nesRing[idx]))
            end
        end
        
        logFile:write(string.format("\nTotal frames: %d\n", frameCount))
        logFile:write(string.format("Total IRQs fired: %d\n", irqCnt))
        logFile:write(string.format("Render count: %d\n", renderCount))
        logFile:write(string.format("Dispatch count: %d\n", dispatchCount))
        logFile:write(string.format("irq6502 calls: %d\n", irq6502Count))
        
        logFile:close()
        emu.displayMessage("Diag", "diag_stuck.csv written — 600 frames captured")
        emu.breakExecution()
    end
end, emu.eventType.endFrame)

-- ====================================================================
-- Periodic NES-level sampling: every ~5000 NES CPU cycles, log state
-- We use an exec callback on a few key addresses instead
-- ====================================================================

-- Track dispatch_on_pc calls
emu.addMemoryCallback(function(addr, value)
    dispatchCount = dispatchCount + 1
    local emuPC = rw(ADDR_PC_LO)
    if dispatchCount % 100 == 0 then
        logState("DISPATCH", string.format("emu_pc=%04X count=%d", emuPC, dispatchCount))
    end
end, emu.callbackType.exec, FUNC_DISPATCH, FUNC_DISPATCH)

-- Track render_video calls
emu.addMemoryCallback(function(addr, value)
    renderCount = renderCount + 1
    logState("RENDER", string.format("count=%d", renderCount))
end, emu.callbackType.exec, FUNC_RENDER_VIDEO, FUNC_RENDER_VIDEO)

-- Track irq6502 calls
emu.addMemoryCallback(function(addr, value)
    irq6502Count = irq6502Count + 1
    local emuPC = rw(ADDR_PC_LO)
    local emuST = rb(ADDR_STATUS)
    logState("IRQ6502", string.format("emu_pc=%04X status=%02X count=%d", emuPC, emuST, irq6502Count))
end, emu.callbackType.exec, FUNC_IRQ6502, FUNC_IRQ6502)

-- ====================================================================
-- Sample NES PC on every NMI (NMI vector exec at the lnNmiHandler)
-- ====================================================================
-- lazyNES NMI handler is at $C192 from ROM
emu.addMemoryCallback(function(addr, value)
    local nesPC = emu.getState().cpu.pc
    nesRingIdx = (nesRingIdx % NES_RING_SIZE) + 1
    nesRing[nesRingIdx] = nesPC
    pcHistNES[nesPC] = (pcHistNES[nesPC] or 0) + 1
end, emu.callbackType.exec, 0xC192, 0xC192)

-- ====================================================================
-- Watch writes to key emulated state
-- ====================================================================

-- Watch _status changes (looking for I flag manipulation)
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

-- Watch _interrupt_condition changes
local lastIntCond = 0
emu.addMemoryCallback(function(addr, value)
    if value ~= lastIntCond then
        local emuPC = rw(ADDR_PC_LO)
        logState("INT_COND", string.format("old=%02X new=%02X emu_pc=%04X", lastIntCond, value, emuPC))
        lastIntCond = value
    end
end, emu.callbackType.write, ADDR_INT_COND, ADDR_INT_COND)

-- Watch _last_nmi_frame changes
emu.addMemoryCallback(function(addr, value)
    local nmiCtr = rb(ADDR_NMI_COUNTER)
    logState("LAST_NMI_W", string.format("val=%02X nmi_ctr=%02X", value, nmiCtr))
end, emu.callbackType.write, ADDR_LAST_NMI, ADDR_LAST_NMI)

-- ====================================================================
-- Startup
-- ====================================================================
emu.displayMessage("Diag", "diag_stuck.lua loaded — capturing 600 frames (10 sec)")
