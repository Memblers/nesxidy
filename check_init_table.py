data = open('exidy.nes','rb').read()
base = 16 + 31*16384

# Init function table at $F8AC in ROM
off = base + 0x38AC
print('Init function table at $F8AC:')
for i in range(0, 20, 2):
    lo = data[off+i]
    hi = data[off+i+1]
    addr = hi*256 + lo
    if addr == 0:
        print('  Entry %d: $%04X (end)' % (i//2, addr))
        break
    print('  Entry %d: $%04X' % (i//2, addr))
