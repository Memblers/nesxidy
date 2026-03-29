#!/usr/bin/env python3
"""Flash cache block waste analyzer for nesxidy NES recompiler.
Reads hex data from flash_dump.hex (one sector per line, space-separated hex).
Analyzes banks 4-6 (PRG ROM 0x10000-0x1BFFF) for recompiler block headers.

Block header format (8 bytes):
  [0-1] entry_pc (little-endian)
  [2-3] exit_pc  (little-endian)
  [4]   code_len
  [5]   epilogue_offset
  [6]   flags
  [7]   sentinel (0xAA = valid block)

Stride = 272 bytes (0x110). First header at sector offset 8.
15 slots per 4KB sector, 4 sectors per bank, 3 banks = 180 total slots.
"""

import sys, os

STRIDE = 272       # 0x110
HEADER_SIZE = 8
CODE_SPACE = STRIDE - HEADER_SIZE   # 264 bytes available for code
SENTINEL = 0xAA
SECTOR_SIZE = 4096
SLOTS_PER_SECTOR = 15
FIRST_HEADER_OFFSET = 8

def parse_sector(data, bank, sec_in_bank):
    """Parse one 4KB sector, return list of block info dicts."""
    blocks = []
    for slot in range(SLOTS_PER_SECTOR):
        offset = FIRST_HEADER_OFFSET + slot * STRIDE
        if offset + HEADER_SIZE > len(data):
            break
        hdr = data[offset:offset + HEADER_SIZE]
        if hdr[7] != SENTINEL:
            continue
        blocks.append({
            'bank': bank,
            'sector': sec_in_bank,
            'slot': slot,
            'entry_pc': hdr[0] | (hdr[1] << 8),
            'exit_pc':  hdr[2] | (hdr[3] << 8),
            'code_len': hdr[4],
            'epi_off':  hdr[5],
            'flags':    hdr[6],
        })
    return blocks

def main():
    hex_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'flash_dump.hex')
    if not os.path.exists(hex_path):
        print(f"ERROR: {hex_path} not found. Create it first.", file=sys.stderr)
        sys.exit(1)

    with open(hex_path, 'r') as f:
        lines = [l.strip() for l in f if l.strip()]

    if len(lines) != 12:
        print(f"WARNING: Expected 12 sectors, got {len(lines)}", file=sys.stderr)

    all_blocks = []
    total_slots = 0
    for i, hex_line in enumerate(lines):
        data = bytes.fromhex(hex_line.replace(' ', ''))
        bank = 4 + i // 4
        sec_in_bank = i % 4
        blocks = parse_sector(data, bank, sec_in_bank)
        all_blocks.extend(blocks)
        total_slots += SLOTS_PER_SECTOR

    empty = total_slots - len(all_blocks)

    print("=" * 60)
    print("  Flash Cache Block Analysis (Banks 4-6)")
    print("=" * 60)
    print(f"Total slots scanned : {total_slots}")
    print(f"Valid blocks found  : {len(all_blocks)}")
    print(f"Empty slots         : {empty}")
    print(f"Slot occupancy      : {100*len(all_blocks)/total_slots:.1f}%")
    print()

    if not all_blocks:
        print("No blocks found!")
        return

    code_lens = [b['code_len'] for b in all_blocks]
    wastes    = [CODE_SPACE - cl for cl in code_lens]
    total_code  = sum(code_lens)
    total_alloc = len(all_blocks) * CODE_SPACE

    print(f"Average code_len    : {total_code/len(all_blocks):.1f} bytes")
    print(f"Median  code_len    : {sorted(code_lens)[len(code_lens)//2]} bytes")
    print(f"Min     code_len    : {min(code_lens)} bytes")
    print(f"Max     code_len    : {max(code_lens)} bytes")
    print(f"Average waste/block : {sum(wastes)/len(wastes):.1f} bytes")
    print(f"Total code bytes    : {total_code}")
    print(f"Total allocated     : {total_alloc}")
    print(f"Utilization         : {100*total_code/total_alloc:.1f}%")
    print()

    # Code length distribution
    buckets = [(0,31),(32,63),(64,95),(96,127),(128,159),(160,191),(192,223),(224,264)]
    print("Code length distribution:")
    for lo, hi in buckets:
        count = sum(1 for cl in code_lens if lo <= cl <= hi)
        bar = '#' * count
        if count:
            print(f"  {lo:3d}-{hi:3d}: {count:3d} {bar}")
    print()

    # Per-bank breakdown
    print("Per-bank breakdown:")
    print(f"  {'Bank':>4}  {'Blocks':>6}  {'AvgCLen':>7}  {'TotCode':>7}  {'TotAlloc':>8}  {'Util%':>5}")
    for bank in range(4, 7):
        bb = [b for b in all_blocks if b['bank'] == bank]
        if bb:
            bc = [b['code_len'] for b in bb]
            ta = len(bb) * CODE_SPACE
            print(f"  {bank:>4}  {len(bb):>6}  {sum(bc)/len(bc):>7.1f}  {sum(bc):>7}  {ta:>8}  {100*sum(bc)/ta:>5.1f}")
    print()

    # All blocks detail
    print("Block details:")
    print(f"  {'Bank':>4} {'Sec':>3} {'Slot':>4} {'EntryPC':>8} {'ExitPC':>8} {'CLen':>4} {'Epi':>3} {'Flg':>5} {'Waste':>5}")
    print(f"  {'----':>4} {'---':>3} {'----':>4} {'--------':>8} {'--------':>8} {'----':>4} {'---':>3} {'-----':>5} {'-----':>5}")
    for b in all_blocks:
        waste = CODE_SPACE - b['code_len']
        flg = f"0x{b['flags']:02X}"
        print(f"  {b['bank']:>4} {b['sector']:>3} {b['slot']:>4}   ${b['entry_pc']:04X}     ${b['exit_pc']:04X}  {b['code_len']:>4} {b['epi_off']:>3} {flg:>5} {waste:>5}")

if __name__ == '__main__':
    main()
