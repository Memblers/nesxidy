-- Quick script to check sector array at $0280-$029F
local frameCount = 0

emu.addEventCallback(function()
    frameCount = frameCount + 1
    if frameCount == 300 then
        -- Read debug area at $0240
        print("===== Debug Area at $0240 =====")
        for i = 0, 15 do
            local val = emu.read(0x0240 + i, emu.memType.cpuMemory)
            print(string.format("$0240+%02d ($%04X): $%02X", i, 0x0240 + i, val))
        end
        
        -- Read sector array at $0280
        print("\n===== Sector Array at $0280 =====")
        for i = 0, 29 do
            local val = emu.read(0x0280 + i, emu.memType.cpuMemory)
            if i % 10 == 0 then print() end
            io.stdout:write(string.format("%02X ", val))
        end
        print("\n")
        
        emu.stop()
    end
end, emu.eventType.endFrame)

print("Sector checker loaded - will dump at frame 300")
