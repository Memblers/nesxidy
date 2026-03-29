"""
Analyze Excitebike trace - focused on flash cache growth.

Key observations from initial analysis:
- flash_byte_program starts at $6000 in WRAM
- Each call to flash_byte_program writes one byte to flash
- The STA (r2),Y at $6041 is the actual data write
- Compilation shows up as sustained WRAM execution

Strategy:
1. Count flash_byte_program calls ($6000 entry) per frame = compilation volume
2. Track unique $8000-$BFFF execution addresses per frame = cache working set
3. Watch for new addresses appearing = fresh blocks compiled
"""

import sys
from collections import Counter, defaultdict

TRACE_FILE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_excitebike.txt"

def analyze():
    # Per-frame tracking
    frame_fbp_calls = Counter()       # flash_byte_program calls per frame
    frame_new_flash_addrs = Counter() # new (first-seen) flash addrs per frame
    all_flash_addrs = set()           # running set of all seen $8000-$BFFF addrs
    
    # Track which 256-byte pages in flash are first executed per frame
    frame_new_pages = defaultdict(set)  # frame -> set of new pages
    all_flash_pages = set()
    
    # Track the actual flash write targets (STA (r2),Y at $6041)
    flash_write_targets = []  # (frame, target_addr)
    
    # Track consecutive WRAM runs (compilation bursts)
    wram_burst_start_frame = None
    wram_burst_length = 0
    wram_bursts = []  # (frame, length)
    
    current_frame = 0
    prev_addr = 0
    
    # Track guest PC pattern: what's in ZP at dispatch time
    # The _pc variable is stored in ZP. Dispatch reads it.
    # Look for the pattern: execution jumps from $C000+ to $8000-$BFFF = cache hit
    # or from $C000+ to $6000+ = compile/interpret
    
    # Simpler: track transitions from fixed bank to flash
    transitions_to_flash = Counter()  # frame -> count of C000->8000 transitions
    transitions_to_compile = Counter()  # frame -> count of going to WRAM (compile)
    
    print(f"Reading {TRACE_FILE}...")
    
    with open(TRACE_FILE, 'r') as f:
        for i, line in enumerate(f):
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            
            # Parse frame
            fr_idx = line.find('Fr:')
            if fr_idx >= 0:
                try:
                    fr_str = ''
                    for c in line[fr_idx+3:fr_idx+12]:
                        if c.isdigit(): fr_str += c
                        else: break
                    if fr_str:
                        current_frame = int(fr_str)
                except:
                    pass
            
            # Count flash_byte_program entry
            if addr == 0x6000:
                frame_fbp_calls[current_frame] += 1
            
            # Track STA (r2),Y at $6041 - the actual flash write
            # Parse the write target from [$xxxx] notation
            if addr == 0x6041:
                bracket_idx = line.find('[$')
                if bracket_idx >= 0:
                    try:
                        target = int(line[bracket_idx+2:bracket_idx+6], 16)
                        flash_write_targets.append((current_frame, target))
                    except:
                        pass
            
            # Track flash execution ($8000-$BFFF)
            if 0x8000 <= addr < 0xC000:
                page = addr >> 8  # 256-byte page
                if page not in all_flash_pages:
                    all_flash_pages.add(page)
                    frame_new_pages[current_frame].add(page)
                if addr not in all_flash_addrs:
                    all_flash_addrs.add(addr)
                    frame_new_flash_addrs[current_frame] += 1
            
            # Track WRAM bursts
            if 0x6000 <= addr < 0x8000:
                if prev_addr < 0x6000 or prev_addr >= 0x8000:
                    # Entering WRAM - start of burst
                    wram_burst_start_frame = current_frame
                    wram_burst_length = 1
                else:
                    wram_burst_length += 1
            else:
                if wram_burst_length > 50:  # significant burst
                    wram_bursts.append((wram_burst_start_frame, wram_burst_length))
                wram_burst_length = 0
            
            # Track transitions
            if prev_addr >= 0xC000 and 0x8000 <= addr < 0xC000:
                transitions_to_flash[current_frame] += 1
            if prev_addr >= 0xC000 and 0x6000 <= addr < 0x8000:
                transitions_to_compile[current_frame] += 1
            
            prev_addr = addr
            
            if i % 1000000 == 0:
                print(f"  {i:,} lines, frame {current_frame}, "
                      f"flash addrs: {len(all_flash_addrs):,}, "
                      f"pages: {len(all_flash_pages)}")
    
    print(f"\nDone. {i+1:,} lines processed.\n")
    
    # === Report ===
    all_frames = sorted(set(
        list(frame_fbp_calls.keys()) + 
        list(frame_new_flash_addrs.keys()) +
        list(transitions_to_flash.keys()) +
        list(transitions_to_compile.keys())
    ))
    if not all_frames:
        print("No frames found!")
        return
    
    print(f"Frame range: {all_frames[0]} to {all_frames[-1]}")
    print(f"Total unique flash execution addresses: {len(all_flash_addrs):,}")
    print(f"Total unique flash pages: {len(all_flash_pages)}")
    print(f"Total flash_byte_program calls: {sum(frame_fbp_calls.values()):,}")
    print(f"Total flash write targets captured: {len(flash_write_targets):,}")
    print(f"WRAM bursts > 50 instructions: {len(wram_bursts)}")
    
    # Flash writes per frame
    print(f"\n=== Flash Byte Programs per Frame (frames with >0 writes) ===")
    frames_with_writes = [(f, frame_fbp_calls[f]) for f in all_frames if frame_fbp_calls[f] > 0]
    print(f"Frames with flash writes: {len(frames_with_writes)} / {len(all_frames)}")
    for fr, cnt in sorted(frames_with_writes)[:50]:
        new_addrs = frame_new_flash_addrs.get(fr, 0)
        t_flash = transitions_to_flash.get(fr, 0)
        t_compile = transitions_to_compile.get(fr, 0)
        new_pg = len(frame_new_pages.get(fr, set()))
        print(f"  Fr:{fr:>6}  fbp:{cnt:>5}  new_addrs:{new_addrs:>4}  "
              f"new_pages:{new_pg:>2}  "
              f"dispatch->flash:{t_flash:>3}  dispatch->compile:{t_compile:>3}")
    
    # New flash addresses per frame (shows cache growth)
    print(f"\n=== Cache Growth: New Flash Addresses per Frame ===")
    cumulative = 0
    for fr in sorted(all_frames):
        new = frame_new_flash_addrs.get(fr, 0)
        fbp = frame_fbp_calls.get(fr, 0)
        if new > 0 or fbp > 0:
            cumulative += new
            new_pg = len(frame_new_pages.get(fr, set()))
            print(f"  Fr:{fr:>6}  new:{new:>4}  cumulative:{cumulative:>6}  "
                  f"fbp_calls:{fbp:>5}  new_pages:{new_pg:>2}")
    
    # WRAM bursts (compilation events)
    print(f"\n=== WRAM Bursts (>50 instructions, = compilation) ===")
    for fr, length in wram_bursts[:40]:
        print(f"  Fr:{fr:>6}  length:{length:>6}")
    if len(wram_bursts) > 40:
        print(f"  ... and {len(wram_bursts)-40} more")
    
    # Flash write target analysis - where are bytes being programmed?
    print(f"\n=== Flash Write Target Distribution ===")
    target_pages = Counter()
    target_sectors = Counter()
    for fr, target in flash_write_targets:
        page = target >> 8
        bank = (target - 0x8000) // 0x4000
        sector = ((target - 0x8000) % 0x4000) // 0x1000
        target_sectors[f"b{bank}s{sector}"] += 1
        target_pages[page] += 1
    
    print(f"Writes per flash sector:")
    for sect, cnt in sorted(target_sectors.items()):
        print(f"  {sect}: {cnt:,}")
    
    print(f"\nTop 20 flash pages written to:")
    for page, cnt in target_pages.most_common(20):
        print(f"  ${page:02X}xx: {cnt:,}")
    
    # Per-frame summary for the first 100 frames
    print(f"\n=== Per-Frame Summary (all frames) ===")
    print(f"{'Frame':>6} {'FBP':>5} {'NewAddr':>7} {'NewPg':>5} {'->Flash':>7} {'->Comp':>6} {'Cumul':>6}")
    cumulative = 0
    for fr in sorted(all_frames):
        new = frame_new_flash_addrs.get(fr, 0)
        cumulative += new
        fbp = frame_fbp_calls.get(fr, 0)
        new_pg = len(frame_new_pages.get(fr, set()))
        t_flash = transitions_to_flash.get(fr, 0)
        t_compile = transitions_to_compile.get(fr, 0)
        if new > 0 or fbp > 0 or t_compile > 0:
            print(f"{fr:>6} {fbp:>5} {new:>7} {new_pg:>5} {t_flash:>7} {t_compile:>6} {cumulative:>6}")

if __name__ == "__main__":
    analyze()
