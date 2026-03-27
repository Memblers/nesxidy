"""Analyze nes_trace_lua.txt for Millipede guest-program dispatch inefficiencies."""
import re
from collections import Counter, defaultdict

TRACE = r"c:\proj\c\NES\nesxidy-co\nesxidy\nes_trace_lua.txt"

# Parse trace lines: "79A5  ??  ??? $79A5  A:10 X:10 Y:6B S:FF P:nvUbdIzC Fr:26 [pc]"
pat = re.compile(
    r'^([0-9A-Fa-f]{4})\s.*'
    r'A:([0-9A-Fa-f]{2})\s+X:([0-9A-Fa-f]{2})\s+Y:([0-9A-Fa-f]{2})\s+'
    r'S:([0-9A-Fa-f]{2})\s+P:(\S+)\s+Fr:(\d+)'
)

records = []
with open(TRACE) as f:
    for line in f:
        m = pat.match(line)
        if m:
            pc = int(m.group(1), 16)
            a  = int(m.group(2), 16)
            x  = int(m.group(3), 16)
            y  = int(m.group(4), 16)
            sp = int(m.group(5), 16)
            flags = m.group(6)
            frame = int(m.group(7))
            records.append((pc, a, x, y, sp, flags, frame))

print(f"Total dispatch records: {len(records):,}")
print(f"Frame range: {records[0][6]} .. {records[-1][6]}")
total_frames = records[-1][6] - records[0][6]
print(f"Total frames spanned: {total_frames:,}")
print(f"Dispatches per frame: {len(records)/max(total_frames,1):.2f}")
print()

# --- 1. PC frequency ---
pc_counts = Counter(r[0] for r in records)
print("=== Top 30 most-dispatched guest PCs ===")
for pc, cnt in pc_counts.most_common(30):
    print(f"  ${pc:04X}  dispatched {cnt:>8,} times  ({100*cnt/len(records):5.1f}%)")
print()

# --- 2. Frame gaps between consecutive dispatches ---
gaps = []
for i in range(1, len(records)):
    gap = records[i][6] - records[i-1][6]
    gaps.append(gap)
if gaps:
    avg_gap = sum(gaps) / len(gaps)
    print(f"=== Frame gaps between dispatches ===")
    print(f"  avg={avg_gap:.2f}  min={min(gaps)}  max={max(gaps)}")
    gap_counts = Counter(gaps)
    print(f"  gap=0 (same frame): {gap_counts.get(0,0):,}  ({100*gap_counts.get(0,0)/len(gaps):.1f}%)")
    print(f"  gap=1:              {gap_counts.get(1,0):,}  ({100*gap_counts.get(1,0)/len(gaps):.1f}%)")
    print(f"  gap=2:              {gap_counts.get(2,0):,}  ({100*gap_counts.get(2,0)/len(gaps):.1f}%)")
    print(f"  gap>=8 (big):       {sum(v for k,v in gap_counts.items() if k>=8):,}")
    print(f"  gap>=24:            {sum(v for k,v in gap_counts.items() if k>=24):,}")
    print()

# --- 3. Transition pairs (from_pc -> to_pc) ---
trans = Counter()
for i in range(1, len(records)):
    trans[(records[i-1][0], records[i][0])] += 1
print("=== Top 30 PC transitions (from -> to) ===")
for (frm, to), cnt in trans.most_common(30):
    print(f"  ${frm:04X} -> ${to:04X}  {cnt:>8,} times")
print()

# --- 4. Identify "hot loops" — same small set of PCs repeating ---
# Look at windows of 4 consecutive dispatches
window_counts = Counter()
for i in range(len(records) - 3):
    window = tuple(r[0] for r in records[i:i+4])
    window_counts[window] += 1
print("=== Top 15 repeating 4-dispatch patterns ===")
for pat_w, cnt in window_counts.most_common(15):
    pcs = " -> ".join(f"${p:04X}" for p in pat_w)
    print(f"  [{pcs}]  {cnt:>8,} times")
print()

# --- 5. Phases: look at how PC distribution changes over time ---
# Split into 10 equal chunks by record index
chunk_size = len(records) // 10
print("=== PC distribution by phase (10 chunks) ===")
for chunk_i in range(10):
    start = chunk_i * chunk_size
    end = start + chunk_size if chunk_i < 9 else len(records)
    chunk = records[start:end]
    frame_lo = chunk[0][6]
    frame_hi = chunk[-1][6]
    pc_c = Counter(r[0] for r in chunk)
    unique_pcs = len(pc_c)
    top3 = pc_c.most_common(3)
    top3_str = ", ".join(f"${p:04X}({c})" for p,c in top3)
    dispatches_in_chunk = len(chunk)
    frame_span = frame_hi - frame_lo if frame_hi > frame_lo else 1
    rate = dispatches_in_chunk / frame_span
    print(f"  Chunk {chunk_i}: frames {frame_lo:>8,}-{frame_hi:>8,}  "
          f"unique_PCs={unique_pcs:>3}  disp/frame={rate:.2f}  top: {top3_str}")
print()

# --- 6. Same-frame dispatches (redispatches within a single frame) ---
frame_dispatch_counts = Counter()
for pc, a, x, y, sp, fl, fr in records:
    frame_dispatch_counts[fr] += 1
multi = {fr: cnt for fr, cnt in frame_dispatch_counts.items() if cnt > 1}
print(f"=== Same-frame re-dispatches ===")
print(f"  Frames with >1 dispatch: {len(multi):,} out of {len(frame_dispatch_counts):,} unique frames")
if multi:
    top_multi = sorted(multi.items(), key=lambda x: -x[1])[:15]
    print(f"  Highest re-dispatch counts:")
    for fr, cnt in top_multi:
        print(f"    Frame {fr:>8,}: {cnt} dispatches")
print()

# --- 7. Frames per dispatch by PC (how "heavy" is each block?) ---
# For each dispatch, how many frames does it "consume" before the next dispatch?
print("=== Average frames consumed per dispatch, by guest PC ===")
pc_frame_costs = defaultdict(list)
for i in range(len(records) - 1):
    pc = records[i][0]
    gap = records[i+1][6] - records[i][6]
    pc_frame_costs[pc].append(gap)
# Sort by total frames consumed (sum of gaps)
pc_total = {pc: sum(gaps_l) for pc, gaps_l in pc_frame_costs.items()}
for pc, total in sorted(pc_total.items(), key=lambda x: -x[1])[:20]:
    gaps_l = pc_frame_costs[pc]
    avg = sum(gaps_l)/len(gaps_l)
    print(f"  ${pc:04X}: {len(gaps_l):>7,} dispatches, avg_gap={avg:5.1f} frames, "
          f"total_frames={total:>10,}  ({100*total/max(total_frames,1):.1f}%)")
print()

# --- 8. Detect "trampoline" patterns — dispatches with 0-frame gap ---
zero_gap_pcs = Counter()
for i in range(1, len(records)):
    if records[i][6] == records[i-1][6]:
        zero_gap_pcs[records[i-1][0]] += 1
        zero_gap_pcs[records[i][0]] += 1  # also count the target
print("=== PCs involved in 0-frame-gap (trampoline) dispatches ===")
for pc, cnt in zero_gap_pcs.most_common(20):
    print(f"  ${pc:04X}: {cnt:>8,} zero-gap appearances")
print()

# --- 9. Show a sample of the gameplay section (after RAM test) ---
# Find where we leave the 79A5/79AB pattern
first_non_ramtest = None
for i, (pc, a, x, y, sp, fl, fr) in enumerate(records):
    if pc not in (0x79A5, 0x79AB, 0x79A1, 0x79A7, 0x79B0, 0x79DD, 0x79D0, 0x79D4):
        first_non_ramtest = i
        break
if first_non_ramtest is not None:
    print(f"=== First non-RAM-test dispatch at record {first_non_ramtest}, "
          f"frame {records[first_non_ramtest][6]:,} ===")
    for j in range(first_non_ramtest, min(first_non_ramtest + 30, len(records))):
        pc, a, x, y, sp, fl, fr = records[j]
        print(f"  [{j:>7}] ${pc:04X}  A:{a:02X} X:{x:02X} Y:{y:02X} SP:{sp:02X} {fl}  Fr:{fr}")
print()
