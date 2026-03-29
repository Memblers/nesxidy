"""
Look for the dispatch that triggered the first compile of $D364.
The compile writes to $92F8 starting around line 251480.
Let me look backwards to find the dispatch_on_pc call.
"""

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    # Read a window of lines before the first $92F8 write
    target_line = 251480
    window_before = 2000  # look 2000 lines before
    
    print(f"Reading lines {target_line - window_before} to {target_line}...")
    
    lines = []
    with open(TRACE_FILE, "r") as f:
        for i, line in enumerate(f):
            if i+1 >= target_line - window_before and i+1 <= target_line:
                lines.append((i+1, line.rstrip()))
            if i+1 > target_line:
                break
    
    # Find dispatch_on_pc entries (at $623B) and their return values
    # Also find where the compile path starts
    
    # Look for _pc store patterns that set _pc to $D364
    # Also look for dispatch_on_pc and not_recompiled labels
    
    print(f"Loaded {len(lines)} lines")
    
    # Find dispatch entries
    for line_num, text in lines:
        # dispatch_on_pc starts with LDA #$00 at $623B
        if text.startswith("623B"):
            print(f"  L{line_num}: {text[:150]}")
        
        # Find where _pc is set to $D364
        # The epilogue writes: LDA #$64; STA _pc; LDA #$D3; STA _pc+1
        if ("$D364" in text or "= $D3" in text) and "STA" in text and line_num > target_line - 500:
            print(f"  L{line_num}: {text[:150]}")
        
        # Look for not_recompiled return — dispatch returns 1 or 2
        # dispatch stores 1 or 2 into r0 (return value) and RTS
        # The label not_recompiled in the assembly stores 1 to r0
        # Look for "LDA #$01" followed by "STA r0" near dispatch code
        if text.startswith("62") and "#$01" in text and "LDA" in text and line_num > target_line - 1000:
            # could be "not_recompiled" returning 1
            pass
    
    # Actually, let me just dump the last 200 lines before the first write
    print(f"\n\n=== Last 200 lines before $92F8 write ===")
    for line_num, text in lines[-200:]:
        print(f"  L{line_num}: {text[:150]}")


if __name__ == "__main__":
    analyze()
