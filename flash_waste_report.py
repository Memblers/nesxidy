"""
Flash Cache Waste Analysis — reads flash banks 4-6 from Mesen MCP server
and computes block utilisation statistics.

Block header (8 bytes, stride 272 = 0x110):
  [0-1] entry_pc  (LE)
  [2-3] exit_pc   (LE)
  [4]   code_len
  [5]   epilogue_offset
  [6]   flags
  [7]   sentinel  (must be 0xAA)

CODE_SPACE = 264 bytes per slot.
15 slots per 4 KB sector, 4 sectors per bank, 3 banks (4-6) → 180 slots.
"""

import json, urllib.request, statistics, sys

MCP_URL = "http://localhost:51234/mcp"
MEMORY_TYPE_NES_PRG_ROM = 45
BANK_START = 4          # first flash bank
NUM_BANKS  = 3          # banks 4, 5, 6
SECTORS_PER_BANK = 4
SECTOR_SIZE = 4096
HEADER_SIZE = 8
STRIDE = 272            # 0x110
SLOTS_PER_SECTOR = 15
CODE_SPACE = STRIDE - HEADER_SIZE   # 264

# ── Mesen helpers ──────────────────────────────────────────────────────
def mcp_read_memory(start: int, length: int) -> bytes:
    """Read *length* bytes from NesPrgRom via the Mesen MCP JSON-RPC endpoint."""
    body = json.dumps({
        "jsonrpc": "2.0",
        "method": "tools/call",
        "params": {
            "name": "get_memory_range",
            "arguments": {
                "memory_type": MEMORY_TYPE_NES_PRG_ROM,
                "start_address": start,
                "length": length,
            },
        },
        "id": 1,
    }).encode()
    req = urllib.request.Request(MCP_URL, data=body,
                                headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        result = json.loads(resp.read())
    if "error" in result and result["error"]:
        raise RuntimeError(f"MCP error: {result['error']}")
    # The result payload contains a hex string in result.result.content[0].text
    # which looks like: {"memory_type":45,"start_address":...,"hex":"AA BB ..."}
    content = result["result"]["content"]
    if isinstance(content, list):
        text = content[0]["text"]
    else:
        text = content
    inner = json.loads(text) if isinstance(text, str) else text
    hex_str = inner["hex"]
    return bytes.fromhex(hex_str.replace(" ", ""))

# ── parsing ────────────────────────────────────────────────────────────
def parse_sector(data: bytes, bank: int, sec_in_bank: int):
    """Return a list of block-info dicts for every valid slot in *data*."""
    blocks = []
    for slot in range(SLOTS_PER_SECTOR):
        off = HEADER_SIZE + slot * STRIDE        # first 8 bytes of sector are sector header, blocks start at offset 8
        hdr = data[off : off + HEADER_SIZE]
        if len(hdr) < HEADER_SIZE:
            continue
        if hdr[7] != 0xAA:
            continue
        entry_pc = hdr[0] | (hdr[1] << 8)
        exit_pc  = hdr[2] | (hdr[3] << 8)
        code_len = hdr[4]
        epil_off = hdr[5]
        flags    = hdr[6]
        waste    = CODE_SPACE - code_len
        blocks.append({
            "bank": bank, "sector": sec_in_bank, "slot": slot,
            "entry_pc": entry_pc, "exit_pc": exit_pc,
            "code_len": code_len, "epil_off": epil_off,
            "flags": flags, "waste": waste,
        })
    return blocks

# ── main ───────────────────────────────────────────────────────────────
def main():
    all_blocks = []
    per_bank = {b: [] for b in range(BANK_START, BANK_START + NUM_BANKS)}

    print("Reading flash sectors from Mesen …")
    for bank in range(BANK_START, BANK_START + NUM_BANKS):
        for sec in range(SECTORS_PER_BANK):
            addr = bank * 0x4000 + sec * SECTOR_SIZE
            data = mcp_read_memory(addr, SECTOR_SIZE)
            blks = parse_sector(data, bank, sec)
            all_blocks.extend(blks)
            per_bank[bank].extend(blks)
            print(f"  Bank {bank} sector {sec} (PRG ${addr:05X}): {len(blks):2d} blocks")

    total_slots = SLOTS_PER_SECTOR * SECTORS_PER_BANK * NUM_BANKS
    valid       = len(all_blocks)
    empty       = total_slots - valid

    print("\n" + "=" * 64)
    print("FLASH CACHE WASTE ANALYSIS")
    print("=" * 64)
    print(f"Total slots:      {total_slots}")
    print(f"Valid blocks:     {valid}")
    print(f"Empty slots:      {empty}")
    print(f"Slot occupancy:   {valid/total_slots*100:.1f}%")

    if not all_blocks:
        print("\nNo valid blocks found – nothing more to report.")
        return

    code_lens = [b["code_len"] for b in all_blocks]
    wastes    = [b["waste"]    for b in all_blocks]

    print(f"\n── Code length stats ──")
    print(f"  Average:  {statistics.mean(code_lens):.1f}")
    print(f"  Median:   {statistics.median(code_lens):.1f}")
    print(f"  Min:      {min(code_lens)}")
    print(f"  Max:      {max(code_lens)}")

    total_code  = sum(code_lens)
    total_alloc = valid * CODE_SPACE
    total_waste = sum(wastes)
    utilization = total_code / total_alloc * 100 if total_alloc else 0

    print(f"\n── Waste summary ──")
    print(f"  Average waste/block: {statistics.mean(wastes):.1f} bytes")
    print(f"  Total code bytes:    {total_code}")
    print(f"  Total allocated:     {total_alloc}")
    print(f"  Total waste:         {total_waste}")
    print(f"  Utilization:         {utilization:.1f}%")

    # Per-bank breakdown
    print(f"\n── Per-bank breakdown ──")
    for bank in range(BANK_START, BANK_START + NUM_BANKS):
        blks = per_bank[bank]
        n = len(blks)
        if n == 0:
            print(f"  Bank {bank}: 0 blocks")
            continue
        cl = [b["code_len"] for b in blks]
        print(f"  Bank {bank}: {n:3d} blocks  avg_code={statistics.mean(cl):.1f}  "
              f"code_sum={sum(cl)}  waste_sum={sum(b['waste'] for b in blks)}")

    # Detailed block table
    print(f"\n── Block detail (sorted by entry_pc) ──")
    print(f"{'Bank':>4} {'Sec':>3} {'Slot':>4}  {'Entry':>6} {'Exit':>6} "
          f"{'CLen':>4} {'Waste':>5} {'Epil':>4} {'Flags':>5}")
    for b in sorted(all_blocks, key=lambda x: x["entry_pc"]):
        print(f"{b['bank']:4d} {b['sector']:3d} {b['slot']:4d}  "
              f"${b['entry_pc']:04X}  ${b['exit_pc']:04X} "
              f"{b['code_len']:4d} {b['waste']:5d} "
              f"{b['epil_off']:4d}  0x{b['flags']:02X}")

    # Also dump to flash_dump.hex for future use (won't re-read Mesen)
    # (skip if caller passes --no-dump)
    if "--no-dump" not in sys.argv:
        try:
            with open("flash_dump.hex", "w") as f:
                for bank in range(BANK_START, BANK_START + NUM_BANKS):
                    for sec in range(SECTORS_PER_BANK):
                        addr = bank * 0x4000 + sec * SECTOR_SIZE
                        raw = mcp_read_memory(addr, SECTOR_SIZE)
                        f.write(" ".join(f"{b:02X}" for b in raw) + "\n")
            print("\n(Saved raw sectors to flash_dump.hex)")
        except Exception as e:
            print(f"\n(Could not save flash_dump.hex: {e})")


if __name__ == "__main__":
    main()
