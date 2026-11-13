# mergeN.py OUT[.zst] P1.txt P1.left P2.txt P2.left ... Pn.txt (2026-11-04): blocks of a multi-pass boundary run:
# each pass's blocks for classes not in its .left, and all of the last pass's blocks.  Checks that each
# pass's classes are exactly the previous pass's left classes.
import sys, subprocess
out, args = sys.argv[1], sys.argv[2:]
def lines(f):  # a .zst input through zstd -dc
    if not f.endswith('.zst'): return open(f)
    return subprocess.Popen(['zstd', '-dc', '-q', f], stdout=subprocess.PIPE, text=True).stdout
def blocks(f):
    cur = []
    for l in lines(f):
        l = l.rstrip('\n')
        if not l:
            if cur: yield cur; cur = []
        else: cur.append(l)
    if cur: yield cur
passes = []
i = 0
while i < len(args):
    txt = args[i]; left = args[i + 1] if i + 1 < len(args) else None
    passes.append((txt, left)); i += 2
prev = None
n = 0
zp = subprocess.Popen(['zstd', '-q', '-T4', '-10', '-f', '-o', out], stdin=subprocess.PIPE, text=True) if out.endswith('.zst') else None
with (zp.stdin if zp else open(out, 'w')) as o:
    for txt, left in passes:
        L = set(l.strip() for l in open(left) if l.strip()) if left else set()
        firsts = set(); kept = 0
        for b in blocks(txt):
            firsts.add(b[0])
            if b[0] in L: continue
            o.write('\n'.join(b) + '\n\n'); kept += 1
        if prev is not None: assert firsts == prev, (txt, len(firsts), len(prev))
        assert L <= firsts, txt
        print('%s: %d blocks, %d kept, %d left' % (txt, len(firsts), kept, len(L)))
        n += kept; prev = L
    assert not prev, 'the last pass left %d classes' % len(prev)
if zp: assert zp.wait() == 0, 'zstd failed'
print('%s: %d blocks' % (out, n))
