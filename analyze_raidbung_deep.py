#!/usr/bin/env python3
"""Deep analysis of Raid on Bungeling Bay DynaMoS trace — find guest execution patterns and failures."""
import sys, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_raidbung.txt"

def analyze_guest_execution():
    """Look at what _pc values DynaMoS sets and track guest dispatch patterns."""
    print("=== Deep DynaMoS Trace Analysis for Raid on Bungeling Bay ===\n")
    
    # Track key events
    pc_writes = []       # Writes to _pc (guest PC register)
    frame_boundaries = [] # Frame changes
    nmi_entries = []     # NMI handler entries (RTI or $C04C pattern)
    dispatch_calls = []  # Calls to dispatch
    stuck_loops = {}     # Detect stuck patterns
    last_frame = "1"
    
    # Track the last 20 PCs to detect loops
    recent_pcs = []
    loop_counts = {}
    
    # Gather addresses used by DynaMoS to track guest state
    guest_pc_sets = []
    
    line_count = 0
    max_lines = 500000
    
    # Collect lines where _pc is written
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        for i, line in enumerate(f):
            if i >= max_lines:
                break
            line_count = i
            
            # Track frame boundaries
            fr_match = re.search(r'Fr:(\d+)', line)
            if fr_match:
                frame = fr_match.group(1)
                if frame != last_frame:
                    frame_boundaries.append((i, int(frame)))
                    last_frame = frame
            
            # Look for writes to _pc (guest program counter)
            if '_pc' in line:
                pc_writes.append((i, line.strip()[:150]))
            
            # Look for NMI handler patterns (guest NMI = $C04C for this game)
            # In DynaMoS, NMI fires and runs guest NMI code
            if 'nmi6502' in line.lower() or 'RTI' in line:
                nmi_entries.append((i, line.strip()[:120]))
            
            # Track PC patterns to detect stuck loops
            m = re.match(r'^([0-9A-Fa-f]{4})\s+(.+?)(?:\s+A:|$)', line)
            if m:
                pc = int(m.group(1), 16)
                recent_pcs.append(pc)
                if len(recent_pcs) > 50:
                    recent_pcs.pop(0)
    
    print(f"Analyzed {line_count} lines")
    print(f"Frame boundaries: {len(frame_boundaries)}")
    if frame_boundaries:
        print(f"  Frames covered: 1 to {frame_boundaries[-1][1]}")
        # Show some frame timings
        for line_no, frame in frame_boundaries[:10]:
            print(f"  Frame {frame} starts at line {line_no}")
        if len(frame_boundaries) > 10:
            print(f"  ... ({len(frame_boundaries)} total frame transitions)")
            for line_no, frame in frame_boundaries[-5:]:
                print(f"  Frame {frame} starts at line {line_no}")
    
    print(f"\n=== _pc (guest PC) references ({len(pc_writes)} occurrences) ===")
    # Show first and last _pc writes
    shown = set()
    for line_no, text in pc_writes[:30]:
        print(f"  Line {line_no:>8}: {text}")
    if len(pc_writes) > 30:
        print(f"  ... ({len(pc_writes)} total)")
        for line_no, text in pc_writes[-10:]:
            print(f"  Line {line_no:>8}: {text}")
    
    print(f"\n=== RTI instructions (NMI returns) ===")
    for line_no, text in nmi_entries[:20]:
        print(f"  Line {line_no:>8}: {text}")
    if len(nmi_entries) > 20:
        print(f"  ... ({len(nmi_entries)} total)")


def find_end_pattern():
    """Look at the LAST part of the trace to see where execution ends up."""
    print("\n=== Last 200 lines of trace ===")
    # Read the last chunk
    lines = []
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            lines.append(line)
            if len(lines) > 200:
                lines.pop(0)
    
    total = 0
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        for _ in f:
            total += 1
    
    print(f"Total lines: {total}")
    print(f"Last 50 lines:")
    for j, line in enumerate(lines[-50:]):
        print(f"  {total - 50 + j:>10}: {line.rstrip()[:150]}")


def find_guest_dispatch():
    """Search for dispatch patterns and guest PC values being loaded."""
    print("\n=== Guest Code Dispatch Analysis ===")
    
    # In DynaMoS, the dispatcher loads a guest PC and jumps to compiled code
    # Look for patterns where _pc/$51/$52 is loaded (these hold guest PC)
    # Also look for the run6502/step6502 function calls
    
    dispatch_count = 0
    guest_pcs_seen = set()
    
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        prev_line = ""
        for i, line in enumerate(f):
            if i >= 300000:
                break
            
            # Look for STA _pc or LDA _pc patterns
            if '$51' in line or '$52' in line or '_pc' in line:
                m = re.match(r'^([0-9A-Fa-f]{4})\s+(.+?)(?:\s+A:|$)', line)
                if m:
                    instr = m.group(2).strip()
                    if 'STA' in instr and ('$51' in instr or '_pc' in instr):
                        # Extract A register value for the low byte of _pc
                        a_match = re.search(r'A:([0-9A-Fa-f]{2})', line)
                        if a_match:
                            a_val = int(a_match.group(1), 16)
                            if dispatch_count < 50:
                                print(f"  Line {i:>8}: {instr}  (A=${a_val:02X})")
                            dispatch_count += 1
            
            prev_line = line
    
    print(f"  Total _pc stores: {dispatch_count}")


if __name__ == '__main__':
    analyze_guest_execution()
    find_end_pattern()
