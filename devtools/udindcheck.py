# udindcheck.py 1ud|2ud STAGEDIR [WORKERS]: an independent check of a 1ud or 2ud certificate stage.  It uses
# nothing from certcube.h or the C++ tools: the puzzle is twsearch's devtools/tws/1ud-si.tws or 2ud-si.tws,
# simulated directly (pypy).  For every line (a solving sequence) it computes the start position (the inverse
# sequence run on Solved) and checks:
#   - every move is a move of the puzzle;
#   - the other obliques (MID2, MID3: U 1, ring 2, D 3) are as in Solved at the start, so the line keeps
#     their U, ring and D sets;
#   - 2ud only: the pair's U pieces are home at the start (the 1ud goal is the 2ud start);
#   - coverage: the line's state (1ud: where each pair orbit's U pieces are; 2ud: where its D pieces are)
#     under the 8 stage symmetries (y rotations, applied as the tws move y; times the L/R mirror, applied by
#     renaming the line's moves) marks a bitmap; no state may be marked by two lines (one line per class),
#     and every state must be marked: 1ud C(24,4)^2 = 112,911,876, 2ud C(20,4)^2 = 23,474,025.
# The rotation and mirror images are cross-checked on the first lines: the images of the renamed line
# (y-rotated) must equal the y move applied to the start position.
import sys, os, glob
from multiprocessing import Pool
from math import comb
MODE, ST = sys.argv[1], sys.argv[2]
W = int(sys.argv[3]) if len(sys.argv) > 3 else 4
TWS = os.path.expanduser('~/orbitsolve/devtools/tws/%s-si.tws' % MODE)

def parse(fn):
    L = [l.split('#')[0].rstrip() for l in open(fn)]; M = {}; S = {}; i = 0; sets = []
    while i < len(L):
        w = L[i].split()
        if w and w[0] == 'Set': sets.append(w[1])
        if w and w[0] == 'Solved':
            i += 1
            while L[i] != 'End': S[L[i].strip()] = [int(x) for x in L[i+1].split()]; i += 2
        if w and w[0] == 'Move':
            nm = w[1]; d = {}; i += 1
            while L[i] != 'End': d[L[i].strip()] = [int(x) - 1 for x in L[i+1].split()]; i += 2
            M[nm] = d
        i += 1
    return sets, S, M
sets, S, M = parse(TWS)
assert sets == ['MID2', 'PAIRA', 'MID3', 'PAIRB'], sets
N = 96; off = {s: 24 * i for i, s in enumerate(sets)}
SOLVED = []
for s in sets: SOLVED += S[s]
P = {}
for b in M:
    p = list(range(N))
    for s in sets:
        q = M[b].get(s)
        if q: p[off[s]:off[s] + 24] = [off[s] + x for x in q]
    P[b] = p
def amt(m): return {"'": 3, "2": 2}.get(m[-1], 1) if m[-1] in "'2" else 1
MP = {}
def perm(m):
    if m not in MP:
        b = m.rstrip("'2")
        if b not in P or b == 'y': raise KeyError(m)
        p = P[b]; idx = list(range(N))
        for _ in range(amt(m)): idx = [idx[p[i]] for i in range(N)]
        MP[m] = idx
    return MP[m]
YP = P['y']
def inv1(m): return m.rstrip("'2") + {1: "'", 2: "2", 3: ""}[amt(m)]
def start(ms):
    st = SOLVED
    for m in reversed(ms):
        p = perm(inv1(m)); st = [st[i] for i in p]
    return st
def mir(m):
    i = 0
    while m[i] in '23': i += 1
    c = {'L': 'R', 'R': 'L'}.get(m[i], m[i]); return inv1(m[:i] + c + m[i+1:])
CYC = 'FRBL'
def rot(k, m):
    i = 0
    while m[i] in '23': i += 1
    c = m[i]; return m[:i] + (CYC[(CYC.index(c) + k) % 4] if c in CYC else c) + m[i+1:]
MIDOK = [(i, SOLVED[i]) for s in ('MID2', 'MID3') for i in range(off[s], off[s] + 24)]
UHOME = [off[s] + i for s in ('PAIRA', 'PAIRB') for i in range(4)]
LAB = 1 if MODE == '1ud' else 3
if MODE == '1ud':
    POS = list(range(24)); TOTAL = comb(24, 4) ** 2
else:
    POS = list(range(4, 24)); TOTAL = comb(20, 4) ** 2
PI = {p: i for i, p in enumerate(POS)}; n1 = comb(len(POS), 4)
def r4(cells):  # rank of a 4-subset of POS (combinadic)
    c = sorted(PI[x] for x in cells)
    return comb(c[0], 1) + comb(c[1], 2) + comb(c[2], 3) + comb(c[3], 4)
def key(st):
    a = [i for i in range(24) if st[off['PAIRA'] + i] == LAB]
    b = [i for i in range(24) if st[off['PAIRB'] + i] == LAB]
    if len(a) != 4 or len(b) != 4 or any(x not in PI for x in a + b): return None
    return r4(a) * n1 + r4(b)
def images(ms, st):
    out = set()
    for s0 in (st, start([mir(m) for m in ms])):
        s = s0
        for k in range(4):
            out.add(key(s)); s = [s[i] for i in YP]
    return out

def work(files):
    bm = bytearray((TOTAL + 7) // 8); bad = []; nl = 0; dup = 0; longest = 0; xcheck = 0
    for f in files:
        for l in open(f):
            ms = l.split(); nl += 1; longest = max(longest, len(ms))
            try: st = start(ms)
            except KeyError as e: bad.append('illegal move %s: %s' % (e, l.strip())); continue
            if any(st[i] != v for i, v in MIDOK): bad.append('another oblique moved: ' + l.strip()); continue
            if MODE == '2ud' and any(st[i] != 1 for i in UHOME): bad.append('not a 2ud start: ' + l.strip()); continue
            im = images(ms, st)
            if None in im: bad.append('bad state: ' + l.strip()); continue
            if xcheck < 200:  # the renamed rotations must give the same set as the y move
                xcheck += 1
                alt = set()
                for k in range(4):
                    alt.add(key(start([rot(k, m) for m in ms]))); alt.add(key(start([rot(k, mir(m)) for m in ms])))
                if alt != im: bad.append('symmetry cross-check failed: ' + l.strip())
            for r in im:
                if bm[r >> 3] >> (r & 7) & 1: dup += 1
                bm[r >> 3] |= 1 << (r & 7)
    return bytes(bm), nl, dup, longest, bad

if __name__ == '__main__':
    files = sorted(glob.glob(ST + '/w*.txt'), key=lambda f: -os.path.getsize(f))
    parts = [[] for _ in range(W)]; sz = [0] * W
    for f in files:  # by size, greedily
        j = sz.index(min(sz)); parts[j].append(f); sz[j] += os.path.getsize(f)
    with Pool(W) as pool: res = pool.map(work, parts)
    total = 0; nl = 0; dup = 0; longest = 0; bad = []; acc = 0
    for bm, n, d, lo, b in res:
        x = int.from_bytes(bm, 'little')
        dup += d + bin(acc & x).count('1'); acc |= x; nl += n; longest = max(longest, lo); bad += b
    marked = bin(acc).count('1')
    print('%s %s: %d lines, longest %d; states marked %d of %d; marked twice %d; problems %d' % (MODE, ST, nl, longest, marked, TOTAL, dup, len(bad)))
    for b in bad[:20]: print('  ' + b)
    print('PASS' if marked == TOTAL and dup == 0 and not bad else 'FAIL')
