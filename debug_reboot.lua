-- Debug script to catch when the game reboots to $2800
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\reboot_debug.log", "w")
local frameCount = 0
local initialStartup = true
local startupFrames = 10  -- Ignore first 10 frames (initial boot)
local rebootCount = 0
local maxFrames = 3600  -- Run for ~60 seconds at 60fps

logFile:write("Reboot debug started\n")
logFile:write("Watching for PC=$2800 after frame " .. startupFrames .. "\n\n")

-- Track previous state for context
local prevPC = 0
local prevA = 0
local prevX = 0
local prevY = 0
local prevSP = 0
local prevStatus = 0

emu.addEventCallback(function()
    frameCount = frameCount + 1
    
    if frameCount > startupFrames then
        initialStartup = false
    end
    
    if frameCount >= maxFrames then
        logFile:write("\n=== Reached " .. maxFrames .. " frames, stopping ===\n")
        logFile:write("Total reboots detected: " .. rebootCount .. "\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

-- Watch for execution at $2800 (reset vector)
emu.addMemoryCallback(function()
    if not initialStartup then
        rebootCount = rebootCount + 1
        local state = emu.getState()
        local cpuState = state.cpu
        
        logFile:write("=== REBOOT DETECTED at frame " .. frameCount .. " ===\n")
        logFile:write("Previous state before reaching $2800:\n")
        logFile:write(string.format("  PC=$%04X A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X\n", 
            prevPC, prevA, prevX, prevY, prevSP, prevStatus))
        logFile:write("Current state:\n")
        logFile:write(string.format("  PC=$%04X A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X\n",
            cpuState.pc, cpuState.a, cpuState.x, cpuState.y, cpuState.sp, cpuState.ps))
        
        -- Dump stack contents
        logFile:write("Stack contents (from SP+1 to $1FF):\n  ")
        for i = cpuState.sp + 1, 0xFF do
            local val = emu.read(0x100 + i, emu.memType.cpuMemory)
            logFile:write(string.format("%02X ", val))
            if (i - cpuState.sp) % 16 == 0 then
                logFile:write("\n  ")
            end
        end
        logFile:write("\n\n")
        logFile:flush()
    end
end, emu.memCallbackType.cpuExec, 0x2800, 0x2802)

-- Track state changes on every instruction (sample every few instructions)
local instrCount = 0
emu.addMemoryCallback(function()
    instrCount = instrCount + 1
    if instrCount % 100 == 0 then  -- Sample every 100 instructions
        local state = emu.getState()
        local cpuState = state.cpu
        prevPC = cpuState.pc
        prevA = cpuState.a
        prevX = cpuState.x
        prevY = cpuState.y
        prevSP = cpuState.sp
        prevStatus = cpuState.ps
    end
end, emu.memCallbackType.cpuExec, 0x0000, 0xFFFF)

logFile:write("Callbacks registered, running emulation...\n\n")
logFile:flush()
