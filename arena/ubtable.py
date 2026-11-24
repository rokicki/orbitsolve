# ubtable.py U1 U2 O4 [A3] (2026-11-12): rewrite the "Working tables" section of ~/orbitsolve/upperbound5.md from fold
# outputs: U1/U2 = 1ud/2ud fold names (fsfold/<st>-NAME/stages), O4 = 4op fold name (fsfold/4op-NAME/op.out).
# A3 = 3ax fold name (fsfold/3ax-NAME/eng.out); without it 3ax stays at ax-m3 (from the Current tables section).
import sys, os, glob, re, time
if len(sys.argv) not in (4, 5): sys.exit("usage: ubtable.py 1UD_NAME 2UD_NAME 4OP_NAME [3AX_NAME]")
F = '/var/tmp/orbitsolve/fsfold/'; U = os.path.expanduser('~/orbitsolve/upperbound5.md')
def stage(d):
    c = {}
    for f in glob.glob(d + '/w*.txt'):
        for l in open(f): n = len(l.split()); c[n] = c.get(n, 0) + 1
    return c
def oplens(p, st='4op'):
    for l in open(p):
        if l.startswith(st + ' classes by length'):
            return {int(a): int(b) for a, b in (x.split(':') for x in l.split('): ', 1)[1].split())}
c1 = stage(F + '1ud-%s/stages/none..aUbU' % sys.argv[1]); c2 = stage(F + '2ud-%s/stages/aUbU..aDbD' % sys.argv[2])
d4 = F + '4op-%s/' % sys.argv[3]; c4 = oplens(d4 + ('eng.out' if os.path.exists(d4 + 'eng.out') else 'op.out'))
s = open(U).read()
cur = s[s.index('## Current tables'):s.index('## Working tables')]
c3 = {}
for l in cur.splitlines():
    m = re.match(r'^\| (\d+) \|(.*)\|$', l)
    if m:
        v = [x.strip().replace(',', '') for x in m.group(2).split('|')]
        if v[2]: c3[int(m.group(1))] = int(v[2])
A3 = sys.argv[4] if len(sys.argv) == 5 else None
if A3: c3 = oplens(F + '3ax-%s/eng.out' % A3, '3ax')
cols = [c1, c2, c3, c4]; f = lambda v: "{:,}".format(v) if v else ""
rows = ["| length | 1ud (%s) | 2ud (%s) | 3ax (%s) | 4op (%s) |" % (sys.argv[1], sys.argv[2], A3 or "ax-m3, unchanged", sys.argv[3]), "|---|---|---|---|---|"]
for L in range(0, 30):
    if any(L in c for c in cols): rows.append("| %d | " % L + " | ".join(f(c.get(L, 0)) for c in cols) + " |")
rows.append("| **classes** | " + " | ".join("**{:,}**".format(sum(c.values())) for c in cols) + " |")
rows.append("| **longest** | " + " | ".join("**%d**" % max(c) for c in cols) + " |")
top = lambda c: "{:,}".format(c[max(c)])
# base = expanded P 80 certificate worsts (11-24: a stage's longest can drop below its base worst, e.g. 1ud 17 -> 16)
BASE = [(17, 5272), (18, 286263), (23, 549), (22, 14965)]
def tops(c, b):
    w, n = b
    if max(c) < w: return "%ds {:,} -> **0** (longest now **%d**: {:,} at %d)".format(n, c[max(c)]) % (w, max(c), max(c))
    return "%ds {:,} -> **%s**".format(n) % (w, top(c))
txt = """## Working tables (%s; arena fastsolve tail folds, not yet in a certificate)

Base: the expanded P 80 certificate (certsolve --expand: 1ud 17s 5,272, 2ud 18s 286,263; optfs/full).  Arena fastsolve
(300 GB tables, D=9) searched the tails of the longest lines for shorter replacements; each fold merges the improved
lines (vline-checked), turns every shortened tail into an action for certextend (plus --proptop 16 pulls), and checks
the result (1ud/2ud certcheck --keep ud PASS; 4op opbound replay + axcheck PASS).  Stages: /var/tmp/orbitsolve/fsfold/
1ud-%s, 2ud-%s, 4op-%s, %s.
Tops: 1ud %s; 2ud %s; 4op %s; 3ax %s.
Plain sum of the working longest: %s (P 80 certified; working tables are not a certificate until released).

""" % (time.strftime('%Y-%m-%d %H:%M'), sys.argv[1], sys.argv[2], sys.argv[3], '3ax-' + A3 if A3 else '3ax ax-m3 (no fold yet)', tops(c1, BASE[0]), tops(c2, BASE[1]), tops(c4, BASE[3]), tops(c3, BASE[2]),
  " + ".join(str(max(c)) for c in cols) + " = %d" % sum(max(c) for c in cols)) + "\n".join(rows) + "\n\n"
a = s.index('## Working tables'); b = s.index('## Certificates')
s = s[:a] + txt + s[b:]
s = re.sub(r"Later folds not yet in a certificate: arena fastsolve folds [^;]*; see Working tables\);", "Later folds not yet in a certificate: arena fastsolve folds %s (%s; see Working tables);" % (time.strftime('%Y-%m-%d'), ", ".join("%s %s at %d" % (n, top(c), max(c)) for n, c in zip(("1ud", "2ud", "3ax", "4op"), cols))), s)
open(U, 'w').write(s)
print("upperbound5.md: 1ud %s, 2ud %s, 3ax %s, 4op %s" % (top(c1), top(c2), top(c3), top(c4)))
