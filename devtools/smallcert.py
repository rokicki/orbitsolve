# smallcert.py 1ud|2ud|3ax|4op STAGEDIR OUTDIR [WORKERS]: write a short version of a certificate stage.
# A line (solving, length L) for class p is left out when some move m that keeps the stage's other orbits
# (1ud/2ud: MID2, MID3; 3ax/4op: CENTERS3, 4, 6, 8) as in Solved takes p's start to a stage state q whose
# class has a line of length < L: then m + line(q) solves p in at most L moves.  Chains of left-out classes
# end at kept lines (lengths strictly decrease), so the full stage can be rebuilt from the kept lines by
# a breadth-first pass over those moves (each class then gets a length <= its original one).
# Same simulation, keys and stage symmetries as udindcheck.py / axindcheck.py (the .tws definitions, pypy);
# no certcube.h.  OUTDIR gets the same file names as STAGEDIR, each holding its kept lines in order.
import sys, os, glob
from array import array
import multiprocessing
from multiprocessing import Pool
from math import comb
if len(sys.argv) < 4:
    print('usage: smallcert.py 1ud|2ud|3ax|4op STAGEDIR OUTDIR [WORKERS]'); sys.exit(1)
print('smallcert.py ' + ' '.join(os.path.basename(a.rstrip('/')) if '/' in a else a for a in sys.argv[1:]))
MODE, ST, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
W = int(sys.argv[4]) if len(sys.argv) > 4 else 8
if MODE not in ('1ud', '2ud', '3ax', '4op'): print('unknown stage ' + MODE); sys.exit(1)
UD = MODE in ('1ud', '2ud')
TWS = os.path.expanduser('~/orbitsolve/devtools/tws/' + (MODE + '-si.tws' if UD else 'obl8%s.tws' % MODE[1:]))

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
PAIR = ('PAIRA', 'PAIRB') if UD else ('CENTERS2', 'CENTERS5')
OTHER = [s for s in sets if s not in PAIR]
SOLVED = []
for s in sets: SOLVED += S[s]
ROTMV = ('y',) if UD else ('x', 'y', 'z')
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
        if b not in P or b in ROTMV: raise KeyError(m)
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
def face(m):
    i = 0
    while m[i] in '23': i += 1
    return i
def mir(m):
    i = face(m); c = {'L': 'R', 'R': 'L'}.get(m[i], m[i]); return inv1(m[:i] + c + m[i+1:])
OTHOK = [(i, SOLVED[i]) for s in OTHER for i in range(off[s], off[s] + 24)]
def rk(cells):
    c = sorted(cells); return sum(comb(x, j + 1) for j, x in enumerate(c))

if UD:
    YP = P['y']
    UHOME = [off[s] + i for s in PAIR for i in range(4)]
    LAB = 1 if MODE == '1ud' else 3
    POS = list(range(24)) if MODE == '1ud' else list(range(4, 24))
    PI = {p: i for i, p in enumerate(POS)}; n1 = comb(len(POS), 4); TOTAL = n1 ** 2
    def key(st):
        if MODE == '2ud' and any(st[i] != 1 for i in UHOME): return None
        a = [i for i in range(24) if st[off['PAIRA'] + i] == LAB]
        b = [i for i in range(24) if st[off['PAIRB'] + i] == LAB]
        if len(a) != 4 or len(b) != 4 or any(x not in PI for x in a + b): return None
        return rk([PI[x] for x in a]) * n1 + rk([PI[x] for x in b])
    def images(ms, st):
        out = set()
        for s0 in (st, start([mir(m) for m in ms])):
            s = s0
            for k in range(4):
                out.add(key(s)); s = [s[i] for i in YP]
        return out
else:
    ROTS = []
    for k in range(4):
        for f in (0, 2):
            idx = list(range(N))
            for _ in range(f): idx = [idx[P['x'][i]] for i in range(N)]
            for _ in range(k): idx = [idx[P['y'][i]] for i in range(N)]
            rs = [SOLVED[i] for i in idx]; lam = [{} for _ in sets]
            for i, (a, b) in enumerate(zip(SOLVED, rs)):
                if lam[i // 24].setdefault(a, b) != b: raise SystemExit('rotation does not relabel Solved consistently')
            inv = [0] * N
            for i, j in enumerate(idx): inv[j] = i
            ROTS.append((lam, inv))
    def rotimg(s, lam, inv):
        t = [lam[i // 24][v] for i, v in enumerate(s)]
        return [t[i] for i in inv]
    SIDE = list(range(4, 20)); SI = {p: i for i, p in enumerate(SIDE)}
    AX1 = [4, 5, 6, 7, 12, 13, 14, 15]; AX2 = [8, 9, 10, 11, 16, 17, 18, 19]
    I1 = {p: i for i, p in enumerate(AX1)}; I2 = {p: i for i, p in enumerate(AX2)}
    TOTAL = comb(16, 8) ** 2 if MODE == '3ax' else comb(8, 4) ** 4
    def key(st):
        r = 0
        for s in PAIR:
            o = off[s]
            if any(st[o + i] != 1 for i in range(4)) or any(st[o + i] != 6 for i in range(20, 24)): return None
            if MODE == '3ax':
                a = [SI[p] for p in SIDE if st[o + p] == 2]
                if len(a) != 8: return None
                r = r * comb(16, 8) + rk(a)
            else:
                if any(st[o + p] not in (2, 4) for p in AX1) or any(st[o + p] not in (3, 5) for p in AX2): return None
                a = [I1[p] for p in AX1 if st[o + p] == 2]; b = [I2[p] for p in AX2 if st[o + p] == 3]
                if len(a) != 4 or len(b) != 4: return None
                r = (r * 70 + rk(a)) * 70 + rk(b)
        return r
    def images(ms, st):
        out = set()
        for s0 in (st, start([mir(m) for m in ms])):
            for lam, inv in ROTS: out.add(key(rotimg(s0, lam, inv)))
        return out

# the moves that keep the other orbits as in Solved; one per distinct effect on the pair orbits
PIDX = [i for s in PAIR for i in range(off[s], off[s] + 24)]
MOVES = []; seen = set()
for b in sorted(P):
    if b in ROTMV: continue
    for a in ('', '2', "'"):
        p = perm(b + a)
        if any(SOLVED[p[i]] != v for i, v in OTHOK): continue
        t = tuple(p[i] for i in PIDX)
        if t in seen: continue
        seen.add(t); MOVES.append(p)

LEN = None
def lines(task):
    f, a, b = task
    with open(f) as fh:
        for j, l in enumerate(fh):
            if j < a: continue
            if j >= b: break
            yield l
def pass1(task):
    out = {}; bad = 0
    for l in lines(task):
        ms = l.split(); st = start(ms)
        if any(st[i] != v for i, v in OTHOK): bad += 1; continue
        im = images(ms, st)
        if None in im: bad += 1; continue
        out.setdefault(len(ms), array('I')).extend(im)
    return out, bad
def pass2(task):
    keep = []; cnt = {}
    for l in lines(task):
        ms = l.split(); L = len(ms); st = start(ms); drop = False
        for p in MOVES:
            k = key([st[i] for i in p])
            if k is not None and LEN[k] < L: drop = True; break
        c = cnt.setdefault(L, [0, 0]); c[0] += 1
        if not drop: keep.append(l); c[1] += 1
    return task, ''.join(keep), cnt

if __name__ == '__main__':
    multiprocessing.set_start_method('fork')
    files = sorted(glob.glob(ST + '/*.txt'))
    tasks = []
    for f in files:
        n = sum(1 for _ in open(f))
        for a in range(0, max(n, 1), 200000): tasks.append((f, a, a + 200000))
    print('%s: %d moves keep the other orbits (distinct on the pair); %d files, %d tasks' % (MODE, len(MOVES), len(files), len(tasks)))
    LEN = bytearray(b'\xff') * TOTAL; dup = 0; bad = 0
    with Pool(W) as pool:
        for out, b in pool.imap_unordered(pass1, tasks):
            bad += b
            for L, ks in out.items():
                for k in ks:
                    if LEN[k] != 255: dup += 1
                    if L < LEN[k]: LEN[k] = L
    unmarked = LEN.count(255)
    print('pass 1: states unmarked %d of %d, marked twice %d, bad lines %d' % (unmarked, TOTAL, dup, bad))
    if unmarked or dup or bad: print('FAIL: not a complete stage'); sys.exit(1)
    os.makedirs(OUT, exist_ok=False)
    res = {}; tot = {}
    with Pool(W) as pool:  # forked after LEN is filled
        for task, txt, cnt in pool.imap_unordered(pass2, tasks):
            res[task] = txt
            for L, (n, k) in cnt.items():
                t = tot.setdefault(L, [0, 0]); t[0] += n; t[1] += k
    for f in files:
        with open(os.path.join(OUT, os.path.basename(f)), 'w') as fo:
            for t in tasks:
                if t[0] == f: fo.write(res[t])
    n = sum(v[0] for v in tot.values()); k = sum(v[1] for v in tot.values())
    print('length: lines kept/total')
    for L in sorted(tot): print('  %2d: %d / %d' % (L, tot[L][1], tot[L][0]))
    print('kept %d of %d lines (%.1f%%)' % (k, n, 100.0 * k / n))
