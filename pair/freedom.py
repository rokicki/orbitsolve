import collections, sys
face={}
for l in open('geom.txt'):
    t=l.split(); face[int(t[0])]=t[1]
def cycles(src):
    dest={s:d for d,s in enumerate(src)}; seen=set(); out=[]
    for p in range(48):
        if p in seen or dest[p]==p: continue
        c=[p]; seen.add(p); q=dest[p]
        while q!=p: c.append(q); seen.add(q); q=dest[q]
        m=c.index(min(c)); out.append(tuple(c[m:]+c[:m]))
    return out
Aof=collections.defaultdict(lambda: collections.defaultdict(set))  # A-part -> len -> set of B-parts
lens=collections.Counter()
for l in open(sys.argv[1]):
    t=l.split(); L=int(t[0])
    if L>int(sys.argv[2]): break
    cy=cycles(list(map(int,t[1:49])))
    A=tuple(sorted(c for c in cy if c[0]<24)); B=tuple(sorted(c for c in cy if c[0]>=24))
    if len(A)==1 and len(A[0])==3 and len(B)==1 and len(B[0])==3:
        Aof[A][L].add(B); lens[L]+=1
print("3+3 algs by length:", dict(lens))
allA=set(Aof)
print("distinct A 3-cycles appearing in a 3+3 alg: ", len(allA), " (of 24*23*22/3 =", 24*23*22//3, ")")
for maxL in (4,5,6,7,8):
    As=[a for a in Aof if any(L<=maxL for L in Aof[a])]
    nB=[len(set().union(*[Aof[a][L] for L in Aof[a] if L<=maxL])) for a in As]
    if As: print(f"<= {maxL}: {len(As)} A-cycles reachable; B partners per A-cycle: min {min(nB)} mean {sum(nB)/len(nB):.1f} max {max(nB)}")
# geometry of A cycles at length 4: faces
fc=collections.Counter()
for a in Aof:
    if 4 in Aof[a]: fc[''.join(sorted(face[p] for p in a[0]))]+=1
print("faces of A 3-cycles at length 4:", fc.most_common(8))
# example: one A-cycle's partners at 4
a=next(a for a in Aof if 4 in Aof[a])
print("example A", [(p,face[p]) for p in a[0]], "B partners len4:", [[(p,face[p]) for p in b[0]] for b in Aof[a][4]])
print("   len5:", [[(p,face[p]) for p in b[0]] for b in Aof[a].get(5,[])], " len6 count:", len(Aof[a].get(6,[])))
