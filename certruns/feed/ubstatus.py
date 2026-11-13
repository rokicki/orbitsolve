#!/usr/bin/env python3
# ubstatus.py: regenerate the chiral-pair length table in upperbound.md from the live stage
# directories, and the "Live status" block (worst counts, boundary-relevant tails, feeds).
# Run from the orbitsolve directory; certruns/feed/ubloop.sh runs it whenever a stage changes.
import glob, collections, re, subprocess, datetime, os
C = 'cert/obl23pair/cf-xfree/stages/'
cols = ['none..aUbU', 'aUbU..aDbD', 'aUaDbUbD..aLaFaRaB', 'aUaLaFaRaBaDbUbD..bLbFbRbB']
names = ['aUbU', 'aDbD', 'a ring', 'b ring']
H = []
for st in cols:
    h = collections.Counter()
    for f in glob.glob(C + st + '/w*.txt'):
        for line in open(f):
            h[len(line.split())] += 1
    H.append(h)
L = open('upperbound.md').read().split('\n')
s = L.index('| length | aUbU | aDbD | a ring | b ring |')
e = s + 2
while L[e].startswith('|') and L[e].split('|')[1].strip().isdigit(): e += 1
lens = sorted(set().union(*H))
rows = ['| %d | %s |' % (n, ' | '.join(f'{h[n]:,}' if h[n] else '' for h in H)) for n in lens]
assert L[e].startswith('| **classes**') and L[e + 1].startswith('| **worst**')
L[s + 2:e + 2] = rows + ['| **classes** | ' + ' | '.join(f'{sum(h.values()):,}' for h in H) + ' |',
                         '| **worst** | ' + ' | '.join(str(max(h)) for h in H) + ' |']
W = [max(h) for h in H]
def tail(f, n=1):
    try: return [l for l in open(f).read().split('\n') if l.strip()][-n:]
    except OSError: return []
def last(f, pat):
    t = [l for l in tail(f, 400) if re.search(pat, l)]
    return t[-1].strip() if t else ''
proc = dict(l.split('\t') for l in open('certruns/feed/processed.tsv').read().split('\n') if '\t' in l)
eta = subprocess.run(['python3', 'certruns/feed/eta.py', 'certruns/feed/run30.log'], capture_output=True, text=True).stdout.strip().split('\n')
now = datetime.datetime.now().strftime('%Y-%m-%d %H:%M')
xa = sum(H[2][n] for n in H[2] if n > 46 - W[3])
xu = sum(H[0][n] for n in H[0] if n > 42 - W[1])
blk = [f'#### Live status (auto-updated by certruns/feed/ubloop.sh; {now})', '',
       f'Plain sum **{sum(W)}** = ' + ' + '.join(map(str, W)) + '.  At the worst length: ' +
       ', '.join(f'{nm} {H[i][W[i]]:,} at {W[i]}' for i, nm in enumerate(names)) + '.',
       f'Boundary work at the current worsts: rings at 46 check the a-ring classes longer than {46 - W[3]} '
       f'({xa:,}); UD at 42 checks the aUbU classes longer than {42 - W[1]} ({xu:,}).  b ring classes at 25+: '
       f'{sum(H[3][n] for n in H[3] if n >= 25):,}.', '',
       '- a ring products (run30, 7pairud-2): ' + '; '.join(x for x in eta[:2] if x),
       '- b ring: ' + (last('certruns/feed/bnight.log', r'^== ') or 'bnight not started') +
       f'; 7pairudb-2 fed to {int(proc.get("7pairudb-2.log", 0)):,} bytes (udbloop, hourly products)',
       f'- aDbD: 7pairup-2 fed to {int(proc.get("7pairup-2.log", 0)):,} of 52,347,976,564 bytes (run25); '
       f'7pairupu-2 to {int(proc.get("7pairupu-2.log", 0)):,} of 1,410,987,175 (run37)', '']
try:
    a = L.index(next(l for l in L if l.startswith('#### Live status')))
    b = a + 1
    while b < len(L) and not L[b].startswith('#'): b += 1
    L[a:b] = blk
except StopIteration:
    a = L.index(next(l for l in L if l.startswith('#### The certified stages,')))
    L[a:a] = blk
open('upperbound.md.new', 'w').write('\n'.join(L))
os.replace('upperbound.md.new', 'upperbound.md')
print(now, ' '.join(f'{w}:{H[i][w]}' for i, w in enumerate(W)))
