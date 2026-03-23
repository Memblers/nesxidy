#!/usr/bin/env python3
"""Extract block headers from flash sector hex data and write to a compact file.
This avoids having to store the entire 48KB of hex data."""

import struct, os

# Block header positions in each 4KB sector (stride = 272 = 0x110)
HEADER_OFFSETS = [8 + i * 272 for i in range(15)]

# All 12 sector hex strings from Mesen debugger (PRG ROM 0x10000-0x1BFFF)
# Each tuple: (bank, sector_in_bank, hex_string)
# Headers extracted at offsets: 8, 280, 552, 824, 1096, 1368, 1640, 1912, 2184, 2456, 2728, 3000, 3272, 3544, 3816

def extract_header(hex_str, byte_offset):
    """Extract 8 bytes starting at byte_offset from space-separated hex string."""
    # In space-separated hex, byte N starts at char position N*3
    start = byte_offset * 3
    # Extract 8 bytes (each byte = 2 hex chars + 1 space, last byte no space)
    result = []
    for i in range(8):
        pos = start + i * 3
        if pos + 2 > len(hex_str):
            return None
        b = int(hex_str[pos:pos+2], 16)
        result.append(b)
    return result

def extract_all_headers(hex_str):
    """Extract all 15 potential block headers from a sector hex string."""
    headers = []
    for off in HEADER_OFFSETS:
        h = extract_header(hex_str, off)
        headers.append(h)
    return headers

# The hex data will be passed via process_sector calls
def process_sectors(sector_hex_list):
    """Process list of (bank, sec, hex_str) tuples, return block info list."""
    blocks = []
    total_slots = 0
    for bank, sec, hex_str in sector_hex_list:
        headers = extract_all_headers(hex_str)
        for slot, hdr in enumerate(headers):
            total_slots += 1
            if hdr is None or hdr[7] != 0xAA:
                continue
            blocks.append({
                'bank': bank,
                'sector': sec,
                'slot': slot,
                'entry_pc': hdr[0] | (hdr[1] << 8),
                'exit_pc':  hdr[2] | (hdr[3] << 8),
                'code_len': hdr[4],
                'epi_off':  hdr[5],
                'flags':    hdr[6],
            })
    return blocks, total_slots
