# Black-box tune of a per-orbit score table H(a, b) (a, b = misplaced in orbit A, B)
# for beam performance: fitness = average solution cost on a training set.
#   H(a, b) = q(a) + q(b) + r(min(a, b)) + s * (a + b)
# q, r piecewise linear over knots; q(0) = r(0) = 0.
import subprocess, sys, numpy as np, os, json, time
W = int(sys.argv[1]) if len(sys.argv) > 1 else 16
train = sys.argv[2] if len(sys.argv) > 2 else 'train400.txt'
tag = f'w{W}'
KQ = [0, 2, 4, 6, 8, 12, 16, 20, 24]
KR = [0, 2, 4, 8, 12, 24]
NF = 4  # face extra for k = 0..3 correct (k = 4 is 0)
def table(p):
  s = p[0]; q = np.concatenate([[0], p[1:1 + len(KQ) - 1]]); r = np.concatenate([[0], p[len(KQ):len(KQ) + len(KR) - 1]])
  qa = np.interp(np.arange(25), KQ, q); ra = np.interp(np.arange(25), KR, r)
  H = np.zeros((25, 25))
  for a in range(25):
    for b in range(25):
      H[a, b] = s * (a + b) + qa[a] + qa[b] + ra[min(a, b)]
  return H - H[0, 0]
def fitness(p, states=train, seed=1):
  fn = f'/tmp/claude-501/tune_{tag}_{os.getpid()}.txt'
  np.savetxt(fn, table(p), fmt='%.3f')
  face = [f'{x:.3f}' for x in list(p[-NF:]) + [0]]
  r = subprocess.run(['../full/pairdata', 'solve', str(W), '-q', '-t', '4', '--seed', str(seed), '--tab2', fn, '--face'] + face,
                     stdin=open(states), capture_output=True, text=True)
  import re
  mm = re.search(r'(\d+)/(\d+) solved, avg ([\d.]+)', r.stderr)
  solved = [mm.group(1), mm.group(2)]; avg = float(mm.group(3))
  return avg + 100 * (int(solved[1]) - int(solved[0]))
n = 1 + len(KQ) - 1 + len(KR) - 1 + NF
p = np.zeros(n); p[0] = 3.0  # baseline: 3 per misplaced piece
best = fitness(p)
print(f'start {best:.3f}', flush=True)
sigma = 1.0
rng = np.random.default_rng(int(time.time()))
for it in range(100000):
  cand = [p + sigma * rng.standard_normal(n) * np.r_[0.3, np.ones(n - 1)] for _ in range(6)]
  fs = [fitness(c) for c in cand]
  k = int(np.argmin(fs))
  if fs[k] < best:
    best, p = fs[k], cand[k]; sigma *= 1.3
    print(f'it {it} best {best:.3f} sigma {sigma:.2f} p {np.round(p, 2).tolist()}', flush=True)
    json.dump({'p': p.tolist(), 'best': best, 'W': W}, open(f'tune_{tag}.json', 'w'))
    np.savetxt(f'tuned_{tag}.txt', table(p), fmt='%.3f')
  else:
    sigma = max(0.05, sigma * 0.9)
