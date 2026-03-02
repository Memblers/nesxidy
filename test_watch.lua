-- test_watch.lua  —  Mesen 2 Lua script to monitor cpu_6502_test results
--
-- Guest Exidy RAM at $0000-$03FF is mapped to NES WRAM at RAM_BASE.
-- We resolve RAM_BASE from the MLB file, then read guest addresses
-- through it.
--
-- Watches guest $003F for the done marker:
--   $AA = all tests passed
--   $55 = failure (error code at guest $0020, context at $0040-$0048)
--
-- Usage: Mesen.exe exidy.nes --lua test_watch.lua

local PROJECT_DIR = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\"

-- Resolve RAM_BASE from MLB
local function find_wram_addr(filename, label)
    local f = io.open(PROJECT_DIR .. filename, "r")
    if not f then return nil end
    for line in f:lines() do
        local prefix, hex, name = line:match("^(%a):(%x+):(.+)$")
        if name == label then
            f:close()
            local addr = tonumber(hex, 16)
            if prefix == "S" or prefix == "W" then return addr + 0x6000
            elseif prefix == "R"              then return addr
            else                                   return addr
            end
        end
    end
    f:close()
    return nil
end

local RAM_BASE = find_wram_addr("exidy.mlb", "_RAM_BASE") or 0x6D00

emu.displayMessage("Test", "test_watch.lua loaded")
emu.log("=== test_watch.lua loaded ===")
emu.log(string.format("  RAM_BASE = $%04X", RAM_BASE))

-- Helper: read guest Exidy RAM address via NES debug reads
local function guest_read(guest_addr)
    return emu.read(RAM_BASE + guest_addr, emu.memType.nesDebug)
end

local done = false
local check_count = 0

-- Check every frame
emu.addEventCallback(function()
    if done then return end
    check_count = check_count + 1

    local marker = guest_read(0x3F)

    if marker == 0xAA then
        done = true
        emu.log("*** ALL TESTS PASSED (guest $003F = $AA) ***")
        emu.displayMessage("Test", "ALL TESTS PASSED!")
        local legacy = guest_read(0x20)
        emu.log(string.format("  guest $0020 = $%02X (should be $AA)", legacy))
        emu.breakExecution()

    elseif marker == 0x55 then
        done = true
        local fail_code = guest_read(0x20)
        emu.log(string.format("*** TEST FAILED: code $%02X ***", fail_code))
        emu.displayMessage("Test", string.format("FAIL: test $%02X", fail_code))

        -- Dump context
        emu.log(string.format("  Context: fail=$%02X X=$%02X Y=$%02X SP=$%02X",
            guest_read(0x40),
            guest_read(0x42),
            guest_read(0x43),
            guest_read(0x44)))
        emu.log(string.format("  Memory: $10=$%02X $11=$%02X $12=$%02X $13=$%02X",
            guest_read(0x45),
            guest_read(0x46),
            guest_read(0x47),
            guest_read(0x48)))

        -- Dump results table $0030-$003E
        local results = ""
        for i = 0x30, 0x3E do
            results = results .. string.format("%02X ", guest_read(i))
        end
        emu.log("  Results table ($0030-$003E): " .. results)

        emu.breakExecution()
    end

    -- Timeout after ~10 seconds (600 frames at 60fps)
    if check_count >= 600 and not done then
        done = true
        local marker_val = guest_read(0x3F)
        local fail_code = guest_read(0x20)
        emu.log(string.format("*** TIMEOUT: guest $003F=$%02X $0020=$%02X — tests did not complete ***",
            marker_val, fail_code))
        emu.displayMessage("Test", "TIMEOUT - tests hung!")

        -- Dump CPU state
        local state = emu.getState()
        emu.log(string.format("  CPU: PC=$%04X A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X",
            state["cpu.pc"] or 0,
            state["cpu.a"] or 0,
            state["cpu.x"] or 0,
            state["cpu.y"] or 0,
            state["cpu.sp"] or 0,
            state["cpu.ps"] or 0))

        emu.breakExecution()
    end
end, emu.eventType.endFrame)
