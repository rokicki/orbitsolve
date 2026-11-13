#!/usr/bin/env pypy3
# vline.py TWS PAIRS: independent check of folded lines by tws simulation.  PAIRS: "old | new" solving
# sequences.  The start position is the inverse of old applied to the tws Solved labels; new must take it
# back to Solved, and be shorter.  Prints PASS/FAIL counts.
import sys, re
if len(sys.argv) != 3: sys.exit("usage: vline.py TWS PAIRS")
print("# vline.py " + " ".join(sys.argv[1:]))
L = [l.split('#')[0].split() for l in open(sys.argv[1])]; L = [l for l in L if l]
solved = {}; moves = {}; i = 0
while i < len(L):
    if L[i][0] in ('Solved', 'Move'):
        name = L[i][1] if L[i][0] == 'Move' else None; i += 1; blk = {}
        while L[i][0] != 'End': blk[L[i][0]] = [int(x) for x in L[i + 1]]; i += 2
        i += 1
        if name: moves[name] = blk
        else: solved = blk
    else: i += 1
def run(state, alg):
    for t in alg:
        m = re.match(r"^(\d*[A-Za-z]+)(\d*)('?)$", t)
        base, amt, pr = m.group(1), int(m.group(2) or 1), m.group(3)
        for _ in range((4 - amt) % 4 if pr else amt):
            for s, p in moves[base].items():
                st = state[s]; state[s] = [st[p[j] - 1] for j in range(len(p))]
def inv(seq):
    return [m if m.endswith("2") else m[:-1] if m.endswith("'") else m + "'" for m in reversed(seq)]
ok = bad = 0
for line in open(sys.argv[2]):
    old, new = [x.split() for x in line.split("|")]
    st = {k: list(v) for k, v in solved.items()}
    run(st, inv(old)); run(st, new)
    if st == solved and len(new) < len(old): ok += 1
    else: bad += 1; print("FAIL:", line.strip())
print(("PASS" if bad == 0 else "FAIL") + ": %d lines checked, %d bad" % (ok + bad, bad))
