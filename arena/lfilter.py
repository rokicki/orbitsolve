# lfilter.py TWS MODE FILE > good.txt (2026-11-12, from op/linecheck.py): prints only lines with no problem: MODE op or ax.  Each line (moves, or NAME<TAB>moves, or LEN<TAB>moves)
# is a solving sequence; its position is the inverse line run on Solved.  Checks that the position
# has every non-pair set colour-solved (the line keeps the other oblique orbits) and is a stage start
# (pair U/D solved; for op also every piece on its axis).  Reports counts per file.
import sys
exec(open('/var/tmp/orbitsolve/op/twsval.py').read().split("sets,S,M=parse")[0])
sets,S,M=parse(sys.argv[1]); mode=sys.argv[2]
pair=['CENTERS2','CENTERS5']
axis={1:'ud',6:'ud',2:'lr',4:'lr',3:'fb',5:'fb'}
def amt(m): return {"'":3,"2":2}.get(m[-1],1) if m[-1] in "'2" else 1
def ap(st,mv):
    base=mv.rstrip("'2")
    for _ in range(amt(mv)):
        for s in sets:
            p=M[base].get(s)
            if p: st[s]=[st[s][p[i]] for i in range(24)]
def inv(ms): return [m.rstrip("'2")+{1:"'",2:"2",3:""}[amt(m)] for m in reversed(ms)]
tot=0
for fn in sys.argv[3:]:
    n=bad=badpair=0; ex=None
    for l in open(fn):
        l=l.rstrip('\n')
        if not l or l[0]=='#': continue
        if '\t' in l: l=l.split('\t',1)[1]
        ms=l.split(); n+=1
        st={s:list(S[s]) for s in sets}
        for m in inv(ms): ap(st,m)
        if any(st[s]!=S[s] for s in sets if s not in pair): bad+=1; ex=ex or l
        else:
            okp=True
            for s in pair:
                for i in range(24):
                    f=S[s][i]; g=st[s][i]
                    if (f in (1,6) and g!=f) or (mode=='op' and axis[f]!=axis[g]) or (mode=='ax' and (f in (1,6))!=(g in (1,6))):
                        badpair+=1; ex=ex or l; okp=False; break
                else: continue
                break
            if okp: print(l)
    tot+=bad+badpair
    sys.stderr.write('%s: %d lines, %d move another set, %d not a stage start%s'%(fn.split('/')[-1],n,bad,badpair,'' if not ex else '; e.g. '+ex[:80]))
sys.stderr.write('total problems: %d\n'%tot)
