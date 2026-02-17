#!/usr/bin/env python3
"""
analyze_trace.py - Analyze reservation trace CSV from Mesen Lua script.

Reads reservation_trace.csv and produces a human-readable analysis
focusing on:
  1. Reservation lifecycle (create → consume)
  2. First dispatches after sa_run completes
  3. Crash detection (unexpected execution addresses)
  4. State anomalies (stale mapper_prg_bank, leftover reservations, etc.)
"""

import csv
import sys
from collections import defaultdict

CSV_PATH = "reservation_trace.csv"

def load_trace(path):
    """Load CSV trace into list of dicts."""
    rows = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # First row is header
            if line.startswith("event,"):
                continue
            parts = line.split(",", 18)  # 19 columns
            if len(parts) < 18:
                # Pad with empty strings
                parts += [""] * (19 - len(parts))
            row = {
                "event": parts[0],
                "frame": int(parts[1]) if parts[1] else 0,
                "nativePC": int(parts[2], 16) if parts[2] else 0,
                "phase": parts[3],
                "emPC": int(parts[4], 16) if parts[4] else 0,
                "emA": int(parts[5], 16) if parts[5] else 0,
                "emX": int(parts[6], 16) if parts[6] else 0,
                "emY": int(parts[7], 16) if parts[7] else 0,
                "emSP": int(parts[8], 16) if parts[8] else 0,
                "emStatus": int(parts[9], 16) if parts[9] else 0,
                "mapperBank": int(parts[10], 16) if parts[10] else 0,
                "resCount": int(parts[11]) if parts[11] else 0,
                "resEnabled": int(parts[12]) if parts[12] else 0,
                "nextFree": int(parts[13]) if parts[13] else 0,
                "flashAddr": int(parts[14], 16) if parts[14] else 0,
                "flashBank": int(parts[15], 16) if parts[15] else 0,
                "flashCacheIdx": int(parts[16]) if parts[16] else 0,
                "codeIdx": int(parts[17]) if parts[17] else 0,
                "extra": parts[18] if len(parts) > 18 else "",
            }
            rows.append(row)
    return rows


def analyze_reservations(rows):
    """Track reservation lifecycle."""
    print("\n" + "=" * 70)
    print("RESERVATION LIFECYCLE")
    print("=" * 70)

    reserves = []
    consumes = []
    res_count_changes = []

    for i, r in enumerate(rows):
        if r["event"] == "RESERVE_SAFE_ENTER":
            reserves.append((i, r))
        elif r["event"] == "CONSUME_ENTER":
            consumes.append((i, r))
        elif r["event"] == "RES_COUNT_WRITE":
            res_count_changes.append((i, r))

    print(f"\nTotal reserve_block_for_pc_safe calls: {len(reserves)}")
    print(f"Total consume_reservation calls: {len(consumes)}")
    print(f"Total reservation_count writes: {len(res_count_changes)}")

    if reserves:
        print("\n--- Reserve calls ---")
        for idx, (i, r) in enumerate(reserves):
            print(f"  [{i:5d}] frame={r['frame']:3d} emPC={r['emPC']:04X} "
                  f"resCount={r['resCount']} nextFree={r['nextFree']:3d} "
                  f"flashCacheIdx={r['flashCacheIdx']:3d} codeIdx={r['codeIdx']:3d} "
                  f"| {r['extra']}")

    if consumes:
        print("\n--- Consume calls ---")
        for idx, (i, r) in enumerate(consumes):
            print(f"  [{i:5d}] frame={r['frame']:3d} emPC={r['emPC']:04X} "
                  f"resCount={r['resCount']} flashCacheIdx={r['flashCacheIdx']:3d} "
                  f"| {r['extra']}")

    if res_count_changes:
        print("\n--- reservation_count changes ---")
        for idx, (i, r) in enumerate(res_count_changes):
            print(f"  [{i:5d}] frame={r['frame']:3d} nativePC={r['nativePC']:04X} "
                  f"| {r['extra']}")

    # Check for unbalanced reservations
    if len(reserves) != len(consumes):
        print(f"\n*** WARNING: {len(reserves)} reserves vs {len(consumes)} consumes! ***")


def analyze_sa_run(rows):
    """Analyze sa_run phase."""
    print("\n" + "=" * 70)
    print("SA_RUN PHASE")
    print("=" * 70)

    sa_enter = None
    sa_periodic = []
    for i, r in enumerate(rows):
        if r["event"] == "SA_RUN_ENTER":
            sa_enter = (i, r)
        elif r["event"] == "SA_PERIODIC":
            sa_periodic.append((i, r))

    if sa_enter:
        i, r = sa_enter
        print(f"\nsa_run entered at frame {r['frame']}")
        print(f"  emPC={r['emPC']:04X} mapperBank={r['mapperBank']:02X}")
    else:
        print("\n*** WARNING: sa_run never entered! ***")

    if sa_periodic:
        print(f"\nSA periodic snapshots ({len(sa_periodic)}):")
        for idx, (i, r) in enumerate(sa_periodic):
            print(f"  frame={r['frame']:3d} resCount={r['resCount']} "
                  f"nextFree={r['nextFree']:3d} mapperBank={r['mapperBank']:02X} "
                  f"| {r['extra']}")


def analyze_main_transition(rows):
    """Analyze the transition from sa_run to main loop."""
    print("\n" + "=" * 70)
    print("SA_RUN → MAIN LOOP TRANSITION")
    print("=" * 70)

    main_enter = None
    leftover_res = []
    first_dispatches = []

    for i, r in enumerate(rows):
        if r["event"] == "MAIN_LOOP_ENTER":
            main_enter = (i, r)
        elif r["event"] == "LEFTOVER_RESERVATION":
            leftover_res.append((i, r))
        elif r["event"] == "DISPATCH" and len(first_dispatches) < 30:
            first_dispatches.append((i, r))

    if main_enter:
        i, r = main_enter
        print(f"\nMain loop entered at frame {r['frame']}")
        print(f"  emPC={r['emPC']:04X} emA={r['emA']:02X} emX={r['emX']:02X} "
              f"emY={r['emY']:02X} emSP={r['emSP']:02X} emStatus={r['emStatus']:02X}")
        print(f"  mapperBank={r['mapperBank']:02X} resCount={r['resCount']} "
              f"resEnabled={r['resEnabled']} nextFree={r['nextFree']}")
        print(f"  flashAddr={r['flashAddr']:04X} flashBank={r['flashBank']:02X} "
              f"flashCacheIdx={r['flashCacheIdx']} codeIdx={r['codeIdx']}")
    else:
        print("\n*** WARNING: Main loop never entered! ***")
        # Check if sa_run is taking too long or script hit event limit
        for r in rows:
            if r["event"] == "EVENT_LIMIT_HIT":
                print("  → Event limit hit before main loop!")
            if r["event"] == "TRACE_DONE":
                print("  → Trace finished but main loop never seen")

    if leftover_res:
        print(f"\n*** LEFTOVER RESERVATIONS AT MAIN LOOP ENTRY: {len(leftover_res)} ***")
        for i, r in leftover_res:
            print(f"  {r['extra']}")
    else:
        print("\n  No leftover reservations (good)")


def analyze_dispatches(rows):
    """Analyze first dispatches after main loop starts."""
    print("\n" + "=" * 70)
    print("FIRST DISPATCHES")
    print("=" * 70)

    dispatches = []
    flash_returns = []
    cross_banks = []

    for i, r in enumerate(rows):
        if r["event"] == "DISPATCH":
            dispatches.append((i, r))
        elif r["event"] == "FLASH_RET":
            flash_returns.append((i, r))
        elif r["event"] == "CROSS_BANK":
            cross_banks.append((i, r))

    print(f"\nTotal dispatches logged: {len(dispatches)}")
    print(f"Total flash_dispatch_return logged: {len(flash_returns)}")
    print(f"Total cross_bank_dispatch logged: {len(cross_banks)}")

    if dispatches:
        print(f"\nFirst {min(30, len(dispatches))} dispatches:")
        for idx, (i, r) in enumerate(dispatches[:30]):
            print(f"  disp#{idx+1:3d} frame={r['frame']:3d} emPC={r['emPC']:04X} "
                  f"mapperBank={r['mapperBank']:02X} | {r['extra']}")

    # Check for dispatch with no matching return
    disp_events = [(i, r) for i, r in enumerate(rows) if r["event"] in 
                   ("DISPATCH", "FLASH_RET", "CROSS_BANK")]
    if disp_events:
        print(f"\nDispatch/Return sequence (first 50):")
        for idx, (i, r) in enumerate(disp_events[:50]):
            marker = ""
            if r["event"] == "DISPATCH":
                marker = ">>>"
            elif r["event"] == "FLASH_RET":
                marker = "<<<"
            elif r["event"] == "CROSS_BANK":
                marker = "---"
            print(f"  {marker} [{i:5d}] {r['event']:20s} emPC={r['emPC']:04X} "
                  f"mapperBank={r['mapperBank']:02X} | {r['extra']}")


def analyze_crashes(rows):
    """Look for crash indicators."""
    print("\n" + "=" * 70)
    print("CRASH DETECTION")
    print("=" * 70)

    crashes = [r for r in rows if r["event"].startswith("CRASH_")]
    if crashes:
        print(f"\n*** {len(crashes)} CRASH EVENTS DETECTED ***")
        for r in crashes[:20]:
            print(f"  {r['event']} frame={r['frame']} | {r['extra']}")
    else:
        print("\n  No crash events detected")

    # Check for signs of hang: many dispatches to same emPC
    if rows:
        main_dispatches = [r for r in rows if r["event"] == "DISPATCH"]
        if main_dispatches:
            pc_counts = defaultdict(int)
            for r in main_dispatches:
                pc_counts[r["emPC"]] += 1
            top_pcs = sorted(pc_counts.items(), key=lambda x: -x[1])[:10]
            print(f"\n  Most dispatched emulated PCs:")
            for pc, count in top_pcs:
                pct = 100.0 * count / len(main_dispatches)
                print(f"    ${pc:04X}: {count:5d} times ({pct:.1f}%)")
                if pct > 50:
                    print(f"    *** SUSPICIOUS: >50% dispatches to same PC = possible infinite loop ***")

    # Check for dispatch_on_pc called during SA (shouldn't happen normally)
    sa_dispatches = [r for r in rows if r["event"] == "DISPATCH_IN_SA"]
    if sa_dispatches:
        print(f"\n*** {len(sa_dispatches)} DISPATCH CALLS DURING SA_RUN ***")
        for r in sa_dispatches[:10]:
            print(f"  frame={r['frame']} emPC={r['emPC']:04X} | {r['extra']}")


def analyze_state_anomalies(rows):
    """Check for state anomalies."""
    print("\n" + "=" * 70)
    print("STATE ANOMALY CHECK")
    print("=" * 70)

    # Check mapper_prg_bank at main loop entry
    main_enter = None
    for r in rows:
        if r["event"] == "MAIN_LOOP_ENTER":
            main_enter = r
            break

    if main_enter:
        bank = main_enter["mapperBank"]
        print(f"\n  mapper_prg_bank at main entry: ${bank:02X}")
        if bank != 0:
            print(f"  *** NOTE: mapper_prg_bank is ${bank:02X}, not 0 ***")
            print(f"  (This is expected if sa_run's last operation was bankswitch_prg(3))")

        # Check emulated PC
        emPC = main_enter["emPC"]
        print(f"  emulated PC at main entry: ${emPC:04X}")
        if emPC < 0x2800 or emPC > 0x3FFF:
            print(f"  *** WARNING: emulated PC ${emPC:04X} outside ROM range $2800-$3FFF ***")

    # Check if reservations_enabled is properly 0 at main entry
    if main_enter and main_enter["resEnabled"] != 0:
        print(f"\n  *** WARNING: reservations_enabled = {main_enter['resEnabled']} at main entry (should be 0) ***")

    # Check for emPC changing unexpectedly during dispatches
    dispatches = [(i, r) for i, r in enumerate(rows) if r["event"] == "DISPATCH"]
    if len(dispatches) >= 2:
        print(f"\n  emPC sequence at dispatch (first 20):")
        for idx in range(min(20, len(dispatches))):
            i, r = dispatches[idx]
            print(f"    disp#{idx+1}: emPC=${r['emPC']:04X}")

    # Check for native PC in compiled code during dispatch
    # The native PC at dispatch entry should be $6205 (dispatch_on_pc)
    for i, r in enumerate(rows):
        if r["event"] == "DISPATCH" and r["nativePC"] != 0x6205:
            print(f"\n  *** ANOMALY: dispatch callback at nativePC=${r['nativePC']:04X} != $6205 ***")
            break


def print_summary(rows):
    """Print high-level summary."""
    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)

    events_by_type = defaultdict(int)
    for r in rows:
        events_by_type[r["event"]] += 1

    print(f"\nTotal events: {len(rows)}")
    print(f"\nEvent counts:")
    for event, count in sorted(events_by_type.items(), key=lambda x: -x[1]):
        print(f"  {event:30s}: {count:6d}")

    phases = set(r["phase"] for r in rows)
    print(f"\nPhases seen: {', '.join(sorted(phases))}")

    frames = set(r["frame"] for r in rows)
    if frames:
        print(f"Frame range: {min(frames)} - {max(frames)}")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else CSV_PATH

    print(f"Loading trace from: {path}")
    try:
        rows = load_trace(path)
    except FileNotFoundError:
        print(f"ERROR: {path} not found. Run Mesen with trace_reservation.lua first.")
        sys.exit(1)

    if not rows:
        print("ERROR: No data in trace file.")
        sys.exit(1)

    print(f"Loaded {len(rows)} events")

    analyze_sa_run(rows)
    analyze_reservations(rows)
    analyze_main_transition(rows)
    analyze_dispatches(rows)
    analyze_crashes(rows)
    analyze_state_anomalies(rows)
    print_summary(rows)


if __name__ == "__main__":
    main()
