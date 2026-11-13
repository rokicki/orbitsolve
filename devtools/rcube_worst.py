#!/usr/bin/env python3
"""rcube_worst.py N [N ...]: worst-case number of moves RCube (row-by-row, "1D") spends on the centres of an
N x N x N cube, in the model of our nxnxn paper (Section on RCube):

  * 15 two-face swaps, in RCube's order; swap (B -> A) moves the pieces of colour A from face B to face A;
  * each swap is 16 substages, the 3-cycles (i, 4+k, 4+(k+1)%4) for i, k = 0..3 (wires 0..3: the orbit's
    four positions on A, wires 4..7: its four positions on B); a 3-cycle (i, a, b) takes the piece at b to i,
    i to a and a to b, and an orbit fires it when the piece at b has colour A and the piece at i does not;
  * the orbits form a grid of rows; in each row, for each of the 240 substages that some orbit of the row
    fires, RCube plays one commutator that serves all those orbits at once: 5 moves plus 2 per orbit served.

Every fired 3-cycle brings one piece home for good, so an orbit fires at most 20 (the last colour comes home
with the others): asymptotically 2 moves per fired 3-cycle, 40 per orbit, (5/6) * 2 per centre cubie.

Exactness.  A swap's firings depend only on where its colour A is, and every swap moves the other colours by
a permutation that depends only on A's places, so the five phases (into R, O, G, Y, W) are independent: any
choice of a colour pattern per phase comes from some scramble.  Per phase the program enumerates every
pattern of the phase's colour on the faces not yet finished and records which substages fire.  A row's worst
case is then, per phase, the largest  5 * |union of the firing sets| + 2 * (sum of their sizes)  over a choice
of one firing set per orbit of the row; orbits are independent.  This is computed exactly for rows of up to
EXACT orbits (search over the distinct firing sets) and bounded above for longer rows by
5 * min(|U|, best union) + 2 * (orbits) * (largest set), which is exact once the largest sets can cover U.

Grid (as in RCube): even N has Y = N/2 - 1 rows of Y orbits; odd N has Y = (N-3)/2 rows of Y + 1 orbits (the
mid / T-centre orbit of each depth sits in its row), plus the odd cube's first step that turns the middle
slices to put the face centres home (RCube's AlignTrueCenters: at most 2 moves).

The answer as inequalities.  Per phase p with U_p substages that ever fire (80, 64, 48, 32, 16), U4_p of them
in some orbit's 4-firing set (70, 54, 38, 22, 4) and G_p (31, 31, 31, 31, 41; see Phase), a row of m orbits costs
at most  8m + 5 min(4m, U_p)  and at most  8m + 5 min(4m, U4_p) + G_p,  and the second is attained for
m >= 28, 21, 16, 12, 6.  So a row costs at most 40m + 1105, with equality for m >= 28; the whole centre solve
is at most Y (40 Y + 1105) for even N (Y = N/2 - 1; exact for N >= 58) and Y (40 (Y + 1) + 1105) + 2 for odd N
(Y = (N - 3)/2): asymptotically 10 N^2 = (5/6) * 2 moves per centre cubie.  Rows of at most 4 orbits are exact
by search (N <= 10).  Between, the program prints the smaller of the two bounds, marked as an upper bound.
"""
import sys
from itertools import combinations
from functools import lru_cache

# RCube's swaps: (source face, target colour); colours named by their faces
SWAPS = [('G', 'R'), ('W', 'R'), ('B', 'R'), ('O', 'R'), ('Y', 'R'),
         ('W', 'O'), ('Y', 'O'), ('B', 'O'), ('G', 'O'),
         ('B', 'G'), ('W', 'G'), ('Y', 'G'),
         ('W', 'Y'), ('B', 'Y'),
         ('B', 'W')]
PHASES = ['R', 'O', 'G', 'Y', 'W']
CYCLES = [(i, 4 + k, 4 + (k + 1) % 4) for i in range(4) for k in range(4)]  # the 16 substages of a swap


def phase_family(target):
    """All firing sets (frozensets of global substage numbers) of the phase that fills `target`, over every
    pattern of the target colour (4 pieces) on the faces not finished before this phase."""
    done = PHASES[:PHASES.index(target)]
    faces = [f for f in 'ROGYWB' if f not in done]
    pos = [(f, q) for f in faces for q in range(4)]
    swaps = [(s, src) for s, (src, tgt) in enumerate(SWAPS) if tgt == target]
    fam = {}
    for places in combinations(range(len(pos)), 4):
        has = {p: False for p in pos}
        for j in places:
            has[pos[j]] = True
        fired = []
        for s, src in swaps:
            wire = [(target, q) for q in range(4)] + [(src, q) for q in range(4)]
            for j, (i, a, b) in enumerate(CYCLES):
                if has[wire[b]] and not has[wire[i]]:
                    hb, hi, ha = has[wire[b]], has[wire[i]], has[wire[a]]
                    has[wire[i]], has[wire[a]], has[wire[b]] = hb, hi, ha
                    fired.append(s * 16 + j)
        if not all(has[(target, q)] for q in range(4)):
            raise SystemExit('model error: phase %s leaves a piece off its face' % target)
        fs = frozenset(fired)
        fam[fs] = fam.get(fs, 0) + 1
    return fam


FAM = {p: phase_family(p) for p in PHASES}
EXACT = 4


class Phase:
    """Per phase: U (substages that ever fire), U4 (those in some 4-firing set), D = U - U4, and
    G = max over sets T meeting D of  5 * |D covered by T| - 2 * sum over T of (4 - |S|),
    the most the substages outside U4 can add (each orbit firing fewer than 4 costs 2 per missing firing)."""
    def __init__(self, p):
        self.p = p
        self.sets = list(FAM[p])
        self.U = frozenset().union(*self.sets)
        self.s4 = [s for s in self.sets if len(s) == 4]
        self.U4 = frozenset().union(*self.s4)
        self.D = self.U - self.U4
        # dynamic program over subsets of D: the least total penalty to cover each subset
        Dl = sorted(self.D)
        bit = {x: 1 << i for i, x in enumerate(Dl)}
        bestPen = {}
        for s in self.sets:
            mk = sum(bit[x] for x in s & self.D)
            if mk:
                pen = 2 * (4 - len(s))
                if mk not in bestPen or pen < bestPen[mk][0]:
                    bestPen[mk] = (pen, s)
        INF = 10 ** 9
        full = 1 << len(Dl)
        cov = [INF] * full
        how = [None] * full
        cov[0] = 0
        for mk in range(full):
            if cov[mk] == INF:
                continue
            for sm, (pen, s) in bestPen.items():
                nm = mk | sm
                if cov[mk] + pen < cov[nm]:
                    cov[nm], how[nm] = cov[mk] + pen, (mk, s)
        bm = max(range(full), key=lambda mk: 5 * bin(mk).count('1') - cov[mk] if cov[mk] < INF else -INF)
        self.G = 5 * bin(bm).count('1') - cov[bm]
        self.T = []
        mk = bm
        while mk:
            pm, s = how[mk]
            self.T.append(s)
            mk = pm
        # size-4 sets covering the rest of U4, greedily (a construction: an upper bound on how many are needed)
        left, n4 = set(self.U4) - set().union(*self.T) if self.T else set(self.U4), 0
        while left:
            s = max(self.s4, key=lambda s: len(s & left))
            left -= s
            n4 += 1
        self.mStar = len(self.T) + n4  # rows of at least this many orbits reach the bound below

    def row(self, m):
        """(value, exact?) for a row of m orbits"""
        if m == 0:
            return 0, True
        if m <= EXACT:
            return exact(self.p, m), True
        ub = min(8 * m + 5 * min(4 * m, len(self.U)), 8 * m + 5 * min(4 * m, len(self.U4)) + self.G)
        return ub, m >= self.mStar


PH = {p: Phase(p) for p in PHASES}


@lru_cache(None)
def exact(p, m):
    """Branch and bound over multisets of m firing sets: the largest 5 * |union| + 2 * (sum of sizes)."""
    sets = sorted(FAM[p], key=len, reverse=True)
    sizes = [len(s) for s in sets]
    U = len(frozenset().union(*sets))
    best = 0

    def rec(start, left, union, total):
        nonlocal best
        if left == 0:
            best = max(best, 5 * len(union) + 2 * total)
            return
        for i in range(start, len(sets)):
            if 5 * min(U, len(union) + left * sizes[i]) + 2 * (total + left * sizes[i]) <= best:
                break
            if i > start and not (sets[i] - union):
                continue
            rec(i, left - 1, union | sets[i], total + sizes[i])

    rec(0, m, frozenset(), 0)
    return best


def worst(n):
    if n < 4:
        return 0, 'no centre orbits'
    if n % 2 == 0:
        Y = n // 2 - 1
        rows, per, extra = Y, Y, 0
    else:
        Y = (n - 3) // 2
        rows, per, extra = Y, Y + 1, 2
    vals = [PH[p].row(per) for p in PHASES]
    row = sum(v for v, _ in vals)
    total = rows * row + extra
    tag = 'exact' if all(e for _, e in vals) else 'upper bound'
    return total, '%s; %d rows of %d orbits, %d moves per row%s' % (tag, rows, per, row, ', + %d to put the odd centres home' % extra if extra else '')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('usage: rcube_worst.py N [N ...]   (--families: show the per-phase firing-set statistics)')
        sys.exit(1)
    if sys.argv[1] == '--families':
        for p in PHASES:
            sets = FAM[p]
            U = frozenset().union(*sets)
            sizes = {}
            for s, c in sets.items():
                sizes[len(s)] = sizes.get(len(s), 0) + c
            P = PH[p]
            print('phase %s: %d patterns, %d substages ever fire (%d in a 4-firing set); patterns by firings %s; G = %d (%d sets), rows of >= %d orbits exact'
                  % (p, sum(sets.values()), len(U), len(P.U4), dict(sorted(sizes.items())), P.G, len(P.T), P.mStar))
        sys.exit(0)
    for a in sys.argv[1:]:
        n = int(a)
        t, how = worst(n)
        cub = 6 * (n - 2) ** 2
        print('n=%d: rcube worst case for the centres is %d moves (%s; %d centre cubies, %.2f moves each)'
              % (n, t, how, cub, t / cub if cub else 0))
