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
--         +$3C  ir_blocks_processed    u32
--         +$40  ir_bytes_before        u32
--         +$44  ir_bytes_after         u32
--         +$48  ir_nodes_killed        u32
--         +$4C  ir_pass_redundant_load u16  (Pass 1: RL+identity+fold)
--         +$4E  ir_pass_dead_store     u16  (Pass 2: dead store+store-back)
--         +$50  ir_pass_dead_load      u16  (Pass 2b)
--         +$52  ir_pass_php_plp        u16  (Pass 3)
--         +$54  ir_pass_pair_rewrite   u16  (Pass 4: pair rewrite + CMP#0)
--         +$56  ir_pass_rmw_fusion    u16  (Pass 5+6: shift/inc/dec fusion)

local BASE = 0x7E30   -- CPU address (WRAM $6000-$7FFF mapped)
local MEM  = emu.memType.nesDebug  -- side-effect-free reads

-- helpers
local function r8(off)
  return emu.read(BASE + off, MEM)
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
local COL_FPS_G = 0x00FF00     -- green  (>= 55 fps)
local COL_FPS_Y = 0xFFFF00     -- yellow (>= 40 fps)
local COL_FPS_R = 0xFF4040     -- red    (<  40 fps)

-- Emulated-FPS measurement state
-- Tracks delta of total_frames (WRAM +$34) over wall-clock time.
-- total_frames is incremented by the emulated program each time it
-- completes a frame (calls metrics_dump_runtime_b2 from render_video).
local efps_clock_prev   = os.clock()
local efps_frames_prev  = 0        -- last sampled total_frames value
local efps_display      = 0.0      -- displayed emulated FPS
local efps_ready        = false    -- waiting for first valid sample
local EFPS_INTERVAL     = 1.0      -- update every ~1 second for stability

emu.addEventCallback(function()
  -- --- Emulated FPS: delta(total_frames) / delta(wall-clock) ---
  local now = os.clock()
  local dt  = now - efps_clock_prev

  -- Read total_frames from metrics WRAM block (only valid after magic)
  local have_magic = (r8(0x39) == 0x4D and r8(0x3A) == 0x45)
  local cur_frames = 0
  if have_magic then cur_frames = r32(0x34) end

  if dt >= EFPS_INTERVAL then
    if efps_ready and have_magic then
      local delta = cur_frames - efps_frames_prev
      if delta >= 0 and dt > 0 then
        efps_display = delta / dt
      end
    end
    efps_clock_prev  = now
    efps_frames_prev = cur_frames
    efps_ready       = have_magic  -- arm for next interval
  end

  -- Pick colour based on emulated FPS
  local fps_col = COL_FPS_R
  if efps_display >= 55 then fps_col = COL_FPS_G
  elseif efps_display >= 40 then fps_col = COL_FPS_Y end

  -- Always draw emulated FPS even before metrics are ready
  if efps_ready then
    emu.drawString(170, 2, string.format("emu %.1f FPS", efps_display), fps_col, COL_BG)
  else
    emu.drawString(170, 2, "emu -- FPS", COL_FPS_Y, COL_BG)
  end

  -- Check magic signature
  if r8(0x39) ~= 0x4D or r8(0x3A) ~= 0x45 then
    emu.drawString(2, 2, "Metrics: no data (waiting for frame dump)", COL_WARN, COL_BG)
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
  emu.drawString(2, y, string.format("Flash: %d%%", flash_p), COL_RT, COL_BG); y = y + 12

  -- IR optimisation metrics
  local ir_blocks  = r32(0x3C)
  local ir_before  = r32(0x40)
  local ir_after   = r32(0x44)
  local ir_killed  = r32(0x48)
  local ir_rl      = r16(0x4C)
  local ir_ds      = r16(0x4E)
  local ir_dl      = r16(0x50)
  local ir_pp      = r16(0x52)
  local ir_pr      = r16(0x54)
  local ir_rmw     = r16(0x56)

  local ir_saved   = ir_before - ir_after
  local ir_pct     = 0
  if ir_before > 0 then ir_pct = ir_saved * 100 / ir_before end

  local COL_IR   = 0xFF80FF  -- magenta/pink
  local COL_IR2  = 0xCC66CC  -- dim magenta for detail lines

  emu.drawString(2, y, "=== IR Optimizer ===", COL_TITLE, COL_BG); y = y + 10

  if ir_blocks > 0 then
    local avg_before = ir_before / ir_blocks
    local avg_after  = ir_after  / ir_blocks
    local avg_saved  = avg_before - avg_after
    emu.drawString(2, y, string.format("Blocks: %d  Nodes killed: %d", ir_blocks, ir_killed), COL_IR, COL_BG); y = y + 9
    emu.drawString(2, y, string.format("Total: %dB -> %dB  (-%dB, %.1f%%)", ir_before, ir_after, ir_saved, ir_pct), COL_IR, COL_BG); y = y + 9
    emu.drawString(2, y, string.format("Avg/blk: %.1fB -> %.1fB  (-%.1fB)", avg_before, avg_after, avg_saved), COL_IR2, COL_BG); y = y + 9
  else
    emu.drawString(2, y, "No IR blocks yet", COL_IR2, COL_BG); y = y + 9
  end

  local ir_total_changes = ir_rl + ir_ds + ir_dl + ir_pp + ir_pr + ir_rmw
  emu.drawString(2, y, string.format("Pass hits (%d total):", ir_total_changes), COL_IR, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("  RedundLoad:%d DeadStore:%d DeadLoad:%d", ir_rl, ir_ds, ir_dl), COL_IR2, COL_BG); y = y + 9
  emu.drawString(2, y, string.format("  PHP/PLP:%d PairRwrt:%d RMW:%d", ir_pp, ir_pr, ir_rmw), COL_IR2, COL_BG); y = y + 9

end, emu.eventType.endFrame)
