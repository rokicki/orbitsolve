# 2026-10-25: write devtools/tws/NAME.scr and NAME.sol: every class whose certificate line in STAGEDIR
# is LEN moves, shuffled (SEED), as ScrambleAlg NAME-NNNNNN (the inverse of its line) and the line.
#   python3 mlexport.py NAME LEN STAGEDIR SEED
import glob, os, random, sys
name, n, st, seed = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
H = os.path.expanduser('~/orbitsolve/devtools/tws/')
inv = lambda m: m[:-1] if m.endswith("'") else m if m.endswith("2") else m + "'"
L = []
for f in sorted(glob.glob(st + '/w*.txt')):
    for l in open(f):
        t = l.split()
        if len(t) == n: L.append(' '.join(t))
random.Random(seed).shuffle(L)
with open(H + name + '.sol.new', 'w') as s, open(H + name + '.scr.new', 'w') as o:
    for i, l in enumerate(L, 1):
        s.write(l + '\n')
        o.write(f"ScrambleAlg {name}-{i:06d}\n{' '.join(inv(m) for m in reversed(l.split()))}\nEnd\n\n")
os.replace(H + name + '.sol.new', H + name + '.sol')
os.replace(H + name + '.scr.new', H + name + '.scr')
print(f"{name}.scr: {len(L)} classes at {n} (seed {seed})")
