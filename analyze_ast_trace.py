#!/usr/bin/env python3
"""Analyze Asteroids host+guest traces to diagnose NMI hang.

Reads the 1.5 GB Mesen host trace (asteroids.txt) efficiently using
buffered line scanning, plus the small Lua guest trace.

Usage:
    python analyze_ast_trace.py
"""
import os, sys, re, time
from collections import Counter, defaultdict

PROJECT = r"c:\proj\c\NES\nesxidy-co\nesxidy"
HOST_TRACE = os.path.join(os.environ["USERPROFILE"],
    r"OneDrive\Documents\Mesen2\Debugger\asteroids.txt")
GUEST_TRACE = os.path.join(PROJECT, "nes_trace_lua.txt")

# --- Addresses of interest (from MLB / vicemap.map) ---
STEP6502   = 0x9221   # _step6502 in PRG (bank-dependent)
INTERP6502 = 0x92C6   # _interpret_6502
DISPATCH   = 0x6286   # _dispatch_on_pc in WRAM
ZP_PC      = 0x67     # guest _pc lo
ZP_PC_HI   = 0x68     # guest _pc hi
NMI6502    = None      # we'll find this
RESET6502  = None
RAM_CLEAR_DONE = 0x7D04  # guest addr after clear loop ends (STA $00,X → next)

def analyze_guest():
    """Parse the small Lua guest trace."""
    print("=" * 60)
    print("GUEST TRACE ANALYSIS")
    print("=" * 60)
    if not os.path.exists(GUEST_TRACE):
        print(f"  NOT FOUND: {GUEST_TRACE}")
        return
    lines = []
    with open(GUEST_TRACE, "r") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            lines.append(line.rstrip())

    print(f"  Total lines: {len(lines)}")
    if not lines:
        return

    # Distinct PCs
    pcs = []
    for l in lines:
        m = re.match(r"([0-9A-Fa-f]{4})", l)
        if m:
            pcs.append(int(m.group(1), 16))

    unique_pcs = sorted(set(pcs))
    print(f"  Distinct PCs: {len(unique_pcs)}")
    for p in unique_pcs:
        cnt = pcs.count(p)
        print(f"    ${p:04X}  ×{cnt}")

    # Show first 20 and last 20
    print(f"\n  First 20 entries:")
    for l in lines[:20]:
        print(f"    {l}")
    print(f"\n  Last 10 entries:")
    for l in lines[-10:]:
        print(f"    {l}")

    # Detect where NMI takes over
    for i, p in enumerate(pcs):
        if p == 0x7B65:  # NMI entry
            fr_match = re.search(r"Fr:(\d+)", lines[i])
            fr = fr_match.group(1) if fr_match else "?"
            print(f"\n  First NMI entry at line {i}, PC=${p:04X}, frame={fr}")
            if i > 0:
                print(f"    Previous: {lines[i-1]}")
            print(f"    NMI line: {lines[i]}")
            break

    # Track S register decline (stack overflow from repeated NMI)
    sp_values = []
    for l in lines:
        m = re.search(r"S:([0-9A-Fa-f]{2})", l)
        if m:
            sp_values.append(int(m.group(1), 16))
    if sp_values:
        print(f"\n  Guest SP: start=${sp_values[0]:02X}, min=${min(sp_values):02X}, end=${sp_values[-1]:02X}")
        # Count NMI entries (PC=$7B65)
        nmi_count = sum(1 for p in pcs if p == 0x7B65)
        print(f"  Total NMI entries: {nmi_count}")

def analyze_host():
    """Scan the large host trace for key events."""
    print("\n" + "=" * 60)
    print("HOST TRACE ANALYSIS")
    print("=" * 60)
    if not os.path.exists(HOST_TRACE):
        print(f"  NOT FOUND: {HOST_TRACE}")
        return

    fsize = os.path.getsize(HOST_TRACE)
    print(f"  File size: {fsize:,} bytes ({fsize/1024/1024:.1f} MB)")

    # Key patterns to search for (compiled for speed)
    # We want to find:
    # 1. When step6502/interpret_6502 is called
    # 2. When nmi6502 is called (JSR to nmi handler)
    # 3. What guest PC value is at key moments
    # 4. The RAM clear loop progress

    # NES CPU address patterns
    # step6502 = $9221, nmi6502 = find from trace
    # Guest PC at ZP $67-$68

    # For a 1.5GB file we do a single pass, collecting stats
    line_count = 0
    first_lines = []
    last_lines = []

    # Track NES CPU PCs
    nes_pc_counter = Counter()

    # Track writes to ZP $67 (guest PC lo)
    # Look for STA $67 or STX $67 patterns
    zp67_writes = 0
    zp68_writes = 0

    # NMI vector fetch (NES reads $FFFA/$FFFB)
    nmi_vector_reads = 0
    rti_count = 0

    # Find references to step6502/nmi6502
    step_calls = 0
    nmi_calls = 0

    # Track frame numbers
    max_frame = 0

    # Key address hits
    addr_9221_hits = 0  # step6502
    addr_92C6_hits = 0  # interpret_6502

    # Sample lines where guest PC changes
    pc_change_samples = []

    t0 = time.time()
    RING_SIZE = 50

    with open(HOST_TRACE, "r", buffering=8*1024*1024) as f:
        for line in f:
            line_count += 1
            if line_count <= 50:
                first_lines.append(line.rstrip())

            # Keep last N lines in a ring
            if len(last_lines) >= RING_SIZE:
                last_lines.pop(0)
            last_lines.append(line.rstrip())

            # Extract NES CPU PC (first 4 hex chars)
            if len(line) >= 4 and line[0] in "0123456789ABCDEF":
                try:
                    nes_pc = int(line[:4], 16)
                except ValueError:
                    continue

                # Sample specific addresses
                if nes_pc == 0x9221:
                    addr_9221_hits += 1
                elif nes_pc == 0x92C6:
                    addr_92C6_hits += 1

                # Count top PCs (sample every 1000th to avoid memory blow-up)
                if line_count % 1000 == 0:
                    nes_pc_counter[nes_pc] += 1

            # Track frame number
            fr_m = re.search(r"Fr:(\d+)", line)
            if fr_m:
                fr = int(fr_m.group(1))
                if fr > max_frame:
                    max_frame = fr

            # Progress
            if line_count % 5_000_000 == 0:
                elapsed = time.time() - t0
                print(f"  ... {line_count/1e6:.1f}M lines, {elapsed:.1f}s")

    elapsed = time.time() - t0
    print(f"  Total lines: {line_count:,} ({elapsed:.1f}s)")
    print(f"  Max frame: {max_frame}")
    print(f"  step6502 ($9221) hits: {addr_9221_hits:,}")
    print(f"  interpret_6502 ($92C6) hits: {addr_92C6_hits:,}")

    print(f"\n  Top 20 NES CPU PCs (sampled every 1000th line):")
    for pc, cnt in nes_pc_counter.most_common(20):
        print(f"    ${pc:04X}  ×{cnt}")

    print(f"\n  First 30 host trace lines:")
    for l in first_lines[:30]:
        print(f"    {l}")

    print(f"\n  Last 30 host trace lines:")
    for l in last_lines[-30:]:
        print(f"    {l}")

def analyze_host_nmi_region():
    """Targeted scan: find the first NMI delivery and surrounding context."""
    print("\n" + "=" * 60)
    print("HOST TRACE — NMI DELIVERY SEARCH")
    print("=" * 60)
    if not os.path.exists(HOST_TRACE):
        return

    # Search for the NES CPU executing the NMI vector handler setup
    # nmi6502() pushes PC and status, then reads [$FFFA] for the NMI vector.
    # In our emulator, nmi6502 is a C function compiled to NES code.
    # Look for when NES CPU writes to ZP $67/$68 with values near $7B65 (NMI entry)
    # Or look for when the host code calls into the guest NMI area.

    # Actually simpler: search for lines where NES cpu writes guest PC ($67)
    # to the NMI handler address.  Or search for JSR _nmi6502.

    # Let's look for the transition: find lines near where guest PC becomes $7B65
    # We know from guest trace that NMI first fires around Fr:834.
    # So scan the host trace for Fr:833 and Fr:834 region.

    t0 = time.time()
    context_before = []
    found_regions = []
    line_count = 0
    CONTEXT = 10

    with open(HOST_TRACE, "r", buffering=8*1024*1024) as f:
        for line in f:
            line_count += 1

            # Keep rolling context
            context_before.append(line.rstrip())
            if len(context_before) > CONTEXT:
                context_before.pop(0)

            # Look for guest _pc being set to $7B65 area
            # This would be a STA/STX to $68 with value $7B
            # Or look for frame 834 region
            if "Fr:834 " in line and len(found_regions) == 0:
                # Capture context
                region = list(context_before)
                # Read ahead
                for _ in range(20):
                    try:
                        ahead = next(f).rstrip()
                        line_count += 1
                        region.append(ahead)
                    except StopIteration:
                        break
                found_regions.append(("First Fr:834", region))
                print(f"  Found Fr:834 at host line ~{line_count}")

            # Also find the first time step6502 is executed
            if line_count <= 500000 and line.startswith("9221"):
                if addr_9221_first is None:
                    pass  # will handle below

            if len(found_regions) >= 3:
                break

            if line_count % 5_000_000 == 0:
                print(f"  ... scanning {line_count/1e6:.1f}M lines")

    elapsed = time.time() - t0
    print(f"  Scanned {line_count:,} lines in {elapsed:.1f}s")

    for label, region in found_regions:
        print(f"\n  --- {label} ---")
        for l in region:
            print(f"    {l}")

def analyze_host_step6502_region():
    """Find where step6502 first runs and the first few guest instructions."""
    print("\n" + "=" * 60)
    print("HOST TRACE — STEP6502 FIRST EXECUTION")
    print("=" * 60)
    if not os.path.exists(HOST_TRACE):
        return

    t0 = time.time()
    context = []
    first_step_region = None
    first_nmi_region = None
    line_count = 0
    CONTEXT = 5

    # Also find first nmi6502 call
    # nmi6502 pushes return address then jumps to NMI routine.
    # In the host trace, look for execution of nmi6502's code.
    # From the map, _nmi6502 should be near step6502.

    with open(HOST_TRACE, "r", buffering=8*1024*1024) as f:
        for line in f:
            line_count += 1

            context.append(line.rstrip())
            if len(context) > CONTEXT:
                context.pop(0)

            # First step6502 execution
            if first_step_region is None and line.startswith("9221"):
                region = list(context)
                for _ in range(30):
                    try:
                        region.append(next(f).rstrip())
                        line_count += 1
                    except StopIteration:
                        break
                first_step_region = ("First step6502 ($9221)", line_count, region)

            # Stop after finding both or after 2M lines
            if first_step_region and line_count > 2_000_000:
                break

    elapsed = time.time() - t0
    print(f"  Scanned {line_count:,} lines in {elapsed:.1f}s")

    if first_step_region:
        label, lnum, region = first_step_region
        print(f"\n  --- {label} (line ~{lnum}) ---")
        for l in region:
            print(f"    {l}")

if __name__ == "__main__":
    analyze_guest()
    print("\n\nRunning host trace analysis (this may take a moment for 1.5GB)...")
    analyze_host_step6502_region()
    analyze_host()
