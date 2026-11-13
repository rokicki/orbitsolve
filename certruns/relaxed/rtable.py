#!/usr/bin/env python3
# Rebuild the distance table in upperbound3.md (between the rtable markers) from the relaxed
# stage copies in /var/tmp/orbitsolve/r{1u,1ud,2ud}.  (Until 2026-10-26 upperbound2.md, with 1f
# and 2f columns from the 2026-10-22 certificates.)
import glob, os, time
ROOT = os.path.expanduser('~/orbitsolve')
def hist(stdir):
    h = {}
    for f in glob.glob(stdir + '/w*.txt'):
        with open(f) as fh:
            for line in fh:
                n = len(line.split()); h[n] = h.get(n, 0) + 1
    return h
def P(t): return {int(a): int(b) for a, b in (x.split(':') for x in t.split())}
f1 = P("0:1 4:26 5:56 6:389 7:1183 8:5732 9:25029 10:70853 11:244157 12:630748 13:1500590 14:2465019 15:3363576 16:3118018 17:2060671 18:597151 19:36652")
f2 = P("0:1 7:16 8:150 9:210 10:526 11:2924 12:15652 13:29917 14:71140 15:123524 16:144961 17:213180 18:311857 19:450413 20:611867 21:620251 22:271869 23:68205 24:272")
V = '/var/tmp/orbitsolve'
cols = [hist(V + '/r1u/stages/none..aUbU'), hist(V + '/r1ud/stages/none..aUbU'), hist(V + '/r2ud/stages/aUbU..aDbD')]
L = sorted(set().union(*cols))
fmt = lambda c, l: f"{c[l]:,}" if l in c else ""
rows = [f"Updated {time.strftime('%Y-%m-%d %H:%M')} (certruns/relaxed/rtable.py).", "",
        "| length | 1u | 1ud | 2ud |", "|---:|---:|---:|---:|"]
for l in L: rows.append(f"| {l} | " + " | ".join(fmt(c, l) for c in cols) + " |")
rows.append("| **classes** | " + " | ".join(f"{sum(c.values()):,}" for c in cols) + " |")
rows.append("| **worst** | " + " | ".join(str(max(c)) for c in cols) + " |")
# several loops run this: the read-modify-write of upperbound3.md holds an exclusive flock
import fcntl
p = ROOT + '/upperbound3.md'
with open('/var/tmp/orbitsolve/rtable.lock', 'w') as lk:
    fcntl.flock(lk, fcntl.LOCK_EX)
    s = open(p).read()
    A, B = '<!-- rtable start -->\n', '<!-- rtable end -->\n'
    i, j = s.index(A) + len(A), s.index(B)
    s = s[:i] + "\n".join(rows) + "\n" + s[j:]
    t = f"{p}.tmp{os.getpid()}"
    open(t, 'w').write(s); os.replace(t, p)
