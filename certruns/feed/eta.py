#!/usr/bin/env python3
# eta.py RUNLOG: progress and finish estimate for a sliced pairfeed run (run29 style), from the
# "# round R slice bytes A B of TOTAL" markers and pairfeed's "chunk i of n (HH:MM)" lines.
import re, sys, time, datetime
if len(sys.argv) != 2: sys.exit("usage: eta.py RUNLOG")
L = open(sys.argv[1]).read().splitlines()
rounds = int(next((re.search(r"rounds=(\d+)", l)[1] for l in L if "rounds=" in l), 2)); done = 0.0; start = None; total = None; cur = None; nch = 1; ich = 0; last = []
for l in L:
    m = re.match(r"# round (\d+) slice bytes (\d+) (\d+) of (\d+) \((\d+:\d+)\)", l)
    if m:
        r, a, b, total = int(m[1]), int(m[2]), int(m[3]), int(m[4])
        cur = (r, a, b); nch, ich = 1, 0
        if start is None: start = m[5]
        continue
    m = re.match(r".*: chunk (\d+) of (\d+) \((\d+:\d+)\)", l)
    if m: ich, nch = int(m[1]) - 1, int(m[2])
    if "failed replays)" in l and cur: ich += 1; last.append(l)
if cur is None: sys.exit("no slice started yet")
r, a, b = cur
frac = ((r - 1) * total + a + (b - a) * ich / nch) / (rounds * total)
now = datetime.datetime.now()
t0 = datetime.datetime.combine(now.date(), datetime.datetime.strptime(start, "%H:%M").time())
el = (now - t0).total_seconds() % 86400  # run started within the last day
print(f"round {r} of {rounds}, slice {a/1e9:.1f}..{b/1e9:.1f} GB of {total/1e9:.1f}, chunk {ich} of {nch} done")
print(f"overall {100*frac:.1f}% after {el/3600:.1f} h", end="")
if frac > 0.005:
    rem = el * (1 - frac) / frac
    print(f"; about {rem/3600:.1f} h left, finish ~{(now + datetime.timedelta(seconds=rem)).strftime('%a %H:%M')}")
else: print()
if last: print("last:", re.sub(r".*positions by length now: ", "", last[-1]))
