import sys
face={}
for l in open('geom.txt'):
    t=l.split(); face[int(t[0])]=(t[1],t[2],t[3])
for h in sys.argv[1:]:
    T=int(h,16); ps=[p for p in range(48) if T>>p&1]
    A=[f"{face[p][0]}{face[p][1]}{face[p][2]}" for p in ps if p<24]; B=[f"{face[p][0]}{face[p][1]}{face[p][2]}" for p in ps if p>=24]
    print(h, " A:", ' '.join(A), "  B:", ' '.join(B))
