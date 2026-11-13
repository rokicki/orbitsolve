# Fit cost-to-go models on (state, cost) data; report fit quality on path states
# (the partly-solved distribution the beam actually sees).
import sys, numpy as np
from feat import parse, features
rows = []
for line in open(sys.argv[1]):
  t = line.split()
  if t[0] == 'p': s, c = t[1], int(t[2])
  else: s, c = t[0], int(t[1])
  if c < 0: continue
  rows.append((features(parse(s)), c, t[0] == 'p'))
names = list(rows[0][0].keys())
X = np.array([[r[0][n] for n in names] for r in rows], dtype=float)
y = np.array([r[1] for r in rows], dtype=float)
isp = np.array([r[2] for r in rows])
print(len(rows), 'rows', isp.sum(), 'path states')
rng = np.random.default_rng(0)
tr = rng.random(len(y)) < 0.7
def fit(cols, label):
  A = np.column_stack([X[:, [names.index(c) for c in cols]], np.ones(len(y))])
  w, *_ = np.linalg.lstsq(A[tr], y[tr], rcond=None)
  e = A[~tr] @ w - y[~tr]
  print(f'{label:40s} rmse {np.sqrt((e**2).mean()):.2f}  mae {np.abs(e).mean():.2f}  ', ' '.join(f'{c}={v:.2f}' for c, v in zip(cols + ['1'], w)))
  return w
fit(['m'], 'm only')
fit(['m', 'opp'], 'm+opp')
fit(['m', 'c2', 'c3', 'crest'], 'm+cycles')
fit(['m', 'opp', 'c2', 'c3', 'crest', 'spread', 'mmin'], 'm+opp+cyc+spread+mmin')
fit([n for n in names if n.startswith('facecorr')] + ['m'], 'facecorr hist')
fit(names, 'all')
# per-m residual view: mean cost by m
for m in range(0, 49, 4):
  sel = X[:, names.index('m')] == m
  if sel.sum(): print(f'm={m:2d} n={sel.sum():5d} cost mean {y[sel].mean():6.2f} sd {y[sel].std():5.2f}  per-mis {y[sel].mean()/max(m,1):.2f}')
