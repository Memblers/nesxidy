"""Trace the compilation context around the $9A72 write.
We know from the previous analysis that $9A72 gets $00 written at line 9489886.
Let's look at the host code executing around that point to understand the full
compilation path and where the $00 byte comes from.
"""
TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"

# Read a window around the $9A72 write (line 9489886)
# Go back far enough to see the compilation context
TARGET_LINE = 9489886
WINDOW_BEFORE = 2000  # lines before the write
WINDOW_AFTER = 200   # lines after

start_line = TARGET_LINE - WINDOW_BEFORE
end_line = TARGET_LINE + WINDOW_AFTER

print(f"=== Reading lines {start_line} to {end_line} around $9A72 write ===\n")

lines = []
with open(TRACE_FILE, 'r') as f:
    for i, line in enumerate(f):
        if i >= start_line and i <= end_line:
            lines.append((i, line.rstrip()))
        if i > end_line:
            break

# Now analyze what's happening
# Look for key patterns:
# 1. Bank switches (STA $C000) - which bank is active during compilation
# 2. The code buffer write pointer (where bytes go before flash)
# 3. The guest opcode being compiled (stored in ZP)
# 4. Calls to compilation functions

# First, let's just print the region around the $9A72 write to see the code flow
# Focus on the area just before the write
print("=== Context 300 lines before $9A72 write ===")
for i, (ln, txt) in enumerate(lines):
    if ln >= TARGET_LINE - 300 and ln <= TARGET_LINE + 20:
        # Highlight the actual write
        marker = " <<<" if ln == TARGET_LINE else ""
        print(f"{ln:>10}: {txt[:160]}{marker}")

# Also look for the beginning of this block's compilation
# by finding when the code buffer address was set to $9A70-ish
print("\n\n=== Bank switches in the window ===")
for ln, txt in lines:
    if 'STA $C000' in txt:
        print(f"{ln:>10}: {txt[:160]}")

# Look for JSR/RTS patterns to understand the call structure
print("\n\n=== Key JSR/JMP in the 200 lines before write ===")
for ln, txt in lines:
    if ln >= TARGET_LINE - 200 and ln <= TARGET_LINE:
        if 'JSR' in txt or 'JMP' in txt or 'RTS' in txt:
            print(f"{ln:>10}: {txt[:160]}")
