--============================================================================
-- trace_logger.lua — Mesen 2 Lua script
-- Records the EMULATED (guest) PC to a file.
--
-- Auto-parses the game's .mlb label file to find correct addresses
-- for the current build.  Works with any game target.
--
-- Output format (compatible with analyze_*.py scripts):
--   ADDR  BB BB BB  MNE OPERAND     A:XX X:XX Y:XX S:XX P:FLAGS Fr:NNNNN
--
-- Usage:  1. Set GAME below to your game name (without .mlb extension)
--         2. Mesen → Script Window → Open this file → Run
--============================================================================

---------------------------------------------------------------------
-- Configuration  —  CHANGE THESE
---------------------------------------------------------------------
-- Game name: must match the .mlb file  (e.g. "nes_donkeykong")
local GAME = "llander"
-- Project directory (where .mlb files live)
local PROJECT_DIR = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\"

-- Output file
local OUTPUT_PATH = PROJECT_DIR .. "nes_trace_lua.txt"

-- Max lines (0 = unlimited)
local MAX_LINES   = 0

-- Buffer flush interval
local FLUSH_EVERY = 4096

---------------------------------------------------------------------
-- Parse the .mlb label file
---------------------------------------------------------------------
local MEM_DBG = emu.memType.nesDebug
local MEM_PRG = emu.memType.nesPrgRom

local mlb_path = PROJECT_DIR .. GAME .. ".mlb"
local symbols = {}   -- symbol_name → { prefix, hex_offset }

local function parse_mlb()
  local f = io.open(mlb_path, "r")
  if not f then
    emu.log("ERROR: Cannot open " .. mlb_path)
    return false
  end
  for line in f:lines() do
    -- Format: "P:1068:_exec6502" or "R:0051:_pc" or "S:028E:_dispatch_on_pc"
    local prefix, hex, name = line:match("^([PRSWG]):([0-9A-Fa-f]+):(.+)$")
    if prefix and hex and name then
      -- Only store first occurrence (some symbols appear as both S: and W:)
      if not symbols[name] then
        symbols[name] = { prefix = prefix, offset = tonumber(hex, 16) }
      end
    end
  end
  f:close()
  return true
end

-- Convert an MLB entry to a CPU address
local function mlb_to_cpu(sym)
  if not sym then return nil end
  local p = sym.prefix
  local off = sym.offset
  if p == "R" then
    return off                -- NES internal RAM → direct CPU address
  elseif p == "S" or p == "W" then
    return 0x6000 + off       -- SRAM/WRAM → $6000 + offset
  elseif p == "P" then
    -- PRG ROM offset → CPU address depends on bank
    -- Bank 0: $0000-$3FFF → $8000-$BFFF (when bank 0 mapped)
    -- Fixed bank 31: $7C000-$7FFFF → $C000-$FFFF
    if off >= 0x7C000 then
      return 0xC000 + (off - 0x7C000)  -- fixed bank
    else
      -- Banked: assume bank N maps to $8000-$BFFF
      return 0x8000 + (off % 0x4000)
    end
  end
  return nil
end

---------------------------------------------------------------------
-- Load symbols
---------------------------------------------------------------------
if not parse_mlb() then
  emu.displayMessage("Trace", "FAILED: cannot read " .. mlb_path)
  return
end

-- Extract addresses we need
local function get_cpu(name)
  local s = symbols[name]
  if not s then
    emu.log("WARN: symbol '" .. name .. "' not found in " .. mlb_path)
    return nil
  end
  return mlb_to_cpu(s)
end

local ZP_PC     = get_cpu("_pc")
local ZP_SP     = get_cpu("_sp")
local ZP_A      = get_cpu("_a")
local ZP_X      = get_cpu("_x")
local ZP_Y      = get_cpu("_y")
local ZP_STATUS = get_cpu("_status")
local MAPPER_PRG = get_cpu("_mapper_prg_bank")
local DISPATCH   = get_cpu("_dispatch_on_pc")
local STEP6502   = get_cpu("_step6502")
local INTERP6502 = get_cpu("_interpret_6502")

-- Validate critical symbols
if not ZP_PC then
  emu.log("FATAL: _pc not found — cannot trace")
  emu.displayMessage("Trace", "FATAL: _pc not found in " .. GAME .. ".mlb")
  return
end

local fmt = string.format
emu.log(fmt("Parsed %s.mlb: _pc=$%04X _a=$%04X _sp=$%04X dispatch=$%04X step=$%04X interp=$%04X",
            GAME, ZP_PC, ZP_A or 0, ZP_SP or 0, DISPATCH or 0, STEP6502 or 0, INTERP6502 or 0))

---------------------------------------------------------------------
-- Helpers
---------------------------------------------------------------------
local function rd(addr)
  if not addr then return 0 end
  return emu.read(addr, MEM_DBG)
end
local function rd16(addr)
  if not addr then return 0 end
  return rd(addr) + rd(addr + 1) * 256
end

---------------------------------------------------------------------
-- PRG ROM reader for guest disassembly
-- Reads the original ROM bytes, not whatever the mapper has mapped.
---------------------------------------------------------------------
local PRG_SIZE = nil

local function read_prg(guest_addr)
  -- Asteroids: CPU ROM $6800-$7FFF and vector ROM $5000-$57FF
  -- stored in NES PRG bank 23 (_rom_asteroids at bank offset 0,
  -- _rom_asteroids_vec at bank offset $1800)
  if GAME == "asteroids" then
    local BANK23 = 23 * 0x4000  -- PRG flat offset of bank 23
    if guest_addr >= 0x6800 and guest_addr <= 0x7FFF then
      local ok, val = pcall(emu.read, BANK23 + (guest_addr - 0x6800), MEM_PRG)
      if ok then return val end
    elseif guest_addr >= 0x5000 and guest_addr <= 0x57FF then
      local ok, val = pcall(emu.read, BANK23 + 0x1800 + (guest_addr - 0x5000), MEM_PRG)
      if ok then return val end
    end
    return nil
  end

  -- Lunar Lander: CPU ROM $6000-$7FFF (8KB) and vector ROM $4800-$5FFF (6KB)
  -- stored in NES PRG bank 23 (_rom_llander at bank offset 0,
  -- _rom_llander_vec at bank offset $2000)
  if GAME == "llander" then
    local BANK23 = 23 * 0x4000  -- PRG flat offset of bank 23
    if guest_addr >= 0x6000 and guest_addr <= 0x7FFF then
      local ok, val = pcall(emu.read, BANK23 + (guest_addr - 0x6000), MEM_PRG)
      if ok then return val end
    elseif guest_addr >= 0x4800 and guest_addr <= 0x5FFF then
      local ok, val = pcall(emu.read, BANK23 + 0x2000 + (guest_addr - 0x4800), MEM_PRG)
      if ok then return val end
    end
    return nil
  end

  if guest_addr < 0x8000 then return nil end

  if not PRG_SIZE then
    -- Detect PRG size by probing
    local ok32, _ = pcall(emu.read, 0x7FFF, MEM_PRG)
    PRG_SIZE = ok32 and 0x8000 or 0x4000
  end

  -- For mapper 30 with 32 banks of 16KB:
  -- The guest ROM image occupies the full 512KB flash.
  -- Guest addresses $8000-$FFFF: the original ROM is typically
  -- stored in the last N banks.  For NROM (32K PRG), it's in the
  -- last 2 banks (bank 30=$8000-$BFFF, bank 31=$C000-$FFFF).
  -- The recompiler stores the original ROM in the last banks.
  --
  -- For now: read from flat PRG with the standard NROM mapping:
  --   $C000-$FFFF → last 16KB of PRG (offset = PRG_total - $4000 + addr-$C000)
  --   $8000-$BFFF → second-to-last 16KB (or first 16KB for NROM-128)
  -- This works for the guest ROM but NOT for recompiled code.

  -- Total PRG size for mapper 30 = 512KB = $80000
  local total_prg = 0x80000  -- 512KB flash
  if guest_addr >= 0xC000 then
    -- Fixed bank (last 16KB)
    local prg_off = total_prg - 0x4000 + (guest_addr - 0xC000)
    local ok, val = pcall(emu.read, prg_off, MEM_PRG)
    if ok then return val end
  elseif guest_addr >= 0x8000 then
    -- For NROM: second-to-last bank
    local prg_off = total_prg - 0x8000 + (guest_addr - 0x8000)
    local ok, val = pcall(emu.read, prg_off, MEM_PRG)
    if ok then return val end
  end
  return nil
end

---------------------------------------------------------------------
-- Pre-compute flag strings
---------------------------------------------------------------------
local FLAG_CACHE = {}
do
  local function btest(v, b) return math.floor(v / (2^b)) % 2 == 1 end
  local L = {[7]={"N","n"},[6]={"V","v"},[5]={"U","u"},[4]={"B","b"},
             [3]={"D","d"},[2]={"I","i"},[1]={"Z","z"},[0]={"C","c"}}
  for ps = 0, 255 do
    local s = ""
    for b = 7, 0, -1 do
      s = s .. (btest(ps,b) and L[b][1] or L[b][2])
    end
    FLAG_CACHE[ps] = s
  end
end

---------------------------------------------------------------------
-- 6502 opcode table
---------------------------------------------------------------------
local IMP,ACC,IMM,ZP,ZPX,ZPY = 0,1,2,3,4,5
local IZX,IZY,ABS,ABX,ABY,IND,REL = 6,7,8,9,10,11,12
local SZ = {[0]=1,[1]=1,[2]=2,[3]=2,[4]=2,[5]=2,[6]=2,[7]=2,[8]=3,[9]=3,[10]=3,[11]=3,[12]=2}
local ops = {
  [0x00]={"BRK",IMP},[0x01]={"ORA",IZX},[0x05]={"ORA",ZP},[0x06]={"ASL",ZP},
  [0x08]={"PHP",IMP},[0x09]={"ORA",IMM},[0x0A]={"ASL",ACC},[0x0D]={"ORA",ABS},
  [0x0E]={"ASL",ABS},[0x10]={"BPL",REL},[0x11]={"ORA",IZY},[0x15]={"ORA",ZPX},
  [0x16]={"ASL",ZPX},[0x18]={"CLC",IMP},[0x19]={"ORA",ABY},[0x1D]={"ORA",ABX},
  [0x1E]={"ASL",ABX},[0x20]={"JSR",ABS},[0x21]={"AND",IZX},[0x24]={"BIT",ZP},
  [0x25]={"AND",ZP},[0x26]={"ROL",ZP},[0x28]={"PLP",IMP},[0x29]={"AND",IMM},
  [0x2A]={"ROL",ACC},[0x2C]={"BIT",ABS},[0x2D]={"AND",ABS},[0x2E]={"ROL",ABS},
  [0x30]={"BMI",REL},[0x31]={"AND",IZY},[0x35]={"AND",ZPX},[0x36]={"ROL",ZPX},
  [0x38]={"SEC",IMP},[0x39]={"AND",ABY},[0x3D]={"AND",ABX},[0x3E]={"ROL",ABX},
  [0x40]={"RTI",IMP},[0x41]={"EOR",IZX},[0x45]={"EOR",ZP},[0x46]={"LSR",ZP},
  [0x48]={"PHA",IMP},[0x49]={"EOR",IMM},[0x4A]={"LSR",ACC},[0x4C]={"JMP",ABS},
  [0x4D]={"EOR",ABS},[0x4E]={"LSR",ABS},[0x50]={"BVC",REL},[0x51]={"EOR",IZY},
  [0x55]={"EOR",ZPX},[0x56]={"LSR",ZPX},[0x58]={"CLI",IMP},[0x59]={"EOR",ABY},
  [0x5D]={"EOR",ABX},[0x5E]={"LSR",ABX},[0x60]={"RTS",IMP},[0x61]={"ADC",IZX},
  [0x65]={"ADC",ZP},[0x66]={"ROR",ZP},[0x68]={"PLA",IMP},[0x69]={"ADC",IMM},
  [0x6A]={"ROR",ACC},[0x6C]={"JMP",IND},[0x6D]={"ADC",ABS},[0x6E]={"ROR",ABS},
  [0x70]={"BVS",REL},[0x71]={"ADC",IZY},[0x75]={"ADC",ZPX},[0x76]={"ROR",ZPX},
  [0x78]={"SEI",IMP},[0x79]={"ADC",ABY},[0x7D]={"ADC",ABX},[0x7E]={"ROR",ABX},
  [0x81]={"STA",IZX},[0x84]={"STY",ZP},[0x85]={"STA",ZP},[0x86]={"STX",ZP},
  [0x88]={"DEY",IMP},[0x8A]={"TXA",IMP},[0x8C]={"STY",ABS},[0x8D]={"STA",ABS},
  [0x8E]={"STX",ABS},[0x90]={"BCC",REL},[0x91]={"STA",IZY},[0x94]={"STY",ZPX},
  [0x95]={"STA",ZPX},[0x96]={"STX",ZPY},[0x98]={"TYA",IMP},[0x99]={"STA",ABY},
  [0x9A]={"TXS",IMP},[0x9D]={"STA",ABX},[0xA0]={"LDY",IMM},[0xA1]={"LDA",IZX},
  [0xA2]={"LDX",IMM},[0xA4]={"LDY",ZP},[0xA5]={"LDA",ZP},[0xA6]={"LDX",ZP},
  [0xA8]={"TAY",IMP},[0xA9]={"LDA",IMM},[0xAA]={"TAX",IMP},[0xAC]={"LDY",ABS},
  [0xAD]={"LDA",ABS},[0xAE]={"LDX",ABS},[0xB0]={"BCS",REL},[0xB1]={"LDA",IZY},
  [0xB4]={"LDY",ZPX},[0xB5]={"LDA",ZPX},[0xB6]={"LDX",ZPY},[0xB8]={"CLV",IMP},
  [0xB9]={"LDA",ABY},[0xBA]={"TSX",IMP},[0xBC]={"LDY",ABX},[0xBD]={"LDA",ABX},
  [0xBE]={"LDX",ABY},[0xC0]={"CPY",IMM},[0xC1]={"CMP",IZX},[0xC4]={"CPY",ZP},
  [0xC5]={"CMP",ZP},[0xC6]={"DEC",ZP},[0xC8]={"INY",IMP},[0xC9]={"CMP",IMM},
  [0xCA]={"DEX",IMP},[0xCC]={"CPY",ABS},[0xCD]={"CMP",ABS},[0xCE]={"DEC",ABS},
  [0xD0]={"BNE",REL},[0xD1]={"CMP",IZY},[0xD5]={"CMP",ZPX},[0xD6]={"DEC",ZPX},
  [0xD8]={"CLD",IMP},[0xD9]={"CMP",ABY},[0xDD]={"CMP",ABX},[0xDE]={"DEC",ABX},
  [0xE0]={"CPX",IMM},[0xE1]={"SBC",IZX},[0xE4]={"CPX",ZP},[0xE5]={"SBC",ZP},
  [0xE6]={"INC",ZP},[0xE8]={"INX",IMP},[0xE9]={"SBC",IMM},[0xEA]={"NOP",IMP},
  [0xEC]={"CPX",ABS},[0xED]={"SBC",ABS},[0xEE]={"INC",ABS},[0xF0]={"BEQ",REL},
  [0xF1]={"SBC",IZY},[0xF5]={"SBC",ZPX},[0xF6]={"INC",ZPX},[0xF8]={"SED",IMP},
  [0xF9]={"SBC",ABY},[0xFD]={"SBC",ABX},[0xFE]={"INC",ABX},
}

local function disasm(gpc)
  local opcode = read_prg(gpc)
  if not opcode then return 1, "??      ", fmt("??? $%04X", gpc) end
  local e = ops[opcode]
  if not e then return 1, fmt("%02X      ", opcode), fmt(".DB $%02X", opcode) end

  local mn, md = e[1], e[2]
  local sz = SZ[md]
  local b1 = (sz >= 2) and (read_prg((gpc+1)%0x10000) or 0) or 0
  local b2 = (sz >= 3) and (read_prg((gpc+2)%0x10000) or 0) or 0

  local bs
  if     sz == 1 then bs = fmt("%02X      ", opcode)
  elseif sz == 2 then bs = fmt("%02X %02X   ", opcode, b1)
  else                bs = fmt("%02X %02X %02X", opcode, b1, b2)
  end

  local op
  if     md==IMP then op=""
  elseif md==ACC then op="A"
  elseif md==IMM then op=fmt("#$%02X",b1)
  elseif md==ZP  then op=fmt("$%02X",b1)
  elseif md==ZPX then op=fmt("$%02X,X",b1)
  elseif md==ZPY then op=fmt("$%02X,Y",b1)
  elseif md==IZX then op=fmt("($%02X,X)",b1)
  elseif md==IZY then op=fmt("($%02X),Y",b1)
  elseif md==ABS then op=fmt("$%04X",b1+b2*256)
  elseif md==ABX then op=fmt("$%04X,X",b1+b2*256)
  elseif md==ABY then op=fmt("$%04X,Y",b1+b2*256)
  elseif md==IND then op=fmt("($%04X)",b1+b2*256)
  elseif md==REL then
    local off=b1; if off>=128 then off=off-256 end
    op=fmt("$%04X",(gpc+2+off)%0x10000)
  else op="" end

  return sz, bs, (op=="" and mn or mn.." "..op)
end

---------------------------------------------------------------------
-- File output
---------------------------------------------------------------------
local file = nil
local buf, buf_n = {}, 0
local total_lines = 0
local frame_count = 0
local finished = false

local function flush()
  if file and buf_n > 0 then
    file:write(table.concat(buf, "\n", 1, buf_n) .. "\n")
    file:flush()
    buf_n = 0
  end
end

local function emit(line)
  buf_n = buf_n + 1
  buf[buf_n] = line
  total_lines = total_lines + 1
  if buf_n >= FLUSH_EVERY then flush() end
end

local function finish(reason)
  if finished then return end
  finished = true; flush()
  if file then file:close(); file = nil end
  emu.log(fmt("Trace done: %d lines (%s)", total_lines, reason))
  emu.displayMessage("Trace", fmt("Done: %dk lines", math.floor(total_lines/1000)))
end

---------------------------------------------------------------------
-- Log one guest state snapshot
---------------------------------------------------------------------
local last_gpc = -1

local function log_guest(tag)
  if finished then return end

  local gpc = rd16(ZP_PC)
  -- Dedup: exec hooks fire on CPU address $9221 regardless of which
  -- PRG bank is mapped (mapper 30 bank aliasing).  When a non-step6502
  -- bank is mapped, the guest _pc hasn't changed — skip the duplicate.
  if gpc == last_gpc then return end

  local ga  = rd(ZP_A)
  local gx  = rd(ZP_X)
  local gy  = rd(ZP_Y)
  local gsp = rd(ZP_SP)
  local gst = rd(ZP_STATUS)

  local _, bs, instr = disasm(gpc)
  local line = fmt("%04X  %s  %-18s A:%02X X:%02X Y:%02X S:%02X P:%s Fr:%d",
                   gpc, bs, instr, ga, gx, gy, gsp, FLAG_CACHE[gst], frame_count)
  if tag then line = line .. " " .. tag end
  emit(line)

  last_gpc = gpc
  if MAX_LINES > 0 and total_lines >= MAX_LINES then finish("max lines") end
end

---------------------------------------------------------------------
-- Open file + write header
---------------------------------------------------------------------
file = io.open(OUTPUT_PATH, "w")
if not file then
  emu.log("ERROR: cannot open " .. OUTPUT_PATH)
  emu.displayMessage("Trace", "ERROR: cannot open output")
  return
end

file:write(fmt("# trace_logger.lua — %s guest PC trace\n", GAME))
file:write(fmt("# _pc=$%02X _a=$%02X _x=$%02X _y=$%02X _sp=$%02X _st=$%02X\n",
               ZP_PC, ZP_A or 0, ZP_X or 0, ZP_Y or 0, ZP_SP or 0, ZP_STATUS or 0))
if DISPATCH then
  file:write(fmt("# dispatch=$%04X\n", DISPATCH))
end
file:flush()

emu.log(fmt("Guest trace started: %s → %s", GAME, OUTPUT_PATH))
emu.displayMessage("Trace", "Recording " .. GAME)

---------------------------------------------------------------------
-- Hook: dispatch_on_pc (JIT mode — main trace source when run_6502()
-- dispatches compiled blocks)
---------------------------------------------------------------------
if DISPATCH then
  emu.addMemoryCallback(function()
    log_guest(nil)
  end, emu.callbackType.exec, DISPATCH)
  emu.log(fmt("  Hook: dispatch_on_pc @ $%04X", DISPATCH))
end

---------------------------------------------------------------------
-- Hook: step6502 / interpret_6502 entry (INTERPRETER_ONLY mode)
-- At entry, ZP _pc holds the guest PC of the instruction about to
-- execute — stable and correct, unlike write hooks which fire on
-- every pc++ during operand fetches (producing bogus intermediates).
---------------------------------------------------------------------
if STEP6502 then
  emu.addMemoryCallback(function()
    log_guest(nil)
  end, emu.callbackType.exec, STEP6502)
  emu.log(fmt("  Hook: step6502 @ $%04X", STEP6502))
end
if INTERP6502 then
  emu.addMemoryCallback(function()
    log_guest(nil)
  end, emu.callbackType.exec, INTERP6502)
  emu.log(fmt("  Hook: interpret_6502 @ $%04X", INTERP6502))
end
if not DISPATCH and not STEP6502 and not INTERP6502 then
  emu.log("  WARN: no exec hooks found, using frame sampling only")
end

---------------------------------------------------------------------
-- Frame counter + periodic flush + overlay
---------------------------------------------------------------------
emu.addEventCallback(function()
  frame_count = frame_count + 1

  -- Periodic flush for crash safety
  if buf_n > 0 then flush() end

  -- On-screen overlay
  if finished then
    emu.drawString(2, 2,
      fmt("Trace DONE %dk Fr:%d", math.floor(total_lines/1000), frame_count),
      0x00FF00, 0x000020)
  else
    emu.drawString(2, 2,
      fmt("gREC %dk Fr:%d  gPC=$%04X", math.floor(total_lines/1000), frame_count, rd16(ZP_PC)),
      0xFF4040, 0x000020)
  end
end, emu.eventType.endFrame)
