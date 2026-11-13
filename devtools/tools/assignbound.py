#!/usr/bin/env python3
"""Assignment bound on merge savings.

    python3 assignbound.py S.bin RAW [DUMP.txt]

S.bin is the n x n uint8 matrix from mergebound (S[i][j] = moves saved when
instance j directly follows instance i).  Each instance gets at most one
successor and one predecessor; order constraints and cycles are ignored, so
the maximum total saving is an upper bound on what any ordering saves (except
where an instance vanishes completely and three instances interact).
With the dump, also a greedy-edge estimate (the best seam anywhere first, no
cycles), without and with the dump's order constraints: a quick preview of a
pairwise merge, scored by pairwise savings only.
Needs numpy and scipy.
"""
import math, sys
import numpy as np
from scipy.optimize import linear_sum_assignment

if len(sys.argv) not in (3, 4):
    sys.exit(__doc__.strip())
data = np.fromfile(sys.argv[1], dtype=np.uint8)
n = math.isqrt(data.size)
if n * n != data.size:
    sys.exit("not a square matrix")
S = data.reshape(n, n).astype(np.int64)
np.fill_diagonal(S, 0)  # "no successor" costs nothing
rows, cols = linear_sum_assignment(S, maximize=True)
bound = int(S[rows, cols].sum())
raw = int(sys.argv[2])
print(f"  assignment bound: saves at most {bound} ({100.0 * bound / raw:.1f}% of raw), merged >= {raw - bound}")

if len(sys.argv) == 4:
    deps = []
    for line in open(sys.argv[3]):
        if line.startswith("#"):
            continue
        f = line.split()
        nd = int(f[0])
        deps.append([(int(f[1 + 2 * k]), int(f[2 + 2 * k], 16)) for k in range(nd)])
    # order constraints as merge.h builds them
    pred = [[] for _ in range(n)]
    comp = {}
    for i, ds in enumerate(deps):
        for c, m in ds:
            for j, mj in comp.get(c, []):
                if m & mj:
                    pred[i].append(j)
            comp.setdefault(c, []).append((i, m))
    S0 = data.reshape(n, n)
    edges = sorted(((int(S0[i, j]), i, j) for i, j in zip(*np.nonzero(S0)) if i != j), reverse=True)

    succ_dag = [[] for _ in range(n)]
    for i in range(n):
        for j in pred[i]:
            succ_dag[j].append(i)

    def greedy(respect):
        # pieces are paths of instances; a seam i -> j joins the piece ending in
        # i to the piece starting with j.  With order constraints, the pieces,
        # contracted to single nodes, must stay acyclic under the constraints
        # (a seam means "immediately before", so a constraint path that leaves
        # a piece and comes back is a contradiction too).
        succ, prv = [-1] * n, [-1] * n
        piece = list(range(n))           # piece id per instance
        members = {k: [k] for k in range(n)}
        total = 0
        for w, i, j in edges:
            if succ[i] >= 0 or prv[j] >= 0 or piece[i] == piece[j]:
                continue
            if respect:
                # is piece(i) reachable from piece(j) along constraints?
                pi, pj = piece[i], piece[j]
                seen, stack, cyc = {pj}, [pj], False
                while stack and not cyc:
                    p = stack.pop()
                    for x in members[p]:
                        for y in succ_dag[x]:
                            q = piece[y]
                            if q == pi:
                                cyc = True
                                break
                            if q not in seen:
                                seen.add(q)
                                stack.append(q)
                        if cyc:
                            break
                if cyc:
                    continue
            succ[i], prv[j] = j, i
            total += w
            pi, pj = piece[i], piece[j]
            for x in members[pj]:
                piece[x] = pi
            members[pi] += members.pop(pj)
        return total

    for respect in (False, True):
        t = greedy(respect)
        print(f"  greedy edge ({'with' if respect else 'without'} order constraints): saves {t} ({100.0 * t / raw:.1f}%), merged {raw - t}")

