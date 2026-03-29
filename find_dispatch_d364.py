"""
Find dispatch_on_pc calls where _pc = $D364 and trace the outcome.
dispatch_on_pc is at WRAM $623B.

It reads _pc (ZP variable) to determine the guest PC.
From the assembly:
  _dispatch_on_pc:
    lda #0
    sta temp
    lda _pc+1    ; high byte of guest PC
    ...

We look for execution at $623B where _pc = $D364.
The ZP variable _pc is at some address. From the trace, we can find it.

Actually, from the trace context dump we can see that _pc is at a ZP address.
Let's search for the dispatch entry and its first instruction.

Strategy: Find lines where PC (instruction address) is $623B (start of dispatch_on_pc).
Then check the frame and look at whether it leads to a compile.
"""

import sys

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    print("Searching for dispatch_on_pc entries (address $623B) near frame 6365...")
    
    current_frame = 0
    dispatch_entries = []
    
    with open(TRACE_FILE, "r") as f:
        for i, line in enumerate(f):
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: current_frame = int(fr_str)
                except: pass
            
            # dispatch_on_pc starts at $623B
            if line.startswith("623B") or line.startswith("623b"):
                dispatch_entries.append((i+1, current_frame, line.rstrip()[:150]))
            
            # Also check for the not_recompiled label — where dispatch fails
            # This would be a label in the assembly. Let me look for it.
            
            if i > 350000:
                break  # just look around the first compile area
    
    print(f"Found {len(dispatch_entries)} dispatch_on_pc entries in first 350K lines")
    for line_num, frame, text in dispatch_entries[:20]:
        print(f"  L{line_num:>8} Fr:{frame:>6}: {text}")
    
    if len(dispatch_entries) > 20:
        print(f"  ... and {len(dispatch_entries) - 20} more")
    
    # Now find _pc value. From the assembly, dispatch_on_pc reads _pc+1 early.
    # The instruction `lda _pc+1` would show the high byte.
    # If we find a dispatch entry where the next few instructions show _pc+1 = $D3
    # and _pc = $64, that's our target.
    
    print("\n\nPhase 2: Finding dispatch entries with _pc=$D364...")
    print("Looking at trace context after each dispatch entry...")
    
    # Find the first dispatch with _pc+1 = $D3 by reading context
    target_dispatches = []
    
    with open(TRACE_FILE, "r") as f:
        lines = []
        dispatch_start = -1
        frame_at_dispatch = 0
        
        for i, line in enumerate(f):
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: current_frame = int(fr_str)
                except: pass
            
            if line.startswith("623B") or line.startswith("623b"):
                dispatch_start = i
                frame_at_dispatch = current_frame
                lines = [line]
            elif dispatch_start >= 0 and len(lines) < 8:
                lines.append(line)
                if len(lines) == 8:
                    # Check if _pc+1 = $D3
                    # The 3rd instruction (index 2) should be: lda _pc+1
                    # which shows the loaded value
                    for l in lines[1:5]:
                        if "_pc" in l and "= $D3" in l:
                            # Found! Now check _pc lo byte
                            for l2 in lines:
                                if "_pc" in l2 and "= $64" in l2:
                                    target_dispatches.append({
                                        "line": dispatch_start + 1,
                                        "frame": frame_at_dispatch,
                                        "context": [ln.rstrip()[:150] for ln in lines]
                                    })
                                    break
                            break
                    dispatch_start = -1
            
            if i > 350000:
                break
    
    print(f"Found {len(target_dispatches)} dispatch calls with _pc near $D3xx")
    for d in target_dispatches[:10]:
        print(f"\n  Line {d['line']}, Frame {d['frame']}:")
        for ctx in d['context']:
            print(f"    {ctx}")


if __name__ == "__main__":
    analyze()
