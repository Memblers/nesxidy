"""Find flash_byte_program executions starting at $6000.
The function uses indirect addressing for IO8(addr)=data, so the trace 
will show STA (r0),Y or similar, not STA $975C directly.
Let me find the first calls and see the pattern."""
import os, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"
fsize = os.path.getsize(TRACE)

# Search for "6000" at start of line in the first 500MB
print("Searching for PC=$6000 (flash_byte_program entry)...")
chunk_size = 50_000_000

for file_offset in range(0, min(fsize, 1_000_000_000), chunk_size):
    with open(TRACE, 'rb') as f:
        f.seek(file_offset)
        raw = f.read(chunk_size + 1000)
    
    # Look for "\n6000 " pattern
    idx = 0
    while True:
        idx = raw.find(b'6000 ', idx)
        if idx < 0:
            break
        # Check that this is at the start of a line
        if idx == 0 or raw[idx-1:idx] == b'\n':
            # Found! Read context
            start = max(0, idx - 500)
            end = min(len(raw), idx + 3000)
            context = raw[start:end].decode('utf-8', errors='replace')
            context_lines = context.split('\n')
            
            # Find the line with 6000
            for ci, cl in enumerate(context_lines):
                if cl.strip().startswith('6000'):
                    print(f"\nFound flash_byte_program at file offset ~{file_offset+idx:,}")
                    # Show 5 lines before and 40 lines after
                    for j in range(max(0,ci-5), min(len(context_lines), ci+45)):
                        print(f"  {context_lines[j].rstrip()[:160]}")
                    break
            
            # Just find the first occurrence
            print("\n--- Looking for the data write instruction ---")
            # In VBCC compiled code, IO8(addr) = data uses STA (r0),Y
            # Let's find all STA instructions in this flash_byte_program call
            for ci, cl in enumerate(context_lines):
                stripped = cl.strip()
                if 'STA' in stripped:
                    parts = stripped.split()
                    if parts:
                        try:
                            addr = int(parts[0][:4], 16)
                            if 0x6000 <= addr <= 0x60FF:
                                print(f"  STA at ${addr:04X}: {stripped[:160]}")
                        except:
                            pass
            
            # Found enough, exit
            break
        idx += 1
    else:
        print(f"  Scanned {(file_offset+chunk_size)/1e6:.0f}MB...")
        continue
    break
