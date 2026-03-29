"""Search the trace for writes to $6CB6 (cache_code[0][2]) and flash $9A72.
Also find the compilation of the block containing guest PC $32DF."""
import os, mmap

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\exidy.txt"
SEARCH_SIZE = 200 * 1024 * 1024  # Search last 200MB

fsize = os.path.getsize(TRACE)
print(f"Trace size: {fsize:,} bytes ({fsize / (1024**3):.2f} GB)")

# Search patterns:
# 1. Writes to $6CB6: STA $6CB6 or STX/STY $6CB6
# 2. Writes to flash $9A72: STA ($XX),Y with effective addr $9A72
# 3. References to $32DF (guest PC being compiled)
# 4. Calls to our ir_emit function at $DE39

targets = [
    b"6CB6",   # cache_code[0][2] address
    b"9A72",   # flash crash address
    b"32DF",   # guest PC
    b"DE39",   # ir_emit_direct_branch_placeholder
    b"6CB4",   # cache_code[0][0] (block start)
]

# Read last 200MB of trace
start_offset = max(0, fsize - SEARCH_SIZE)
print(f"Searching from offset {start_offset:,} ({start_offset / (1024**3):.2f} GB)")

# Read in 10MB chunks and search
CHUNK = 10 * 1024 * 1024
OVERLAP = 1024  # Overlap between chunks to avoid missing matches at boundaries

results = {t: [] for t in targets}

with open(TRACE, 'rb') as f:
    f.seek(start_offset)
    pos = start_offset
    chunk_buf = b""
    chunks_read = 0
    
    while pos < fsize:
        chunk = f.read(CHUNK)
        if not chunk:
            break
        
        data = chunk_buf + chunk  # Prepend overlap from previous chunk
        
        for target in targets:
            offset = 0
            while True:
                idx = data.find(target, offset)
                if idx == -1:
                    break
                # Get the line containing this match
                line_start = data.rfind(b'\n', max(0, idx - 200), idx)
                if line_start == -1:
                    line_start = max(0, idx - 200)
                else:
                    line_start += 1
                line_end = data.find(b'\n', idx)
                if line_end == -1:
                    line_end = min(len(data), idx + 200)
                line = data[line_start:line_end].decode('ascii', errors='replace').strip()
                
                # Calculate absolute position
                abs_pos = pos - len(chunk_buf) + idx
                
                # Only record interesting lines (writes, not reads)
                # For 6CB6, we want STA/STX/STY
                # For 9A72, any reference
                results[target].append((abs_pos, line))
                offset = idx + 1
        
        # Keep last OVERLAP bytes for next iteration
        chunk_buf = chunk[-OVERLAP:]
        pos += len(chunk)
        chunks_read += 1
        if chunks_read % 10 == 0:
            print(f"  Scanned {(pos - start_offset) / (1024**2):.0f} MB...")

# Print results
for target, hits in results.items():
    target_str = target.decode()
    print(f"\n=== ${target_str} matches: {len(hits)} ===")
    if len(hits) > 30:
        print(f"  (showing last 30)")
        hits = hits[-30:]
    for abs_pos, line in hits:
        # Truncate long lines
        if len(line) > 150:
            line = line[:150] + "..."
        print(f"  [{abs_pos:>12,}] {line}")
