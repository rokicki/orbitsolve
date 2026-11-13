# twsval.py TWS SCR SOL [PAIRSETS]: replay each ScrambleAlg on Solved; report states where the
# non-pair sets are not colour-solved, where the pair's U/D or axis split is broken (4op start), and
# solution lines (NAME<TAB>moves) that don't return the scramble to Solved.
import sys
def parse(fn):
    L=[l.split('#')[0].rstrip() for l in open(fn)]; M={}; S={}; i=0; sets=[]
    while i<len(L):
        w=L[i].split()
        if w and w[0]=='Set': sets.append(w[1])
        if w and w[0]=='Solved':
            i+=1
            while L[i]!='End': S[L[i].strip()]=[int(x) for x in L[i+1].split()]; i+=2
        if w and w[0]=='Move':
            nm=w[1]; d={}; i+=1
            while L[i]!='End': d[L[i].strip()]=[int(x)-1 for x in L[i+1].split()]; i+=2
            M[nm]=d
        i+=1
    return sets,S,M
sets,S,M=parse(sys.argv[1])
def ap(st,mv):
    base=mv.rstrip("'2"); n={"'":3,"2":2}.get(mv[-1],1) if mv!=base else 1
    for _ in range(n):
        for s in sets:
            p=M[base].get(s)
            if p: st[s]=[st[s][p[i]] for i in range(24)]
    return st
def run(st,line):
    for m in line.split(): st=ap(st,m)
    return st
pair=sys.argv[4].split(',') if len(sys.argv)>4 else ['CENTERS2','CENTERS5']
sc={}; cur=None
for l in open(sys.argv[2]):
    w=l.split()
    if w[:1]==['ScrambleAlg']: cur=w[1]; sc[cur]=''
    elif w[:1]==['End']: cur=None
    elif cur and w: sc[cur]+=' '+l.strip()
bad=0; badstart=0
axis={1:'ud',6:'ud',2:'lr',4:'lr',3:'fb',5:'fb'}
for n,a in sc.items():
    st=run({s:list(S[s]) for s in sets},a)
    ok=all(st[s]==S[s] for s in sets if s not in pair)
    for s in pair:
        for i in range(24):
            f=S[s][i]; g=st[s][i]
            if (f in (1,6) and g!=f) or axis[f]!=axis[g]: ok=False
    if not ok: badstart+=1
sol=[l.rstrip('\n').split('\t') for l in open(sys.argv[3])]
for n,line in sol:
    st=run(run({s:list(S[s]) for s in sets},sc[n]),line)
    if any(st[s]!=S[s] for s in sets): bad+=1
print(len(sc),'scrambles,',badstart,'not a 4op start;',len(sol),'solutions,',bad,'bad')
