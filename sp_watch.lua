-- Debug script - watch for writes to SP that set it to 0
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\sp_watch.log", "w")
local frameCount = 0

logFile:write("SP write watch started\n")
logFile:write("Watching for writes to $005A (emulated SP)\n\n")

-- Watch writes to SP
emu.addMemoryCallback(function(addr, value)
    if frameCount > 5 then
        -- Get NES CPU state at time of write
        local nesPC = emu.getState().cpu.pc
        local nesA = emu.getState().cpu.a
        local nesX = emu.getState().cpu.x
        local nesY = emu.getState().cpu.y
        
        if value == 0x00 then
            logFile:write(string.format("Frame %d: SP <- $00 DETECTED!\n", frameCount))
            logFile:write(string.format("  NES CPU: PC=$%04X A=$%02X X=$%02X Y=$%02X\n", nesPC, nesA, nesX, nesY))
            
            -- Read current emulated PC
            local emuPcLo = emu.read(0x0058, emu.memType.cpuMemory)
            local emuPcHi = emu.read(0x0059, emu.memType.cpuMemory)
            logFile:write(string.format("  Emulated PC: $%04X\n", emuPcLo + emuPcHi * 256))
            logFile:flush()
            emu.breakExecution()
        elseif value == 0xFD then
            logFile:write(string.format("Frame %d: SP <- $FD (normal reset)\n", frameCount))
            logFile:flush()
        else
            logFile:write(string.format("Frame %d: SP <- $%02X\n", frameCount, value))
        end
    end
end, emu.callbackType.write, 0x005A, 0x005A)

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount > 600 then
        logFile:write("=== 600 frames reached ===\n")
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)

logFile:write("Watching...\n")
logFile:flush()
