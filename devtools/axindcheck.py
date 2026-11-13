# axindcheck.py 3ax|4op STAGEDIR [WORKERS]: an independent check of a 3ax or 4op certificate stage (lNN.txt files).
# It uses nothing from certcube.h or the C++ tools: the puzzle is twsearch's devtools/tws/obl8ax.tws or obl8op.tws
# (8x8x8 centre orbits; the pair is CENTERS2 + CENTERS5; faces: U 0-3, then 4-7, 8-11, 12-15, 16-19, D 20-23,
# opposite side faces 4-7 / 12-15 and 8-11 / 16-19), simulated directly (pypy).  For every line (solving):
#   - every move is a move of the puzzle (no whole-cube rotations);
#   - the other centre orbits (CENTERS3, 4, 6, 8) are solved at the start, so the line keeps them;
#   - the start is a stage start: the pair's U and D pieces are home (3ax); and 4op: also every side piece
#     on its own axis (the 3ax goal);
#   - coverage: the line's state (3ax: per orbit, which 8 of the 16 side cells hold the first axis's pieces,
#     C(16,8)^2 = 165,636,900; 4op: per orbit and axis, which 4 of its 8 cells hold one face's pieces,
#     C(8,4)^4 = 24,010,000) under the 16 stage symmetries marks a bitmap; no state marked by two lines, every
#     state marked.  Symmetries: the y and x2 rotations as the puzzle's own rotation moves (with the colour
#     relabelling they induce on Solved), times the L/R mirror by renaming the line's moves.  Cross-check on the
#     first lines: the rotation images must equal the starts of the renamed lines.
import sys, os, glob
from multiprocessing import Pool
from math import comb
MODE, ST = sys.argv[1], sys.argv[2]
W = int(sys.argv[3]) if len(sys.argv) > 3 else 4
TWS = os.path.expanduser('~/orbitsolve/devtools/tws/obl8%s.tws' % ('ax' if MODE == '3ax' else 'op'))

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
N = 24 * len(sets); off = {s: 24 * i for i, s in enumerate(sets)}
PAIR = ('CENTERS2', 'CENTERS5'); OTHER = [s for s in sets if s not in PAIR]
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
        if b not in P or b in ('x', 'y', 'z'): raise KeyError(m)
        p = P[b]; idx = list(range(N))
        for _ in range(amt(m)): idx = [idx[p[i]] for i in range(N)]
        MP[m] = idx
    return MP[m]
def inv1(m): return m.rstrip("'2") + {1: "'", 2: "2", 3: ""}[amt(m)]
def run(st, ms):
    for m in ms:
        p = perm(m); st = [st[i] for i in p]
    return st
def start(ms): return run(SOLVED, [inv1(m) for m in reversed(ms)])

# rotations: sigma in <y, x2>; image of a start s: relabel by lambda (Solved.sigma = lambda o Solved), then sigma^-1
ROTS = []
for k in range(4):
    for f in (0, 2):
        idx = list(range(N))
        for _ in range(f): idx = [idx[P['x'][i]] for i in range(N)]
        for _ in range(k): idx = [idx[P['y'][i]] for i in range(N)]
        sig = idx
        rs = [SOLVED[i] for i in sig]  # Solved run through sigma
        lam = [{} for _ in sets]  # per orbit: the labelling differs between orbits (3ax pair: F and B alike)
        for i, (a, b) in enumerate(zip(SOLVED, rs)):
            if lam[i // 24].setdefault(a, b) != b: raise SystemExit('rotation k=%d f=%d does not relabel Solved consistently' % (k, f))
        inv = [0] * N
        for i, j in enumerate(sig): inv[j] = i
        ROTS.append((lam, inv, k, f))
def rotimg(s, lam, inv):
    t = [lam[i // 24][v] for i, v in enumerate(s)]
    return [t[i] for i in inv]
def face(m):
    i = 0
    while m[i] in '23': i += 1
    return i
def mir(m):
    i = face(m); c = {'L': 'R', 'R': 'L'}.get(m[i], m[i]); return inv1(m[:i] + c + m[i+1:])
CYC = 'FRBL'
def rotname(k, f, m):
    i = face(m); c = m[i]
    if f: c = {'U': 'D', 'D': 'U', 'F': 'B', 'B': 'F'}.get(c, c)
    c = CYC[(CYC.index(c) + k) % 4] if c in CYC else c
    return m[:i] + c + m[i+1:]

OTHOK = [(i, SOLVED[i]) for s in OTHER for i in range(off[s], off[s] + 24)]
SIDE = list(range(4, 20)); SI = {p: i for i, p in enumerate(SIDE)}
AX1 = [4, 5, 6, 7, 12, 13, 14, 15]; AX2 = [8, 9, 10, 11, 16, 17, 18, 19]
I1 = {p: i for i, p in enumerate(AX1)}; I2 = {p: i for i, p in enumerate(AX2)}
def rk(cells):
    c = sorted(cells); return sum(comb(x, j + 1) for j, x in enumerate(c))
if MODE == '3ax':
    TOTAL = comb(16, 8) ** 2
    def key(st):
        r = 0
        for s in PAIR:
            o = off[s]
            if any(st[o + i] != 1 for i in range(4)) or any(st[o + i] != 6 for i in range(20, 24)): return None
            a = [SI[p] for p in SIDE if st[o + p] == 2]
            if len(a) != 8: return None
            r = r * comb(16, 8) + rk(a)
        return r
else:
    TOTAL = comb(8, 4) ** 4
    def key(st):
        r = 0
        for s in PAIR:
            o = off[s]
            if any(st[o + i] != 1 for i in range(4)) or any(st[o + i] != 6 for i in range(20, 24)): return None
            if any(st[o + p] not in (2, 4) for p in AX1) or any(st[o + p] not in (3, 5) for p in AX2): return None
            a = [I1[p] for p in AX1 if st[o + p] == 2]; b = [I2[p] for p in AX2 if st[o + p] == 3]
            if len(a) != 4 or len(b) != 4: return None
            r = (r * 70 + rk(a)) * 70 + rk(b)
        return r

def images(ms, st):
    out = set()
    for s0 in (st, start([mir(m) for m in ms])):
        for lam, inv, k, f in ROTS: out.add(key(rotimg(s0, lam, inv)))
    return out

def work(task):
    f, a, b = task
    bm = bytearray((TOTAL + 7) // 8); bad = []; nl = 0; dup = 0; longest = 0; xcheck = 0
    with open(f) as fh:
        for j, l in enumerate(fh):
            if j < a: continue
            if j >= b: break
            ms = l.split(); nl += 1; longest = max(longest, len(ms))
            try: st = start(ms)
            except KeyError as e: bad.append('illegal move %s: %s' % (e, l.strip())); continue
            if any(st[i] != v for i, v in OTHOK): bad.append('another orbit moved: ' + l.strip()); continue
            if key(st) is None: bad.append('not a %s start: %s' % (MODE, l.strip())); continue
            im = images(ms, st)
            if None in im: bad.append('symmetry image not a start: ' + l.strip()); continue
            if a == 0 and xcheck < 100:
                xcheck += 1
                alt = set()
                for k in range(4):
                    for fl in (0, 1):
                        alt.add(key(start([rotname(k, fl, m) for m in ms])))
                        alt.add(key(start([rotname(k, fl, mir(m)) for m in ms])))
                if alt != im: bad.append('symmetry cross-check failed: ' + l.strip())
            for r in im:
                if bm[r >> 3] >> (r & 7) & 1: dup += 1
                bm[r >> 3] |= 1 << (r & 7)
    return bytes(bm), nl, dup, longest, bad

if __name__ == '__main__':
    tasks = []
    for f in sorted(glob.glob(ST + '/l*.txt')):
        n = sum(1 for _ in open(f))
        for a in range(0, max(n, 1), 400000): tasks.append((f, a, a + 400000))
    acc = 0; nl = 0; dup = 0; longest = 0; bad = []
    with Pool(W) as pool:
        for bm, n, d, lo, b in pool.imap_unordered(work, tasks):
            x = int.from_bytes(bm, 'little')
            dup += d + bin(acc & x).count('1'); acc |= x; nl += n; longest = max(longest, lo); bad += b
    marked = bin(acc).count('1')
    print('%s %s: %d lines, longest %d; states marked %d of %d; marked twice %d; problems %d' % (MODE, ST, nl, longest, marked, TOTAL, dup, len(bad)))
    for b in bad[:20]: print('  ' + b)
    print('PASS' if marked == TOTAL and dup == 0 and not bad else 'FAIL')
