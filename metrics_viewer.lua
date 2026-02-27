-- metrics_viewer.lua  —  Mesen 2 Lua script
-- Reads the metrics WRAM block at $7E30 and draws an overlay.
--
-- Usage:  In Mesen → Script Window → Open this file → Run
--
-- WRAM layout (written by metrics.c):
--   $7E30 +$00  bfs_addresses_visited  u16
--         +$02  bfs_entry_points       u16
--         +$04  bitmap_marked          u16
--         +$06  blocks_compiled        u16
--         +$08  blocks_failed          u16
--         +$0A  blocks_skipped         u16
--         +$0C  code_bytes_written     u16
--         +$0E  flash_sectors_used     u16
--         +$10  bfs_time  (cycles)     u32
--         +$14  cache_hits             u32
--         +$18  cache_misses           u32
--         +$1C  cache_interpret        u32
--         +$20  opt2_stat_total        u32  (branches entered optimizer)
--         +$24  opt2_stat_direct       u32  (successfully patched)
--         +$28  peephole_php_elided    u32
--         +$2C  peephole_plp_elided    u32
--         +$30  dynamic_compile_frames u32
--         +$34  total_frames           u32
--         +$38  flash_occupancy_pct    u8
--         +$39  magic 'M' 'E'

local BASE = 0x7E30   -- CPU address (WRAM $6000-$7FFF mapped)

-- helpers
local function r8(off)
  return emu.read(BASE + off, emu.memType.nesMemory)
end
local function r16(off)
  return r8(off) + r8(off+1) * 256
end
local function r32(off)
  return r16(off) + r16(off+2) * 65536
end

-- colours  (plain RGB for Mesen 2 drawString)
local COL_BG    = 0x000020     -- dark blue background
local COL_TITLE = 0x00FFFF     -- cyan titles
local COL_SA    = 0x00FF00     -- bright green
local COL_RT    = 0xFFFF00     -- yellow
local COL_WARN  = 0xFF2020     -- bright red

emu.addEventCallback(function()
  -- Check magic signature
  if r8(0x39) ~= 0x4D or r8(0x3A) ~= 0x45 then
    emu.drawString(2, 2, "Metrics: no data", COL_WARN, COL_BG)
    return
  end

  local y = 2

  -- SA metrics (new layout)
  local bfs_addrs    = r16(0x00)
  local bfs_entries  = r16(0x02)
  local bitmap_total = r16(0x04)
  local blk_compiled = r16(0x06)
  local blk_failed   = r16(0x08)
  local blk_skipped  = r16(0x0A)
  local code_bytes   = r16(0x0C)
  local sectors_used = r16(0x0E)
  local bfs_time     = r32(0x10)

  emu.drawString(2, y, "=== Static Analysis ===", COL_TITLE, COL_BG); y = y + 10
  emu.drawString(2, y, string.format("BFS visit:%d entry:%d", bfs_addrs, bfs_entries), COL_SA, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Bitmap:   %d addrs", bitmap_total), COL_SA, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Compiled: %d fail:%d skip:%d", blk_compiled, blk_failed, blk_skipped), COL_SA, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Code: %dB  Sectors:%d", code_bytes, sectors_used), COL_SA, COL_BG); y = y + 12

  -- Runtime metrics
  local hits    = r32(0x14)
  local misses  = r32(0x18)
  local recomp  = r32(0x1C)
  local opt_total = r32(0x20)
  local opt_patch = r32(0x24)
  local ph_php  = r32(0x28)
  local ph_plp  = r32(0x2C)
  local dyn_fr  = r32(0x30)
  local tot_fr  = r32(0x34)
  local flash_p = r8(0x38)

  local hit_pct = 0
  if (hits + misses) > 0 then
    hit_pct = hits * 100 / (hits + misses)
  end

  local col_rate = COL_RT
  if hit_pct < 90 then col_rate = COL_WARN end

  emu.drawString(2, y, "=== Runtime ===", COL_TITLE, COL_BG); y = y + 10
  emu.drawString(2, y, string.format("Hits: %d  Miss: %d", hits, misses), col_rate, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Hit%%: %.1f%%", hit_pct), col_rate, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Interpret:  %d", recomp), COL_RT, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Opt total:%d patched:%d", opt_total, opt_patch), COL_RT, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Peephole PHP:%d PLP:%d", ph_php, ph_plp), COL_RT, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("DynFrames:%d Tot:%d", dyn_fr, tot_fr), COL_RT, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("Flash: %d%%", flash_p), COL_RT, COL_BG); y = y + 9

end, emu.eventType.endFrame)
