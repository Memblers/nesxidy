"""Analyze the guest trace to find when IRQs stop working.
The guest trace logs each dispatch PC. We want to see:
1. When did IRQ handler ($7643) last appear?
2. What happened before the final $4026 spin?
3. How many $4026 dispatches per frame early vs late?
"""
import re

TRACE = r"c:\proj\c\NES\nesxidy-co\nesxidy\nes_trace_lua.txt"

# First pass: find total lines, last IRQ handler appearance, frame distribution
total_lines = 0
last_irq_line = -1
last_irq_frame = -1
frame_counts = {}  # frame -> count of dispatches
frame_4026_counts = {}  # frame -> count of $4026 dispatches
frame_irq_counts = {}  # frame -> count of IRQ handler dispatches ($7643)
current_frame = 0

# Collect first and last 100 lines for context
first_lines = []
last_lines = []

with open(TRACE, 'r') as f:
    for i, line in enumerate(f):
        total_lines += 1
        if i < 100:
            first_lines.append(line.rstrip())
        
        # Parse frame number
        m = re.search(r'Fr:(\d+)', line)
        if m:
            current_frame = int(m.group(1))
        
        # Parse PC
        parts = line.split()
        if parts:
            try:
                pc = int(parts[0], 16)
            except:
                continue
            
            frame_counts[current_frame] = frame_counts.get(current_frame, 0) + 1
            
            if pc == 0x4026:
                frame_4026_counts[current_frame] = frame_4026_counts.get(current_frame, 0) + 1
            
            if pc == 0x7643:
                last_irq_line = i
                last_irq_frame = current_frame
                frame_irq_counts[current_frame] = frame_irq_counts.get(current_frame, 0) + 1
        
        # Keep rolling buffer of last 100
        last_lines.append(line.rstrip())
        if len(last_lines) > 100:
            last_lines.pop(0)

print(f"Total lines: {total_lines:,}")
print(f"Total frames: {max(frame_counts.keys()) if frame_counts else 0}")
print(f"Last IRQ handler ($7643) at line {last_irq_line:,}, frame {last_irq_frame}")
print()

# Show first 20 lines
print("=== FIRST 20 LINES ===")
for line in first_lines[:20]:
    print(line)
print()

# Show last 20 lines
print("=== LAST 20 LINES ===")
for line in last_lines[-20:]:
    print(line)
print()

# Show frames with IRQ handler dispatches
irq_frames = sorted(frame_irq_counts.keys())
print(f"=== FRAMES WITH IRQ HANDLER ($7643) === ({len(irq_frames)} total)")
if len(irq_frames) <= 40:
    for fr in irq_frames:
        print(f"  Frame {fr}: {frame_irq_counts[fr]} IRQ dispatches, {frame_counts.get(fr, 0)} total, {frame_4026_counts.get(fr, 0)} @$4026")
else:
    for fr in irq_frames[:10]:
        print(f"  Frame {fr}: {frame_irq_counts[fr]} IRQ dispatches, {frame_counts.get(fr, 0)} total, {frame_4026_counts.get(fr, 0)} @$4026")
    print(f"  ... ({len(irq_frames) - 20} more) ...")
    for fr in irq_frames[-10:]:
        print(f"  Frame {fr}: {frame_irq_counts[fr]} IRQ dispatches, {frame_counts.get(fr, 0)} total, {frame_4026_counts.get(fr, 0)} @$4026")

print()

# Show frames around where IRQs stop
if last_irq_frame >= 0:
    print(f"=== FRAMES AROUND LAST IRQ (frame {last_irq_frame}) ===")
    for fr in range(last_irq_frame - 5, last_irq_frame + 15):
        if fr in frame_counts:
            irq = frame_irq_counts.get(fr, 0)
            w = frame_4026_counts.get(fr, 0)
            print(f"  Frame {fr}: total={frame_counts[fr]:4d}, @$4026={w:4d}, IRQ_handler={irq}")

# Also check: are there other common PCs in the final frames?
print()
print("=== PC DISTRIBUTION IN FINAL 5 FRAMES ===")
max_frame = max(frame_counts.keys())
# Need a second pass for the last few frames, or recount
# Actually, let's just show what PCs appear in the last_lines
pc_counts = {}
for line in last_lines:
    parts = line.split()
    if parts:
        try:
            pc = int(parts[0], 16)
            pc_counts[pc] = pc_counts.get(pc, 0) + 1
        except:
            pass
for pc, count in sorted(pc_counts.items(), key=lambda x: -x[1])[:10]:
    print(f"  ${pc:04X}: {count} dispatches")
