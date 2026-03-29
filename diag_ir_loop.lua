-- diag_ir_loop.lua  —  Mesen 2 Lua diagnostic for IR hang
--
-- Detects infinite dispatch loop: if dispatch_on_pc re-dispatches
-- the same guest PC repeatedly, dumps the compiled block's native
-- bytes to a file and breaks execution.
--
-- Usage:  Mesen.exe exidy.nes --lua diag_ir_loop.lua

local PROJECT_DIR = "c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\"
local OUT_FILE    = PROJECT_DIR .. "diag_ir_dump.txt"

-- ── MLB resolver (same pattern as hit_rate.lua) ──────────────────
local function find_addr_in_mlb(filename, label)
    local f = io.open(PROJECT_DIR .. filename, "r")
    if not f then return nil end
    for line in f:lines() do
        local prefix, hex, name = line:match("^(%a):(%x+):(.+)$")
        if name == label then
            f:close()
            local addr = tonumber(hex, 16)
            if prefix == "S" or prefix == "W" then return addr + 0x6000
            elseif prefix == "P"              then return addr + 0x8000
            elseif prefix == "R"              then return addr  -- ZP / register
            else                                   return addr
            end
        end
    end
    f:close()
    return nil
end

local function resolve(label)
    return find_addr_in_mlb("exidy.mlb", label)
end

-- ── Key addresses ────────────────────────────────────────────────
local ADDR_PC_LO         = resolve("_pc")         -- ZP $5E
local ADDR_PC_HI         = resolve("_pc") and (resolve("_pc") + 1)
local ADDR_STATUS        = resolve("_status")      -- ZP $64
local ADDR_A             = resolve("_a")           -- ZP $61
local ADDR_DISPATCH      = resolve("_dispatch_on_pc")       -- $6218
local ADDR_CROSS_DISPATCH= resolve("_cross_bank_dispatch")  -- $6203
local ADDR_NOT_RECOMPILED= resolve("not_recompiled")        -- $62E8
local ADDR_CODE_INDEX    = resolve("_code_index")  -- ZP $A2
local ADDR_ENTRY_PC_LO   = resolve("_cache_entry_pc_lo")    -- ZP $A0
local ADDR_ENTRY_PC_HI   = resolve("_cache_entry_pc_hi")    -- ZP $A1
local ADDR_FLASH_ADDR    = resolve("_flash_code_address")    -- WRAM
local ADDR_FLASH_BANK    = resolve("_flash_code_bank")       -- WRAM

-- Sentinel: show we loaded
emu.displayMessage("Diag", "diag_ir_loop.lua loaded OK")
emu.log("=== diag_ir_loop.lua loaded ===")
emu.log(string.format("  _pc         = $%04X", ADDR_PC_LO or 0))
emu.log(string.format("  dispatch    = $%04X", ADDR_DISPATCH or 0))
emu.log(string.format("  cross_disp  = $%04X", ADDR_CROSS_DISPATCH or 0))
emu.log(string.format("  code_index  = $%04X", ADDR_CODE_INDEX or 0))
emu.log(string.format("  entry_pc_lo = $%04X", ADDR_ENTRY_PC_LO or 0))

-- ── State tracking ───────────────────────────────────────────────
local last_guest_pc = -1
local repeat_count = 0
local REPEAT_THRESHOLD = 200   -- if same guest PC dispatched 200x → hang
local dispatch_count = 0
local dumped = false

-- Ping-pong detection: track unique PCs in a sliding window
local WINDOW_SIZE = 2000       -- look at last 2000 dispatches
local DIVERSITY_THRESHOLD = 4  -- if ≤3 unique PCs in window → stuck
local pc_ring = {}             -- ring buffer of recent guest PCs
local pc_ring_idx = 0
local pc_ring_full = false

-- ── Helper: read ZP/RAM via nesDebug ─────────────────────────────
local function zp(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

local function zp16(addr)
    local lo = emu.read(addr, emu.memType.nesDebug)
    local hi = emu.read(addr + 1, emu.memType.nesDebug)
    return hi * 256 + lo
end

-- ── Helper: read byte from CPU address space (maps flash banks) ──
local function cpu_read(addr)
    return emu.read(addr, emu.memType.nesDebug)
end

-- ── Dump block to file ───────────────────────────────────────────
local function dump_block(guest_pc)
    if dumped then return end
    dumped = true

    local f = io.open(OUT_FILE, "w")
    if not f then
        emu.log("ERROR: cannot open " .. OUT_FILE)
        return
    end

    f:write(string.format("=== IR HANG DIAGNOSTIC ===\n"))
    f:write(string.format("Guest PC stuck at: $%04X\n", guest_pc))
    f:write(string.format("Dispatch count: %d repeats\n\n", repeat_count))

    -- Read emulator state
    local state = emu.getState()
    local cpu_a  = state["cpu.a"]  or 0
    local cpu_x  = state["cpu.x"]  or 0
    local cpu_y  = state["cpu.y"]  or 0
    local cpu_sp = state["cpu.sp"] or 0xFF
    local cpu_pc = state["cpu.pc"] or 0
    local cpu_ps = state["cpu.ps"] or 0
    f:write(string.format("CPU: A=$%02X X=$%02X Y=$%02X S=$%02X PC=$%04X P=$%02X\n",
        cpu_a, cpu_x, cpu_y, cpu_sp, cpu_pc, cpu_ps))

    -- Read guest state from ZP
    f:write(string.format("Guest: _pc=$%04X _a=$%02X _status=$%02X\n",
        zp16(ADDR_PC_LO), zp(ADDR_A), zp(ADDR_STATUS)))
    f:write(string.format("       code_index=$%02X entry_pc=$%04X\n",
        zp(ADDR_CODE_INDEX),
        zp(ADDR_ENTRY_PC_HI) * 256 + zp(ADDR_ENTRY_PC_LO)))

    -- Try to find what flash address dispatch will JMP to.
    -- dispatch_on_pc stores the computed address at .dispatch_addr
    -- which is a self-modifying JMP.  We can read pc_flags to find
    -- the flash address for the guest PC.
    --
    -- The pc_flags table maps guest PC → native flash address info.
    -- Let's scan flash for the block by reading the JMP target that
    -- dispatch just executed.  The trace will show the actual bytes.

    -- Approach: read the 256 bytes at the flash code for this block.
    -- Since dispatch JMP'd into flash at some $8000-$BFFF address,
    -- we can see what block the CPU is IN right now.
    local real_pc = cpu_pc
    f:write(string.format("\nReal PC = $%04X\n", real_pc))

    -- If CPU is in flash ($8000-$BFFF), dump surrounding block
    if real_pc >= 0x8000 and real_pc <= 0xBFFF then
        -- Block code starts at a 16-byte aligned address.
        -- Header is 8 bytes before code start, sentinel at header+7 = code_start-1.
        -- Align real_pc down to 16-byte boundary, then search backward.
        local aligned = real_pc - (real_pc % 16)  -- floor to 16-byte
        local block_start = aligned
        for probe = aligned, math.max(aligned - 4096, 0x8000), -16 do
            local sentinel = cpu_read(probe - 1)  -- header byte 7 = sentinel
            if sentinel == 0xAA then
                block_start = probe
                break
            end
        end

        f:write(string.format("Block code starts at: $%04X\n", block_start))
        f:write(string.format("Header at: $%04X\n", block_start - 8))
        f:write(string.format("CPU PC offset into block: +$%03X\n", real_pc - block_start))

        -- Dump header (8 bytes)
        f:write("\nHeader bytes: ")
        for i = 0, 7 do
            f:write(string.format("%02X ", cpu_read(block_start - 8 + i)))
        end
        f:write("\n")

        -- Raw hex dump (16 rows of 16 bytes = 256 bytes)
        f:write("\nRaw hex dump:\n")
        for row = 0, 15 do
            local base = row * 16
            f:write(string.format("  +$%02X [%04X]: ", base, block_start + base))
            for col = 0, 15 do
                local addr = block_start + base + col
                if addr <= 0xBFFF then
                    f:write(string.format("%02X ", cpu_read(addr)))
                else
                    f:write("?? ")
                end
            end
            f:write("\n")
        end

        -- Dump code bytes (up to 256) with mini-disassembly
        f:write("\nCode bytes (offset: hex | disasm):\n")
        local pos = 0
        local max_bytes = 256
        while pos < max_bytes do
            local addr = block_start + pos
            if addr > 0xBFFF then break end
            local opcode = cpu_read(addr)
            local sz = 1  -- default

            -- 6502 instruction size lookup (simplified)
            local mode = ""
            local op_name = string.format("$%02X", opcode)

            -- Common opcodes we care about
            local known = {
                [0x08] = {"PHP", 1}, [0x28] = {"PLP", 1},
                [0x48] = {"PHA", 1}, [0x68] = {"PLA", 1},
                [0x18] = {"CLC", 1}, [0x38] = {"SEC", 1},
                [0xEA] = {"NOP", 1}, [0x60] = {"RTS", 1},
                [0x00] = {"BRK", 1}, [0x40] = {"RTI", 1},
                [0xAA] = {"TAX", 1}, [0xA8] = {"TAY", 1},
                [0x8A] = {"TXA", 1}, [0x98] = {"TYA", 1},
                [0xCA] = {"DEX", 1}, [0x88] = {"DEY", 1},
                [0xE8] = {"INX", 1}, [0xC8] = {"INY", 1},
                -- Immediate
                [0xA9] = {"LDA #", 2}, [0xA2] = {"LDX #", 2},
                [0xA0] = {"LDY #", 2}, [0xC9] = {"CMP #", 2},
                [0xE0] = {"CPX #", 2}, [0xC0] = {"CPY #", 2},
                [0x09] = {"ORA #", 2}, [0x29] = {"AND #", 2},
                [0x49] = {"EOR #", 2}, [0x69] = {"ADC #", 2},
                [0xE9] = {"SBC #", 2},
                -- ZP
                [0x85] = {"STA zp", 2}, [0x86] = {"STX zp", 2},
                [0x84] = {"STY zp", 2}, [0xA5] = {"LDA zp", 2},
                [0xA6] = {"LDX zp", 2}, [0xA4] = {"LDY zp", 2},
                [0xC5] = {"CMP zp", 2}, [0x24] = {"BIT zp", 2},
                [0x45] = {"EOR zp", 2}, [0x05] = {"ORA zp", 2},
                [0x25] = {"AND zp", 2}, [0x65] = {"ADC zp", 2},
                [0xE5] = {"SBC zp", 2}, [0xE6] = {"INC zp", 2},
                [0xC6] = {"DEC zp", 2}, [0x46] = {"LSR zp", 2},
                [0x06] = {"ASL zp", 2}, [0x26] = {"ROL zp", 2},
                [0x66] = {"ROR zp", 2},
                -- Absolute
                [0x8D] = {"STA abs", 3}, [0x8E] = {"STX abs", 3},
                [0x8C] = {"STY abs", 3}, [0xAD] = {"LDA abs", 3},
                [0xAE] = {"LDX abs", 3}, [0xAC] = {"LDY abs", 3},
                [0xCD] = {"CMP abs", 3}, [0x2C] = {"BIT abs", 3},
                [0x4D] = {"EOR abs", 3}, [0x0D] = {"ORA abs", 3},
                [0x2D] = {"AND abs", 3}, [0x6D] = {"ADC abs", 3},
                [0xED] = {"SBC abs", 3}, [0xEE] = {"INC abs", 3},
                [0xCE] = {"DEC abs", 3}, [0x4E] = {"LSR abs", 3},
                [0x0E] = {"ASL abs", 3}, [0x2E] = {"ROL abs", 3},
                [0x6E] = {"ROR abs", 3},
                -- Indirect
                [0x6C] = {"JMP ind", 3},
                -- Abs,X / Abs,Y
                [0xBD] = {"LDA a,X", 3}, [0xB9] = {"LDA a,Y", 3},
                [0x9D] = {"STA a,X", 3}, [0x99] = {"STA a,Y", 3},
                [0xDD] = {"CMP a,X", 3}, [0xD9] = {"CMP a,Y", 3},
                [0xDE] = {"DEC a,X", 3}, [0xFE] = {"INC a,X", 3},
                [0x7D] = {"ADC a,X", 3}, [0x79] = {"ADC a,Y", 3},
                [0xFD] = {"SBC a,X", 3}, [0xF9] = {"SBC a,Y", 3},
                [0x5D] = {"EOR a,X", 3}, [0x59] = {"EOR a,Y", 3},
                [0x1D] = {"ORA a,X", 3}, [0x19] = {"ORA a,Y", 3},
                [0x3D] = {"AND a,X", 3}, [0x39] = {"AND a,Y", 3},
                [0xBC] = {"LDY a,X", 3}, [0xBE] = {"LDX a,Y", 3},
                -- Branches (relative, 2 bytes)
                [0x10] = {"BPL", 2, "br"}, [0x30] = {"BMI", 2, "br"},
                [0x50] = {"BVC", 2, "br"}, [0x70] = {"BVS", 2, "br"},
                [0x90] = {"BCC", 2, "br"}, [0xB0] = {"BCS", 2, "br"},
                [0xD0] = {"BNE", 2, "br"}, [0xF0] = {"BEQ", 2, "br"},
                -- JSR / JMP abs
                [0x20] = {"JSR", 3, "abs"}, [0x4C] = {"JMP", 3, "abs"},
            }

            local info = known[opcode]
            if info then
                op_name = info[1]
                sz = info[2]
            end

            -- Format line
            local hex_bytes = string.format("%02X", opcode)
            local operand_str = ""
            if sz >= 2 and (addr + 1) <= 0xBFFF then
                local b1 = cpu_read(addr + 1)
                hex_bytes = hex_bytes .. string.format(" %02X", b1)
                if info and info[3] == "br" then
                    -- Branch: compute target
                    local offset = b1
                    if offset >= 128 then offset = offset - 256 end
                    local target = pos + 2 + offset  -- relative to block start
                    operand_str = string.format(" → +$%02X (offset %d)", target, offset)
                elseif sz == 2 then
                    operand_str = string.format(" $%02X", b1)
                end
            end
            if sz >= 3 and (addr + 2) <= 0xBFFF then
                local b1 = cpu_read(addr + 1)
                local b2 = cpu_read(addr + 2)
                hex_bytes = hex_bytes .. string.format(" %02X", b2)
                if info and info[3] == "abs" then
                    operand_str = string.format(" $%04X", b2 * 256 + b1)
                elseif sz == 3 then
                    operand_str = string.format(" $%04X", b2 * 256 + b1)
                end
            end

            -- Highlight interesting things
            local marker = ""
            if opcode == 0x4C and sz == 3 then
                local b1 = cpu_read(addr + 1)
                local b2 = cpu_read(addr + 2)
                if b1 == 0xFF and b2 == 0xFF then
                    marker = " *** UNPATCHED JMP $FFFF ***"
                end
            end
            if addr == cpu_pc then
                marker = marker .. " <<< CPU HERE"
            end

            f:write(string.format("  +$%03X [%04X]: %-12s  %s%s%s\n",
                pos, addr, hex_bytes, op_name, operand_str, marker))

            pos = pos + sz
            -- Stop at BRK or after enough bytes
            if opcode == 0x00 then break end
        end
    else
        f:write("CPU not in flash range - dumping WRAM state instead\n")
        -- Dump cache_code buffer from WRAM ($69CE)
        local cache_code_addr = 0x69CE  -- _cache_code
        f:write(string.format("\ncache_code[0] at $%04X:\n", cache_code_addr))
        for row = 0, 15 do
            local base = row * 16
            f:write(string.format("  +$%02X: ", base))
            for col = 0, 15 do
                f:write(string.format("%02X ", cpu_read(cache_code_addr + base + col)))
            end
            f:write("\n")
        end
    end

    -- Dump stack (16 bytes around SP)
    f:write(string.format("\nStack (SP=$%02X):\n", cpu_sp))
    f:write("  $01F0: ")
    for i = 0xF0, 0xFF do
        f:write(string.format("%02X ", emu.read(0x100 + i, emu.memType.nesDebug)))
    end
    f:write("\n  $01E0: ")
    for i = 0xE0, 0xEF do
        f:write(string.format("%02X ", emu.read(0x100 + i, emu.memType.nesDebug)))
    end
    f:write("\n")

    f:close()
    emu.log("*** DUMP WRITTEN TO " .. OUT_FILE .. " ***")
    emu.displayMessage("Diag", "HANG DETECTED - dump written!")
end

-- ── Hook: exec callback on dispatch_on_pc ────────────────────────
-- Every time dispatch_on_pc is reached, read the guest _pc that
-- is about to be dispatched.
if ADDR_DISPATCH then
    emu.addMemoryCallback(function()
        local gpc = zp16(ADDR_PC_LO)
        dispatch_count = dispatch_count + 1

        if gpc == last_guest_pc then
            repeat_count = repeat_count + 1
            if repeat_count >= REPEAT_THRESHOLD and not dumped then
                emu.log(string.format("*** HANG DETECTED: guest PC $%04X dispatched %d times ***",
                    gpc, repeat_count))
                dump_block(gpc)
                emu.breakExecution()
            end
        else
            last_guest_pc = gpc
            repeat_count = 1
        end
    end, emu.callbackType.exec, ADDR_DISPATCH)
    emu.log("Exec callback registered at dispatch_on_pc = $" .. string.format("%04X", ADDR_DISPATCH))
end

-- ── Hook: also catch cross_bank_dispatch ─────────────────────────
if ADDR_CROSS_DISPATCH then
    emu.addMemoryCallback(function()
        -- cross_bank_dispatch is the exit path; _pc was already set
        -- by the epilogue.  Log it.
        local gpc = zp16(ADDR_PC_LO)
        dispatch_count = dispatch_count + 1

        if gpc == last_guest_pc then
            repeat_count = repeat_count + 1
            if repeat_count >= REPEAT_THRESHOLD and not dumped then
                emu.log(string.format("*** HANG DETECTED (xbank): guest PC $%04X dispatched %d times ***",
                    gpc, repeat_count))
                dump_block(gpc)
                emu.breakExecution()
            end
        else
            last_guest_pc = gpc
            repeat_count = 1
        end
    end, emu.callbackType.exec, ADDR_CROSS_DISPATCH)
    emu.log("Exec callback registered at cross_bank_dispatch = $" .. string.format("%04X", ADDR_CROSS_DISPATCH))
end

-- ── Periodic status + hang-by-silence detection ─────────────────
local frame_count = 0
local last_status_dispatch_count = 0
local stall_frames = 0
local STALL_THRESHOLD = 120   -- ~2 seconds of no dispatches = stuck in a block

emu.addEventCallback(function()
    frame_count = frame_count + 1

    -- Check if dispatch count has not changed (= CPU stuck in one block)
    if dispatch_count == last_status_dispatch_count then
        stall_frames = stall_frames + 1
        if stall_frames >= STALL_THRESHOLD and not dumped then
            emu.log(string.format("*** STALL DETECTED: no dispatch for %d frames, last_pc=$%04X ***",
                stall_frames, last_guest_pc))
            -- For a stall, real CPU PC tells us where the infinite loop is
            dump_block(zp16(ADDR_PC_LO))
            emu.breakExecution()
        end
    else
        stall_frames = 0
    end
    last_status_dispatch_count = dispatch_count

    if frame_count % 300 == 0 then  -- every ~5 seconds
        emu.log(string.format("Status: %d dispatches, last_pc=$%04X, repeat=%d, stall=%d",
            dispatch_count, last_guest_pc, repeat_count, stall_frames))
    end
end, emu.eventType.endFrame)
