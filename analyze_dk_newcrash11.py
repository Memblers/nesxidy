"""Binary search for the line containing [$975C] and A:A0."""
import os

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# Search in 200MB chunks
target_pattern = b'[$975C]'
a0_pattern = b'A:A0'

for offset in range(0, fsize, 200_000_000):
    with open(TRACE, 'rb') as f:
        f.seek(offset)
        raw = f.read(200_000_000 + 500)
    
    # Check if this chunk contains our target
    idx = raw.find(target_pattern)
    while idx >= 0:
        # Check if it's the write we want (A:A0)
        line_start = raw.rfind(b'\n', max(0, idx - 200), idx)
        if line_start < 0:
            line_start = 0
        else:
            line_start += 1
        line_end = raw.find(b'\n', idx)
        if line_end < 0:
            line_end = len(raw)
        line = raw[line_start:line_end]
        
        if b'A:A0' in line and b'STA' in line and b'X:19' in line:
            # Found it! Get context
            context_start = max(0, line_start - 30000)
            context_end = min(len(raw), line_end + 5000)
            context = raw[context_start:context_end].decode('utf-8', errors='replace')
            context_lines = context.split('\n')
            
            # Find the target line in context
            for ci, cl in enumerate(context_lines):
                if '[$975C]' in cl and 'A:A0' in cl and 'X:19' in cl:
                    print(f"Found at byte offset ~{offset + line_start:,}")
                    print(f"\n=== 250 lines before and 50 after ===")
                    for j in range(max(0, ci - 250), min(len(context_lines), ci + 50)):
                        marker = ">>>" if j == ci else "   "
                        print(f"{marker} {context_lines[j].rstrip()[:180]}")
                    break
            
            # Exit after first match
            print("\nDone.")
            exit()
        
        idx = raw.find(target_pattern, idx + 1)
    
    print(f"  Scanned {(offset+200_000_000)/1e9:.1f}GB...")

print("Not found!")
