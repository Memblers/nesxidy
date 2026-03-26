#!/usr/bin/env python3
"""
Disassemble Atari Millipede arcade program ROMs.

Memory map:
  $0000-$03FF  RAM (1KB)
  $0400-$040F  POKEY 1
  $0800-$080F  POKEY 2
  $1000-$13BF  Video RAM (960 bytes)
  $13C0-$13FF  Sprite RAM (64 bytes)
  $2000        IN0 (read)
  $2001        IN1 (read) / Trackball select (write)
  $2010        IN2 (read)
  $2011        IN3 (read)
  $2030        EAROM read
  $2480-$249F  Palette RAM (32 bytes, write)
  $2500-$2507  Output latches (write)
  $2600        IRQ acknowledge (write)
  $2680        Watchdog reset (write)
  $4000-$7FFF  Program ROM (16KB)
  $8000-$FFFF  ROM mirror
"""

import struct
import sys
import os
from collections import defaultdict

ROM_DIR = r"c:\proj\c\NES\nesxidy-co\nesxidy\roms\milliped"
OUTPUT = r"c:\proj\c\NES\nesxidy-co\nesxidy\roms\milliped\Millipede.asm"

ROM_FILES = [
    ("136013-104.mn1", 0x4000),  # $4000-$4FFF
    ("136013-103.l1",  0x5000),  # $5000-$5FFF
    ("136013-102.jk1", 0x6000),  # $6000-$6FFF
    ("136013-101.h1",  0x7000),  # $7000-$7FFF
]

ROM_BASE = 0x4000
ROM_SIZE = 0x4000  # 16KB

# 6502 opcode table: (mnemonic, addressing_mode, bytes)
# Addressing modes:
#   imp=implied, acc=accumulator, imm=immediate, zp=zero page,
#   zpx=zp,X, zpy=zp,Y, abs=absolute, abx=abs,X, aby=abs,Y,
#   ind=indirect, izx=(ind,X), izy=(ind),Y, rel=relative
OPCODES = {
    0x00: ("BRK", "imp", 1),
    0x01: ("ORA", "izx", 2),
    0x05: ("ORA", "zp",  2),
    0x06: ("ASL", "zp",  2),
    0x08: ("PHP", "imp", 1),
    0x09: ("ORA", "imm", 2),
    0x0A: ("ASL", "acc", 1),
    0x0D: ("ORA", "abs", 3),
    0x0E: ("ASL", "abs", 3),
    0x10: ("BPL", "rel", 2),
    0x11: ("ORA", "izy", 2),
    0x15: ("ORA", "zpx", 2),
    0x16: ("ASL", "zpx", 2),
    0x18: ("CLC", "imp", 1),
    0x19: ("ORA", "aby", 3),
    0x1D: ("ORA", "abx", 3),
    0x1E: ("ASL", "abx", 3),
    0x20: ("JSR", "abs", 3),
    0x21: ("AND", "izx", 2),
    0x24: ("BIT", "zp",  2),
    0x25: ("AND", "zp",  2),
    0x26: ("ROL", "zp",  2),
    0x28: ("PLP", "imp", 1),
    0x29: ("AND", "imm", 2),
    0x2A: ("ROL", "acc", 1),
    0x2C: ("BIT", "abs", 3),
    0x2D: ("AND", "abs", 3),
    0x2E: ("ROL", "abs", 3),
    0x30: ("BMI", "rel", 2),
    0x31: ("AND", "izy", 2),
    0x35: ("AND", "zpx", 2),
    0x36: ("ROL", "zpx", 2),
    0x38: ("SEC", "imp", 1),
    0x39: ("AND", "aby", 3),
    0x3D: ("AND", "abx", 3),
    0x3E: ("ROL", "abx", 3),
    0x40: ("RTI", "imp", 1),
    0x41: ("EOR", "izx", 2),
    0x45: ("EOR", "zp",  2),
    0x46: ("LSR", "zp",  2),
    0x48: ("PHA", "imp", 1),
    0x49: ("EOR", "imm", 2),
    0x4A: ("LSR", "acc", 1),
    0x4C: ("JMP", "abs", 3),
    0x4D: ("EOR", "abs", 3),
    0x4E: ("LSR", "abs", 3),
    0x50: ("BVC", "rel", 2),
    0x51: ("EOR", "izy", 2),
    0x55: ("EOR", "zpx", 2),
    0x56: ("LSR", "zpx", 2),
    0x58: ("CLI", "imp", 1),
    0x59: ("EOR", "aby", 3),
    0x5D: ("EOR", "abx", 3),
    0x5E: ("LSR", "abx", 3),
    0x60: ("RTS", "imp", 1),
    0x61: ("ADC", "izx", 2),
    0x65: ("ADC", "zp",  2),
    0x66: ("ROR", "zp",  2),
    0x68: ("PLA", "imp", 1),
    0x69: ("ADC", "imm", 2),
    0x6A: ("ROR", "acc", 1),
    0x6C: ("JMP", "ind", 3),
    0x6D: ("ADC", "abs", 3),
    0x6E: ("ROR", "abs", 3),
    0x70: ("BVS", "rel", 2),
    0x71: ("ADC", "izy", 2),
    0x75: ("ADC", "zpx", 2),
    0x76: ("ROR", "zpx", 2),
    0x78: ("SEI", "imp", 1),
    0x79: ("ADC", "aby", 3),
    0x7D: ("ADC", "abx", 3),
    0x7E: ("ROR", "abx", 3),
    0x81: ("STA", "izx", 2),
    0x84: ("STY", "zp",  2),
    0x85: ("STA", "zp",  2),
    0x86: ("STX", "zp",  2),
    0x88: ("DEY", "imp", 1),
    0x8A: ("TXA", "imp", 1),
    0x8C: ("STY", "abs", 3),
    0x8D: ("STA", "abs", 3),
    0x8E: ("STX", "abs", 3),
    0x90: ("BCC", "rel", 2),
    0x91: ("STA", "izy", 2),
    0x94: ("STY", "zpx", 2),
    0x95: ("STA", "zpx", 2),
    0x96: ("STX", "zpy", 2),
    0x98: ("TYA", "imp", 1),
    0x99: ("STA", "aby", 3),
    0x9A: ("TXS", "imp", 1),
    0x9D: ("STA", "abx", 3),
    0xA0: ("LDY", "imm", 2),
    0xA1: ("LDA", "izx", 2),
    0xA2: ("LDX", "imm", 2),
    0xA4: ("LDY", "zp",  2),
    0xA5: ("LDA", "zp",  2),
    0xA6: ("LDX", "zp",  2),
    0xA8: ("TAY", "imp", 1),
    0xA9: ("LDA", "imm", 2),
    0xAA: ("TAX", "imp", 1),
    0xAC: ("LDY", "abs", 3),
    0xAD: ("LDA", "abs", 3),
    0xAE: ("LDX", "abs", 3),
    0xB0: ("BCS", "rel", 2),
    0xB1: ("LDA", "izy", 2),
    0xB4: ("LDY", "zpx", 2),
    0xB5: ("LDA", "zpx", 2),
    0xB6: ("LDX", "zpy", 2),
    0xB8: ("CLV", "imp", 1),
    0xB9: ("LDA", "aby", 3),
    0xBA: ("TSX", "imp", 1),
    0xBC: ("LDY", "abx", 3),
    0xBD: ("LDA", "abx", 3),
    0xBE: ("LDX", "aby", 3),
    0xC0: ("CPY", "imm", 2),
    0xC1: ("CMP", "izx", 2),
    0xC4: ("CPY", "zp",  2),
    0xC5: ("CMP", "zp",  2),
    0xC6: ("DEC", "zp",  2),
    0xC8: ("INY", "imp", 1),
    0xC9: ("CMP", "imm", 2),
    0xCA: ("DEX", "imp", 1),
    0xCC: ("CPY", "abs", 3),
    0xCD: ("CMP", "abs", 3),
    0xCE: ("DEC", "abs", 3),
    0xD0: ("BNE", "rel", 2),
    0xD1: ("CMP", "izy", 2),
    0xD5: ("CMP", "zpx", 2),
    0xD6: ("DEC", "zpx", 2),
    0xD8: ("CLD", "imp", 1),
    0xD9: ("CMP", "aby", 3),
    0xDD: ("CMP", "abx", 3),
    0xDE: ("DEC", "abx", 3),
    0xE0: ("CPX", "imm", 2),
    0xE1: ("SBC", "izx", 2),
    0xE4: ("CPX", "zp",  2),
    0xE5: ("SBC", "zp",  2),
    0xE6: ("INC", "zp",  2),
    0xE8: ("INX", "imp", 1),
    0xE9: ("SBC", "imm", 2),
    0xEA: ("NOP", "imp", 1),
    0xEC: ("CPX", "abs", 3),
    0xED: ("SBC", "abs", 3),
    0xEE: ("INC", "abs", 3),
    0xF0: ("BEQ", "rel", 2),
    0xF1: ("SBC", "izy", 2),
    0xF5: ("SBC", "zpx", 2),
    0xF6: ("INC", "zpx", 2),
    0xF8: ("SED", "imp", 1),
    0xF9: ("SBC", "aby", 3),
    0xFD: ("SBC", "abx", 3),
    0xFE: ("INC", "abx", 3),
}

# Known I/O and hardware labels
IO_LABELS = {
    0x0400: "POKEY1_AUDF1",
    0x0401: "POKEY1_AUDC1",
    0x0402: "POKEY1_AUDF2",
    0x0403: "POKEY1_AUDC2",
    0x0404: "POKEY1_AUDF3",
    0x0405: "POKEY1_AUDC3",
    0x0406: "POKEY1_AUDF4",
    0x0407: "POKEY1_AUDC4",
    0x0408: "POKEY1_AUDCTL",  # write: AUDCTL, read: ALLPOT (DSW1)
    0x0409: "POKEY1_STIMER",
    0x040A: "POKEY1_RANDOM",  # read: RANDOM
    0x040B: "POKEY1_POTGO",
    0x040E: "POKEY1_IRQEN",   # write: IRQEN, read: IRQST
    0x040F: "POKEY1_SKCTL",   # write: SKCTL, read: SKSTAT
    0x0800: "POKEY2_AUDF1",
    0x0801: "POKEY2_AUDC1",
    0x0802: "POKEY2_AUDF2",
    0x0803: "POKEY2_AUDC2",
    0x0804: "POKEY2_AUDF3",
    0x0805: "POKEY2_AUDC3",
    0x0806: "POKEY2_AUDF4",
    0x0807: "POKEY2_AUDC4",
    0x0808: "POKEY2_AUDCTL",
    0x0809: "POKEY2_STIMER",
    0x080A: "POKEY2_RANDOM",
    0x080B: "POKEY2_POTGO",
    0x080E: "POKEY2_IRQEN",
    0x080F: "POKEY2_SKCTL",
    0x2000: "IN0",            # Player 1 inputs / VBLANK
    0x2001: "IN1",            # Player 2 inputs / trackball Y
    0x2010: "IN2",            # Joystick / coin
    0x2011: "IN3",            # Self-test / cabinet
    0x2030: "EAROM_READ",
    0x2480: "PALETTE_RAM",    # $2480-$249F
    0x2500: "OUT_LATCH_0",
    0x2501: "OUT_LATCH_1",
    0x2502: "OUT_LATCH_2",
    0x2503: "OUT_LATCH_3",
    0x2504: "OUT_LATCH_4",
    0x2505: "OUT_TBEN",       # Trackball enable
    0x2506: "OUT_FLIP",       # Flip screen
    0x2507: "OUT_INT_ACK",    # Interrupt acknowledge
    0x2600: "IRQ_ACK",
    0x2680: "WATCHDOG",
    0x2700: "EAROM_CTRL",     # EAROM control: b0=CK, b1=/C1, b2=C2, b3=CS1
    0x2780: "EAROM_WRITE",    # EAROM address+data latch ($2780-$27BF)
}


def load_rom():
    """Load all ROM files into a single 16KB array."""
    rom = bytearray(ROM_SIZE)
    for filename, base_addr in ROM_FILES:
        path = os.path.join(ROM_DIR, filename)
        with open(path, "rb") as f:
            data = f.read()
        offset = base_addr - ROM_BASE
        rom[offset:offset + len(data)] = data
        print(f"  Loaded {filename} ({len(data)} bytes) at ${base_addr:04X}")
    return rom


def rom_byte(rom, addr):
    """Read a byte from the ROM image at address addr ($4000-$7FFF)."""
    if addr < ROM_BASE or addr >= ROM_BASE + ROM_SIZE:
        return None
    return rom[addr - ROM_BASE]


def rom_word(rom, addr):
    """Read a little-endian 16-bit word."""
    lo = rom_byte(rom, addr)
    hi = rom_byte(rom, addr + 1)
    if lo is None or hi is None:
        return None
    return lo | (hi << 8)


def format_operand(mode, rom, pc):
    """Format the operand string for a given addressing mode."""
    if mode == "imp":
        return ""
    elif mode == "acc":
        return "A"
    elif mode == "imm":
        return f"#${rom_byte(rom, pc+1):02X}"
    elif mode == "zp":
        return f"${rom_byte(rom, pc+1):02X}"
    elif mode == "zpx":
        return f"${rom_byte(rom, pc+1):02X},X"
    elif mode == "zpy":
        return f"${rom_byte(rom, pc+1):02X},Y"
    elif mode == "abs":
        addr = rom_word(rom, pc + 1)
        return f"${addr:04X}"
    elif mode == "abx":
        addr = rom_word(rom, pc + 1)
        return f"${addr:04X},X"
    elif mode == "aby":
        addr = rom_word(rom, pc + 1)
        return f"${addr:04X},Y"
    elif mode == "ind":
        addr = rom_word(rom, pc + 1)
        return f"(${addr:04X})"
    elif mode == "izx":
        return f"(${rom_byte(rom, pc+1):02X},X)"
    elif mode == "izy":
        return f"(${rom_byte(rom, pc+1):02X}),Y"
    elif mode == "rel":
        offset = rom_byte(rom, pc + 1)
        if offset >= 0x80:
            offset -= 0x100
        target = (pc + 2 + offset) & 0xFFFF
        return f"${target:04X}"
    return ""


def trace_code(rom, entry_points):
    """
    Recursive-descent code tracing from entry points.
    Returns set of addresses that are code.
    """
    code_addrs = set()
    work = list(entry_points)
    visited = set()

    while work:
        pc = work.pop()
        if pc in visited:
            continue
        if pc < ROM_BASE or pc >= ROM_BASE + ROM_SIZE:
            continue
        visited.add(pc)

        opcode = rom_byte(rom, pc)
        if opcode is None:
            continue
        if opcode not in OPCODES:
            # Unknown opcode - stop tracing this path
            code_addrs.add(pc)
            continue

        mnem, mode, nbytes = OPCODES[opcode]
        for i in range(nbytes):
            code_addrs.add(pc + i)

        # Follow branches
        if mode == "rel":
            offset = rom_byte(rom, pc + 1)
            if offset >= 0x80:
                offset -= 0x100
            target = (pc + 2 + offset) & 0xFFFF
            work.append(target)

        # Unconditional branch: don't fall through
        if mnem in ("JMP",):
            if mode == "abs":
                target = rom_word(rom, pc + 1)
                if target is not None:
                    work.append(target)
            continue  # don't fall through

        if mnem in ("RTS", "RTI", "BRK"):
            continue  # don't fall through

        # JSR: follow the subroutine AND fall through
        if mnem == "JSR":
            target = rom_word(rom, pc + 1)
            if target is not None:
                work.append(target)

        # Fall through to next instruction
        work.append(pc + nbytes)

    return code_addrs


def find_subroutine_entries(rom, code_addrs):
    """Find all JSR targets to label as subroutines."""
    subs = set()
    for pc in sorted(code_addrs):
        opcode = rom_byte(rom, pc)
        if opcode is None:
            continue
        if opcode not in OPCODES:
            continue
        mnem, mode, nbytes = OPCODES[opcode]
        if mnem == "JSR" and mode == "abs":
            target = rom_word(rom, pc + 1)
            if target is not None:
                subs.add(target)
    return subs


def find_branch_targets(rom, code_addrs):
    """Find all branch/jump targets for labeling."""
    targets = set()
    for pc in sorted(code_addrs):
        opcode = rom_byte(rom, pc)
        if opcode is None:
            continue
        if opcode not in OPCODES:
            continue
        mnem, mode, nbytes = OPCODES[opcode]
        if mode == "rel":
            offset = rom_byte(rom, pc + 1)
            if offset >= 0x80:
                offset -= 0x100
            target = (pc + 2 + offset) & 0xFFFF
            targets.add(target)
        elif mnem == "JMP" and mode == "abs":
            target = rom_word(rom, pc + 1)
            if target is not None:
                targets.add(target)
    return targets


# Per-PC inline comments for EAROM control writes (context-specific)
EAROM_PC_COMMENTS = {
    # earom_read_byte ($78CC)
    0x78CC: "EAROM_WRITE addr",             # STA $2780,X -- latch address
    0x78D1: "EAROM_CTRL: CS1+READ, CK=0",  # STY $2700 (Y=$08)
    0x78D5: "EAROM_CTRL: CS1+READ, CK=1",  # STY $2700 (Y=$09) -- clock high
    0x78D9: "EAROM_CTRL: CS1+READ, CK=0",  # STY $2700 (Y=$08) -- clock low
    0x78DE: "EAROM_READ",                   # LDA $2030 -- read data byte
    0x78E1: "EAROM_CTRL: deselect",         # STY $2700 (Y=$00) -- chip off
    # earom_save ($78E5)
    0x78EB: "EAROM_CTRL: idle/deselect",    # STA $2700 (A=$00)
    0x78F8: "EAROM_CTRL: standby setup",    # STA $2700 (A=$02)
    0x78FD: "EAROM_CTRL: CS1+standby=store",# STA $2700 (A=$0A) -- commit
    # earom_save verify-fail rewrite path
    0x7918: "EAROM_CTRL: write mode setup", # STA $2700 (A=$06)
    0x791E: "EAROM_WRITE addr+data",        # STA $2780,X -- latch for write
    0x7923: "EAROM_CTRL: CS1+write=commit", # STA $2700 (A=$0E) -- write
}

def get_io_comment(mode, rom, pc):
    """Return an inline comment for known I/O addresses."""
    # Check for per-PC EAROM annotation first
    if pc in EAROM_PC_COMMENTS:
        return f"  ; {EAROM_PC_COMMENTS[pc]}"
    if mode in ("abs", "abx", "aby"):
        addr = rom_word(rom, pc + 1)
        if addr in IO_LABELS:
            return f"  ; {IO_LABELS[addr]}"
        if 0x1000 <= addr <= 0x13BF:
            return "  ; Video RAM"
        if 0x13C0 <= addr <= 0x13FF:
            return "  ; Sprite RAM"
        if 0x2480 <= addr <= 0x249F:
            return f"  ; Palette RAM + ${addr - 0x2480:02X}"
        if addr in RAM_NAMES:
            return f"  ; {RAM_NAMES[addr]}"
        if 0x0000 <= addr <= 0x03FF:
            return f"  ; RAM"
    if mode in ("zp", "zpx", "zpy"):
        zp_addr = rom_byte(rom, pc + 1)
        if zp_addr in ZP_NAMES:
            return f"  ; {ZP_NAMES[zp_addr]}"
    if mode in ("izy", "izx"):
        zp_addr = rom_byte(rom, pc + 1)
        if zp_addr in ZP_NAMES:
            return f"  ; [{ZP_NAMES[zp_addr]}]"
    return ""


def detect_ascii_string(rom, start_offset):
    """Check if data starting at offset forms a printable ASCII string (min 4 chars)."""
    s = []
    i = start_offset
    while i < ROM_SIZE:
        b = rom[i]
        if b == 0x00:
            break  # NUL terminator
        if 0x20 <= b <= 0x7E or b == 0xA0:  # printable or NBSP
            s.append(chr(b) if b != 0xA0 else ' ')
            i += 1
        elif b >= 0xC0 and b <= 0xDA:
            # Millipede uses high-bit-set chars for special display chars
            s.append(chr(b & 0x7F))
            i += 1
        else:
            break
    if len(s) >= 4:
        return ''.join(s), i - start_offset
    return None, 0


def is_self_spin(rom, pc):
    """Check if instruction at pc is a branch-to-self (infinite loop)."""
    opcode = rom_byte(rom, pc)
    if opcode in (0x90, 0xB0, 0xF0, 0xD0, 0x10, 0x30, 0x50, 0x70):
        offset = rom_byte(rom, pc + 1)
        if offset == 0xFE:
            return True
    return False


# Known zero-page variable names (from MAME milliped.cpp + deep analysis)
# Object arrays: 16 slots (X=0..15) in parallel ZP arrays
#   $00-$0F = tile/char code  $10-$1F = Y direction/speed
#   $20-$2F = X position      $30-$3F = Y position
#   $40-$4F = move speed/dir   $50-$5F = color + active flag
ZP_NAMES = {
    # Object arrays (slot 0 only, others indexed)
    0x0F: "player_tile",      # slot 0 tile (=$03 when alive)
    0x1F: "player_ydir",      # slot 0 Y direction
    0x2F: "player_ypos",      # slot 0 Y pixel position
    0x3F: "player_xpos",      # slot 0 X pixel position
    0x4F: "player_speed",     # slot 0 move speed
    0x5F: "player_color",     # slot 0 color/active
    # Trackball
    0x60: "trackball_frac_x",  # sub-pixel X accumulator
    0x61: "trackball_frac_y",  # sub-pixel Y accumulator
    # Timers
    0x62: "timer_lo",          # frame counter low (inc every VBLANK)
    0x63: "timer_hi",          # frame counter high
    # High score storage
    0x79: "palette_xor",       # palette XOR mask (attract color cycling)
    # VRAM pointers
    0x94: "vram_ptr_lo",       # VRAM address low (sub_599C result)
    0x95: "vram_ptr_hi",       # VRAM address high
    # Game state
    0x96: "game_state",        # $FF=attract, $00=gameplay, $01=timed play
    0x97: "wave_timer",        # inter-wave delay countdown
    0x98: "current_player",    # 0=P1, 1=P2 (cocktail alternating)
    0x99: "num_players",       # 1 or 2
    0x9A: "vblank_flag",       # set by IRQ, cleared by main loop (LSR)
    0x9B: "temp_9B",           # scratch register
    0x9C: "temp_9C",           # scratch register
    # Collision
    0xA0: "collision_mode",    # $00=normal, $80=DDT kill mode
    0xA1: "text_ptr_lo",       # VRAM text cursor low
    0xA2: "text_ptr_hi",       # VRAM text cursor high
    0xA3: "string_ptr_lo",     # string data pointer low
    0xA4: "string_ptr_hi",     # string data pointer high
    # Lives and segments
    0xA5: "p1_lives",          # P1 lives remaining
    0xA6: "p2_lives",          # P2 lives remaining
    0xA7: "spawn_request",     # nonzero = millipede segment needs spawning
    0xA8: "temp_flip",         # temp sprite flip bit
    0xAA: "fire_shift_reg",    # fire button shift register
    0xAB: "p1_segments",       # P1 millipede segments to kill (starts $0C)
    0xAC: "p2_segments",       # P2 millipede segments to kill
    0xAD: "p1_speed_level",    # P1 difficulty speed (1-3)
    0xAE: "p2_speed_level",    # P2 difficulty speed
    # Spawn timers
    0xAF: "spawn_delay_0",     # enemy spawn cooldown timer 0
    0xB0: "spawn_delay_1",     # enemy spawn cooldown timer 1
    0xB1: "spawn_delay_2",     # enemy spawn cooldown timer 2
    0xB2: "starting_lives",    # starting lives from DIP switches
    0xB3: "p1_extra_lives",    # P1 extra life tracking
    0xB4: "p2_extra_lives",    # P2 extra life tracking
    0xB5: "game_started",      # set once per player turn start
    # Scores (BCD)
    0xB6: "p1_score_lo",       # P1 score low byte (BCD)
    0xB7: "p2_score_lo",       # P2 score low byte (BCD)
    0xB8: "p1_score_mid",      # P1 score mid byte (BCD)
    0xB9: "p2_score_mid",      # P2 score mid byte (BCD)
    0xBA: "p1_score_hi",       # P1 score high / wave counter (BCD)
    0xBB: "p2_score_hi",       # P2 score high / wave counter (BCD)
    0xBC: "p1_bonus_lo",       # P1 next extra-life threshold (lo BCD)
    0xBD: "p2_bonus_lo",       # P2 next extra-life threshold (lo BCD)
    0xBE: "p1_bonus_hi",       # P1 next extra-life threshold (hi BCD)
    0xBF: "p2_bonus_hi",       # P2 next extra-life threshold (hi BCD)
    # Enemy control
    0xC0: "swarm_timer",       # swarm enemy spawn timer
    0xC2: "mushroom_regen",    # mushroom regeneration timer
    0xC6: "bullet_speed",      # current bullet speed
    0xC7: "sound_trigger",     # 1 = play sound this frame
    0xC8: "sound_effect_id",   # current sound effect
    0xC9: "inchworm_timer",    # inchworm step timer
    # Trackball accumulators (consumed by player movement)
    0xCC: "trackball_accum_x", # X delta accumulated by IRQ
    0xCD: "trackball_accum_y", # Y delta accumulated by IRQ
    0xCE: "trackball_dir_x",   # X direction memory (smoothing)
    0xCF: "trackball_dir_y",   # Y direction memory (smoothing)
    0xD0: "trackball_prev_x",  # previous raw trackball X
    0xD1: "trackball_prev_y",  # previous raw trackball Y
    0xD2: "start_press_cnt",   # start button press counter
    # Player activity
    0xD3: "p1_active",         # $FF=not playing/game over, else active
    0xD4: "p2_active",         # $FF=not playing/game over, else active
    0xD5: "screen_dirty",      # nonzero = screen needs full redraw
    0xD6: "output_mirror_0",   # mirror of OUT_LATCH_0
    0xD7: "output_mirror_1",   # mirror of OUT_LATCH_1
    0xD8: "output_mirror_2",   # mirror of OUT_LATCH_2
    0xD9: "credit_count",      # credits / sprite display count
    0xDA: "extra_coin_flag",   # additional coin indicator
    # Sound/display
    0xE3: "bg_sound_enable",   # nonzero = background hum on
    0xE4: "dip_switch_2",      # DSW2 (lives, bonus, cocktail)
    0xE5: "game_flags",        # bits 0-1 set each frame by IRQ
    0xE6: "palette_cycle",     # attract mode palette cycling counter
    0xE7: "player_ind_tile",   # PLAYER 1/2 indicator tile
    0xE8: "p1_mush_kills",     # P1 mushroom destruction count
    0xE9: "p2_mush_kills",     # P2 mushroom destruction count
    0xEA: "p1_spawn_thresh",   # P1 wave-based spawn threshold
    0xEB: "p2_spawn_thresh",   # P2 wave-based spawn threshold
    # Flip screen (cocktail)
    0xEF: "flip_screen",       # $00=normal, $80=flipped (cocktail P2)
    0xF0: "y_flip_xor",        # $00 or $F8 -- XOR with Y positions
    0xF1: "x_flip_xor",        # $00 or $FF -- XOR with X positions
    0xF2: "dir_flip_xor",      # $00 or $FE -- XOR with move directions
    0xF3: "earom_write_idx",   # EAROM sequential write index
    0xF4: "earom_dirty",       # EAROM dirty bitfield
    0xF5: "play_timer_lo",     # BCD play timer low (inc every 256 frames)
    0xF6: "play_timer_hi",     # BCD play timer high
    0xF7: "dip_switch_1",      # DSW1 (coinage, bonus interval)
    0xF8: "combined_dip",      # combined DIP byte (trackball mode)
    0xF9: "swarm_active_cnt",  # number of active swarm enemies
    0xFA: "max_swarm",         # max swarm enemies (wave-dependent)
    0xFB: "lead_zero_supp",    # $80=suppress leading zeros
    0xFC: "mushroom_deficit",  # bonus mushroom scoring trigger
    0xFD: "p1_mush_destroyed", # P1 mushrooms destroyed count
    0xFE: "p2_mush_destroyed", # P2 mushrooms destroyed count
    0xFF: "general_countdown", # countdown for timed events
}

# RAM $0200+ variable comments
RAM_NAMES = {
    0x0200: "obj_subtimer",      # per-object movement sub-timers [16]
    0x0210: "mushroom_backup",   # compressed mushroom field bitmap
    0x0288: "earom_shadow",      # EAROM shadow (27 bytes: scores, initials)
    0x029B: "coin_accounting",   # BCD coin/play/time counters
    0x02C8: "trackball_state_x", # previous quadrature state X
    0x02C9: "trackball_state_y", # previous quadrature state Y
    0x02CA: "ddt_bombs_left",    # DDT bombs available
    0x02CB: "death_timer",       # nonzero = player dying (countdown)
    0x02CC: "sound_pitch",       # base pitch for current SFX
    0x02CE: "freeze_timer",      # nonzero = game logic frozen (death anim)
    0x02CF: "bonus_mult_state",  # bonus scoring progression
    0x02D0: "obj_spawn_timer",   # per-object spawn/move timers [13]
    0x02DE: "obj_anim_state",    # per-object animation state [16]
    0x0300: "field_proc_state",  # mushroom field processing state
    0x0305: "field_proc_active", # nonzero = field needs processing
    0x0309: "field_flip_flag",   # field L/R orientation
    0x030A: "p1_extra_awarded",  # P1 extra life awarded flag
    0x030B: "p2_extra_awarded",  # P2 extra life awarded flag
    0x0380: "mush_restore_prog", # mushroom restoration progress
}

# Known subroutine names (gameplay functions identified from analysis)
KNOWN_LABELS = {
    # Main loop subroutines
    0x4900: "frame_init",              # one-time per-round init (audio, objects)
    0x620E: "display_init",            # score display, playfield borders
    0x5592: "attract_credit_display",  # attract mode / coin/credit handling
    0x615A: "score_display",           # render P1/P2/high scores to VRAM
    0x57A4: "game_active_gate",        # check credits+start; N=1-->active, N=0-->attract
    0x78E5: "earom_save",              # incremental EAROM high-score save
    0x5D73: "timed_play_logic",        # timed play countdown / attract state
    0x45FC: "death_anim_handler",      # death puff/decay + freeze timer ($02CE)
    0x4C9F: "player_movement",         # read trackball --> move player sprite
    0x43A7: "ddt_bomb_spawner",        # random DDT bomb placement from pool
    0x4DC5: "bullet_move_collide",     # bullet movement + mushroom/enemy hit test
    0x5935: "millipede_spawner",       # fulfill $A7 spawn requests for segments
    0x50CA: "death_sound",             # descending pitch death SFX
    0x4B21: "millipede_movement",      # head/body AI: row traversal, turns, drops
    0x5105: "extra_enemy_spawner",     # spider/beetle/mosquito/flea by wave
    0x47E8: "spider_bee_movement",     # sinusoidal movement + proximity scoring
    0x454F: "inchworm_handler",        # inchworm spawn + horizontal movement
    0x408E: "bee_dragonfly_handler",   # bee animation + POKEY2 buzzing
    0x4A31: "mosquito_flea_spawner",   # vertical dropper that creates mushrooms
    0x41CD: "swarm_enemy_logic",       # earwig/swarm movement + direction changes
    0x5310: "ddt_cloud_expand",        # DDT poison cloud expansion (slot 0)
    0x61DC: "level_transition",        # mushroom restoration between waves
    0x53EC: "wave_complete_check",     # all enemies dead --> new wave
    0x5A03: "mushroom_field_regen",    # restore damaged mushrooms + scoring
    0x5B4B: "extra_life_check",        # score vs threshold --> award life
    0x6878: "mushroom_field_redraw",   # full field processing loop
    # Utility routines
    0x599C: "xy_to_vram",              # (X,Y) --> VRAM address + tile read
    0x7600: "negate_a",                # EOR #$FF / CLC / ADC #1
    0x75FE: "abs_value_a",             # if A>=0 return A, else negate
    0x7609: "write_tile_vram",         # write A to VRAM via ($A1),Y
    0x760F: "advance_vram_row",        # add 32 to $A1/$A2 (next row)
    0x7621: "display_bcd_byte",        # split A into nibbles --> digits
    0x73EB: "display_text_by_idx",     # index A --> text string --> VRAM
    0x5AE4: "add_to_score",            # BCD add A/$9B to player score
    0x4F7D: "obj_collision_score",     # collision detection + score award
    0x4348: "movement_subtimer",       # decrement obj timers --> phase change
    0x6083: "clamp_trackball",         # clamp input delta to +/-8
    0x77D2: "quadrature_decode",       # decode 2-bit trackball encoder
    0x602D: "mushroom_backup_restore", # compress/restore mushroom field bitmap
    0x4407: "new_wave_setup",          # player+millipede chain init for new wave
    0x6DF6: "cocktail_sprite_fixup",   # sprite handling for cocktail cabinet
    0x6BC7: "sound_engine_update",     # POKEY sound engine tick
    0x6AB8: "output_latch_update",     # update output latches from mirrors
    0x77F0: "read_trackball_analog",   # read raw IN0/IN1 trackball values
    0x78CC: "earom_read_byte",          # read one EAROM byte at address X
    0x78BE: "earom_read_all",          # read all 27 EAROM bytes
    0x7846: "earom_validate",          # validate/load EAROM defaults
    0x6379: "restore_mushroom_row",    # restore one row of mushrooms
    0x40F9: "bee_movement_tick",       # single bee movement step
    0x4D47: "check_field_collision",   # check if object hit mushroom field
    0x4153: "bee_boundary_check",      # bee screen boundary check
    0x415E: "get_spawn_threshold",     # get wave-based spawn threshold
    0x4179: "spawn_enemy_slot",        # spawn enemy in given slot
}

# Block-level comments for major subroutines
BLOCK_COMMENTS = {
    0x401F: [
        "; Main game loop entry point. Called after RESET initialization.",
        "; Calls frame_init once, then loops on wait_vblank calling ~24",
        "; subroutines per frame for all game logic.",
    ],
    0x4026: [
        "; Per-frame VBLANK wait loop. Spins on $9A (set by IRQ handler).",
        "; After VBLANK: watchdog, self-test check, then all game subsystems.",
    ],
    0x408E: [
        "; Bee/dragonfly handler. Animates bee sprites (tiles $38-$3A),",
        "; manages their sinusoidal movement, and drives POKEY2 channel 4",
        "; for the buzzing sound effect. Iterates object slots 0-12.",
    ],
    0x40F9: [
        "; Single bee movement tick. Updates Y/X position for one bee slot.",
        "; Checks field collision and screen boundaries.",
    ],
    0x41CD: [
        "; Swarm/earwig enemy logic. Handles tiles $34-$37 (4-frame anim).",
        "; Swarm enemies move in packs, changing direction at field edges.",
    ],
    0x43A7: [
        "; DDT bomb spawner. Randomly places DDT bombs on the playfield from",
        "; a pool tracked by $02CA. Position indexed by wave number.",
    ],
    0x4407: [
        "; New wave setup. Initializes player (tile=$03), spawns millipede",
        "; chain (slots 1-N based on $AB segment count), fills remaining",
        "; slots as independent head segments. Sets difficulty for wave.",
    ],
    0x454F: [
        "; Inchworm spawner & movement. Inchworms (tiles $1C-$1D) crawl",
        "; horizontally and damage mushrooms. Spawned in slot 12.",
    ],
    0x45FC: [
        "; Death animation handler. Manages death puff sprites ($FA/$FF),",
        "; decrements freeze timer ($02CE). While frozen, all enemy",
        "; movement routines exit early.",
    ],
    0x47E8: [
        "; Spider and bee movement. Spiders (tiles $1E-$1F) follow",
        "; sinusoidal paths. Scoring depends on proximity to player:",
        "; 300/600/900 points for far/mid/close kills.",
    ],
    0x4900: [
        "; Frame initialization. Called once per round/life.",
        "; Resets audio state and object slots for new round.",
    ],
    0x4A31: [
        "; Mosquito/flea spawner. Vertical droppers that fall straight down",
        "; the screen, creating new mushrooms in their path.",
    ],
    0x4B21: [
        "; Millipede movement AI -- the core gameplay routine.",
        "; Iterates slots 0-11 for millipede segments. Head ($3D) and body",
        "; ($39) segments traverse rows horizontally. On hitting an edge or",
        "; mushroom: drop one row (Y +/-8) and reverse horizontal direction.",
        "; Heads have smarter targeting; segments promote to heads when the",
        "; leading segment is killed. Speed increases at lower rows.",
    ],
    0x4C9F: [
        "; Player movement. Reads trackball accumulators ($CC/$CD, set by IRQ),",
        "; clamps delta to +/-8 via clamp_trackball, updates player position",
        "; ($2F=Y, $3F=X). Collision-checks new position via xy_to_vram;",
        "; if tile != 0, movement is blocked. Clamps Y to $0B-$F4.",
    ],
    0x4DC5: [
        "; Bullet movement and collision detection. Moves the player's shot",
        "; upward each frame, tests for hits against mushrooms and enemies.",
        "; On mushroom hit: damages mushroom (4 hit stages -> destroy).",
        "; On enemy hit: calls obj_collision_score for appropriate points.",
    ],
    0x4F7D: [
        "; Object collision/scoring. Given object in slot X, determines what",
        "; was hit based on tile code. Returns score in Y. Handles:",
        ";   millipede body->head promotion, spider proximity scoring,",
        ";   mushroom damage, DDT chain reactions.",
    ],
    0x50CA: [
        "; Player death sound. When $02CB != 0 (death timer active),",
        "; plays a descending pitch sound effect. Also causes the inner",
        "; game loop (enemy movement) to be skipped.",
    ],
    0x5105: [
        "; Extra enemy spawner. Spawns spider, beetle, mosquito, or flea",
        "; depending on current wave number and difficulty settings.",
        "; Higher waves spawn more aggressive enemy types.",
    ],
    0x5310: [
        "; DDT cloud expansion. When player triggers a DDT bomb, slot 0",
        "; transitions through tiles $10-$13 (expanding cloud stages).",
        "; Any millipede segment touching the cloud is killed.",
    ],
    0x53EC: [
        "; Wave-complete detection. Scans all enemy slots; if all millipede",
        "; segments are dead, triggers new_wave_setup for next wave.",
        "; Increments wave counter ($BA).",
    ],
    0x5592: [
        "; Attract mode / credit display. Manages coin counting, credit",
        "; display, start button lamp, and DIP switch configuration.",
        "; In attract mode ($96=$FF): displays demo text messages.",
        "; In gameplay: returns immediately.",
    ],
    0x57A4: [
        "; Game-active gate. Returns N=1 (game active) or N=0 (attract).",
        "; Checks $D3 AND $D4: both $FF = no players active = attract.",
        "; When active: displays PLAYER N text, monitors fire button",
        "; shift register ($AA). 5 consecutive presses initiates game.",
    ],
    0x5935: [
        "; Millipede segment spawner. When $A7 (spawn_request) is nonzero,",
        "; finds an empty slot and spawns a new millipede segment.",
    ],
    0x599C: [
        "; XY-to-VRAM lookup (core collision function). Takes Y position",
        "; in A (divided by 8), X position in $9B. Computes VRAM address",
        "; -> ($94/$95). Returns tile value at that location (AND #$7F",
        "; strips the 'processed' flag). Used for ALL field collisions.",
    ],
    0x5A03: [
        "; Mushroom field scroll/regeneration. Restores damaged mushrooms",
        "; between waves, scoring bonus points for each restoration.",
    ],
    0x5AE4: [
        "; Add to score. BCD-adds A (low) + $9B (high) to current player's",
        "; score ($B6/$B8/$BA). Checks extra-life thresholds. Uses SED.",
    ],
    0x5B4B: [
        "; Extra life check. Compares current score against bonus threshold",
        "; ($BC/$BE). Awards extra life and advances threshold.",
    ],
    0x5D73: [
        "; Timed play / attract logic. Handles $96 game state countdown.",
        "; Returns C=0 (continue game logic) or C=1 (skip game logic).",
    ],
    0x61DC: [
        "; Level transition. Manages mushroom restoration between waves",
        "; and during attract mode cycling ($FF <-> $FE).",
    ],
    0x620E: [
        "; Display initialization. Renders score labels, playfield borders,",
        "; and other static UI elements to VRAM at round start.",
    ],
    0x6878: [
        "; Mushroom field redraw. Full field processing loop, called when",
        "; $0305 (field_active) is nonzero.",
    ],
    0x6AB8: [
        "; Output latch update. Copies $D6-$D8 mirrors to hardware latches.",
    ],
    0x6BC7: [
        "; Sound engine update. Called from IRQ handler during VBLANK.",
        "; Processes POKEY sound channel state for all active effects.",
    ],
    0x6DF6: [
        "; Cocktail cabinet sprite fixup. Adjusts sprite coordinates and",
        "; flip bits for player 2 in cocktail (flip-screen) mode.",
    ],
    0x73EB: [
        "; Display text by index. Maps index A -> pointer from table at",
        "; $7454 -> writes string to VRAM. Bit 7 of last char = terminator.",
    ],
    0x7600: [
        "; Negate A. Computes two's complement: EOR #$FF / CLC / ADC #1.",
        "; Used extensively for flip-screen coordinate inversion.",
    ],
    0x7643: [
        "; IRQ handler (VBLANK, ~61 Hz). Saves registers, checks VBLANK via",
        "; IN0 bit 6. If VBLANK: sets vblank_flag, increments frame timer,",
        "; reads trackball/joystick input, copies 16 object slots to sprite",
        "; RAM ($13C0-$13FF), calls sound engine. Acknowledges IRQ at $2600.",
    ],
    0x7929: [
        "; Hardware RESET. SEI/CLD, stack=$FF. Clears all RAM ($0000-$03FF)",
        "; and VRAM ($1000-$13FF). Silences both POKEYs. Loads default",
        "; palette. Runs walking-bit RAM test. On success: initializes game",
        "; state to attract mode ($96=$FF), reads DIP switches, loads EAROM",
        "; high scores, then jumps to game_main_loop.",
    ],
    0x77D2: [
        "; Quadrature decoder for trackball. Decodes 2-bit optical encoder",
        "; state changes into +/-1 movement delta.",
    ],
    0x78CC: [
        "; Read one EAROM byte. Address in X is latched via $2780,X.",
        "; Sequences the ER2055 control lines through $2700 (EAROM_CTRL):",
        ";   $08 = CS1+READ mode, CK=0   (select chip, read mode)",
        ";   $09 = CS1+READ mode, CK=1   (clock high -- latch data)",
        ";   $08 = CS1+READ mode, CK=0   (clock low -- complete pulse)",
        ";   Read $2030 (EAROM_READ) to get the data byte",
        ";   $00 = deselect chip (standby)",
        "; ER2055 control bits: b0=CK, b1=/C1, b2=C2, b3=CS1",
        ";   C1=1,C2=0 = READ | C1=0,C2=1 = WRITE | C1=0,C2=0 = STANDBY",
    ],
    0x78E5: [
        "; Incremental EAROM save. Called every frame; only acts every 4th.",
        "; If earom_dirty ($F4) has bits set, commits the current byte to",
        "; the ER2055 EAROM chip via the control sequence at $2700:",
        ";   $00 = idle/deselect",
        ";   $02 = /C1=0,C2=0 -- standby mode setup",
        ";   $0A = CS1+standby -- triggers internal store cycle",
        "; On verify failure (read-back mismatch), re-writes via:",
        ";   $06 = C2=1,/C1=0 -- write mode setup",
        ";   Write address+data to $2780,X (EAROM_WRITE)",
        ";   $0E = CS1+C2=1,/C1=0 -- write mode, chip selected (commit)",
    ],
    0x78BE: [
        "; EAROM read all. Reads 27 bytes from EAROM chip into shadow",
        "; buffer at $0288-$02A2 (high scores, initials, bookkeeping).",
        "; Loops X=26..0, calling earom_read_byte for each address.",
    ],
}

def build_label_name(addr, subs, branch_targets, vectors):
    """Generate a label name for an address."""
    if addr in vectors:
        return vectors[addr]
    if addr in KNOWN_LABELS:
        return KNOWN_LABELS[addr]
    if addr in subs:
        return f"sub_{addr:04X}"
    if addr in branch_targets:
        return f"L{addr:04X}"
    return None


def detect_data_tables(rom, code_addrs):
    """
    Detect likely data tables: runs of bytes in ROM not traced as code.
    Returns set of ranges (start, end) that are data.
    """
    # Everything in ROM that's not code is data
    data_addrs = set()
    for addr in range(ROM_BASE, ROM_BASE + ROM_SIZE):
        if addr not in code_addrs:
            data_addrs.add(addr)
    return data_addrs


def disassemble(rom):
    """Full disassembly of the Millipede ROM."""
    # Get vectors (mirrored: $7FFA-$7FFF = $FFFA-$FFFF)
    nmi_vec  = rom_word(rom, 0x7FFA)
    reset_vec = rom_word(rom, 0x7FFC)
    irq_vec  = rom_word(rom, 0x7FFE)

    print(f"\nVectors:")
    print(f"  NMI:   ${nmi_vec:04X}")
    print(f"  RESET: ${reset_vec:04X}")
    print(f"  IRQ:   ${irq_vec:04X}")

    vectors = {}
    if nmi_vec:
        vectors[nmi_vec] = "NMI_handler"
    if reset_vec:
        vectors[reset_vec] = "RESET"
    if irq_vec:
        vectors[irq_vec] = "IRQ_handler"

    # Add known labels derived from analysis
    known = {
        0x4000: "cold_start",       # JMP to RESET (power-on entry)
        0x401F: "game_main_loop",   # main game loop entry (after init)
        0x4026: "wait_vblank",      # LSR vblank_flag / BCC wait loop
        0x7930: "clear_all_memory", # zero RAM + VRAM loop
        0x7958: "silence_audio",    # zero all POKEY channels
        0x7966: "load_palette",     # load default palette from ROM tables
        0x797D: "ram_test",         # walking-bit RAM test
        0x7A31: "init_game_state",  # set attract mode, read DIPs, init EAROM
    }
    for addr, name in known.items():
        vectors[addr] = name

    # Merge KNOWN_LABELS into subroutine names
    for addr, name in KNOWN_LABELS.items():
        vectors[addr] = name

    # Trace code from all entry points
    entry_points = []
    if reset_vec:
        entry_points.append(reset_vec)
    if nmi_vec:
        entry_points.append(nmi_vec)
    if irq_vec:
        entry_points.append(irq_vec)

    print(f"\nTracing code from {len(entry_points)} entry points...")
    code_addrs = trace_code(rom, entry_points)
    print(f"  Found {len(code_addrs)} code bytes")

    # Find subroutine entries and branch targets
    subs = find_subroutine_entries(rom, code_addrs)
    branch_targets = find_branch_targets(rom, code_addrs)
    all_labels = subs | branch_targets | set(vectors.keys())
    print(f"  Found {len(subs)} subroutines, {len(branch_targets)} branch targets")

    # Build label map
    labels = {}
    for addr in sorted(all_labels):
        name = build_label_name(addr, subs, branch_targets, vectors)
        if name:
            labels[addr] = name

    # Also collect all JMP (indirect) targets for reference
    jmp_ind_targets = set()
    for pc in sorted(code_addrs):
        opcode = rom_byte(rom, pc)
        if opcode == 0x6C:  # JMP (ind)
            target = rom_word(rom, pc + 1)
            jmp_ind_targets.add(target)

    data_addrs = detect_data_tables(rom, code_addrs)

    # ---- Generate assembly output ----
    lines = []
    lines.append("; ===========================================================================")
    lines.append("; Millipede (Atari, 1982) -- Full Annotated Disassembly")
    lines.append("; 6502 @ 1.512 MHz")
    lines.append(";")
    lines.append("; ROM: 16KB ($4000-$7FFF), mirrored at $8000-$FFFF")
    lines.append("; Generated by disasm_millipede.py")
    lines.append("; ===========================================================================")
    lines.append(";")
    lines.append("; GAME STRUCTURE OVERVIEW")
    lines.append("; ----------------------")
    lines.append("; The main loop (game_main_loop) calls ~24 subroutines per frame:")
    lines.append(";   1. attract_credit_display  - coin/credit handling, attract text")
    lines.append(";   2. score_display            - render P1/P2/high scores to VRAM")
    lines.append(";   3. game_active_gate         - check start button; N=1-->play, N=0-->attract")
    lines.append(";   4. earom_save               - incremental EAROM high-score persistence")
    lines.append(";   5. timed_play_logic         - timed play countdown / attract state")
    lines.append(";   6. death_anim_handler       - death puff/decay + freeze timer")
    lines.append(";   7. player_movement          - trackball --> player sprite position")
    lines.append(";   8. ddt_bomb_spawner         - DDT bomb placement")
    lines.append(";   9. bullet_move_collide      - bullet movement + hit detection")
    lines.append(";  10. millipede_spawner         - spawn millipede segments")
    lines.append(";  11. death_sound               - descending pitch death SFX")
    lines.append(";  12. millipede_movement        - head/body AI (turns, drops, targeting)")
    lines.append(";  13. extra_enemy_spawner       - spider/beetle/mosquito/flea by wave")
    lines.append(";  14. spider_bee_movement       - sinusoidal pattern + proximity scoring")
    lines.append(";  15. inchworm_handler           - horizontal crawler + mushroom damage")
    lines.append(";  16. bee_dragonfly_handler      - animation + POKEY2 buzzing sound")
    lines.append(";  17. mosquito_flea_spawner      - vertical dropper, creates mushrooms")
    lines.append(";  18. swarm_enemy_logic          - earwig/swarm movement")
    lines.append(";  19. ddt_cloud_expand           - DDT poison cloud growth")
    lines.append(";  20. level_transition           - mushroom restoration between waves")
    lines.append(";  21. wave_complete_check        - all enemies dead --> next wave")
    lines.append(";  22. mushroom_field_regen       - restore damaged mushrooms")
    lines.append(";  23. extra_life_check           - score vs threshold --> bonus life")
    lines.append(";  24. mushroom_field_redraw      - full field rendering")
    lines.append(";")
    lines.append("; OBJECT SYSTEM (16 sprite slots, 6 parallel ZP arrays)")
    lines.append("; -------------------------------------------------------")
    lines.append(";   $00-$0F,X = tile/char code    $10-$1F,X = Y direction/speed")
    lines.append(";   $20-$2F,X = X position        $30-$3F,X = Y position")
    lines.append(";   $40-$4F,X = move speed/dir    $50-$5F,X = color + active flag")
    lines.append(";")
    lines.append(";   Slot 0:    Player shooter (tile $03) / DDT cloud ($10-$13)")
    lines.append(";   Slots 1-11: Millipede segments + enemy reuse")
    lines.append(";   Slot 12:   DDT bomb / overflow enemy")
    lines.append(";   Slots 13-15: Bullets / special effects")
    lines.append(";")
    lines.append("; GAME STATE ($96): $FF=attract, $00=normal play, $01+=timed play")
    lines.append("; PLAYER ACTIVE: $D3/$D4 = $FF means player not active; both $FF = attract")
    lines.append(";")
    lines.append("; ENEMY TYPES BY TILE:")
    lines.append(";   $00-$07: Millipede segments (animated)    $08-$09: Head turning")
    lines.append(";   $10-$13: DDT poison cloud (expanding)    $14-$1B: Beetle")
    lines.append(";   $1C-$1D: Inchworm                        $1E-$1F: Spider")
    lines.append(";   $34-$37: Swarm/earwig                    $38-$3A: Bee/dragonfly")
    lines.append(";   $FA/$FF: Death puff animation")
    lines.append(";")
    lines.append("; ===========================================================================")
    lines.append("")
    lines.append("; --- Hardware Equates ---")
    lines.append("")
    lines.append("; POKEY 1 ($0400-$040F)")
    lines.append("POKEY1_AUDF1    = $0400")
    lines.append("POKEY1_AUDC1    = $0401")
    lines.append("POKEY1_AUDF2    = $0402")
    lines.append("POKEY1_AUDC2    = $0403")
    lines.append("POKEY1_AUDF3    = $0404")
    lines.append("POKEY1_AUDC3    = $0405")
    lines.append("POKEY1_AUDF4    = $0406")
    lines.append("POKEY1_AUDC4    = $0407")
    lines.append("POKEY1_AUDCTL   = $0408     ; write: AUDCTL / read: ALLPOT (DSW1)")
    lines.append("POKEY1_STIMER   = $0409")
    lines.append("POKEY1_RANDOM   = $040A     ; read: random number")
    lines.append("POKEY1_POTGO    = $040B")
    lines.append("POKEY1_IRQEN    = $040E     ; write: IRQEN / read: IRQST")
    lines.append("POKEY1_SKCTL    = $040F     ; write: SKCTL / read: SKSTAT")
    lines.append("")
    lines.append("; POKEY 2 ($0800-$080F)")
    lines.append("POKEY2_AUDF1    = $0800")
    lines.append("POKEY2_AUDC1    = $0801")
    lines.append("POKEY2_AUDF2    = $0802")
    lines.append("POKEY2_AUDC2    = $0803")
    lines.append("POKEY2_AUDF3    = $0804")
    lines.append("POKEY2_AUDC3    = $0805")
    lines.append("POKEY2_AUDF4    = $0806")
    lines.append("POKEY2_AUDC4    = $0807")
    lines.append("POKEY2_AUDCTL   = $0808")
    lines.append("POKEY2_STIMER   = $0809")
    lines.append("POKEY2_RANDOM   = $080A")
    lines.append("POKEY2_POTGO    = $080B")
    lines.append("POKEY2_IRQEN    = $080E")
    lines.append("POKEY2_SKCTL    = $080F")
    lines.append("")
    lines.append("; Input Ports")
    lines.append("IN0             = $2000     ; P1 inputs / VBLANK (bit 6)")
    lines.append("IN1             = $2001     ; P2 inputs / trackball Y")
    lines.append("IN2             = $2010     ; joystick / coin / tilt")
    lines.append("IN3             = $2011     ; self-test / cabinet")
    lines.append("")
    lines.append("; Other I/O")
    lines.append("EAROM_READ      = $2030     ; EAROM data read")
    lines.append("PALETTE_BASE    = $2480     ; palette RAM ($2480-$249F, 32 bytes)")
    lines.append("OUT_LATCH_0     = $2500     ; output latch 0")
    lines.append("OUT_LATCH_1     = $2501     ; output latch 1")
    lines.append("OUT_LATCH_2     = $2502     ; output latch 2")
    lines.append("OUT_LATCH_3     = $2503     ; output latch 3")
    lines.append("OUT_LATCH_4     = $2504     ; output latch 4")
    lines.append("OUT_TBEN        = $2505     ; trackball enable (D7)")
    lines.append("OUT_FLIP        = $2506     ; flip screen (D7)")
    lines.append("OUT_INT_ACK     = $2507     ; interrupt acknowledge (D7)")
    lines.append("IRQ_ACK         = $2600     ; IRQ acknowledge (write any value)")
    lines.append("WATCHDOG        = $2680     ; watchdog reset")
    lines.append("EAROM_CTRL      = $2700     ; EAROM control latch (ER2055)")
    lines.append(";                             ;   b0=CK (clock), b1=/C1, b2=C2, b3=CS1")
    lines.append(";                             ;   READ=$08, WRITE=$06, STANDBY=$00")
    lines.append(";                             ;   +CS1 (b3) to select chip; +CK (b0) to clock")
    lines.append("EAROM_WRITE     = $2780     ; EAROM address+data latch ($2780-$27BF)")
    lines.append("")
    lines.append("; Memory Map")
    lines.append("RAM_START       = $0000")
    lines.append("RAM_END         = $03FF")
    lines.append("VRAM_START      = $1000     ; 960 bytes of video RAM")
    lines.append("VRAM_END        = $13BF")
    lines.append("SPRITE_RAM      = $13C0     ; 64 bytes, 16 sprites x 4")
    lines.append("SPRITE_RAM_END  = $13FF")
    lines.append("ROM_START       = $4000")
    lines.append("")
    lines.append("; --- Zero-Page Game Variables ---")
    lines.append("game_state      = $96       ; $FF=attract, $00=normal, $01+=timed")
    lines.append("current_player  = $98       ; 0=P1, 1=P2")
    lines.append("vblank_flag     = $9A       ; set by IRQ, cleared by main loop")
    lines.append("p1_lives        = $A5       ; P1 lives remaining")
    lines.append("p2_lives        = $A6       ; P2 lives remaining")
    lines.append("p1_segments     = $AB       ; millipede segments to kill (P1)")
    lines.append("p1_score_lo     = $B6       ; P1 score low (BCD)")
    lines.append("p1_score_mid    = $B8       ; P1 score mid (BCD)")
    lines.append("p1_score_hi     = $BA       ; P1 score high + wave counter (BCD)")
    lines.append("p1_active       = $D3       ; $FF=not playing, else active")
    lines.append("p2_active       = $D4       ; $FF=not playing, else active")
    lines.append("credit_count    = $D9       ; number of credits")
    lines.append("flip_screen     = $EF       ; $00=normal, $80=cocktail P2")
    lines.append("dip_switch_1    = $F7       ; DSW1 (coinage, bonus)")
    lines.append("dip_switch_2    = $E4       ; DSW2 (lives, difficulty)")
    lines.append("")
    lines.append("; --- RAM Variables ($0200+) ---")
    lines.append("death_timer     = $02CB     ; nonzero = player dying (countdown)")
    lines.append("freeze_timer    = $02CE     ; nonzero = game frozen (death anim)")
    lines.append("ddt_bombs_left  = $02CA     ; DDT bombs available")
    lines.append("field_active    = $0305     ; nonzero = mushroom field needs redraw")
    lines.append("")
    lines.append(f"; Vectors (at $7FFA-$7FFF, mirrored to $FFFA-$FFFF)")
    lines.append(f";   NMI:   ${nmi_vec:04X}")
    lines.append(f";   RESET: ${reset_vec:04X}")
    lines.append(f";   IRQ:   ${irq_vec:04X}")
    lines.append("")

    if jmp_ind_targets:
        lines.append("; Indirect JMP targets (jump tables):")
        for t in sorted(jmp_ind_targets):
            lines.append(f";   JMP (${t:04X})")
        lines.append("")

    lines.append("")
    lines.append("    .org $4000")
    lines.append("")

    # Disassemble the full ROM
    pc = ROM_BASE
    in_data = False
    data_line_bytes = []
    data_line_addr = None

    def flush_data():
        nonlocal data_line_bytes, data_line_addr
        if data_line_bytes:
            hex_str = ",".join(f"${b:02X}" for b in data_line_bytes)
            lines.append(f"    .byte {hex_str}")
            data_line_bytes = []
            data_line_addr = None

    while pc < ROM_BASE + ROM_SIZE:
        # Check for vector table at end
        if pc == 0x7FFA:
            flush_data()
            lines.append("")
            lines.append("; ==========================================================================")
            lines.append("; 6502 Vector Table ($7FFA-$7FFF, mirrors to $FFFA-$FFFF)")
            lines.append("; ==========================================================================")
            lines.append("")
            nmi_label = labels.get(nmi_vec, f"${nmi_vec:04X}")
            reset_label = labels.get(reset_vec, f"${reset_vec:04X}")
            irq_label = labels.get(irq_vec, f"${irq_vec:04X}")
            lines.append(f"    .word {nmi_label:<20s} ; $FFFA: NMI vector")
            lines.append(f"    .word {reset_label:<20s} ; $FFFC: RESET vector")
            lines.append(f"    .word {irq_label:<20s} ; $FFFE: IRQ/BRK vector")
            break

        is_code = pc in code_addrs

        if is_code:
            flush_data()
            in_data = False

            # Label?
            if pc in labels:
                lines.append("")
                # Block-level comments for major subroutines
                if pc in BLOCK_COMMENTS:
                    lines.append("; " + "=" * 60)
                    for cline in BLOCK_COMMENTS[pc]:
                        lines.append(cline)
                    lines.append("; " + "=" * 60)
                elif pc in subs or pc in vectors.values() or pc in vectors:
                    lines.append("; " + "-" * 60)
                lines.append(f"{labels[pc]}:")

            opcode = rom_byte(rom, pc)
            if opcode not in OPCODES:
                # Unknown opcode byte in code area
                lines.append(f"    .byte ${opcode:02X}            ; ${pc:04X} - unknown opcode")
                pc += 1
                continue

            mnem, mode, nbytes = OPCODES[opcode]
            operand = format_operand(mode, rom, pc)

            # Build hex bytes string
            hex_bytes = " ".join(f"{rom_byte(rom, pc+i):02X}" for i in range(nbytes))

            # Build instruction string
            if operand:
                instr = f"{mnem} {operand}"
            else:
                instr = mnem

            # I/O comment
            comment = get_io_comment(mode, rom, pc)

            # Self-spin loop annotation
            if is_self_spin(rom, pc):
                comment = "  ; <<< SPIN LOOP (safety/watchdog catch)"

            # Add label references in comments for branches/jumps
            if mode == "rel":
                offset = rom_byte(rom, pc + 1)
                if offset >= 0x80:
                    offset -= 0x100
                target = (pc + 2 + offset) & 0xFFFF
                if target in labels:
                    instr = f"{mnem} {labels[target]}"
            elif mnem == "JMP" and mode == "abs":
                target = rom_word(rom, pc + 1)
                if target in labels:
                    instr = f"JMP {labels[target]}"
            elif mnem == "JSR":
                target = rom_word(rom, pc + 1)
                if target in labels:
                    instr = f"JSR {labels[target]}"

            # Format the line
            addr_str = f"{pc:04X}"
            line = f"    {instr:<32s}; {addr_str}: {hex_bytes}{comment}"
            lines.append(line)

            pc += nbytes
        else:
            # Data byte
            if not in_data:
                flush_data()
                # Check if we need a label here
                if pc in labels:
                    lines.append("")
                    lines.append(f"{labels[pc]}:")
                elif data_line_addr is None:
                    # Start a new data section comment
                    pass
                in_data = True
                data_line_addr = pc

                # Try to detect ASCII string at start of data region
                rom_offset = pc - ROM_BASE
                ascii_str, ascii_len = detect_ascii_string(rom, rom_offset)
                if ascii_str and ascii_len >= 4:
                    lines.append(f"                                    ; ASCII: \"{ascii_str}\"")

            if pc in labels and data_line_bytes:
                flush_data()
                lines.append("")
                lines.append(f"{labels[pc]}:")
                data_line_addr = pc

            b = rom_byte(rom, pc)
            data_line_bytes.append(b)
            if len(data_line_bytes) >= 16:
                # Add address comment on first line of data block
                if data_line_addr == pc - 15:
                    hex_str = ",".join(f"${b:02X}" for b in data_line_bytes)
                    lines.append(f"    .byte {hex_str:<64s}; ${data_line_addr:04X}")
                else:
                    flush_data()
                data_line_bytes = []
                data_line_addr = pc + 1
            pc += 1

    flush_data()

    # Statistics
    total_code = len(code_addrs)
    total_data = len(data_addrs & set(range(ROM_BASE, ROM_BASE + ROM_SIZE)))
    lines.append("")
    lines.append(f"; ==========================================================================")
    lines.append(f"; Statistics:")
    lines.append(f";   Code bytes: {total_code:5d} ({100*total_code/ROM_SIZE:.1f}%)")
    lines.append(f";   Data bytes: {total_data:5d} ({100*total_data/ROM_SIZE:.1f}%)")
    lines.append(f";   Total ROM:  {ROM_SIZE:5d} bytes (16KB)")
    lines.append(f";   Subroutines: {len(subs)}")
    lines.append(f";   Branch targets: {len(branch_targets)}")
    lines.append(f"; ==========================================================================")

    return "\n".join(lines)


def main():
    print("Millipede Arcade Disassembler")
    print("=" * 40)
    print(f"\nLoading ROMs from {ROM_DIR}...")
    rom = load_rom()

    asm = disassemble(rom)

    print(f"\nWriting {OUTPUT}...")
    with open(OUTPUT, "w", encoding="ascii", errors="replace") as f:
        f.write(asm)
        f.write("\n")

    line_count = asm.count("\n") + 1
    print(f"Done! {line_count} lines written.")


if __name__ == "__main__":
    main()

