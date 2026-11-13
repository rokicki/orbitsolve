# Arena week run: plan (2026-11-12, draft; the user has not decided to run it)

All four stages concurrently for 7 days, unattended, watched by Claude on armory.  Each stage: one
300 GB fastsolve table in /dev/shm (huge pages), 20 threads, two stages per socket (numactl
cpunodebind + membind), default SLACK (improvements only).  No larger table for 4op.

## How deep can each table go in 300 GB and <= 3 days?

Measured on arena 2026-11-11/12 (D=9 builds, two builds per socket, so ~half a socket each):

| stage | depth-9 leaves | depth-9 fill | leaves d9/d8 | slots used at D=9 | depth-10 leaves (est.) | depth-10 fill (est.) | slots used at D=10 (est.) |
|---|---|---|---|---|---|---|---|
| 1ud | 1.13e11 | 1.2 h | 28.8 | 3.2% | 3.2e12 | **~34 h** | ~45-50% |
| 2ud | 1.09e11 | 1.5 h | 31.1 | 4.1% | 3.4e12 | **~47 h** | ~65% (1.24e12 states for 1.2e12 slots) |
| 3ax (no rotations) | ~6e11 | ~11 h | ~33 | ~25% | ~2e13 | ~16 days | over-full |
| 4op (no rotations) | 2.20e11 | 6.5 h | 33.4 | 9.9% | 7.3e12 | ~9 days | over-full (~3x) |

**Measured 2026-11-12 (laptop scale model, lev3/occ.sh):** a 2-bit table loses most of the extra level once
it is half full, because deeper states that collide read as D.  1ud tables sized so the last two levels
fill like arena's (3.1% at D-1, as arena 1ud at 9; 4.9% as 2ud):

| new level fills | nodes, one level more | as on arena |
|---|---|---|
| 10% | 7.8x fewer (1ud), 7.5x (2ud) | - |
| 21% | 4.3x (4op) | - |
| 46-48% | 1.9x (1ud), 2.0x (3ax) | 1ud D=10 (~47%) |
| 63% | 1.45x | 2ud D=10 (~64%) |

So over 7 days: 1ud D=10 = 34 h build then ~1.9x: (168-34) x 1.9 = ~255 D=9-hours, **~1.5x net**.  2ud D=10 =
47 h then ~1.45x: (168-47) x 1.45 = ~175, **no gain**.  3ax/4op D=10: 9-16 days, over-full.
**Plan: 1ud rebuilds to D=10 (FTLEV=3); 2ud, 3ax, 4op keep their D=9 tables** (already in /dev/shm if arena is
not rebooted: no build at all).  Starting the 1ud build before Friday gives it more runway (decision for the user).
Whole 17-move 1ud lines (search to 16) stay out of reach: ~30 min per line even at D=10, months for 3,769.

## Code change needed (before the run)

The table has 2-bit codes: today code 0 = <=E (exact set), E+1, E+2, unset = >=D+1, so D <= E+2.
For D=10 with E=7 (exact set stays 2 GB): **code 0 = <= E+1** (exact set gives <= E, else E+1),
then E+2, E+3, unset = >= E+4; DFS 3 levels from layer E (level 1 writes code 0).  FS_TABVER bump
(x86v4) so old tables are never misread.  Test before use: laptop SLACK=0 cross-check (the
xc/ 80+80 sequences) between a D=E+2 and a D=E+3 table on all four stages, plus arena x86 vs laptop
ARM; MAXEST for level 3.

## Unattended runner (chunked queues)

Per stage on arena: `week/<st>/queue/NNN.txt` (2,000 generating tails each, in priority order),
`week/<st>/done/` (each finished chunk + its .out/.err), `runner.sh <st>`:
- loop: take the lowest-numbered chunk in queue/, run solve7 on it (the table maps from /dev/shm in
  ~2 s; the first chunk builds it), move chunk + output to done/, repeat; sleep and retry when the
  queue is empty; `touch week/<st>/STOP` ends it after the current chunk.
- a crash or kill loses at most one chunk; restarting runner.sh resumes (the table is still in shm).
- the watcher re-prioritises by rewriting the pending chunks only, never the running one.
- one line per chunk in week/<st>/runner.log (time, chunk, tails, found, s/tail).

Queues at start (each from the latest fold, longest lines first, only lines still at the top):
- 1ud (D=10): the sample chunk; the remaining 16-move tails of the 17s (~50% shorten); then 15-move
  tails of the 16s (fewer 16s shrink the UD boundary passes).
- 2ud (D=9, existing table): 15-move tails of the remaining 18s (shuffled), then 16-move tails.
- 3ax (D=9): the 748 15/16-move tails, then 17-move tails of the 23s.
- 4op (D=9): the remaining 14/15-move tails, then 16-move tails of the 22s.
1ud has no search during its build (~34 h); 3ax keeps searching on that socket.

## Watcher (Claude on armory)

Every 2-4 h:
1. arena alive? (4 runners, load ~80; /dev/shm tables present); restart a dead runner.
2. pull new done/ outputs; fold: fold.py + vline.py (1ud/2ud/4op), fsfold.sh 1ud|2ud NAME
   (certextend + pulls + certcheck), fsfold4op.sh NAME (opbound + axcheck), 3ax: same as 4op with
   axbound (script to write); refuse any line with a rotation (x/y/z).
3. requeue: tails of lines still at the top after the fold, not yet searched; keep each queue
   >= 1 day ahead.
4. log one status line per stage to week/status.md and update upperbound5.md Working tables
   (ubtable.py).  Never touch the repo or certsafe.

## To do before Friday night

1. Code: D=E+3 mode (FTLEV=3) + tests: **done** (all four stages laptop, 1ud arena x86 = laptop; FTNOBUILD guard).
2. runner.sh, queue builder (mkqueue.py: stage, fold dir, tail lengths, searched-before set),
   status line; dry run on arena with tiny chunks on the current tables.
3. Fold toolchain on armory: certextend, certmerge, certcheck, pairlogcheck, opbound, axbound,
   axcheck, axcertupdate.sh (built for armory), pools/, optfs/full stages, fold.py, vline.py,
   lfilter.py, fsfold*.sh, ubtable.py, pypy; armory -> arena ssh key; a 3ax fold script.
   Check one full fold cycle on armory against the laptop result.
4. Runbook for the armory Claude (this file + exact commands, failure cases: runner died, arena
   rebooted (remount /dev/shm size+huge, rebuild), table fingerprint mismatch, disk/shm full).
5. Start order Friday: stop the current jobs; 3ax/4op runners on the existing tables; 1ud/2ud
   runners with the new binary (builds start).
