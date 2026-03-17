#!/usr/bin/env python3
"""Fast trace file searcher for Galaxian — reads line-by-line without loading entire file."""
import sys, re

TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_galaxian.txt"

def search(start_line, end_line, pattern):
    rx = re.compile(pattern)
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        for i, line in enumerate(f):
            if i < start_line:
                continue
            if i >= end_line:
                break
            if rx.search(line):
                print(f"{i:>10}: {line.rstrip()}")

def dump(start_line, count):
    with open(TRACE, 'r', encoding='utf-8', errors='replace') as f:
        for i, line in enumerate(f):
            if i < start_line:
                continue
            if i >= start_line + count:
                break
            print(f"{i:>10}: {line.rstrip()}")

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'search':
        search(int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
    elif cmd == 'dump':
        dump(int(sys.argv[1 + 0]), int(sys.argv[2]))
        # usage: trace_search_galaxian.py dump <start_line> <count>
