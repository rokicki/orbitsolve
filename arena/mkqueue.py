#!/usr/bin/env pypy3
# mkqueue.py [--skip FILE]... [--chunk N] OUTDIR START TAILS... (2026-11-12): queue chunks for runner.sh.
# TAILS: generating tails in priority order (files concatenated); lines in a --skip file (searched before) and
# repeats are dropped.  Writes OUTDIR/NNNNN.txt (N lines each, default 2000), numbered from START; prints
# the count and the next free number.
import sys, os
args = sys.argv[1:]; skip = set(); chunk = 2000
print("# mkqueue.py " + " ".join(args), file=sys.stderr)
while args and args[0].startswith("--"):
    if args[0] == "--skip" and len(args) > 1:
        skip.update(l.strip() for l in open(args[1]) if l.strip()); args = args[2:]
    elif args[0] == "--chunk" and len(args) > 1:
        chunk = int(args[1]); args = args[2:]
    else: sys.exit("unknown option " + args[0])
if len(args) < 3: sys.exit("usage: mkqueue.py [--skip FILE]... [--chunk N] OUTDIR START TAILS...")
out, start = args[0], int(args[1]); os.makedirs(out, exist_ok=True)
seen = set(skip); lines = []
for fn in args[2:]:
    for l in open(fn):
        l = " ".join(l.split())
        if l and l not in seen: seen.add(l); lines.append(l)
n = start
for i in range(0, len(lines), chunk):
    open(os.path.join(out, "%05d.txt" % n), "w").write("".join(x + "\n" for x in lines[i:i + chunk])); n += 1
print("%d tails in %d chunks (%05d..%05d); next %05d" % (len(lines), n - start, start, n - 1, n))
