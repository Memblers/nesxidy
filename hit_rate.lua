-- hit_rate.lua  —  Mesen 2 Lua script
-- Real-time OSD: rolling hit% of the last 128 dispatch_on_pc outcomes.
--
-- "Hit"  = dispatch_on_pc found compiled native code and executed it
-- "Miss" = dispatch_on_pc fell back to C for interpret or recompile
--
-- Hooks two addresses in the dispatch asm (from vicemap.map).
-- Update these after rebuilding the ROM:
local DISPATCH_RETURN = 0x62AF   -- _flash_dispatch_return_no_regs (common hit path)
local NOT_RECOMPILED  = 0x62E4   -- .not_recompiled (all miss paths converge here)

-- ── Ring buffer of last N dispatch outcomes ──────────────────────────
local RING = 128
local buf = {}
for i = 1, RING do buf[i] = 0 end   -- 0=empty, 1=hit, 2=miss
local pos    = 1
local hits   = 0
local filled = 0

local function record(kind)
    if buf[pos] == 1 then hits = hits - 1 end   -- evict old hit
    buf[pos] = kind
    if kind == 1 then hits = hits + 1 end
    pos = pos % RING + 1
    if filled < RING then filled = filled + 1 end
end

-- ── Exec callbacks ───────────────────────────────────────────────────
-- Hit: _flash_dispatch_return falls through to _flash_dispatch_return_no_regs,
-- so one hook at the common address catches both entry points.
emu.addMemoryCallback(function() record(1) end,
    emu.callbackType.exec, DISPATCH_RETURN, DISPATCH_RETURN)

-- Miss: dispatch_on_pc branches to not_recompiled for all non-compiled PCs
-- ($00=uninitialized, bit7=not compiled, INTERPRETED-only).
emu.addMemoryCallback(function() record(2) end,
    emu.callbackType.exec, NOT_RECOMPILED, NOT_RECOMPILED)

-- ── OSD overlay ──────────────────────────────────────────────────────
local GREEN  = 0x00FF00
local YELLOW = 0xFFFF00
local RED    = 0xFF4040
local BG     = 0x000020
local GREY   = 0x303030
local BORDER = 0x808080

emu.addEventCallback(function()
    if filled == 0 then return end

    local pct = hits * 100 / filled
    local col = GREEN
    if pct < 90 then col = YELLOW end
    if pct < 70 then col = RED end

    -- Small bar (64px wide)
    local bx, by, bw, bh = 2, 214, 64, 5
    local fw = math.floor(pct * bw / 100)
    emu.drawRectangle(bx, by, bw, bh, GREY, true)
    emu.drawRectangle(bx, by, fw, bh, col, true)
    emu.drawRectangle(bx, by, bw, bh, BORDER, false)

    -- Text
    emu.drawString(2, 221, string.format("Hit:%d/%d %.0f%%", hits, filled, pct), col, BG)

end, emu.eventType.endFrame)

emu.displayMessage("HitRate", "Tracking last " .. RING .. " dispatches")
