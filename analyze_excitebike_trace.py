"""
Analyze the Excitebike trace log to understand flash cache growth.

The Mesen2 trace shows the actual NES CPU executing the recompiler's output.
Key regions:
  $0000-$07FF  NES RAM (guest ZP + stack + RAM)
  $6000-$7FFF  WRAM (host helpers, flash_byte_program, etc)
  $8000-$BFFF  Flash cache / switchable PRG bank
  $C000-$FFFF  Fixed bank 31 (dispatch, compile, interpreter)

Flash programming (flash_byte_program in WRAM) does:
  STA $C000  (bankswitch for flash command sequence)
  STA $9555  (flash command AA)
  STA $AAAA  (flash command 55)
  STA $9555  (flash command A0 = byte program)
  STA $xxxx  (actual data write)
  
We want to find:
1. How many flash byte programs happen per frame (= compilation activity)
2. What guest PCs trigger compilation (= mid-block misses)
3. Whether the same ROM regions get re-compiled
"""

import sys
from collections import Counter, defaultdict

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    # Track per-frame stats
    frame_flash_writes = Counter()  # frame -> count of flash byte programs
    frame_compile_count = Counter()  # frame -> count of compilation events
    
    # Track flash write sequences
    # flash_byte_program pattern: STA $9555 = $A0 then STA $xxxx = data
    in_flash_program = False
    flash_program_target = None
    flash_writes_total = 0
    flash_write_addrs = Counter()  # target address -> count
    
    # Track compilation: when we see execution in WRAM ($6000-$7FFF) 
    # followed by flash writes, that's compilation
    
    # Track unique PCs per frame to find the "working set"
    frame_unique_pcs = defaultdict(set)
    
    # Track the current frame
    current_frame = None
    prev_frame = None
    frame_starts = []  # (line_num, frame_num)
    
    # Track execution in key address ranges per frame
    frame_flash_exec = Counter()   # instructions executed in $8000-$BFFF
    frame_fixed_exec = Counter()   # instructions executed in $C000-$FFFF  
    frame_wram_exec = Counter()    # instructions executed in $6000-$7FFF
    
    # Track 6502 opcode at $9555 writes (flash commands)
    flash_cmds = Counter()  # (addr, value) counts
    
    # Detect flash_byte_program calls by watching for the sequence:
    # STA $9555 = $A0 (byte program command)
    last_sta_9555_val = None
    pending_program = False
    
    # Compilation detection: look for sectors of WRAM execution
    # which indicate recompile_block is running
    wram_run_start = None
    wram_run_length = 0
    compile_events = []  # (line, frame, wram_run_length)
    
    # Read the entire trace
    print(f"Reading {TRACE_FILE}...")
    line_count = 0
    
    with open(TRACE_FILE, 'r') as f:
        for i, line in enumerate(f):
            line_count += 1
            parts = line.split()
            if len(parts) < 2:
                continue
            
            # Parse address
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            
            # Parse frame number
            fr_idx = line.find('Fr:')
            if fr_idx >= 0:
                try:
                    fr_end = line.index(' ', fr_idx + 3) if ' ' in line[fr_idx+3:fr_idx+15] else fr_idx + 10
                    current_frame = int(line[fr_idx+3:fr_end].strip())
                except:
                    pass
            
            if current_frame and current_frame != prev_frame:
                frame_starts.append((i, current_frame))
                prev_frame = current_frame
            
            # Track address range per frame
            if current_frame:
                if 0x8000 <= addr < 0xC000:
                    frame_flash_exec[current_frame] += 1
                elif addr >= 0xC000:
                    frame_fixed_exec[current_frame] += 1
                elif 0x6000 <= addr < 0x8000:
                    frame_wram_exec[current_frame] += 1
            
            # Detect flash_byte_program: look for STA $9555 = $A0
            # In the Mesen trace, a write to $9555 with value $A0 is the 
            # "byte program" flash command. The NEXT STA to $8000-$BFFF 
            # is the actual data byte being programmed.
            instr = line[6:60] if len(line) > 60 else line[6:]
            
            if 'STA $9555' in instr:
                # Check what value A has
                a_idx = line.find('A:')
                if a_idx >= 0:
                    try:
                        a_val = int(line[a_idx+2:a_idx+4], 16)
                        if a_val == 0xA0:
                            pending_program = True
                    except:
                        pass
            elif pending_program and 'STA' in instr:
                # This STA after A0 command is the actual byte program
                # Find the target address
                sta_parts = instr.split()
                if len(sta_parts) >= 2:
                    target_str = sta_parts[1].strip()
                    # Remove $ prefix and any suffixes
                    target_str = target_str.replace('$', '').split()[0]
                    try:
                        target_addr = int(target_str, 16)
                        if 0x8000 <= target_addr < 0xC000:
                            flash_writes_total += 1
                            flash_write_addrs[target_addr] += 1
                            if current_frame:
                                frame_flash_writes[current_frame] += 1
                    except:
                        pass
                pending_program = False
            
            # Progress
            if i % 1000000 == 0:
                print(f"  Processed {i:,} lines (frame {current_frame})...")
    
    print(f"\nProcessed {line_count:,} lines total")
    print(f"Frame range: {min(frame_starts, key=lambda x: x[1])[1]} to {max(frame_starts, key=lambda x: x[1])[1]}")
    print(f"Total frames seen: {len(set(f for _, f in frame_starts))}")
    
    # Report flash write activity per frame
    print(f"\n=== Flash Byte Programs ===")
    print(f"Total flash writes to $8000-$BFFF: {flash_writes_total:,}")
    
    if frame_flash_writes:
        frames_with_writes = sorted(frame_flash_writes.keys())
        print(f"\nFrames with flash writes: {len(frames_with_writes)}")
        print(f"\nFlash writes per frame (top 30):")
        for fr, cnt in frame_flash_writes.most_common(30):
            flash_pct = frame_flash_exec.get(fr, 0)
            fixed_pct = frame_fixed_exec.get(fr, 0)
            wram_pct = frame_wram_exec.get(fr, 0)
            total = flash_pct + fixed_pct + wram_pct
            print(f"  Fr:{fr:>6}  writes:{cnt:>5}  "
                  f"flash:{flash_pct*100//max(total,1):>2}% "
                  f"fixed:{fixed_pct*100//max(total,1):>2}% "
                  f"wram:{wram_pct*100//max(total,1):>2}%")
    
    # Flash write address distribution
    print(f"\n=== Flash Write Target Addresses ===")
    print(f"Unique addresses written: {len(flash_write_addrs):,}")
    
    # Group by sector (4KB)
    sector_writes = Counter()
    for addr, cnt in flash_write_addrs.items():
        bank = (addr - 0x8000) // 0x4000
        sector_in_bank = ((addr - 0x8000) % 0x4000) // 0x1000
        sector_writes[f"bank{bank} sect{sector_in_bank}"] += cnt
    
    print(f"\nWrites per flash sector:")
    for sect, cnt in sorted(sector_writes.items()):
        print(f"  {sect}: {cnt:,}")
    
    # Per-frame execution profile (every 10th frame)
    print(f"\n=== Execution Profile by Frame (sampled) ===")
    all_frames = sorted(set(f for _, f in frame_starts))
    print(f"{'Frame':>8} {'Flash':>8} {'Fixed':>8} {'WRAM':>8} {'FlWrites':>8}")
    for fr in all_frames[::10]:  # every 10th frame
        fe = frame_flash_exec.get(fr, 0)
        fx = frame_fixed_exec.get(fr, 0)
        fw = frame_wram_exec.get(fr, 0)
        ffw = frame_flash_writes.get(fr, 0)
        print(f"{fr:>8} {fe:>8,} {fx:>8,} {fw:>8,} {ffw:>8}")

if __name__ == "__main__":
    analyze()
