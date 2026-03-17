"""Dump raw flash write bytes to verify block header decoding."""

flash_writes = []
current_frame = 0
with open(r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt", "r") as f:
    for i, line in enumerate(f):
        if not line.startswith("60"):
            fr_idx = line.find("Fr:")
            if fr_idx >= 0:
                try:
                    fr_str = ""
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str: current_frame = int(fr_str)
                except: pass
            continue
        parts = line.split()
        if not parts: continue
        try: addr = int(parts[0], 16)
        except: continue
        fr_idx = line.find("Fr:")
        if fr_idx >= 0:
            try:
                fr_str = ""
                for c in line[fr_idx+3:fr_idx+12]:
                    if c.isdigit(): fr_str += c
                    else: break
                if fr_str: current_frame = int(fr_str)
            except: pass
        if addr == 0x6041:
            bracket_idx = line.find("[$")
            if bracket_idx < 0: continue
            try:
                target = int(line[bracket_idx+2:bracket_idx+6], 16)
            except: continue
            a_idx = line.find("A:")
            try: data = int(line[a_idx+2:a_idx+4], 16)
            except: continue
            flash_writes.append((current_frame, target, data))
            if len(flash_writes) >= 500: break

# Print raw writes grouped by contiguous blocks
prev = 0
block_num = 0
block_start_idx = 0
for i, (fr, target, data) in enumerate(flash_writes):
    if i > 0 and (target < prev or target > prev + 4):
        block_size = i - block_start_idx
        print(f"--- Block {block_num} ends ({block_size} bytes) ---\n")
        block_num += 1
        block_start_idx = i
    
    # Compute offset within current block
    if i == block_start_idx:
        block_base = target
    offset = target - block_base
    
    label = ""
    if offset == 0: label = " entry_pc_lo"
    elif offset == 1: label = " entry_pc_hi"
    elif offset == 2: label = " exit_pc_lo"
    elif offset == 3: label = " exit_pc_hi"
    elif offset == 4: label = " code_len"
    elif offset == 5: label = " epilogue_off"
    elif offset == 6: label = " flags/cycles"
    elif offset == 7: label = " sentinel"
    elif offset == 8: label = " <code start>"
    
    print(f"  [{i:>3}] Fr:{fr} ${target:04X} +{offset:>3} = ${data:02X}{label}")
    prev = target

block_size = len(flash_writes) - block_start_idx
print(f"--- Block {block_num} ends ({block_size} bytes) ---")
