"""Combine pair solutions from one row of a big cube and measure cancellation.

Input: fast -o output lines  "c0..c47 | alg | alg | ..."  (moves on the (2,3) pair).
For a row, each pair k is (2, j_k): face turns and 2-prefixed slices are the
same physical layers for every pair, 3-prefixed slices are pair-specific
layers.  Algorithms of different pairs act on disjoint orbits (diagonals are
ignored), so any interleaving is valid as long as each pair's own order is
respected -- or, more loosely, its commutation lattice.

    python3 combine.py SOLFILE [PAIRS] [TRIALS]
"""
import itertools, random, sys
from functools import lru_cache

N = 8
FACES = "ULFRBD"
NORMAL = {'U': (0, 1, 0), 'L': (-1, 0, 0), 'F': (0, 0, 1), 'R': (1, 0, 0), 'B': (0, 0, -1), 'D': (0, -1, 0)}
AXIS = {'U': 1, 'D': 1, 'L': 0, 'R': 0, 'F': 2, 'B': 2}

# pair positions (geom.txt: index face row col x y z)
pos = {}
coord = {}
for l in open('geom.txt'):
    t = l.split()
    i = int(t[0]); c = tuple(map(int, t[4:7]))
    pos[c] = i; coord[i] = c


def parse(m):
    d = 1
    if m[0].isdigit():
        d = int(m[0]); m = m[1:]
    f = m[0]
    a = 1 if len(m) == 1 else (2 if m[1] == '2' else 3)
    return f, d, a


@lru_cache(None)
def quarter(f, d):
    """dst[p] after one clockwise quarter turn of layer d from face f."""
    n = NORMAL[f]; ax = [k for k in range(3) if n[k]][0]; sgn = n[ax]
    layer = sgn * (N - 1 - 2 * (d - 1))
    o1, o2 = (ax + 1) % 3, (ax + 2) % 3
    dst = list(range(48))
    for i, p in coord.items():
        cub = p[ax] - (1 if p[ax] == N else -1 if p[ax] == -N else 0)
        if cub != layer:
            continue
        q = list(p)
        if sgn > 0:
            q[o1], q[o2] = p[o2], -p[o1]
        else:
            q[o1], q[o2] = -p[o2], p[o1]
        dst[i] = pos[tuple(q)]
    return tuple(dst)


def alg_dst(moves):
    """where each pair position's piece ends up after the algorithm"""
    loc = list(range(48))
    for m in moves:
        f, d, a = parse(m)
        q = quarter(f, d)
        for _ in range(a):
            loc = [q[x] for x in loc]
    return tuple(loc)


def apply(col, dst):
    out = [0] * 48
    for s in range(48):
        out[dst[s]] = col[s]
    return out


def solved(col):
    return all(col[p] == (p % 24) // 4 for p in range(48))


def compose(d1, d2):  # d1 then d2
    return tuple(d2[x] for x in d1)


# ---------------------------------------------------------------- lattice
BITMAP = '--bitmap' in sys.argv


def lattice(algs):
    """pred[j] = set of i<j that must precede j: non-commuting (exact perms),
    or with --bitmap simply overlapping supports (moved-piece bitmaps)"""
    ds = [alg_dst(a) for a in algs]
    sup = [sum(1 << p for p in range(48) if d[p] != p) for d in ds]
    n = len(algs)
    pred = [set() for _ in range(n)]
    for i in range(n):
        for j in range(i + 1, n):
            if BITMAP:
                dep = sup[i] & sup[j] != 0
            else:
                dep = compose(ds[i], ds[j]) != compose(ds[j], ds[i])
            if dep:
                pred[j].add(i)
    # transitive closure not needed for "available" tests
    return pred, ds


def linear_extensions(n, pred):
    full = (1 << n) - 1
    cnt = {0: 1}
    for mask in range(full + 1):
        if mask not in cnt:
            continue
        for j in range(n):
            if not mask >> j & 1 and all(mask >> i & 1 for i in pred[j]):
                cnt[mask | 1 << j] = cnt.get(mask | 1 << j, 0) + cnt[mask]
    return cnt[full]


# ---------------------------------------------------------------- merging
def setup_depth(alg):
    k = 0
    while 2 * (k + 1) <= len(alg):
        f, d, a = parse(alg[k])
        g, e, b = parse(alg[len(alg) - 1 - k])
        if d != 1 or e != 1 or f != g or (a + b) % 4:
            break
        k += 1
    return k


LAYERS = None  # --cube N: per pair k, physical depths (a, b) for its "2" and "3" slices


def layer_moves(alg, k):
    """alg text -> list of (axis, layer id, amount, category).  Row model: 3-slices
    are pair-specific; cube model: slices map to physical depths LAYERS[k]."""
    out = []
    sd = setup_depth(alg)
    for i, m in enumerate(alg):
        f, d, a = parse(m)
        if LAYERS is not None:
            lid = (f, 1 if d == 1 else LAYERS[k][d - 2])
        else:
            lid = (f, d) if d < 3 else (f, d, k)
        cat = 'setup-face' if (i < sd or i >= len(alg) - sd) else ('face' if d == 1 else f'{d}-slice')
        out.append((AXIS[f], lid, a, cat))
    return out


TAGS = {}
TIMES = {}  # category -> [total, removed]


class Seq:
    """stack of same-axis groups: [axis, {layer: amount}]"""

    def __init__(self):
        self.g = []
        self.cost = 0

    def copy(self):
        s = Seq()
        s.g = [[a, dict(d), dict(c)] for a, d, c in self.g]
        s.cost = self.cost
        return s

    def push(self, mv, stats=None):
        ax, lid, a = mv[:3]
        cat = mv[3] if len(mv) > 3 else None
        if stats is not None and cat:
            TAGS.setdefault(cat, [0, 0])[0] += 1
        if self.g and self.g[-1][0] == ax:
            d = self.g[-1][1]
            old = d.get(lid, 0)
            new = (old + a) % 4
            self.cost += (new != 0) - (old != 0)
            if stats is not None and old:
                kind = 1 if lid[1] == 1 else (2 if len(lid) == 2 and LAYERS is None else 3)
                stats[kind] += 2 if new == 0 else 1
                # incoming move is absorbed; on a full cancel the existing one goes too
                TAGS[cat][1] += 1
                if new == 0 and self.g[-1][2].get(lid):
                    TAGS[self.g[-1][2][lid]][1] += 1
            if new:
                d[lid] = new
                if not old:
                    self.g[-1][2][lid] = cat
            else:
                d.pop(lid, None)
                self.g[-1][2].pop(lid, None)
            if not d:
                self.g.pop()
        else:
            self.g.append([ax, {lid: a}, {lid: cat}])
            self.cost += 1

    def add(self, mvs, stats=None):
        for mv in mvs:
            self.push(mv, stats)


def delta(seq, mvs):
    """cost increase from appending mvs; only the tail can interact, so copy
    just enough groups (an algorithm can unwind at most len(mvs) groups)"""
    s = Seq()
    s.g = [[a, dict(d), dict(c)] for a, d, c in seq.g[-(len(mvs) + 1):]]
    s.add(mvs)
    before = sum(len(d) for _, d, _ in seq.g[-(len(mvs) + 1):])
    after = sum(len(d) for _, d, _ in s.g)
    return after - before


REORDER = '--reorder' in sys.argv


def variants(alg):
    """Equivalent texts of alg: if alg = X c X' with X face turns only, any
    order of X (with X' its exact reverse inverse) acts identically."""
    k = 0
    while 2 * (k + 1) <= len(alg):
        f, d, a = parse(alg[k])
        g, e, b = parse(alg[len(alg) - 1 - k])
        if d != 1 or e != 1 or f != g or (a + b) % 4:
            break
        k += 1
    if k < 2 or not REORDER:
        return [alg]
    X = alg[:k]
    core = alg[k:len(alg) - k]
    out = []
    for perm in itertools.permutations(X):
        inv = []
        for m in reversed(perm):
            f, d, a = parse(m)
            inv.append(f + ('' if a == 3 else '2' if a == 2 else "'"))
        out.append(list(perm) + core + inv)
    return out


def merge(pairs, use_lattice, look=1, stats=None):
    """pairs: list of (algs, pred).  Greedy: pick the available algorithm
    with the smallest cost increase (ties: longest alg first)."""
    seq = Seq()
    done = [set() for _ in pairs]
    lm = [[[layer_moves(v, k) for v in variants(a)] for a in algs] for k, (algs, pred) in enumerate(pairs)]
    total = sum(len(a) for algs, _ in pairs for a in algs)
    placed = 0
    while placed < total:
        best = None
        for k, (algs, pred) in enumerate(pairs):
            for j in range(len(algs)):
                if j in done[k]:
                    continue
                if use_lattice:
                    if not all(i in done[k] for i in pred[j]):
                        continue
                else:
                    if j != len(done[k]):
                        continue
                for vi, v in enumerate(lm[k][j]):
                    d = delta(seq, v)
                    key = (d - len(algs[j]), -len(algs[j]))  # saving relative to raw length
                    if best is None or key < best[0]:
                        best = (key, k, j, vi)
        _, k, j, vi = best
        seq.add(lm[k][j][vi], stats)
        done[k].add(j)
        placed += len(pairs[k][0][j])
    return seq.cost


def merge_beam(pairs, B):
    """Beam over merge orders (lattice-respecting): states ranked by moves
    saved so far (placed raw length - merged cost)."""
    lm = [[[layer_moves(v, k) for v in variants(a)] for a in algs] for k, (algs, pred) in enumerate(pairs)]
    total = sum(len(algs) for algs, _ in pairs)
    beam = [(0, Seq(), tuple(0 for _ in pairs), 0)]  # (saved, seq, done bitmasks, raw placed)
    for step in range(total):
        cand = []
        for saved, seq, done, raw in beam:
            for k, (algs, pred) in enumerate(pairs):
                for j in range(len(algs)):
                    if done[k] >> j & 1 or not all(done[k] >> i & 1 for i in pred[j]):
                        continue
                    for v in lm[k][j]:
                        d = delta(seq, v)
                        cand.append((saved + len(algs[j]) - d, k, j, v, seq, done, raw + len(algs[j])))
        cand.sort(key=lambda c: -c[0])
        nb, seen = [], set()
        for sv, k, j, v, seq, done, raw in cand:
            nd = tuple(x | (1 << j) if i == k else x for i, x in enumerate(done))
            key = (nd, tuple((a, tuple(sorted(d.items()))) for a, d, _ in seq.g[-2:]))
            if key in seen:
                continue
            seen.add(key)
            s2 = seq.copy()
            s2.add(v)
            nb.append((sv, s2, nd, raw))
            if len(nb) >= B:
                break
        beam = nb
    return min(b[1].cost for b in beam)


def main():
    global LAYERS
    argv = sys.argv[1:]
    cube = None
    global MB
    MB = 0
    if '--mbeam' in argv:
        i = argv.index('--mbeam')
        MB = int(argv[i + 1])
        del argv[i:i + 2]
    if '--cube' in argv:
        i = argv.index('--cube')
        cube = int(argv[i + 1])
        del argv[i:i + 2]
    args = [a for a in argv if not a.startswith('--')]
    fn = args[0]
    P = int(args[1]) if len(args) > 1 else 8
    T = int(args[2]) if len(args) > 2 else 16
    if cube:
        half = cube // 2  # oblique pairs (i,j), 2 <= i < j <= half (odd n: middle slice excluded)
        pairs_ij = [(i, j) for i in range(2, half + 1) for j in range(i + 1, half + 1)]
        P = len(pairs_ij)
        print(f"cube {cube}: {P} oblique pairs")
    sols = []
    for l in open(fn):
        parts = l.strip().split(' | ')
        col = list(map(int, parts[0].split()))
        algs = [[m for m in p.split() if m != '*'] for p in parts[1:]]  # '*' = moves diagonal pieces
        sols.append((col, algs))
    # --- lattice statistics
    ncomm = npairs = 0
    ext = []
    adj = adjn = 0
    for col, algs in sols[:128]:
        c = col
        pred, ds = lattice(algs)
        for d in ds:
            c = apply(c, d)
        assert solved(c), "solution does not solve"
        n = len(algs)
        npairs += n * (n - 1) // 2
        ncomm += sum(1 for j in range(n) for i in range(j) if i not in pred[j])
        adj += sum(1 for j in range(1, n) if (j - 1) not in pred[j])
        adjn += n - 1
        ext.append(linear_extensions(n, pred))
    ext.sort()
    print(f"lattice over {len(ext)} solutions: {ncomm}/{npairs} alg pairs commute ({100*ncomm/npairs:.1f}%), "
          f"adjacent swaps allowed {adj}/{adjn} ({100*adj/adjn:.1f}%)")
    print(f"  linear extensions per solution: median {ext[len(ext)//2]}, max {ext[-1]}, "
          f"fraction with >1: {sum(e > 1 for e in ext)/len(ext):.2f}")
    # --- merging
    rng = random.Random(1)
    stats = {1: 0, 2: 0, 3: 0}
    res = {k: 0 for k in ('raw', 'perpair', 'seq', 'greedy', 'greedy+lat')}
    for t in range(T):
        pick = [rng.choice(sols) for _ in range(P)] if P > len(sols) else rng.sample(sols, P)
        if cube:
            LAYERS = [(i, j) if rng.random() < 0.5 else (j, i) for i, j in pairs_ij]
        pairs = []
        for k, (col, algs) in enumerate(pick):
            pred, _ = lattice(algs)
            pairs.append((algs, pred))
        raw = sum(len(a) for algs, _ in pairs for a in algs)
        per = 0
        s_all = Seq()
        for k, (algs, _) in enumerate(pairs):
            s = Seq()
            for a in algs:
                s.add(layer_moves(a, k))
                s_all.add(layer_moves(a, k))
            per += s.cost
        res['raw'] += raw
        res['perpair'] += per
        res['seq'] += s_all.cost
        res['greedy'] += merge(pairs, False)
        import time
        t0 = time.perf_counter()
        res['greedy+lat'] += merge(pairs, True, stats=stats)
        TIMES['greedy+lat'] = TIMES.get('greedy+lat', 0) + time.perf_counter() - t0
        if MB:
            res.setdefault('beam-merge', 0)
            t0 = time.perf_counter()
            res['beam-merge'] += merge_beam(pairs, MB)
            TIMES['beam-merge'] = TIMES.get('beam-merge', 0) + time.perf_counter() - t0
    print("fraction of each move kind removed in the greedy+lattice merge:")
    for cat, (n, r) in sorted(TAGS.items()):
        print(f"  {cat:11s} {r:7d}/{n:7d} = {100*r/n:5.1f}%")
    tot = sum(stats.values())
    print("greedy+lat moves saved by layer: " + ", ".join(
        f"{n} {100*stats[k]/tot:.0f}%" for k, n in ((1, 'face'), (2, '2-slice'), (3, 'slice (pair-specific / physical)'))))
    print(f"{P} pairs x {T} trials, moves per pair (raw = no cancellation):")
    for k, v in TIMES.items():
        print(f"  merge time {k}: {v / T:.2f} s per cube")
    for k, v in res.items():
        print(f"  {k:11s} {v / T / P:6.2f}   ({100 * (1 - v / res['raw']):.1f}% saved)")


main()
