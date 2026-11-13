# m one-hot + face-class counts (own/opp/adj composition per orbit-face)
import sys, numpy as np
from feat import parse, flow, face_classes, CLASSES
S, y, isp = [], [], []
for line in open(sys.argv[1]):
  t = line.split()
  s, c = (t[1], int(t[2])) if t[0] == 'p' else (t[0], int(t[1]))
  if c < 0: continue
  S.append(parse(s)); y.append(c)
y = np.array(y, float)
m = np.array([sum(int(col[p] != (p % 24) // 4) for p in range(48)) for col in S])
FC = np.array([face_classes(col) for col in S])
M1 = np.eye(49)[m]
rng = np.random.default_rng(0); tr = rng.random(len(y)) < 0.7
def fit(A, label):
  w, *_ = np.linalg.lstsq(A[tr], y[tr], rcond=None)
  e = A[~tr] @ w - y[~tr]
  print(f'{label:30s} rmse {np.sqrt((e**2).mean()):.2f} mae {np.abs(e).mean():.2f}')
  return w
fit(np.column_stack([m, np.ones(len(y))]), 'linear m')
fit(M1, 'm one-hot')
w = fit(np.column_stack([M1, FC]), 'm one-hot + face classes')
for c, v in zip(CLASSES, w[49:]): print('  own/opp/adj', c, f'{v - w[49 + CLASSES.index((4,0,0))]:+.2f}')
