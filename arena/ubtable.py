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
cert = [{}, {}, {}, {}]       # the certified table (Current tables), to show each cell as "certified -> now"
for l in cur.splitlines():
    m = re.match(r'^\| (\d+) \|(.*)\|$', l)
    if m:
        v = [x.strip().replace(',', '') for x in m.group(2).split('|')]
        for i in range(4):
            if v[i]: cert[i][int(m.group(1))] = int(v[i])
c3 = dict(cert[2])
A3 = sys.argv[4] if len(sys.argv) == 5 else None
if A3: c3 = oplens(F + '3ax-%s/eng.out' % A3, '3ax')
cols = [c1, c2, c3, c4]; f = lambda v: "{:,}".format(v) if v else ""
def cell(i, L):
    a, b = cert[i].get(L, 0), cols[i].get(L, 0)
    return f(b) if a == b else "%s -> **%s**" % (f(a) or "0", f(b) or "0")
rows = ["| length | 1ud (f%s) | 2ud (f%s) | 3ax (f%s) | 4op (f%s) |" % tuple(x.lstrip('f') for x in (sys.argv[1], sys.argv[2], A3 or "ax-m3", sys.argv[3])), "|---|---|---|---|---|"]
for L in range(0, 30):
    if any(L in c for c in cols + cert): rows.append("| %d | " % L + " | ".join(cell(i, L) for i in range(4)) + " |")
rows.append("| **classes** | " + " | ".join("**{:,}**".format(sum(c.values())) for c in cols) + " |")
rows.append("| **longest** | " + " | ".join(("**%d**" % max(c)) if max(c) == max(cert[i]) else "%d -> **%d**" % (max(cert[i]), max(c)) for i, c in enumerate(cols)) + " |")
top = lambda c: "{:,}".format(c[max(c)])
tot = lambda cs: sum(max(c) for c in cs)
txt = """## Working tables (%s; arena fastsolve tail folds, not yet in a certificate)

Arena fastsolve (300/700/1400 GB tables) shortens the tails of the longest lines; each fold merges them (vline-checked),
propagates with certextend and checks (certcheck --keep ud / axcheck PASS).  Cells show certified -> now where they
differ.  Plain sum **%d** (certified %d).  Stages: armory /var/tmp/orbitsolve/fsfold/1ud-%s, 2ud-%s, 3ax-%s, 4op-%s.

""" % (time.strftime('%Y-%m-%d %H:%M'), tot(cols), tot(cert), sys.argv[1], sys.argv[2], A3 or "(none)", sys.argv[3]) + "\n".join(rows) + "\n\n"
a = s.index('## Working tables'); b = s.index('## Certificates')
s = s[:a] + txt + s[b:]
s = re.sub(r"Later folds not yet in a certificate: arena fastsolve folds [^;]*; see Working tables\);", "Later folds not yet in a certificate: arena fastsolve folds %s (%s; see Working tables);" % (time.strftime('%Y-%m-%d'), ", ".join("%s %s at %d" % (n, top(c), max(c)) for n, c in zip(("1ud", "2ud", "3ax", "4op"), cols))), s)
open(U, 'w').write(s)
print("upperbound5.md: 1ud %s, 2ud %s, 3ax %s, 4op %s" % (top(c1), top(c2), top(c3), top(c4)))
