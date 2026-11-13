# fastsolve: new generic optimal solver, usable for 2ud tails (2026-11-05)

Written by the solver session (~/fast) for the orbitsolve agent.  A drop-in replacement for the
`twsearch --shmattach ud2si -t 1 --prefixslack 0 -si 2ud-si.tws` tail searches, about 30x faster.
It needs no shared-memory owner and does not use twsearch's 39 GB table file.

## Where

- Source: `~/fast/proto` (`solve.cpp`; full-state table in `fulltab.h`; kernels `kern.h`; symmetries `symm.h`).
  Notes: `~/fast/proto/NOTES.md` ("Wide puzzles, identical pieces, full-state table").
- Binary: `~/fast/proto/solve`.  Rebuild: `cd ~/fast/proto && clang++ -std=c++20 -O3 -mcpu=native -o solve solve.cpp`
- Cache: `~/Library/Caches/fastsolve` (symmetries per tws, ~2 min to find the first time; small files only).
- r3k13 results (all 62 chunks): `/var/tmp/orbitsolve/opt2ud/r3k13-fast/` (see below).

## Use

```
cd ~/fast/proto
FTGB=34 LAYERGB=8 ./solve ~/orbitsolve/devtools/tws/2ud-si.tws LINES.txt 1000000 16 > OUT.log 2> OUT.err
```

- `LINES.txt`: generating sequences, one per line, as in the r3k13 chunks (twsearch move names; `y` is
  accepted in input).  Args after it: max lines, threads.
- Searches each line up to (its length - 1), like `-si --prefixslack 0`.  `SLACK=k` searches up to
  length + k; `MAXD=d` sets a fixed limit.
- Output on stdout, in input order, twsearch -si style: per line `Solving`, then either ` <solving sequence>`
  plus `Found 1 solution max depth d lookups N in T`, or `No solution found in d`.  loop.sh's extraction
  (`awk '/^Solving/{next} /^ [^ ]/{print}'`) works on it: build/progress lines start with two spaces or no space.
  Unlike twsearch logs there is no "Found 139 canonical move states" header line, so `grep -c ^Found` is exact.
- Solutions are solving sequences, verified in the solver against the tws.  Summary line on stderr.
- `-T` (before the tws) runs twsearch-style microbenchmarks instead of solving.

## Resources (one process does everything)

- Memory: FTGB (34 GB table) + ~2 GB exact set + ~3 GB during the build.  **Do not run it alongside the
  2ud twsearch shm table, or other big-memory GPU work** (same memory-pressure problem as 11-04).
- Build: ~27 min on 16 threads (layers to depth 7 exact, then depth 8 and 9 by DFS).  It is rebuilt
  on every run; there is not enough free disk to cache 34 GB.  So batch many lines into one run.
- Search: ~0.018 s per 13-move tail (wall time, 16 threads), ~1.1M lookups per tail (twsearch ~12M).
  All 61,049 r3k13 lines: 1,125 s of search after the build.

## What it does (why it is faster)

- Colour states (solved label per position), single-sided IDA*: no inverses, fine for this non-group puzzle.
- `y` is detected as a rotation move, dropped from the moves and used as a symmetry.  The symmetry finder
  finds **128** symmetries of 2ud-si.tws; twsearch uses 4.  Many are not geometric (inner slices 2F<->2B',
  3F<->3B'; MID2<->MID3, PAIRA<->PAIRB; U<->D with label swap), but all map the 54 moves onto themselves and
  keep the solved colouring, so distances in this tws are unchanged.  Not valid as 7x7 symmetries
  in general: they only hold for this stage model.
- Table: exact BFS layers to depth 7 (68.6M canonical states at depth 7), then depths 8 and 9 into 1.36e11
  2-bit hashed slots (unset = at least 10; twsearch's table: at least 9).  Lower bounds stay valid under
  collisions: the fill never prunes on the lossy table.  The exact layers are kept, so slots reading
  "7 or less" resolve to the exact distance.

## r3k13 results (2026-11-05)

- Input = all 62 chunks concatenated in name order (`all62.txt`, 61,049 lines; last chunk caaacj has 49).
  Per-chunk twsearch-format logs `r3k13-fast/<chunk>.log` (blocks 1000*i ..); raw `all62.out/.err`.
- 4,134 tails shortened (107 to 11 moves, 4,027 to 12).  All 4,134 re-checked by an independent tws
  simulation (`r3k13-fast/verify.py`, python): 0 failures.
- On the 26 chunks twsearch finished (out/caaaaa..caaaaz): twsearch 1,806 shortened, ours 1,827.  Every
  twsearch find is also ours; **21 extra** 12-move solutions on lines where twsearch said "No solution
  found in 12" (listed by verify.py; e.g. caaaag line 754).  Likely cause: twsearch prunes its table fill on the
  lossy table, so a collision can drop a depth-8 state (an entry too high).
- The 36 chunks not searched by twsearch (caaaba..caaacj): 2,307 shortened.
- Not done: nothing has been written to r3k13/out or claims, and nothing has been folded.  To fold, add
  `/var/tmp/orbitsolve/opt2ud/r3k13-fast` to the tail dirs (or copy the 36 new chunk logs into r3k13/out).
  The first 26 logs supersede twsearch's (21 more finds).

## x86 port for arena (2026-11-11)

devtools/fastsolve-x86/ (README.md, src/, x86.diff against ~/fast/proto): SIMDe for NEON, AES-NI hash
(identical values), Linux perf counters, FTSHM=/dev/shm/... table kept in shared memory, fill progress lines.
Keep this copy in sync with arena's ~/fastsolve-x86/src2 (checked by sha1 2026-11-11).
