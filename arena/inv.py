import sys
# solving sequence -> generating sequence (reverse, invert each move)
def inv(m):
    if m.endswith("2"): return m
    if m.endswith("'"): return m[:-1]
    return m + "'"
for line in sys.stdin:
    t = line.split()
    print(" ".join(inv(m) for m in reversed(t)))
