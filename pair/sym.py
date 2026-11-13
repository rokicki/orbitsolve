import itertools, collections, sys
pos = {}; coord = {}
for l in open('geom.txt'):
    t = l.split(); i=int(t[0]); coord[i]=tuple(map(int,t[4:7])); pos[coord[i]]=i
syms=[]
for perm in itertools.permutations(range(3)):
    for signs in itertools.product([1,-1],repeat=3):
        m = [ (perm[k],signs[k]) for k in range(3)]
        img = {}
        ok=True
        for i,c in coord.items():
            n = tuple(c[m[k][0]]*m[k][1] for k in range(3))
            if n not in pos: ok=False; break
            img[i]=pos[n]
        if not ok: continue
        det = 1
        # determinant sign
        import numpy as np
        M = np.zeros((3,3))
        for k in range(3): M[k][m[k][0]] = m[k][1]
        det = round(np.linalg.det(M))
        if all((img[i]>=24)!=(i>=24) for i in range(48)):
            syms.append((perm,signs,det,img))
print(len(syms), "orbit-swapping symmetries (dets:", collections.Counter(s[2] for s in syms),")")
stats = collections.defaultdict(collections.Counter); tot=collections.Counter()
for l in open(sys.argv[1]):
    t=l.split(); L=int(t[0])
    if L>int(sys.argv[2]): break
    src=list(map(int,t[1:49])); tot[L]+=1
    for k,(perm,signs,det,img) in enumerate(syms):
        # B part equals image of A part: src[img[d]] == img[src[d]] for d in A
        if all(src[img[d]]==img[src[d]] for d in range(24)):
            stats[L][(perm,signs,det)]+=1
for L in sorted(tot):
    print("len",L,"algs",tot[L], "B = sigma(A) for:", [(f"{p}{''.join('+' if s>0 else '-' for s in sg)} det{d}",c) for (p,sg,d),c in stats[L].most_common()], "none:", tot[L]-sum(1 for _ in [])) 
