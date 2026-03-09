#!/usr/bin/env python3
"""Analyze DK PRG ROM for instructions that reference ROM addresses."""

import sys

# 6502 instruction length table (by opcode)
SIZES = [0]*256
# Implied/Accumulator = 1 byte
for i in range(256): SIZES[i] = 1
# 2-byte instructions
for op in [0xA9,0xA2,0xA0,0xC9,0xE0,0xC0,0x29,0x09,0x49,0x69,0xE9]: SIZES[op] = 2  # imm
for op in [0xA5,0xA6,0xA4,0xC5,0xE4,0xC4,0x25,0x05,0x45,0x65,0xE5,0x85,0x86,0x84,0x24]: SIZES[op] = 2  # zp
for op in [0xB5,0xB4,0x95,0x94,0xB6,0x96,0xD5,0x35,0x15,0x55,0x75,0xF5]: SIZES[op] = 2  # zpx/zpy
for op in [0xE6,0xC6,0x06,0x46,0x26,0x66,0xF6,0xD6,0x16,0x56,0x36,0x76]: SIZES[op] = 2  # zp rmw
for op in [0x10,0x30,0x50,0x70,0x90,0xB0,0xD0,0xF0]: SIZES[op] = 2  # branches
for op in [0xA1,0x81,0xC1,0x21,0x01,0x41,0x61,0xE1]: SIZES[op] = 2  # (ind,X)
for op in [0xB1,0x91,0xD1,0x31,0x11,0x51,0x71,0xF1]: SIZES[op] = 2  # (ind),Y
SIZES[0x00] = 2  # BRK
# 3-byte instructions
for op in [0xAD,0xAE,0xAC,0xCD,0xEC,0xCC,0x2D,0x0D,0x4D,0x6D,0xED,0x2C,  # abs reads
           0x8D,0x8E,0x8C,  # abs stores
           0xBD,0xB9,0xBE,0xBC,0xDD,0xD9,0x3D,0x39,0x1D,0x19,0x5D,0x59,0x7D,0x79,0xFD,0xF9,  # abs,X/Y
           0x9D,0x99,  # STA abs,X/Y
           0xEE,0xCE,0x0E,0x4E,0x2E,0x6E,  # abs RMW
           0xFE,0xDE,0x1E,0x5E,0x3E,0x7E,  # abs,X RMW
           0x4C,0x20,0x6C]: SIZES[op] = 3  # JMP/JSR/JMPi

OPNAMES = {
    0xAD:'LDA', 0xAE:'LDX', 0xAC:'LDY', 0xCD:'CMP', 0xEC:'CPX', 0xCC:'CPY',
    0x2D:'AND', 0x0D:'ORA', 0x4D:'EOR', 0x6D:'ADC', 0xED:'SBC', 0x2C:'BIT',
    0xBD:'LDA,X', 0xB9:'LDA,Y', 0xBE:'LDX,Y', 0xBC:'LDY,X',
    0xDD:'CMP,X', 0xD9:'CMP,Y', 0x3D:'AND,X', 0x39:'AND,Y',
    0x1D:'ORA,X', 0x19:'ORA,Y', 0x5D:'EOR,X', 0x59:'EOR,Y',
    0x7D:'ADC,X', 0x79:'ADC,Y', 0xFD:'SBC,X', 0xF9:'SBC,Y',
    0x8D:'STA', 0x9D:'STA,X', 0x99:'STA,Y', 0x8E:'STX', 0x8C:'STY',
    0xEE:'INC', 0xCE:'DEC', 0x0E:'ASL', 0x4E:'LSR', 0x2E:'ROL', 0x6E:'ROR',
    0xFE:'INC,X', 0xDE:'DEC,X', 0x1E:'ASL,X', 0x5E:'LSR,X', 0x3E:'ROL,X', 0x7E:'ROR,X',
}

NONINDEXED_READS = {0xAD,0xAE,0xAC,0xCD,0xEC,0xCC,0x2D,0x0D,0x4D,0x6D,0xED,0x2C}
INDEXED_READS = {0xBD,0xB9,0xBE,0xBC,0xDD,0xD9,0x3D,0x39,0x1D,0x19,0x5D,0x59,0x7D,0x79,0xFD,0xF9}
RMW_OPS = {0xEE,0xCE,0x0E,0x4E,0x2E,0x6E,0xFE,0xDE,0x1E,0x5E,0x3E,0x7E}
STORE_OPS = {0x8D,0x9D,0x99,0x8E,0x8C}
# Instructions needing ROM data but pointing to $8000-$BFFF (the mirror range for NROM-128)
# DK is NROM-128: $C000-$FFFF has PRG, $8000-$BFFF mirrors it.

prg = open(r"roms\nes\_active.prg", "rb").read()
base_addr = 0xC000  # DK is NROM-128, PRG at $C000

ni_reads = []
idx_reads = []
rmw_reads = []
store_reads = []
rom_8000 = []  # Instructions referencing $8000-$BFFF (mirror)

i = 0
while i < len(prg):
    op = prg[i]
    sz = SIZES[op]
    if sz >= 3 and i + 2 < len(prg):
        addr = prg[i+1] | (prg[i+2] << 8)
        pc = base_addr + i
        name = OPNAMES.get(op, f'${op:02X}')
        
        # ROM references: $8000-$BFFF (mirror) or $C000-$FFFF
        if addr >= 0x8000:
            entry = f"  ${pc:04X}: {name:8s} ${addr:04X}"
            if addr >= 0x8000 and addr < 0xC000:
                rom_8000.append(entry + "  [MIRROR $8000-$BFFF]")
            
            if op in NONINDEXED_READS:
                ni_reads.append(entry)
            elif op in INDEXED_READS:
                val = prg[addr & 0x3FFF] if (addr & 0x3FFF) < len(prg) else '?'
                idx_reads.append(f"{entry}  (base byte={val})")
            elif op in RMW_OPS:
                rmw_reads.append(entry)
            elif op in STORE_OPS:
                store_reads.append(entry)
    i += sz

print(f"=== DK PRG-ROM Analysis ({len(prg)} bytes, base ${base_addr:04X}) ===")
print(f"\nNon-indexed absolute ROM reads (constant-foldable): {len(ni_reads)}")
for r in ni_reads:
    # Also show the ROM byte value
    parts = r.split('$')
    addr_str = parts[-1].strip()
    addr = int(addr_str, 16)
    rom_val = prg[addr & 0x3FFF] if (addr & 0x3FFF) < len(prg) else '?'
    print(f"{r}  = ${rom_val:02X}" if isinstance(rom_val, int) else f"{r}  = {rom_val}")

print(f"\nIndexed absolute ROM reads (need data table): {len(idx_reads)}")
for r in idx_reads: print(r)

# Collect unique base addresses for indexed
bases = {}
for r in idx_reads:
    # parse address from the entry
    parts = r.split('$')
    addr = int(parts[2].split()[0], 16)
    op_name = r.split(':')[1].split('$')[0].strip()
    key = f"{op_name} ${addr:04X}"
    bases[key] = bases.get(key, 0) + 1

print(f"\nUnique indexed base addresses:")
for k, v in sorted(bases.items()):
    print(f"  {k}  x{v}")

print(f"\nRMW on ROM: {len(rmw_reads)}")
for r in rmw_reads: print(r)

print(f"\nStores to ROM: {len(store_reads)}")
for r in store_reads: print(r)

print(f"\n$8000-$BFFF mirror references: {len(rom_8000)}")
for r in rom_8000: print(r)

# Also check indx/indy instructions (always interpreted currently)
indx_ops = {0xA1,0x81,0xC1,0x21,0x01,0x41,0x61,0xE1}
indy_ops = {0xB1,0x91,0xD1,0x31,0x11,0x51,0x71,0xF1}
indx_count = 0
indy_count = 0
indy_list = []
i = 0
while i < len(prg):
    op = prg[i]
    if op in indx_ops: indx_count += 1
    if op in indy_ops:
        indy_count += 1
        zp = prg[i+1] if i+1 < len(prg) else 0
        pc = base_addr + i
        name = OPNAMES.get(op, f'${op:02X}')
        indy_list.append(f"  ${pc:04X}: op=${op:02X} zp=${zp:02X}")
    i += SIZES[op]

print(f"\n(ind,X) instructions (always interpreted): {indx_count}")
print(f"(ind),Y instructions (template compiled): {indy_count}")

# Also: SEI/CLI (always interpreted)
sei_cli = 0
i = 0
while i < len(prg):
    op = prg[i]
    if op in (0x58, 0x78): sei_cli += 1
    i += SIZES[op]
print(f"SEI/CLI (always interpreted): {sei_cli}")

# RTI
rti = sum(1 for i2 in range(len(prg)) if prg[i2] == 0x40 and SIZES[prg[i2]] == 1)
print(f"RTI (always interpreted): ... (need proper count)")
