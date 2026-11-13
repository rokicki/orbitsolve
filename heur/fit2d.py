# Cost-to-go by (mA, mB) misplaced per orbit, from independently re-solved states.
import sys, numpy as np
from feat import parse
S = [l.split() for l in open('resolve256.txt')]
tgt = (np.arange(48) % 24) // 4
mA, mB, y = [], [], []
for s, c in S:
  col = parse(s); w = col != tgt
  mA.append(w[:24].sum()); mB.append(w[24:].sum()); y.append(int(c))
mA, mB, y = map(np.array, (mA, mB, y))
lo, hi, m = np.minimum(mA, mB), np.maximum(mA, mB), mA + mB
rng = np.random.default_rng(1); tr = rng.random(len(y)) < 0.7
def ev(pred, label):
  e = pred[~tr] - y[~tr]; print(f'{label:34s} rmse {np.sqrt((e**2).mean()):.3f}')
# 1: mean by m
mm = np.array([y[tr & (m == k)].mean() if (tr & (m == k)).any() else np.nan for k in range(49)])
mm = np.where(np.isnan(mm), np.interp(np.arange(49), np.where(~np.isnan(mm))[0], mm[~np.isnan(mm)]), mm)
ev(mm[m], 'mean by m')
ev(3.0 * m * (y[tr] @ m[tr]) / (3.0 * m[tr] @ m[tr]), 'scaled linear m')
# 2: parametric: one-hot m + one-hot lo (0..6) + lo>6 linear
def P(lo, m):
  return np.column_stack([np.eye(49)[m]] + [(lo == k) for k in range(7)] + [np.maximum(lo - 6, 0)])
w, *_ = np.linalg.lstsq(P(lo[tr], m[tr]).astype(float), y[tr], rcond=None)
ev(P(lo, m) @ w, 'one-hot m + lo terms')
print('lo effect (moves):', ' '.join(f'{w[49 + k]:+.2f}' for k in range(7)), f'slope>6 {w[56]:+.2f}')
# 3: cell means with shrink to parametric
par = P(lo, m) @ w
cell = {}
for a, b, v, t in zip(lo, hi, y - par, tr):
  if t: cell.setdefault((a, b), []).append(v)
adj = np.array([np.sum(cell.get((a, b), [0])) / (len(cell.get((a, b), [])) + 5) for a, b in zip(lo, hi)])
ev(par + adj, 'parametric + shrunk cell means')
# write full-data table H[a][b] (moves), a,b = misplaced in orbit A,B
w, *_ = np.linalg.lstsq(P(lo, m).astype(float), y, rcond=None)
cell = {}
for a, b, v in zip(lo, hi, y - P(lo, m) @ w): cell.setdefault((a, b), []).append(v)
H = np.zeros((25, 25))
for a in range(25):
  for b in range(25):
    l, h = min(a, b), max(a, b); mm_ = a + b
    base = P(np.array([l]), np.array([mm_])) @ w
    c = cell.get((l, h), [])
    H[a, b] = base[0] + (np.sum(c) / (len(c) + 5) if c else 0)
H[0, 0] = 0
np.savetxt('tab2d.txt', H, fmt='%.3f')
print('H[a][b] for a,b in 0..8:'); print(np.round(H[:9, :9], 1))
