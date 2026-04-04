-- metrics_viewer.lua  —  Mesen 2 Lua script
-- Reads metrics directly from struct/global addresses resolved via .mlb.
--
-- Usage:  In Mesen → Script Window → Open this file → Run
--
-- Addresses are auto-resolved from the game's .mlb label file so the
-- script stays correct across rebuilds.  No hardcoded WRAM offsets.

---------------------------------------------------------------------
-- Configuration  —  CHANGE GAME TO MATCH YOUR BUILD
---------------------------------------------------------------------
local GAME = "millipede"
local PROJECT_DIR = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\"
local MEM = emu.memType.nesMemory

---------------------------------------------------------------------
-- .mlb parser (same approach as trace_logger.lua / hit_rate.lua)
---------------------------------------------------------------------
local mlb_path = PROJECT_DIR .. GAME .. ".mlb"

local function find_addr_in_mlb(path, label)
  local f = io.open(path, "r")
  if not f then return nil end
  for line in f:lines() do
    local prefix, hex, name = line:match("^(%a):(%x+):(.+)$")
    if name == label then
      f:close()
      local addr = tonumber(hex, 16)
      if prefix == "S" or prefix == "W" then return addr + 0x6000
      elseif prefix == "P"              then return addr + 0x8000
      else                                   return addr
      end
    end
  end
  f:close()
  return nil
end

-- Validate the .mlb exists
do
  local f = io.open(mlb_path, "r")
  if not f then
    emu.displayMessage("Metrics", "ERROR: cannot open " .. GAME .. ".mlb")
    return
  end
  f:close()
end

local function sym(label)
  return find_addr_in_mlb(mlb_path, label)
end

---------------------------------------------------------------------
-- Resolve all addresses once at startup
---------------------------------------------------------------------
-- sa_metrics_t struct base (offsets match core/metrics.h)
local SA = sym("_sa_metrics")          -- base of sa_metrics_t
-- runtime_metrics_t struct base
local RT = sym("_runtime_metrics")     -- base of runtime_metrics_t
-- Standalone globals (not inside the structs)
local CACHE_HITS      = sym("_cache_hits")
local CACHE_MISSES    = sym("_cache_misses")
local CACHE_INTERPRET = sym("_cache_interpret")
local OPT2_TOTAL     = sym("_opt2_stat_total")
local OPT2_DIRECT    = sym("_opt2_stat_direct")
local OPT2_XBANK    = sym("_opt2_diag_xbank")
local OPT2_ALIGN    = sym("_opt2_diag_align")
local OPT2_SELFLOOP = sym("_opt2_diag_selfloop")
local OPT2_NOCOMP   = sym("_opt2_diag_nocompile")
-- Idle detection (standalone globals, not inside a struct)
local SA_IDLE_COUNT       = sym("_sa_idle_count")
local SA_IDLE_CACHE_COUNT = sym("_sa_idle_cache_count")
local SA_IDLE_CACHE       = sym("_sa_idle_cache")

if not SA or not RT then
  emu.displayMessage("Metrics", "ERROR: _sa_metrics/_runtime_metrics not in " .. mlb_path)
  return
end

emu.log(string.format("[metrics] mlb=%s  SA=$%04X  RT=$%04X  hits=$%04X  miss=$%04X",
  mlb_path, SA, RT, CACHE_HITS or 0, CACHE_MISSES or 0))

---------------------------------------------------------------------
-- Memory read helpers
---------------------------------------------------------------------
local function r8(addr)
  if not addr then return 0 end
  return emu.read(addr, MEM)
end
local function r16(addr)
  if not addr then return 0 end
  return emu.read(addr, MEM) + emu.read(addr + 1, MEM) * 256
end
local function r32(addr)
  if not addr then return 0 end
  return r16(addr) + r16(addr + 2) * 65536
end

---------------------------------------------------------------------
-- Colours
---------------------------------------------------------------------
local COL_BG    = 0x000020
local COL_TITLE = 0x00FFFF
local COL_SA    = 0x00FF00
local COL_RT    = 0xFFFF00
local COL_WARN  = 0xFF2020
local COL_FPS_G = 0x00FF00
local COL_FPS_Y = 0xFFFF00
local COL_FPS_R = 0xFF4040
local COL_IR    = 0xFF80FF
local COL_IR2   = 0xCC66CC
local COL_IDLE  = 0x80FF80
local COL_IDLE2 = 0x60CC60

---------------------------------------------------------------------
-- Emulated-FPS measurement
---------------------------------------------------------------------
local efps_clock_prev   = os.clock()
local efps_frames_prev  = 0
local efps_display      = 0.0
local efps_ready        = false
local EFPS_INTERVAL     = 1.0

---------------------------------------------------------------------
-- sa_metrics_t field offsets (all packed, no padding on 6502)
---------------------------------------------------------------------
local SA_bfs_addresses_visited = 0   -- u16
local SA_bfs_entry_points      = 2   -- u16
local SA_bitmap_marked         = 4   -- u16
local SA_blocks_compiled       = 6   -- u16
local SA_blocks_failed         = 8   -- u16
local SA_blocks_skipped        = 10  -- u16
local SA_code_bytes_written    = 12  -- u16
local SA_flash_sectors_used    = 14  -- u16
local SA_bfs_start_cycle       = 16  -- u32
local SA_bfs_end_cycle         = 20  -- u32

---------------------------------------------------------------------
-- runtime_metrics_t field offsets
---------------------------------------------------------------------
local RT_optimizer_runs         = 0   -- u32
local RT_opt_branches_patched   = 4   -- u32
local RT_peephole_php_elided    = 8   -- u32
local RT_peephole_plp_elided    = 12  -- u32
local RT_dynamic_compile_frames = 16  -- u32
local RT_total_frames           = 20  -- u32
local RT_flash_free_bytes       = 24  -- u16
local RT_flash_occupancy_pct    = 26  -- u8
local RT_ir_blocks_processed    = 27  -- u32
local RT_ir_nodes_killed        = 31  -- u32
local RT_ir_bytes_before        = 35  -- u32
local RT_ir_bytes_after         = 39  -- u32
local RT_ir_pass_redundant_load = 43  -- u16
local RT_ir_pass_dead_store     = 45  -- u16
local RT_ir_pass_dead_load      = 47  -- u16
local RT_ir_pass_php_plp        = 49  -- u16
local RT_ir_pass_pair_rewrite   = 51  -- u16
local RT_ir_pass_rmw_fusion     = 53  -- u16
local RT_ir_instrs_eliminated   = 55  -- u16
local RT_ir_instr_overflow      = 57  -- u16

emu.addEventCallback(function()
  -- --- Emulated FPS ---
  local now = os.clock()
  local dt  = now - efps_clock_prev
  local tot_fr = r32(RT + RT_total_frames)
  local cur_frames = tot_fr

  if dt >= EFPS_INTERVAL then
    if efps_ready then
      local delta = cur_frames - efps_frames_prev
      if delta >= 0 and dt > 0 then
        efps_display = delta / dt
      end
    end
    efps_clock_prev  = now
    efps_frames_prev = cur_frames
    efps_ready       = true
  end

  local fps_col = COL_FPS_R
  if efps_display >= 55 then fps_col = COL_FPS_G
  elseif efps_display >= 40 then fps_col = COL_FPS_Y end

  if efps_ready then
    emu.drawString(170, 2, string.format("emu %.1f FPS", efps_display), fps_col, COL_BG)
  else
    emu.drawString(170, 2, "emu -- FPS", COL_FPS_Y, COL_BG)
  end

  local y = 2

  -- === Static Analysis (BFS — always runs) ===
  local bfs_addrs    = r16(SA + SA_bfs_addresses_visited)
  local bfs_entries  = r16(SA + SA_bfs_entry_points)

  emu.drawString(2, y, "=== Static Analysis (BFS) ===", COL_TITLE, COL_BG); y = y + 10
  emu.drawString(2, y, string.format("BFS visit:%d entry:%d", bfs_addrs, bfs_entries), COL_SA, COL_BG); y = y + 9

  -- === Compile Stats (warm reboot only) ===
  local bitmap_total = r16(SA + SA_bitmap_marked)
  local blk_compiled = r16(SA + SA_blocks_compiled)
  local blk_failed   = r16(SA + SA_blocks_failed)
  local blk_skipped  = r16(SA + SA_blocks_skipped)
  local code_bytes   = r16(SA + SA_code_bytes_written)
  local sectors_used = r16(SA + SA_flash_sectors_used)
  local sa_compiled   = (bitmap_total + blk_compiled + code_bytes) > 0

  if sa_compiled then
    emu.drawString(2, y, "=== Compile (SA) ===", COL_TITLE, COL_BG); y = y + 10
    emu.drawString(2, y, string.format("Bitmap:   %d addrs", bitmap_total), COL_SA, COL_BG); y = y + 9
    emu.drawString(2, y, string.format("Compiled: %d fail:%d skip:%d", blk_compiled, blk_failed, blk_skipped), COL_SA, COL_BG); y = y + 9
    emu.drawString(2, y, string.format("Code: %dB  Sectors:%d", code_bytes, sectors_used), COL_SA, COL_BG); y = y + 9
  else
    emu.drawString(2, y, "Compile: skipped (cold boot)", 0x808080, COL_BG); y = y + 9
  end

  -- === Runtime ===
  emu.drawString(2, y, "=== Runtime ===", COL_TITLE, COL_BG); y = y + 10
  emu.drawString(2, y, string.format("Frames: %d", tot_fr), COL_RT, COL_BG); y = y + 9

  -- JIT dispatch stats (only non-zero when NOT in interpreter-only mode)
  local hits    = CACHE_HITS    and r32(CACHE_HITS) or 0
  local misses  = CACHE_MISSES  and r32(CACHE_MISSES) or 0
  local interp  = CACHE_INTERPRET and r32(CACHE_INTERPRET) or 0
  local jit_active = (hits + misses + interp) > 0

  if jit_active then
    local hit_pct = 0
    if (hits + misses) > 0 then hit_pct = hits * 100 / (hits + misses) end
    local col_rate = COL_RT
    if hit_pct < 90 then col_rate = COL_WARN end
    emu.drawString(2, y, string.format("Hits:%d Miss:%d (%.1f%%)", hits, misses, hit_pct), col_rate, COL_BG); y = y + 9
    emu.drawString(2, y, string.format("Interpret: %d", interp), COL_RT, COL_BG); y = y + 9

    local opt_total = OPT2_TOTAL  and r16(OPT2_TOTAL) or 0
    local opt_patch = OPT2_DIRECT and r16(OPT2_DIRECT) or 0
    local ph_php  = r32(RT + RT_peephole_php_elided)
    local ph_plp  = r32(RT + RT_peephole_plp_elided)
    local dyn_fr  = r32(RT + RT_dynamic_compile_frames)
    local flash_p = r8(RT + RT_flash_occupancy_pct)
    emu.drawString(2, y, string.format("Opt:%d patch:%d", opt_total, opt_patch), COL_RT, COL_BG); y = y + 9
    -- Diagnostic breakdown of unpatched (only if any diagnostics recorded)
    local d_xbank = OPT2_XBANK    and r16(OPT2_XBANK) or 0
    local d_align = OPT2_ALIGN    and r16(OPT2_ALIGN) or 0
    local d_sloop = OPT2_SELFLOOP and r16(OPT2_SELFLOOP) or 0
    local d_nocomp = OPT2_NOCOMP  and r16(OPT2_NOCOMP) or 0
    if (d_xbank + d_align + d_sloop + d_nocomp) > 0 then
      emu.drawString(2, y, string.format("xbnk:%d aln:%d loop:%d nc:%d", d_xbank, d_align, d_sloop, d_nocomp), 0xAAAA00, COL_BG); y = y + 9
    end
    emu.drawString(2, y, string.format("Peephole PHP:%d PLP:%d", ph_php, ph_plp), COL_RT, COL_BG); y = y + 9
    emu.drawString(2, y, string.format("DynFrames:%d Flash:%d%%", dyn_fr, flash_p), COL_RT, COL_BG); y = y + 9

    -- === IR Optimizer ===
    local ir_blocks  = r32(RT + RT_ir_blocks_processed)
    local ir_before  = r32(RT + RT_ir_bytes_before)
    local ir_after   = r32(RT + RT_ir_bytes_after)
    local ir_killed  = r32(RT + RT_ir_nodes_killed)

    emu.drawString(2, y, "=== IR Optimizer ===", COL_TITLE, COL_BG); y = y + 10
    if ir_blocks > 0 then
      local ir_saved = ir_before - ir_after
      local ir_pct   = 0
      if ir_before > 0 then ir_pct = ir_saved * 100 / ir_before end
      emu.drawString(2, y, string.format("Blocks:%d Killed:%d", ir_blocks, ir_killed), COL_IR, COL_BG); y = y + 9
      emu.drawString(2, y, string.format("%dB->%dB (-%dB %.1f%%)", ir_before, ir_after, ir_saved, ir_pct), COL_IR, COL_BG); y = y + 9

      local ir_rl  = r16(RT + RT_ir_pass_redundant_load)
      local ir_ds  = r16(RT + RT_ir_pass_dead_store)
      local ir_dl  = r16(RT + RT_ir_pass_dead_load)
      local ir_pp  = r16(RT + RT_ir_pass_php_plp)
      local ir_pr  = r16(RT + RT_ir_pass_pair_rewrite)
      local ir_rmw = r16(RT + RT_ir_pass_rmw_fusion)
      emu.drawString(2, y, string.format("RL:%d DS:%d DL:%d PP:%d PR:%d RMW:%d",
        ir_rl, ir_ds, ir_dl, ir_pp, ir_pr, ir_rmw), COL_IR2, COL_BG); y = y + 9
    else
      emu.drawString(2, y, "No IR blocks", COL_IR2, COL_BG); y = y + 9
    end
  else
    emu.drawString(2, y, "JIT: off (interpreter only)", 0x808080, COL_BG); y = y + 9
  end

  -- === Idle Detection (at bottom — can have many PCs) ===
  local idle_detect = SA_IDLE_COUNT       and r8(SA_IDLE_COUNT) or 0
  local idle_cache  = SA_IDLE_CACHE_COUNT and r8(SA_IDLE_CACHE_COUNT) or 0

  emu.drawString(2, y, "=== Idle Detection ===", COL_TITLE, COL_BG); y = y + 10
  emu.drawString(2, y, string.format("Auto-detected: %d  Cached: %d", idle_detect, idle_cache), COL_IDLE, COL_BG); y = y + 9

  if idle_cache > 0 and SA_IDLE_CACHE then
    local pc_strs = {}
    for i = 0, idle_cache - 1 do
      local pc = r16(SA_IDLE_CACHE + i * 2)
      if pc ~= 0 then
        table.insert(pc_strs, string.format("$%04X", pc))
      end
    end
    if #pc_strs > 0 then
      for row = 1, #pc_strs, 4 do
        local line = "  "
        for col = row, math.min(row + 3, #pc_strs) do
          line = line .. pc_strs[col] .. " "
        end
        emu.drawString(2, y, line, COL_IDLE2, COL_BG); y = y + 9
      end
    end
  end

end, emu.eventType.endFrame)
