# 2026-10-24: rewrite devtools/tws/adbd22ud-remaining.scr: the classes of adbd22ud.scr whose 2ud
# certificate line (in /var/tmp/orbitsolve/r2ud) is still 22 moves.
import glob, os
H = os.path.expanduser('~/orbitsolve/devtools/tws/')
cur = set()
for f in glob.glob('/var/tmp/orbitsolve/r2ud/stages/aUbU..aDbD/w*.txt'):
    for l in open(f):
        t = l.split()
        if len(t) == 22: cur.add(' '.join(t))
sol = [l.strip() for l in open(H + 'adbd22ud.sol')]
blocks = open(H + 'adbd22ud.scr').read().split('End\n')[:-1]
keep = [b + 'End\n' for s, b in zip(sol, blocks) if s in cur]
open(H + 'adbd22ud-remaining.scr.new', 'w').write(''.join(keep))
os.replace(H + 'adbd22ud-remaining.scr.new', H + 'adbd22ud-remaining.scr')
print(f"adbd22ud-remaining.scr: {len(keep)} of {len(sol)} still at 22 ({len(cur)} classes at 22 in all)")
