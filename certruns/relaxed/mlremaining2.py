# 2026-10-25: rewrite devtools/tws/NAME-remaining.scr: the classes of devtools/tws/NAME.scr whose
# certificate line in STAGEDIR is still LEN moves (same names and order as NAME.scr).
#   python3 mlremaining2.py NAME LEN STAGEDIR
import glob, os, sys
name, n, st = sys.argv[1], int(sys.argv[2]), sys.argv[3]
H = os.path.expanduser('~/orbitsolve/devtools/tws/')
cur = set()
for f in glob.glob(st + '/w*.txt'):
    for l in open(f):
        t = l.split()
        if len(t) == n: cur.add(' '.join(t))
sol = [l.strip() for l in open(H + name + '.sol')]
blocks = [b + 'End\n' for b in open(H + name + '.scr').read().split('End\n')[:-1]]
keep = [b for s, b in zip(sol, blocks) if s in cur]
open(H + name + '-remaining.scr.new', 'w').write(''.join(keep))
os.replace(H + name + '-remaining.scr.new', H + name + '-remaining.scr')
print(f"{name}-remaining.scr: {len(keep)} of {len(sol)} still at {n} ({len(cur)} classes at {n} in all)")
