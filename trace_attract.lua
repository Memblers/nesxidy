-- trace_attract.lua  —  Mesen 2 Lua script
-- Samples the guest (Exidy) PC from ZP $5B/$5C every NMI and logs
-- time spent at key addresses to diagnose attract-mode delay loops.
--
-- Key addresses (Side Trac):
--   $29CC-$29D8  Bare delay loop (65536 iterations)
--   $29C5-$29CB  Multi-pass delay wrapper
--   $2E58-$2E6D  Attract delay with coin poll
--   $2E33-$2E55  Attract demo loop (gameplay demo)
--   $2CB0        Coin check subroutine
--   $2B0E        IRQ handler
--   $2D89        Draw title screen
--
-- Usage: load in Mesen 2 Debug > Script Window, then run the ROM.
--        Every ~60 NMI frames it prints a summary of where guest PC
--        was during those frames.

local PC_LO   = 0x5B   -- ZP: _pc low byte
local PC_HI   = 0x5C   -- ZP: _pc high byte
local CLK_LO  = 0x49   -- ZP: _clockticks6502 low byte

local frame_count = 0
local sample_interval = 60  -- report every N NMI frames (~1 second)
local start_time = os.clock()

-- Histogram: count how many NMI frames the guest PC falls in each range
local bins = {}
local function bin_name(gpc)
    if gpc >= 0x29CC and gpc <= 0x29D8 then return "delay_bare($29CC)"
    elseif gpc >= 0x29C5 and gpc <= 0x29CB then return "delay_wrap($29C5)"
    elseif gpc >= 0x2E58 and gpc <= 0x2E6D then return "attract_delay($2E58)"
    elseif gpc >= 0x2E33 and gpc <= 0x2E55 then return "attract_demo($2E33)"
    elseif gpc >= 0x2E02 and gpc <= 0x2E32 then return "attract_init($2E02)"
    elseif gpc >= 0x2CB0 and gpc <= 0x2CFF then return "coin_check($2CB0)"
    elseif gpc >= 0x2B0E and gpc <= 0x2B62 then return "irq_handler($2B0E)"
    elseif gpc >= 0x2D89 and gpc <= 0x2E01 then return "draw_title($2D89)"
    else return string.format("other($%04X)", gpc)
    end
end

emu.addEventCallback(function()
    -- Read guest PC from ZP
    local lo = emu.read(PC_LO, emu.memType.nesInternalRam)
    local hi = emu.read(PC_HI, emu.memType.nesInternalRam)
    local gpc = hi * 256 + lo

    local name = bin_name(gpc)
    bins[name] = (bins[name] or 0) + 1
    frame_count = frame_count + 1

    if frame_count >= sample_interval then
        local elapsed = os.clock() - start_time
        emu.log(string.format("=== Guest PC sample (%.1fs elapsed, %d frames) ===", elapsed, frame_count))
        -- Sort by count descending
        local sorted = {}
        for k, v in pairs(bins) do
            table.insert(sorted, {name = k, count = v})
        end
        table.sort(sorted, function(a, b) return a.count > b.count end)
        for _, entry in ipairs(sorted) do
            emu.log(string.format("  %3d frames: %s", entry.count, entry.name))
        end
        bins = {}
        frame_count = 0
        start_time = os.clock()
    end
end, emu.eventType.nmi)

emu.log("trace_attract.lua loaded — sampling guest PC every NMI frame")
