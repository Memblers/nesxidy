"""Look at the actual trace format and find the loop pattern."""
TRACE = r"C:\Users\membl\OneDrive\Documents\Mesen2\Debugger\nes_donkeykong.txt"

# Show first 30 lines to understand format
print("=== FIRST 30 LINES ===")
with open(TRACE, 'r') as f:
    for i in range(30):
        line = f.readline()
        print(f"{i:>6}: {line.rstrip()[:150]}")

# Show lines around 1M mark 
print("\n=== LINES AROUND 1,000,000 ===")
with open(TRACE, 'r', buffering=1024*1024) as f:
    for i, line in enumerate(f):
        if 999990 <= i <= 1000010:
            print(f"{i:>10}: {line.rstrip()[:150]}")
        if i > 1000010:
            break

# Show lines around 5M mark
print("\n=== LINES AROUND 5,000,000 ===")
with open(TRACE, 'r', buffering=1024*1024) as f:
    for i, line in enumerate(f):
        if 4999990 <= i <= 5000010:
            print(f"{i:>10}: {line.rstrip()[:150]}")
        if i > 5000010:
            break
