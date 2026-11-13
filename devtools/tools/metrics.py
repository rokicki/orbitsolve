#!/usr/bin/env python3
"""Recount a gullicube -o solution in other metrics.

Reads gullicube output (solution lines start with a space) on stdin, takes N
from the first line.  Consecutive same-axis moves commute, so each maximal
same-axis run is an axis group with a net quarter-turn amount a[1..N] per
layer (measured from the U/L/F side).
  layer : nonzero layers (the metric gullicube optimises)
  obtm  : outer block turns (kU = top k layers, any k), whole-cube rotations
          free (dwalton76's count): number of cuts 1..N-1 with a[i] != a[i+1]
  btm   : block turns (any contiguous block counts 1), rotations free
"""
import sys, re
from functools import lru_cache

AX = {'U': (0, 1), 'D': (0, 0), 'L': (1, 1), 'R': (1, 0), 'F': (2, 1), 'B': (2, 0)}
TW = {'': 1, '2': 2, "'": 3}

@lru_cache(None)
def maxzero(c1, c2, c3):
  # max number of disjoint zero-sum (mod 4) groups covering all nonzero diffs
  if c1 + c2 + c3 == 0: return 0
  best = 0
  for g in ((1,0,1),(0,2,0),(2,1,0),(0,1,2),(4,0,0),(0,0,4),(1,0,1)):
    if c1 >= g[0] and c2 >= g[1] and c3 >= g[2]:
      best = max(best, 1 + maxzero(c1-g[0], c2-g[1], c3-g[2]))
  return best

def group_cost(a, N):
  layer = sum(1 for v in a[1:N+1] if v)
  obtm = sum(1 for i in range(1, N) if a[i] != a[i+1])
  btm = None
  for c in range(4):  # free rotation shifts every layer by c
    b = [0] + [(v - c) % 4 for v in a[1:N+1]] + [0]
    d = [(b[i+1] - b[i]) % 4 for i in range(N+1)]
    cnt = [d.count(1), d.count(2), d.count(3)]
    k = sum(cnt)
    cost = k - maxzero(*cnt)
    btm = cost if btm is None else min(btm, cost)
  return layer, obtm, btm

def main():
  text = sys.stdin.read().split('\n')
  N = int(next(re.match(r'N=(\d+)', l).group(1) for l in text if l.startswith('N=')))
  toks = ' '.join(l for l in text if l.startswith(' ') and not l.startswith('  ')).split()
  tot = [0, 0, 0]
  cur, a = None, None
  def flush():
    if cur is not None:
      for i, v in enumerate(group_cost(a, N)): tot[i] += v
  for t in toks:
    m = re.fullmatch(r"(\d*)([ULFRBD])(2|'|)", t)
    dep = int(m.group(1) or 1)
    ax, pos = AX[m.group(2)]
    p = dep if pos else N + 1 - dep
    amt = TW[m.group(3)] if pos else (4 - TW[m.group(3)]) % 4
    if ax != cur:
      flush(); cur, a = ax, [0] * (N + 2)
    a[p] = (a[p] + amt) % 4
  flush()
  print(f"N={N} layer {tot[0]} obtm {tot[1]} btm {tot[2]}")

main()
