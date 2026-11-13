import sys, collections
def parse(m):
    d = 1
    if m[0].isdigit(): d = int(m[0]); m = m[1:]
    f = m[0]; a = 1 if len(m) == 1 else (2 if m[1] == '2' else 3)
    return f, d, a
def inv(m):
    f, d, a = parse(m); return (f, d, (4 - a) % 4)
ORDER = "ULFRBD"
for fn in sys.argv[1:]:
    depth = collections.Counter(); orders = collections.Counter(); n = 0
    facefirst = 0
    for l in open(fn):
        t = l.split()
        alg = [x for x in t if not x.lstrip('-').isdigit()]
        n += 1
        k = 0
        while 2 * (k + 1) <= len(alg):
            a, b = alg[k], alg[len(alg) - 1 - k]
            fa = parse(a)
            if fa[1] != 1 or inv(b) != fa: break
            k += 1
        depth[k] += 1
        if parse(alg[0])[1] == 1: facefirst += 1
        if k >= 2:
            faces = ''.join(parse(m)[0] for m in alg[:k])
            orders[faces] += 1
    print(f"{fn}: {n} algs; starts with a face turn: {100*facefirst/n:.1f}%; face-conjugator depth:",
          {d: f"{100*c/n:.1f}%" for d, c in sorted(depth.items())})
    if orders:
        # for each unordered face pair among depth>=2 conjugators, how often each order occurs
        pairc = collections.Counter()
        for o, c in orders.items():
            pairc[''.join(sorted(o[:2], key=ORDER.index)) + (' in-order' if ORDER.index(o[0]) < ORDER.index(o[1]) else ' reversed')] += c
        print("   depth>=2 first-two-face order vs ULFRBD:", dict(sorted(pairc.items())[:12]))
