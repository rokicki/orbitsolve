import itertools, collections, sys
coord={}; face={}
for l in open('geom.txt'):
    t=l.split(); i=int(t[0]); coord[i]=tuple(map(int,t[4:7])); face[i]=t[1]
def cycles(src):
    dest={s:d for d,s in enumerate(src)}; seen=set(); out=[]
    for p in range(48):
        if p in seen or dest[p]==p: continue
        c=[p]; seen.add(p); q=dest[p]
        while q!=p: c.append(q); seen.add(q); q=dest[q]
        out.append(c)
    return out
def rel(a,b):
    # all signed permutations taking coord a to coord b
    ca,cb=coord[a],coord[b]; r=[]
    for perm in itertools.permutations(range(3)):
        for sg in itertools.product([1,-1],repeat=3):
            if all(ca[perm[k]]*sg[k]==cb[k] for k in range(3)): r.append((perm,sg))
    return r
def relname(a,b):
    ca,cb=coord[a],coord[b]
    same=[k for k in range(3) if abs(ca[k])==abs(cb[k])]
    # describe: which axes negated / swapped
    rs=rel(a,b)
    return ' | '.join(f"{p}{''.join('+-'[s<0] for s in sg)}" for p,sg in rs)
maxL=int(sys.argv[2])
bylen=collections.defaultdict(collections.Counter); facerel=collections.defaultdict(collections.Counter)
nalg=collections.Counter()
for l in open(sys.argv[1]):
    t=l.split(); L=int(t[0])
    if L>maxL: break
    src=list(map(int,t[1:49])); cy=cycles(src)
    A=[c for c in cy if c[0]<24]; B=[c for c in cy if c[0]>=24]
    nalg[L]+=1
    if len(A)!=1 or len(B)!=1 or len(A[0])!=len(B[0]): 
        bylen[L]['(not single matched cycles) '+str(sorted(len(c) for c in A))+str(sorted(len(c) for c in B))]+=1; continue
    a,b=A[0],B[0]; n=len(a)
    # try all rotations of b to find one where a single signed perm maps a_k -> b_k for all k
    found=None
    for rot in range(n):
        bb=b[rot:]+b[:rot]
        common=set(rel(a[0],bb[0]))
        for k in range(1,n): common&=set(rel(a[k],bb[k]))
        if common: found=(bb,common); break
    if found:
        bb,common=found
        bylen[L]['conjugate by symmetry']+=1
    else:
        bylen[L]['no symmetry']+=1
    # per-piece face relation under best alignment (rotation 0..n with most 'opposite face')
    for rot in range(n):
        bb=b[rot:]+b[:rot]
        key=tuple(sorted(f"{face[x]}{face[y]}" for x,y in zip(a,bb)))
        facerel[L][key]+=1
        break
for L in sorted(nalg):
    print("len",L,"algs",nalg[L], dict(bylen[L]))
