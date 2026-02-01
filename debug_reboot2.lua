-- Debug script to inspect PC history when reboot is detected
-- Uses Mesen Lua API
-- Watches for writes to PC that set it to $2800 after startup
-- REVISION 11 - detect jump INTO reset area from outside

local PC_HISTORY_ADDR = 0x0B1B
local PC_HISTORY_INDEX_ADDR = 0x00D8
local PC_LO_ADDR = 0x0058
local PC_HI_ADDR = 0x0059
local EXIDY_SP_ADDR = 0x005A  -- Exidy emulated stack pointer
local STACK_BASE = 0x01       -- Stack base relative to RAM_BASE

local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\reboot2_debug.log", "w")
logFile:write("Reboot detection script loaded. REVISION 11 - detect jump INTO reset area $2800-$2820 from outside\n")
logFile:write("Watching for PC transitions into reset area after startup.\n\n")
logFile:flush()

local frameCount = 0
local rebootDetected = false
local resetCount = 0  -- Count all resets
local lastPcHi = 0
local lastPcLo = 0
local lastValidPC = nil  -- Track previous PC for transition detection
local logFileClosed = false  -- Prevent writing after close

-- Keep our own PC history since the C one isn't working
local pcHistory = {}
local historySize = 64
local historyIndex = 1

-- Track SP changes  
local spChangeLog = {}
local spChangeCount = 0

-- Track stack memory writes
local stackWriteLog = {}
local stackWriteCount = 0

-- Sample SP every 10 frames
local spSampleLog = {}

-- RAM_BASE is at $6C00 (from vicemap.map), so Exidy stack is at $6D00-$6DFF
local RAM_BASE = 0x6C00
local EXIDY_STACK_BASE = RAM_BASE + 0x100

local function readByte(addr)
    -- Try nesDebug instead of cpuMemory for reading internal RAM
    return emu.read(addr, emu.memType.nesDebug)
end

local function readWord(addr)
    local lo = emu.read(addr, emu.memType.nesDebug)
    local hi = emu.read(addr + 1, emu.memType.nesDebug)
    return lo + (hi * 256)
end

-- Watch writes to SP to see when it changes
emu.addMemoryCallback(function(addr, value)
    spChangeCount = spChangeCount + 1
    if spChangeCount <= 100 then
        local pc = readWord(PC_LO_ADDR)
        spChangeLog[spChangeCount] = {frame = frameCount, sp = value, pc = pc}
    end
end, emu.callbackType.write, EXIDY_SP_ADDR, EXIDY_SP_ADDR)

-- Watch writes to stack memory area ($6D00-$6DFF) - entire stack
-- Log writes to top of stack ($6DFB-$6DFF) and any zero writes
emu.addMemoryCallback(function(addr, value)
    stackWriteCount = stackWriteCount + 1
    local isTopOfStack = (addr >= EXIDY_STACK_BASE + 0xFB)
    local isZeroWrite = (value == 0)
    if stackWriteCount <= 200 or isTopOfStack or isZeroWrite then
        local pc = readWord(PC_LO_ADDR)
        local sp = readByte(EXIDY_SP_ADDR)
        local logEntry = {frame = frameCount, addr = addr, value = value, pc = pc, sp = sp}
        -- Always log top-of-stack or zero writes, otherwise first 200
        if #stackWriteLog < 200 or isTopOfStack then
            table.insert(stackWriteLog, logEntry)
        end
    end
end, emu.callbackType.write, EXIDY_STACK_BASE, EXIDY_STACK_BASE + 0xFF)

-- Watch writes to PC high byte - record all changes
emu.addMemoryCallback(function(addr, value)
    local pcLo = readByte(PC_LO_ADDR)
    local fullPC = pcLo + (value * 256)
    local exidySP = readByte(EXIDY_SP_ADDR)
    
    -- Record to our own history
    pcHistory[historyIndex] = {frame = frameCount, pc = fullPC, hi = value, lo = pcLo, sp = exidySP}
    historyIndex = (historyIndex % historySize) + 1
    
    -- Track if we jumped INTO the reset entry point ($2800 exactly) from outside
    local isResetEntry = (fullPC == 0x2800)
    local wasOutsideResetArea = (lastValidPC ~= nil and lastValidPC ~= 0x2800)
    
    -- Log EVERY transition to $2800, not just the first
    if frameCount > 10 and isResetEntry and wasOutsideResetArea then
        -- PC jumped to reset vector $2800 from somewhere else!
        resetCount = resetCount + 1
        logFile:write(string.format("\n!!! RESET #%d: PC jumped from $%04X to $%04X at frame %d !!!\n", 
            resetCount, lastValidPC or 0, fullPC, frameCount))
        logFile:flush()
        rebootDetected = true
        
        -- Get return address info - look at what's at stack top
        -- RAM_BASE is $0C00, Exidy stack is at $0D00-$0DFF
        local stackTop1 = readByte(EXIDY_STACK_BASE + exidySP + 1)
        local stackTop2 = readByte(EXIDY_STACK_BASE + exidySP + 2)
        local stackTop3 = readByte(EXIDY_STACK_BASE + exidySP + 3)
        
        -- Dump more stack area
        logFile:write(string.format("!!! RESET DETECTED: PC jumped from $%04X to $%04X at frame %d !!!\n", lastValidPC or 0, fullPC, frameCount))
        logFile:write(string.format("PC changed from $%02X%02X to $%04X at frame %d\n", lastPcHi, lastPcLo, fullPC, frameCount))
        logFile:write(string.format("Exidy SP: $%02X\n", exidySP))
        logFile:write(string.format("Stack bytes at SP+1,+2,+3 (at $%04X+): $%02X $%02X $%02X\n", 
            EXIDY_STACK_BASE + exidySP + 1, stackTop1, stackTop2, stackTop3))
        
        -- Dump stack from $6DFA-$6DFF
        logFile:write("Stack top area ($6DFA-$6DFF):\n")
        for i = 0xFA, 0xFF do
            logFile:write(string.format("  $6D%02X: $%02X\n", i, readByte(EXIDY_STACK_BASE + i)))
        end
        logFile:write("\n")
        
        -- Dump SP change log
        logFile:write("=== SP CHANGE LOG (first 100 changes) ===\n")
        for i = 1, math.min(spChangeCount, 100) do
            local entry = spChangeLog[i]
            if entry then
                logFile:write(string.format("  [%2d] Frame %3d: SP=$%02X  PC=$%04X\n", i, entry.frame, entry.sp, entry.pc))
            end
        end
        logFile:write(string.format("Total SP changes: %d\n", spChangeCount))
        logFile:write("\n")
        
        -- Dump stack write log
        logFile:write("=== STACK MEMORY WRITES ($6D00-$6DFF) - top and zeros ===\n")
        if #stackWriteLog == 0 then
            logFile:write("  NO WRITES TO STACK AREA!\n")
        else
            for i, entry in ipairs(stackWriteLog) do
                logFile:write(string.format("  [%3d] Frame %3d: $%04X <- $%02X  (PC=$%04X SP=$%02X)\n", 
                    i, entry.frame, entry.addr, entry.value, entry.pc, entry.sp))
            end
        end
        logFile:write(string.format("Total stack writes: %d\n", stackWriteCount))
        logFile:write("\n")
        
        -- Dump SP samples
        logFile:write("=== SP SAMPLES (every 10 frames) ===\n")
        for i, entry in ipairs(spSampleLog) do
            logFile:write(string.format("  Frame %3d: SP=$%02X PC=$%04X\n", entry.frame, entry.sp, entry.pc))
        end
        logFile:write("\n")
        
        logFile:write("=== REBOOT DETECTED ===\n")
        logFile:write(string.format("Frame: %d\n", frameCount))
        logFile:write("\n")
        logFile:write("PC History (last 64 PC changes, oldest to newest):\n")
        
        -- Print from oldest to newest
        for i = 1, historySize do
            local idx = ((historyIndex - 2 + i) % historySize) + 1
            local entry = pcHistory[idx]
            if entry then
                local marker = ""
                if i == historySize then
                    marker = " <-- CAUSED REBOOT"
                end
                logFile:write(string.format("  [%2d] Frame %3d: PC=$%04X SP=$%02X%s\n", i, entry.frame, entry.pc, entry.sp, marker))
            end
        end
        logFile:write("======================\n")
        logFile:flush()
        -- Don't close file or break - keep running to see if this was a false positive
        -- logFile:close()
        -- logFileClosed = true
        -- emu.breakExecution()
    end
    lastPcHi = value
    lastPcLo = pcLo
    lastValidPC = fullPC  -- Track for transition detection
end, emu.callbackType.write, PC_HI_ADDR, PC_HI_ADDR)

-- Count frames and sample SP
local frame15Dumped = false
emu.addEventCallback(function()
    frameCount = frameCount + 1
    -- Log every 200 frames to show script is running
    if frameCount % 200 == 0 and not logFileClosed then
        local pc = readWord(PC_LO_ADDR)
        local sp = readByte(EXIDY_SP_ADDR)
        logFile:write(string.format("Frame %d: PC=$%04X SP=$%02X\n", frameCount, pc, sp))
        logFile:flush()
    end
    -- Dump stack at frame 15 (after writes at frame 12)
    if frameCount == 15 and not frame15Dumped and not logFileClosed then
        frame15Dumped = true
        logFile:write("=== STACK DUMP AT FRAME 15 ===\n")
        logFile:write("Stack area $6DFC-$6DFF:\n")
        for i = 0xFC, 0xFF do
            local val = readByte(EXIDY_STACK_BASE + i)
            logFile:write(string.format("  $6D%02X: $%02X\n", i, val))
        end
        logFile:write("\n")
        logFile:flush()
    end
    -- Sample SP every 10 frames
    if frameCount % 10 == 0 and #spSampleLog < 50 then
        local sp = readByte(EXIDY_SP_ADDR)
        local pc = readWord(PC_LO_ADDR)
        table.insert(spSampleLog, {frame = frameCount, sp = sp, pc = pc})
    end
end, emu.eventType.endFrame)
