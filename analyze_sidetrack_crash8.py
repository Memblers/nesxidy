"""Dump the complete _b17 function execution for the first bank 17 call.
_b17 is at $843E in bank 17, executing from line ~9448447 to ~9448513.
We need to see ALL instructions to find the 5 code buffer writes.
"""
TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# First bank 17 call: lines 9448447-9448513
# Print every single line
print("=== FIRST _b17 call (guest PC $32E1, first branch in block) ===")
with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i >= 9448440 and i <= 9448530:
            print(f"  {i:>10}: {line.rstrip()[:165]}")
        if i > 9448530:
            break

# Also look at the second bank 17 call (line 9449310)
# to see if the pattern is different
print("\n\n=== SECOND _b17 call (next branch) ===")
with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i >= 9449310 and i <= 9449400:
            print(f"  {i:>10}: {line.rstrip()[:165]}")
        if i > 9449400:
            break

# Also look at the LAST bank 17 call before the flash write (line 9451693)
# This is the one most likely for the $32DF block
print("\n\n=== FIFTH bank 17 call (line 9451693, likely $32DF block's BEQ) ===")
with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i >= 9451680 and i <= 9451920:
            print(f"  {i:>10}: {line.rstrip()[:165]}")
        if i > 9451920:
            break
