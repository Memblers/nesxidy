-- Dump debug and sector array memory to file
local logFile = io.open("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\sector_dump.txt", "w")
local frameCount = 0

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount == 300 then
        -- Read debug area at $6000
        logFile:write("===== Debug Area at $6000-$600F =====\n")
        for i = 0, 15 do
            local val = emu.read(0x6000 + i, emu.memType.nesMemory)
            logFile:write(string.format("$%04X: $%02X\n", 0x6000 + i, val))
        end
        
        -- Read sector array at $6010
        logFile:write("\n===== Sector Array at $6010-$602F =====\n")
        for i = 0, 31 do
            local val = emu.read(0x6010 + i, emu.memType.nesMemory)
            logFile:write(string.format("$%04X (sector %2d): $%02X\n", 0x6010 + i, i, val))
        end
        
        logFile:close()
        emu.stop()
    end
end, emu.eventType.endFrame)
