#!/usr/bin/env pypy3
# fold.py [--pairs FILE] LINES INPUT OUT [OUT...] > improved.txt
# LINES: stage solving lines (one per class); INPUT: the generating tails given to fastsolve, in order;
# OUT: fastsolve stdout(s) for INPUT (blocks in input order; a partial last file is fine).
# For each line, every tail (last k moves) whose generating form was searched and shortened is replaced by
# the solution found; the shortest result wins.  Prints "old_len new_len line" for improved lines only;
# summary on stderr.  --pairs FILE also writes "old line | new line" for vline.py.
import sys

def inv(seq):
    out = []
    for m in reversed(seq):
        if m.endswith("2"): out.append(m)
        elif m.endswith("'"): out.append(m[:-1])
        else: out.append(m + "'")
    return out

def blocks(path):
    # yields one result per "Solving" block: list of moves, or None
    cur = False; sol = None
    for line in open(path):
        if line.startswith("Solving"):
            if cur: yield sol
            cur = True; sol = None
        elif cur and line.startswith(" ") and len(line) > 1 and line[1] != " ":
            sol = line.split()
        elif cur and line.startswith("Found"):
            yield sol; cur = False; sol = None
        elif cur and line.startswith("No solution"):
            yield None; cur = False; sol = None

def main():
    print("# fold.py " + " ".join(sys.argv[1:]), file=sys.stderr)
    pf = None
    if len(sys.argv) > 2 and sys.argv[1] == "--pairs":
        pf = open(sys.argv[2], "w"); del sys.argv[1:3]
    elif len(sys.argv) > 1 and sys.argv[1].startswith("-"):
        sys.exit("unknown option " + sys.argv[1])
    if len(sys.argv) < 4:
        sys.exit("usage: fold.py [--pairs FILE] LINES INPUT OUT [OUT...] > improved.txt")
    tails = [l.split() for l in open(sys.argv[2]) if l.strip()]
    res = []
    for p in sys.argv[3:]: res.extend(blocks(p))
    best = {}   # generating tail -> shortest solving replacement
    for t, s in zip(tails, res):
        if s is not None and len(s) < len(t):
            k = " ".join(t)
            if k not in best or len(s) < len(best[k]): best[k] = s
    lens = sorted(set(len(t) for t in tails))
    n = imp = 0; saved = {}
    for line in open(sys.argv[1]):
        mv = line.split()
        if not mv: continue
        n += 1; bestl = mv
        for k in lens:
            if k > len(mv): continue
            g = " ".join(inv(mv[len(mv) - k:]))
            if g in best:
                cand = mv[:len(mv) - k] + best[g]
                if len(cand) < len(bestl): bestl = cand
        if len(bestl) < len(mv):
            imp += 1; d = len(mv) - len(bestl); saved[d] = saved.get(d, 0) + 1
            print(len(mv), len(bestl), " ".join(bestl))
            if pf: pf.write(" ".join(mv) + " | " + " ".join(bestl) + "\n")
    print("# %d tails searched (of %d), %d shortened; %d of %d lines improved %s" %
          (len(res), len(tails), len(best), imp, n, dict(sorted(saved.items()))), file=sys.stderr)

main()
