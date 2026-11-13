import sys, collections
geo = {}
for l in open('geom.txt'):
    i,f,r,c = l.split(); geo[int(i)] = (f,int(r),int(c))
def cycles(src):
    # after alg, pos d holds what was at src[d]: piece moves src[d] -> d
    dest = {s:d for d,s in enumerate(src)}
    seen=set(); cyc=[]
    for p in range(48):
        if p in seen or dest[p]==p: continue
        c=[p]; seen.add(p); q=dest[p]
        while q!=p: c.append(q); seen.add(q); q=dest[q]
        cyc.append(c)
    return cyc
def name(p): f,r,c=geo[p]; return f"{'AB'[p//24]}{f}{r}{c}"
maxlen = int(sys.argv[2]) if len(sys.argv)>2 else 6
shown = collections.Counter()
for l in open(sys.argv[1]):
    t = l.split(); L=int(t[0])
    if L>maxlen: break
    src = list(map(int,t[1:49])); alg=' '.join(t[49:])
    cyc = cycles(src)
    sig = tuple(sorted((len(c), c[0]//24) for c in cyc))
    if shown[(L,sig)] < 2:
        shown[(L,sig)] += 1
        print(L, alg.ljust(28), '  '.join('('+' '.join(name(p) for p in c)+')' for c in cyc))
