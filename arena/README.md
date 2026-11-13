# arena: fastsolve tail searches and folds (laptop or armory)

Arena (10.0.0.155, LAN only) runs fastsolve on the tails of the longest stage lines; these scripts fetch its
outputs, fold the shortenings into the stages, check them and update the upperbound5.md Working tables.
Same paths on every machine; pypy3 must be on PATH (`~/pypy/current`).

| script | what |
|---|---|
| `ar CMD`, `aget REMOTE LOCAL` | ssh / scp to arena (`ARENA_HOST`; key `~/.ssh/id_gizmo` if present, else the default key) |
| `arwatch.sh N` | exits with a report when arena runs fewer than N solvers or is idle |
| `refold.sh U1 U2 O4 A3` | fetch arena `runs/*.out`, `fold.py` + `vline.py` per stage, `fsfold.sh` (1ud, 2ud), `fsfoldax.sh` (4op, 3ax; an existing fold dir is reused), `ubtable.py` |
| `fold.py`, `vline.py`, `inv.py`, `lfilter.py` (+ `twsval.py`) | apply tail shortenings to the stage lines; check old/new line pairs on the .tws |
| `fsfold.sh 1ud\|2ud NAME` | certmerge + certextend (pool of all finds) + proptop pulls + certcheck, from the expanded P 80 stage |
| `fsfoldax.sh 3ax\|4op NAME` | lfilter + axbound/opbound --rounds 0 on the base table + axcertupdate + axcheck |
| `ubtable.py U1 U2 O4 [A3]` | rewrite the upperbound5.md Working tables |
| `runner.sh`, `mkqueue.py` | arena-side chunk queue runner for the week plan (copies live on arena in ~/fastsolve-x86/week) |

Binaries (build per machine, in devtools/): `for b in certextend certmerge certcheck pairlogcheck axbound opbound
axcheck; do clang++ -std=c++20 -O3 -mcpu=native -pthread -o $b $b.cpp; done`.

Data, not in git (copied laptop -> armory 2026-11-13; same paths):

| path | size | what |
|---|---|---|
| /var/tmp/orbitsolve/optfs/{1ud,2ud,3ax,4op} | 60 MB | tail lists (all.txt), stage lines (*-solving.txt), arena outputs (*.out), fold results |
| /var/tmp/orbitsolve/optfs/full/stages | 1.4 GB | expanded P 80 stages (certsolve --expand of the repo certificate): the fold base |
| /var/tmp/orbitsolve/opt3ax/merge/ax-m3.tab, opt4op/merge/4op-m1.tab | 0.7 GB | 3ax / 4op base tables |
| /var/tmp/orbitsolve/fold/pools/*.cls | 4 GB | axbound/opbound pools (ud.cls, b1all.cls + the small ones) |
| ~/orbitsolve/dist/gullicube/data/{pair3,p8s10,p9,eg8,c12x}.cls | 7.5 MB | certextend base pools |
| /var/tmp/orbitsolve/fsfold/{3ax-f6,4op-f7u} | 1.4 GB | latest 3ax / 4op folds (both stages stopped; refold.sh reuses them) |

Arena state (2026-11-13): 1ud (runs/1ud-prod2.out) and 2ud (runs/2ud-prod4.out) running, one per socket; 4op
and 3ax stopped (tables kept in /dev/shm).  journal.md has the history; arena-week.md the week plan.
