#!/usr/bin/env pypy3
# mktails.py STAGE LEN K [--shuffle SEED] [--max N] > list.txt (2026-11-13): the next fastsolve list for STAGE.
# Takes the lines of exactly LEN moves in STAGE's latest fold (optfs/<st>/lastfold: 1ud/2ud stages/<dir>/w*.txt,
# 3ax/4op cert/stages/<st>/l*.txt) and prints the generating form (reversed, each move inverted) of their last K
# moves (K = LEN: the whole line), one per line, without repeats and without any tail already in optfs/<st>/all.txt
# (queued before).  Order: as in the stage files, or shuffled with --shuffle SEED; --max N keeps the first N.
# Also adds those lines to optfs/<st>/lines.txt (fold.py's LINES), so their shortenings fold.  Counts on stderr.
import sys, os, glob, random
args = sys.argv[1:]; seed = None; mx = None
print("# mktails.py " + " ".join(args), file=sys.stderr)
opts = {}
while len(args) > 3 and args[-2] in ("--shuffle", "--max"):
    opts[args[-2]] = args[-1]; args = args[:-2]
if len(args) != 3 or args[0] not in ("1ud", "2ud", "3ax", "4op"):
    sys.exit("usage: mktails.py 1ud|2ud|3ax|4op LEN K [--shuffle SEED] [--max N] > list.txt")
for k, v in opts.items():
    if k == "--shuffle": seed = int(v)
    else: mx = int(v)
st, L, K = args[0], int(args[1]), int(args[2])
if not 1 <= K <= L: sys.exit("need 1 <= K <= LEN")
O = "/var/tmp/orbitsolve/optfs/" + st + "/"; F = "/var/tmp/orbitsolve/fsfold/"
last = open(O + "lastfold").read().strip()
sd = {"1ud": "stages/none..aUbU/w*.txt", "2ud": "stages/aUbU..aDbD/w*.txt"}.get(st, "cert/stages/" + st + "/l*.txt")
files = sorted(glob.glob(F + st + "-" + last + "/" + sd))
if not files: sys.exit("no stage files for %s-%s" % (st, last))
def inv(m):
    if m.endswith("2"): return m
    if m.endswith("'"): return m[:-1]
    return m + "'"
lines = []
for f in files:
    for l in open(f):
        w = l.split()
        if len(w) == L: lines.append(w)
searched = set(l.strip() for l in open(O + "all.txt"))
out = []; seen = set(); olds = set()
for w in lines:
    t = " ".join(inv(m) for m in reversed(w[-K:]))
    if t in searched: olds.add(t); continue
    if t in seen: continue
    seen.add(t); out.append(t)
old = len(olds)
if seed is not None: random.Random(seed).shuffle(out)
if mx is not None: out = out[:mx]
have = set(l.strip() for l in open(O + "lines.txt"))
add = [" ".join(w) for w in lines if " ".join(w) not in have]
with open(O + "lines.txt", "a") as fo:
    for l in dict.fromkeys(add): fo.write(l + "\n")
sys.stdout.write("".join(t + "\n" for t in out))
print("%s-%s: %d lines of %d moves; %d new %d-move tails (%d already queued); %d lines added to lines.txt"
      % (st, last, len(lines), L, len(out), K, old, len(dict.fromkeys(add))), file=sys.stderr)
