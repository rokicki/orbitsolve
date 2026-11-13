# MLP cost-to-go on the raw pair state (48 positions x 6 colours one-hot).
# usage: nn.py data.txt [data2.txt ...]   (lines "state cost" or "p state cost")
import sys, numpy as np, torch, torch.nn as nn
torch.set_num_threads(4)
S, y, g = [], [], []
gid = 0
for fn in sys.argv[1:]:
  for line in open(fn):
    t = line.split()
    if t[0] != 'p': gid += 1  # a new solution path (or an independent state)
    s, c = (t[1], int(t[2])) if t[0] == 'p' else (t[0], int(t[1]))
    if c < 0: continue
    S.append(np.frombuffer(s.encode(), np.uint8) - 48); y.append(c); g.append(gid)
S = np.array(S); y = np.array(y, np.float32); g = np.array(g)
tgt = (np.arange(48) % 24) // 4
m = (S != tgt).sum(1)
X = np.eye(6, dtype=np.float32)[S].reshape(len(S), -1)
rng = np.random.default_rng(0); tr = (rng.random(gid + 1) < 0.8)[g]  # split by path
# baseline: mean by m
mm = np.array([y[tr & (m == k)].mean() if (tr & (m == k)).any() else 0 for k in range(49)])
e = mm[m[~tr]] - y[~tr]
print(f'{len(y)} rows; baseline mean-by-m rmse {np.sqrt((e**2).mean()):.3f}')
Xt, yt = torch.tensor(X), torch.tensor(y - mm[m])  # learn the residual given m
net = nn.Sequential(nn.Linear(288, 256), nn.ReLU(), nn.Linear(256, 256), nn.ReLU(), nn.Linear(256, 1))
opt = torch.optim.Adam(net.parameters(), lr=1e-3, weight_decay=1e-5)
itr = np.where(tr)[0]; ite = np.where(~tr)[0]
for ep in range(60):
  rng.shuffle(itr)
  net.train()
  for i in range(0, len(itr), 256):
    b = itr[i:i + 256]
    loss = ((net(Xt[b]).squeeze(1) - yt[b]) ** 2).mean()
    opt.zero_grad(); loss.backward(); opt.step()
  if ep % 10 == 9:
    net.eval()
    with torch.no_grad():
      pe = net(Xt[ite]).squeeze(1) - yt[ite]
      pt = net(Xt[itr]).squeeze(1) - yt[itr]
    print(f'epoch {ep+1}: train rmse {pt.pow(2).mean().sqrt():.3f}  test rmse {pe.pow(2).mean().sqrt():.3f}')
torch.save(net.state_dict(), 'nn.pt')
