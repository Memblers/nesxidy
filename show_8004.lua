-- Show instruction at $8004

local logPath = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\show_8004.log"
local logFile = io.open(logPath, "w")

local function readByte(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

-- Read more bytes at $8000-$80A0 (push16 and more)
logFile:write("Bytes at push16 ($8000) - looking for writes to $5A:\n")
for i = 0, 160 do
    local addr = 0x8000 + i
    local b = readByte(addr)
    -- Look for patterns that write to $5A: 85 5A (STA $5A), 86 5A (STX $5A), C6 5A (DEC $5A)
    local prevByte = 0
    if i > 0 then prevByte = readByte(addr - 1) end
    local marker = ""
    if b == 0x5A and (prevByte == 0x85 or prevByte == 0x86 or prevByte == 0xC6 or prevByte == 0xE6) then
        marker = " <-- WRITE TO $5A"
    end
    logFile:write(string.format("  $%04X: $%02X%s\n", addr, b, marker))
end

logFile:write("Done\n")
logFile:flush()
logFile:close()
emu.log("Wrote to show_8004.log")
