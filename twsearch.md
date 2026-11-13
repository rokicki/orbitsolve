# twsearch notes (orbit bounds work)

Brief.  Runs and measurements as they happened: journal.md.

## The -si bug (found 2026-11-02)

- In `~/twsearch2` (commit 9b10363), `-s`/`-si` read input through `processlines()` in `src/cpp/cmdlineops.cpp`,
  which never sets `globalinputmovecount`. It stays 0, so `-si` (solve.cpp: `if (onlyimprovements && d >=
  globalinputmovecount) break;`) stops before searching: every line prints "No solution found in 0". Test:
  `printf "U U\n" | twsearch -si 3x3x3.tws` finds nothing (should find `U2`). The older `~/twsearch` (2025-04-02)
  set it in processlines; it was lost when processlines gained the `allowrotations` argument.
- Workaround in use: a patched copy at `/var/tmp/orbitsolve/twsi` (processlines counts the line's moves, whitespace or
  comma separated, into `globalinputmovecount`); `~/twsearch2` itself is untouched. To do: fix upstream (count the
  parsed moves in processlines, or have parsedomovelist_generously return the count) and add a `-si` regression test.
- Throughput: `-si` and `--shortenseqs` on big batches of short lines use about one core (per-line, per-depth
  thread setup).  Design for a `--bigbatch N` mode (parallel over lines, shared table, in-order output,
  extension between drained generations): below.

## Table-load bug (found and fixed 2026-11-03)

- Since 96382f4 (2023-10-06, "Made table loading more than twice as fast"), `prunetable::unpackblock`
  (src/cpp/pruneio.cpp) could write 1-3 bytes past the end of a 256 KB block: a multi-byte decode entry near the
  block end also decodes the bits after the block's last code.  Blocks unpack in parallel (since 2018), so those
  bytes corrupted the next block's first bytes whenever that block finished first, and the last block wrote past
  the table.
- Found because a shared-memory owner segfaulted on the 1ud table (the overrun ran off the end of the mapping).
- Reproduced on a clean clone (/var/tmp/orbitsolve/twsfresh) with only a table checksum added: three loads of one
  file gave three checksums.  Measured with an instrumented copy: about 53,000 of 159,744 block starts corrupted per
  load, about 56,000 2-bit entries too high and 89,000 too low (of 1.7e11).
- Fix: write only the bytes that remain in the block (`ii < dc->bytewidth && ii < i`).  After it, every load (-t 16,
  -t 1) gives one checksum, equal to a single-threaded load of the old code.  Committed by the user; also in twsi2.
- Scope: only tables loaded from a file, with more than one thread.  Found solutions are never wrong; an entry too
  high can make a search miss a solution, so "no shorter line" verdicts from loaded tables are not fully reliable.
  Ours from loaded tables: the 3ax shared-memory -si runs and tails, hq23, 4op, the shortenseqs runs.  Certificates
  are unaffected (every line is checked independently).

## Builds

- /var/tmp/orbitsolve/twsi: ~/twsearch2 plus the -si fix.
- /var/tmp/orbitsolve/twsi2: also --prefixslack K (caps the work-chunk prefix at depth - (table depth) - K - 1;
  K = 0 best: about 3x on batches of short searches), --chunkfactor/--shardmargin/--smallchunks (no better), a
  TIMING line after the table load, and shared memory: --shmhold NAME (owner: creates /NAME, loads or fills the
  table in place, prints a ready line, holds it until SIGINT/SIGTERM/SIGHUP, which remove it), --shmattach NAME
  (read-only client: never extends or writes), --shmremove NAME (after a SIGKILL).

## Batch runs that work (-si, --shortenseqs)

- One owner with the table (-M 40000; --startprunedepth or a saved table to get it full), 16 clients each
  `-t 1 --prefixslack 0`, input split into chunks claimed with mkdir; big chunks (10,000 lines), since each client
  start soft-faults the whole 42 GB mapping (about 25 s).  Examples: opt3ax/hq23/shm/handoff.sh, opt4op/shm/handoff.sh,
  shorten/shm/shm23c.sh.
- Build the table first with one process running the shortest lines (it extends as lookups grow), stop it after the
  last fill has been written, then switch to shared memory.
- --distinguishall (needed by --shortenseqs here) is a different puzzle: its own table.
- Never run two 40 GB searches at once; pause the ML agent and the fold loop first.  No rotations in --moves; no
  multi-GB --newcanon.

## Move sets and branching (3ax/4op; devtools/tws/*.moveset, use `--moves `cat hq23.moveset``)

H = F,B,L,R (all powers) + 2U2,3U2,2D2,3D2; +Q = 2U,3U,2D,3D quarter turns; +2 = 2F2,2B2,2L2,2R2; +3 = 3F2,3B2,3L2,3R2;
V = also vertical-slice quarter turns; ALL = no --moves.  Branching (-C30) and table depth at -M 40000:

| set | fill depth @ 1.6e11 | gain at 10: nc3 | nc4 | nc5 |
|---|---|---|---|---|
| h | 12 | 1.34 | 1.53 | 1.69 (7 s) |
| h2, h3 | 11 | 1.18 | 1.35 | 1.49 (31-37 s, 1 GB) |
| h23 | 10 | 1.13 | 1.28 | |
| hq | 9 | 1.13 | 1.22 | |
| hq2, hq3 | 9 | 1.08 | 1.15 | |
| hq23 | 9 | 1.07 | 1.13 (40 s, 1.3 GB) | |
| hq23v | 8 | 1.02 | | |
| ALL | 8 | | | |


| set | plain | nc3 | nc4 | nc5 |
|---|---|---|---|---|
| h | 9.439 | 9.027 | 8.823 | 8.655 (7 s, 0.2 GB) |
| h2, h3 | 11.807 | 11.521 | 11.242 (3 s, 0.1 GB) | 11.033 (31-37 s, 1 GB) |
| h23 | 14.353 | 14.106 | 13.802 (7 s, 0.2 GB) | |
| hq | 15.856 | 15.572 | 15.381 (9 s, 0.3 GB) | |
| hq2, hq3 | 18.267 | 18.056 | 17.865 (20 s, 0.7 GB) | |
| hq23 | 20.725 | 20.541 | 20.332 (40 s, 1.3 GB) | |
| hq23v | 29.302 | 29.244 (10 s, 0.25 GB) | | |
| ALL | 33.175 | (no --newcanon on the full set) | | |

Measured: ALL fills to depth 8 (3ax, 4op); hq23 --newcanon 3 to depth 9 (3ax); --distinguishall ALL to depth 8.

## --bigbatch (design only, not started)

### Design (2026-11-02)

Goal: big batches of independent lines (`-s`/`-si`, `--shortenseqs`) use every core with one shared pruning
table, while still using micro-threads, still extending the table, and printing results in input order, in
exactly today's format.

#### Why (measured)

3ax `-si` run, all moves, -M 40000, -t 16, lengths 11-12: 334 lines/min at ~108% CPU.  A `sample` profile shows
per line and per depth: `solve()` makes work chunks, spawns and joins threads; with d - initd <= 1 there is
one chunk (one thread works), above it the chunks are milliseconds of work and the time goes to chunk setup
(`slowmodm2` in `innersetup`, ~25%) and the global work lock (~25%).  One short search has too little work for
16 threads; the parallelism has to be across lines.

#### How twsearch works today (constraints)

- One solve at a time, process-wide state: `workchunks`, `workat`, `solutionsfound`/`solutionsneeded`,
  `lastsolution`, `globalinputmovecount`, `randomized`, `workinguthreading`, `solveworkers[]`; one
  `get_global_lock()` for work handout and solution printing.
- Micro-threads: each `solveworker` interleaves up to 16 `uthr` searches so table prefetches overlap; they
  need several chunks per OS thread.
- Output printed as it happens: "Solving" (solveit), the solution (possibsolution, under the lock),
  "Found ..."/"No solution found in N" (solve).
- `checkextend` (between depth iterations) refills and may "demote" (rewrite every entry, baseval++): a
  lookup that overlaps a demote or a baseval change decodes wrongly -> too-low pruning bound is fine, too-high
  is not; either way it must not happen during lookups.
- `makeworkchunks` has a static `seen` cache (not thread-safe); `duration()` uses a global timer.
- `--shortenseqs` (shorten.cpp): per line, many window solves through `setsolvecallback` (globals `srcsol`,
  `solseen`), a static window memo `fini`, a static prunetable; verbose progress lines per line.

#### Design: fork-based generations (chosen first)

The parent builds or loads the table (and extends it, e.g. `--startprunedepth`), then runs the input in
generations:

1. Read the next K input lines (a generation).
2. Fork N children (`--bigbatch N`).  The table is shared copy-on-write; each child is unmodified twsearch
   running the normal command (`-s`, `-si`, `--shortenseqs`) with `-t 1` and its micro-threads, on the lines
   it takes.  Line handout: a shared counter in an anonymous shared mapping (dynamic balance), or index mod N.
3. Each child writes each line's complete output into its own slot (per-line buffer: shared-memory ring or a
   per-child temp file with line index markers).  The parent prints line i only after lines 0..i-1, exactly
   as the serial mode would.
4. When all children have exited (the generation has drained), the parent adds up their lookup counts and
   runs `checkextend` with all threads; any fill or demote happens with no reader alive.  Then the next
   generation.

Children must never write the table: a hard read-only switch (no `checkextend`, no fill, no `addlookups`
into the shared struct; lookups counted locally and reported).  Generation size K trades fork overhead and
extension latency against drain stalls (the slowest line of a generation holds it); e.g. K = 64 x N lines,
or by time.

Why first: no refactor of the solver core; every child has its own globals, so `-si` and `--shortenseqs`
(callbacks, statics, memo) work unchanged.  Extension happens naturally at the drain.

#### Alternative: in-process contexts (later, if this goes upstream)

Move the per-solve globals into a `solvectx`; the existing mode uses one shared context, `--bigbatch` one per
OS thread (`numthreads` 1 per context, own micro-threads, per-context lock and output buffer, per-context
`makeworkchunks` scratch and timer).  Extension: a pause flag; workers park at a depth or line boundary; fill
with all threads; release.  `--shortenseqs` needs its callback globals and `fini` per context.  Cleaner and no
fork, but it touches the hot loop and every global.

#### Options

- `--bigbatch N`: N solving processes (or threads).  Valid with `-s`, `-si`, `--shortenseqs`; rejected with
  phase2, callbacks, `--randomstart`, `--alloptimal` (at first).
- Generation size (e.g. `--bigbatchgen K`), optional.

#### Risks

1. Extension while lookups run (silent: a missed shortening becomes a false "optimal").  Fork design: only
   between generations, no children alive.  Test: force an extension mid-input; compare with serial output.
2. Children writing the shared table (copy-on-write pages duplicate the 40 GB): read-only switch; assert.
3. fork with live threads: fork only after every fill/IO thread is joined; check on macOS and Linux.
4. Ordering and memory: per-line buffers; a hard line delays printing (not work) until its generation drains.
5. Drain stalls: one slow line holds a generation; keep generations large, print progress on stderr.
6. In-process variant only: hidden statics (`makeworkchunks` seen, `duration()`, `myrand`, shorten's
   `fini`/`srcsol`), lock contention through `get_global_lock`, regressions in the default mode.

#### Testing

- Same output (byte for byte, apart from timings) as serial `-si` and `--shortenseqs` on samples, including
  lines with improvements; `U U` -> `U2`.
- A forced extension mid-batch: no missed improvements versus serial.
- Throughput and CPU on 3ax lengths 11-12 (today 334 lines/min).

## Full-disk table write (2026-11-03)

writept opens the cache file with ios::trunc and calls error("! I/O error") if the write fails: on a full disk the old table file is destroyed and the process exits, losing the in-memory table too.  Our build twsi4 checks statvfs against the exact encoded size (bitcost / 8) plus 2 GB first and skips the write if it doesn't fit.  On this laptop, deleting files may not free space: APFS local Time Machine snapshots pin the blocks even under the /var/tmp exclusion.

## Fill bug: rotation dedup of fill chunks (found 2026-11-05)
On a tws with rotations, makeworkchunks (forpruning) drops a fill chunk whose state equals an earlier one's up to rotation, ignoring that the two chunks' canonical-sequence states differ. The canonical order is not rotation-equivariant (y' and y2 reverse the per-axis order), so some states are never filled and read too high. That makes the pruning inadmissible: about 1.1-1.3% of depth-5/6 states on 2ud-si go missing, and ~1% of -si searches miss solutions (21 in r3k13). Test and numbers: journal.md 11-05 17:55, /var/tmp/orbitsolve/twdedup. Workaround: no dedup in fills (~4.4x more fill work), or dedup only exact states in the same canonical state.
Fix tested (2026-11-05): in fills, chunks ignore the canonical state machine (mask 0 when forpruning) and each chunk's DFS starts from canonical state 0. The depth-6 2ud-si table matches the no-dedup fill except for 4 header bytes (a counter), at ~1.9x the buggy fill's work instead of 4.4x. Build: /var/tmp/orbitsolve/twdedup/nomask.

## Upstream fixes (2026-11-08, pushed by the user: de23f09, dafd8b0)

On GitHub main; ~/twsearch2 pulled to dafd8b0 and rebuilt (its uncommitted README edit kept). Test dirs twfix, twdedup, twsfresh deleted.
- de23f09 fill fix: fill chunks on puzzles with rotations carry no canonical state; fillstart starts from state 0 (also no more canonnext[-1] walk on non-canonical prefixes, which the 11-05 nomask build had).  Tables byte-identical to a no-dedup fill (except the fillcnt header field, bytes 66-69) on 2ud-si (t1/t4/t16), 1ud-si, obl8ax, obl8op, arecibo, gearcubeextreme, puck, trapentrix; 3x3x3 unchanged.  Work ~1.6x the buggy fill on ours, 1.1-1.2x on samples.  A seam rule (skip a next move of the last chunk move's class) gave identical tables for ~6% less work; left out.  Test: samples/test/fillrot.tws depth-5 fill must set 2,996,953 (buggy 2,962,722).
- dafd8b0 -si fix: parsedomovelist_generously returns the move count; processlines/2/5 set globalinputmovecount (lost in 268d1a3, 2026-05-15).  Test: -si "U U" finds U2.
- make test-cpp-samples and test-prune pass; test-cpp-cli needs the Rust workspace (not in this repo).
- Our working build twsi4 is a patched copy without the fill fix: rebuild tables only from a build with de23f09 (a twsi5 = main + --prefixslack + the statvfs write guard, when needed).
