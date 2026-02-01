-- Debug IRQ handling
-- Version 1

local logFile = io.open("debug_irq_log.txt", "w")
logFile:write("=== IRQ Debug Log v1 ===\n")

local irq_entry_count = 0
local sta_5000_count = 0
local rti_count = 0
local frame_count = 0

-- Track IRQ handler entry at $2B0E
emu.addMemoryCallback(function()
    irq_entry_count = irq_entry_count + 1
    local regs = emu.getState()["cpu"]
    logFile:write(string.format("Frame %d: IRQ entry #%d, A=%02X SP=%02X Status=%02X\n", 
        frame_count, irq_entry_count, regs.a, regs.sp, regs.ps))
    logFile:flush()
end, emu.callbackType.exec, 0x2B0E)

-- Track STA $5000 at $2B27
emu.addMemoryCallback(function()
    sta_5000_count = sta_5000_count + 1
    local regs = emu.getState()["cpu"]
    logFile:write(string.format("Frame %d: STA $5000 #%d, A=%02X\n", 
        frame_count, sta_5000_count, regs.a))
    logFile:flush()
end, emu.callbackType.exec, 0x2B27)

-- Track RTI instructions (how IRQ handler exits)
emu.addMemoryCallback(function()
    rti_count = rti_count + 1
    local regs = emu.getState()["cpu"]
    logFile:write(string.format("Frame %d: RTI #%d, Status after=%02X\n", 
        frame_count, rti_count, regs.ps))
    logFile:flush()
end, emu.callbackType.exec, 0x2B5E)  -- RTI is at end of IRQ handler

-- Frame counter
emu.addEventCallback(function()
    frame_count = frame_count + 1
    if frame_count % 60 == 0 then
        logFile:write(string.format("--- Frame %d: IRQ entries=%d, STA $5000=%d, RTI=%d ---\n",
            frame_count, irq_entry_count, sta_5000_count, rti_count))
        logFile:flush()
    end
end, emu.eventType.endFrame)

logFile:write("Script loaded, monitoring started\n")
logFile:flush()

emu.log("IRQ Debug script v1 loaded")
