# 2026-10-26: replace the blocks of a boundary file (blocks: stage line L0, then alternatives; a
# blank line after each) whose L0 starts a block of FIX by FIX's block; print the result.
#   python3 bsplice.py PART FIX > OUT
import sys
def blocks(fn):
    out, cur = [], []
    for l in open(fn):
        l = l.rstrip('\n')
        if l: cur.append(l)
        elif cur: out.append(cur); cur = []
    if cur: out.append(cur)
    return out
fix = {b[0]: b for b in blocks(sys.argv[2])}
n = 0
for b in blocks(sys.argv[1]):
    if b[0] in fix: b = fix.pop(b[0]); n += 1
    print('\n'.join(b)); print()
if fix: sys.exit('bsplice: %d FIX blocks not in PART' % len(fix))
print('bsplice: %d blocks replaced' % n, file=sys.stderr)
