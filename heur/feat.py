# Features of a pair state (48 colours; p = orbit*24 + face*4 + k, target face (p%24)//4,
# faces ULFRBD, opposite pairs U-D, L-R, F-B).
import numpy as np
OPP = [5, 3, 4, 1, 2, 0]

def parse(s):
  return np.frombuffer(s.encode(), dtype=np.uint8) - 48

def flow(col):
  # F[o, f, c] = number of pieces of colour c on face f in orbit o
  F = np.zeros((2, 6, 6), dtype=np.int32)
  for p in range(48):
    F[p // 24, (p % 24) // 4, col[p]] += 1
  return F

def cyc_greedy(F):
  # greedy cycle extraction on one orbit's off-diagonal flow: 2-cycles, then 3-cycles, then rest
  G = F.copy()
  np.fill_diagonal(G, 0)
  c2 = 0
  for a in range(6):
    for b in range(a + 1, 6):
      k = min(G[a, b], G[b, a]); c2 += k; G[a, b] -= k; G[b, a] -= k
  c3 = 0
  changed = True
  while changed:
    changed = False
    for a in range(6):
      for b in range(6):
        for c in range(6):
          if a != b and b != c and a != c:
            k = min(G[a, b], G[b, c], G[c, a])
            if k: c3 += k; G[a, b] -= k; G[b, c] -= k; G[c, a] -= k; changed = True
  rest = G.sum()
  return c2, c3, rest

def features(col):
  F = flow(col)
  f = {}
  mis = [24 - np.trace(F[o]) for o in range(2)]
  f['m'] = mis[0] + mis[1]
  f['mmin'] = min(mis); f['mmax'] = max(mis)
  opp = sum(F[o, a, OPP[a]] for o in range(2) for a in range(6))
  f['opp'] = opp
  corr = np.array([[F[o, a, a] for a in range(6)] for o in range(2)])
  for k in range(5):
    f['facecorr%d' % k] = int((corr == k).sum())
  c2 = c3 = rest = 0
  for o in range(2):
    a, b, c = cyc_greedy(F[o]); c2 += a; c3 += b; rest += c
  f['c2'] = c2; f['c3'] = c3; f['crest'] = rest
  # number of colours present on a face besides its own (spread)
  spread = sum(int(((F[o, a] > 0).sum()) - (F[o, a, a] > 0)) for o in range(2) for a in range(6))
  f['spread'] = spread
  return f

# face class: composition of one (orbit, face) as (own, opposite, adjacent) counts
CLASSES = [(a, b, 4 - a - b) for a in range(5) for b in range(5 - a)]
CIDX = {c: i for i, c in enumerate(CLASSES)}
def face_classes(col):
  F = flow(col)
  cnt = np.zeros(len(CLASSES))
  for o in range(2):
    for f in range(6):
      own = F[o, f, f]; opp = F[o, f, OPP[f]]
      cnt[CIDX[(own, opp, 4 - own - opp)]] += 1
  return cnt
