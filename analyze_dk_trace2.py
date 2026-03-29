#!/usr/bin/env python3
"""Quick scan of full DK trace for flash writes and dispatch results."""
import re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

flash_writes = 0
interpret_calls = 0
dispatch_calls = 0
compile_returns = 0  # dispatch returns 1

# DK addresses from mlb:
# flash_byte_program = WRAM $6000 (CPU $6000)
# dispatch_on_pc = WRAM $623B
# interpret_6502 = bank0 $9225 (but called via trampoline)

# Also track: how often does the compile path fire?
# Look for the "cache_misses++" pattern — the INC instruction
# after the compile guard. Or look for bankswitch to bank 2 (compile bank).

frame_flash = {}  # frame -> count of flash writes
frame_interpret = {}  # frame -> count of interpret calls

# Sample every 1M lines for speed
SAMPLE = 500000
sample_lines = []

print("Scanning full trace (10M+ lines)...")
with open(TRACE, 'r') as f:
    for i, line in enumerate(f):
        text = line.rstrip()
        
        m = re.match(r'\s*([0-9A-Fa-f]{4})\s', text)
        if not m:
            continue
        pc = int(m.group(1), 16)
        
        # flash_byte_program entry
        if pc == 0x6000:
            flash_writes += 1
            fm = re.search(r'Fr:(\d+)', text)
            if fm:
                fr = int(fm.group(1))
                frame_flash[fr] = frame_flash.get(fr, 0) + 1
        
        # dispatch_on_pc entry
        if pc == 0x623B:
            dispatch_calls += 1
        
        # interpret_6502 - look for JSR to interpret
        if 'interpret_6502' in text or '_interpret_6502' in text:
            interpret_calls += 1
            fm = re.search(r'Fr:(\d+)', text)
            if fm:
                fr = int(fm.group(1))
                frame_interpret[fr] = frame_interpret.get(fr, 0) + 1
        
        # Track the STA that writes actual byte ($6041)
        if pc == 0x6041:
            fm = re.search(r'Fr:(\d+)', text)
            # Just count - we already counted via $6000 entry

        if i % 2000000 == 0 and i > 0:
            print(f"  ...{i/1000000:.0f}M lines scanned, {flash_writes} flash writes so far...")

print(f"\nTotal lines scanned: {i+1}")
print(f"Flash byte program calls: {flash_writes}")
print(f"Dispatch calls: {dispatch_calls}")
print(f"Interpret references: {interpret_calls}")

if frame_flash:
    print(f"\n=== Flash writes per frame (all frames with writes) ===")
    for fr in sorted(frame_flash.keys()):
        print(f"  Frame {fr}: {frame_flash[fr]} flash writes")
    print(f"\nTotal frames with flash activity: {len(frame_flash)}")

if frame_interpret:
    # Summarize interpret calls
    total_interp = sum(frame_interpret.values())
    print(f"\n=== Interpret calls summary ===")
    print(f"Total interpret calls: {total_interp}")
    print(f"Frames with interprets: {len(frame_interpret)}")
    # Show frames with most interprets
    sorted_frames = sorted(frame_interpret.items(), key=lambda x: -x[1])
    print("Top 20 frames by interpret count:")
    for fr, cnt in sorted_frames[:20]:
        print(f"  Frame {fr}: {cnt} interprets")
