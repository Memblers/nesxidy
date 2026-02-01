#!/usr/bin/env python3
"""Analyze PC trace to find reset patterns"""

import csv
from collections import defaultdict

def analyze_trace(filename):
    # Read all PC values
    entries = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            entries.append({
                'frame': int(row['frame']),
                'pc': int(row['pc'], 16),
                'sp': int(row['sp'], 16),
                'source': row.get('source', 'hi')  # for backwards compatibility
            })
    
    print(f"Total PC changes: {len(entries)}")
    
    # Find all jumps to $2800-$28FF (reset area)
    print("\n=== Jumps to $28xx ===")
    prev = None
    reset_count = 0
    for i, entry in enumerate(entries):
        pc = entry['pc']
        if pc >= 0x2800 and pc <= 0x28FF:
            if prev is None or prev['pc'] < 0x2800 or prev['pc'] > 0x28FF:
                reset_count += 1
                print(f"  Reset #{reset_count}: Frame {entry['frame']}: {prev['pc'] if prev else 0:04X} -> {pc:04X} (SP={entry['sp']:02X})")
        prev = entry
    
    # Find unique PC values and their frequency
    pc_counts = defaultdict(int)
    for entry in entries:
        pc_counts[entry['pc']] += 1
    
    print(f"\n=== Unique PC values: {len(pc_counts)} ===")
    
    # Show PC values sorted by frequency
    print("\n=== Top 20 most frequent PC values ===")
    sorted_pcs = sorted(pc_counts.items(), key=lambda x: -x[1])[:20]
    for pc, count in sorted_pcs:
        print(f"  ${pc:04X}: {count} times")
    
    # Find frame ranges where PC is in different areas
    print("\n=== PC area by frame range ===")
    current_area = None
    area_start = 0
    for entry in entries:
        pc = entry['pc']
        if pc >= 0x2800 and pc < 0x2A00:
            area = "init ($28xx)"
        elif pc >= 0x2A00 and pc < 0x2B00:
            area = "main ($2Axx)"
        elif pc >= 0x3000 and pc < 0x4000:
            area = "game ($3xxx)"
        else:
            area = f"other (${pc:04X})"
        
        if area != current_area:
            if current_area is not None:
                print(f"  Frame {area_start}-{entry['frame']}: {current_area}")
            current_area = area
            area_start = entry['frame']
    
    # Find SP anomalies (sudden changes)
    print("\n=== SP changes > 2 ===")
    prev = None
    for entry in entries:
        if prev is not None:
            sp_diff = abs(entry['sp'] - prev['sp'])
            if sp_diff > 2 and sp_diff < 250:  # ignore wrap-around
                print(f"  Frame {entry['frame']}: SP {prev['sp']:02X} -> {entry['sp']:02X} (diff={sp_diff}) at PC=${entry['pc']:04X}")
        prev = entry

if __name__ == "__main__":
    analyze_trace("c:\\proj\\c\\NES\\nesxidy-co\\nesxidy\\pc_trace.csv")
