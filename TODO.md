# TODO: remaining work on gullicube (formerly `full/fullsolve`)


## 0. TOP: twsearch fill bug (2026-11-05) -- fixed upstream 2026-11-08 (de23f09; -si fix dafd8b0).  Still to do: a working build twsi5 = main + --prefixslack + statvfs guard, and rebuild the tables we use (2ud-si, 1ud-si, obl8) with it
On a tws with rotations, the fill's work chunks drop rotation-duplicate prefixes while keeping their canonical
state, so ~1% of states are never filled and searches miss solutions (journal.md 11-05 17:55, twsearch.md).
Fix tested at depth 6: /var/tmp/orbitsolve/twdedup/nomask (chunks without the canonical state machine for
fills; the fill DFS below each chunk starts from canonical state 0).  To do:
- Confirm the fixed build finds the 21 missed r3k13 solutions (depth-7, -M 8000 table, ~30 min; or depth 8).
- Make it the working build (twsi5) and rebuild the tables we use (2ud-si 39G at depth 8, ~1.9x the old fill
  time; also 1ud-si, obl8 3ax/4op when they are used again).
- Upstream patch: only the rotation branch of makeworkchunks plus fillstart; the same dedup path in solves
  with --nodeduplicatesolve (which turns dedup on) needs the same treatment.
- Optionally re-search lines that twsearch reported "No solution" on, using the fixed tables (or fastsolve).

> **Renamed (2026-09-27):** the solver `fullsolve` is now **gullicube** (`dist/gullicube`,
> binary `./gullicube`); older text below that says fullsolve means the same program.
> Data files moved to `dist/gullicube/data/`; the solver reads `data/` beside the executable
> as named on the command line (`argv[0]`), else `./data` (no environment or HOME lookups;
> nothing but the echoed command line reaches stdout).  Run via PATH alone is not supported.

> **Layout (2026-09-26):** the solver's source of truth is `dist/gullicube`
> (source, `*.cls` data files, Makefile, README); edit and build it there.
> The former `full/` directory is now `devtools/`: data generators, tests,
> benchmarks and large intermediate pools, built against `dist`'s headers.
> Older sections below still say `full/` for files that now live in one of
> the two.


Status and background are in `progress.md`; the pipeline is described in
`full/README.md`.

## Done this round

- 2026-09-27: pair-beam anatomy and experiments (progress §7j).
  - Survivor lists in `TableBeam`: on, identical results, −16% cycles.
  - Depth-8 wing table: tested, not adopted.
  - Measured and dropped:
    - inverse pairing;
    - mask grouping;
    - short-algorithm prefixes;
    - width schedules;
    - a wide first level;
    - an exact disjoint opening seeding the beam (the testbed `seedbeam` and
      `hillclimb.py`).
  - GPU beam (Metal, `devtools/gpu`): identical to the CPU beam, 16× one
    thread at W 4096; 2-ply lookahead; bigger working pools show the
    algorithm set is the ceiling.

- 2026-09-26 (later): symmetry-class data files (≈33 MB total, expanded on
  load, parallel prefetch-batched finish tables). Mined wing pools, wing table
  beam, seam-aware costs (wings, pairs, diagonals, mids). One merge for
  pairs + diagonals + mids, C++ merge beam, bucketed greedy merge, no replay
  in quality mode, merge time budget. `-2` whole-cube by default. Frontier
  benchmark against brobdicube (progress §7h). gullicube runs from any
  directory. 2³/3³ verified.

- 2026-09-26: pair finish table (5–7% fewer pair moves). Diagonal/mid pools
  from the pair pools with a table beam + finish tables (diagonals −25%, mids
  −30%). N=4–10 now 75/122/199/275/381/484/617. CLI conventions applied.
  `-2` seeded tie-break, `--grow`/`--reps`/`--wingwidth`. Beam memory bounded.
  Heuristic study: colour-count heuristics don't help (progress §7b).

**Guaranteed fallback.** Every phase now always finishes:
- **Pairs, diagonals, mid-centres:** beam → retry with 4× wider beams (up to
  256, bigger pool for pairs) → 3-cycle decomposition from complete tables
  (`full/mkcycles`: `pair3.d.pool` 8,096, `diag3.pool` 4,048, `mid3.pool`
  4,048).
  - Each decomposition step strictly reduces the number of wrong positions,
    so it always terminates.
  - `--fallback-only` forces this path everywhere. It passes the independent
    check at N = 9, 10, 16 and 21, and runs at 256³ on the fast path.
- **Wings:** the parity fix plus the complete wing 3-cycle finisher, run from
  the root node, always produces a solution.
- **Corners:** brobdicube's IDA\* with pruning tables.

**`-2` (widths 1, 2, 4, …, keep the best, Ctrl-C returns it), plus a
restructured driver (`Res` loaded once, `solveOnce()` with local state).**

**Fast-mode speed work: 4K³ from 74 s to 10.1 s** (details in `progress.md`
§7).

**Fast mode (`--fast`).**
- Greedy width 1, 23k-algorithm pair pool, no merging.
- Pair moves are not replayed on the cube; diagonal side effects are applied
  as permutations.
- Quadratic time: 1K³ in 4.3 s, 2K³ in 17 s, 4K³ in 75 s (16 threads).
- About 1 pair in 5,000 needs the retry; none has needed the 3-cycle fallback.

## A. Make quality mode run at 1K³ and beyond (overnight runs)

The goal: given hours, the full solver should beat brobdicube well past 1K³,
where fast mode loses on moves (90 moves per pair against brobdicube's 41–50).

1. **Scalable merger.**
   - The greedy in `full/merge.h` rescans every available instance each step:
     roughly quadratic in the number of pairs, O(N⁴) overall, so it's
     unusable past about 200³.
   - Fix: index available instances by the axis and layers of their first
     group; only those can interact with the tail. Keep a heap of savings and
     rescore only after the tail changes. The target is near-linear in the
     number of instances.
   - Merge savings grow with the number of pairs (7% at 8, 17% at 512 and still
     rising). Measure how far they go at 10⁵–10⁶ pairs; that decides the 1K³+
     story.
2. **No O(N³) cube replay in quality mode.** Use the fast path's approach
   (apply only diagonal-flagged algorithms as permutations), but in *merged*
   order. The merger has to report that order.
3. **Memory and streaming at 8K³+.**
   - 4K³ has 2.1M pairs, which is fine in memory. 16K³ has 33M pairs, and 65K³
     has about 530M pairs plus a 25 GB centre grid.
   - So: solve, merge and emit in blocks of rows (as brobdicube does in
     strips), and write moves incrementally.
   - Decide how much merging across block boundaries is worth.
4. **Time budget / anytime operation.** An option such as `--time T` that picks
   beam widths and endgame depth to fit the budget. Or run fast mode first and
   then improve pairs (wider beams, depth-2 endgame) until time runs out,
   keeping the best solution per pair.
5. **Checkpoint and resume** for overnight runs: persist per-pair solutions
   and progress.
6. **Streamed output:** `-o` currently builds the whole move list in memory,
   which is hundreds of millions of moves at 4K³. Stream it instead.
7. **Independent verification at scale.** The sticker simulator is O(N²) per
   move and infeasible past about 100³. Options:
   - an orbit-level replayer that shares no code with brobdicube, i.e.
     permutation actions per orbit type with depth substitution;
   - or spot checks: replay a random sample of orbits through the whole
     sequence.
8. **Startup:** cache the endgame hash tables (`-E`, ~5 s to build) the way the
   pools are cached.

## B. Move-count improvements

1. **Merge beam in C++.** It exists only in Python (`pair/combine.py`) and is
   worth 1–2.5 moves per pair at small N.
2. **Coordinated wing sharing:** per-depth beams produce alternatives, then
   combinations are chosen to reuse commutators across depths (brobdicube's
   parallel trick, done deliberately). A naive joint beam was worse; see
   `progress.md` §5.
3. **Diagonal and mid solvers:** add an endgame and derating. They sit at about
   47–49 raw moves per orbit against about 29 per orbit for pairs.
4. **Shared application across pairs:** apply one algorithm to many pairs at
   once, as brobdicube's master sequences do. This is the main way to compete
   asymptotically. It needs algorithms that stay valid when their "3" slice is
   applied at several depths (orbit (j₁,j₂) sees two slices), so it means new
   generation and validation.
5. **Pair solver "try harder" knobs for overnight use:**
   - depth-2 endgame at every node (`psolve -D`);
   - the move-count-bucketed beam (`psolve -b`), which beat the level beam at
     equal width;
   - larger pools (p9) with support-mask candidate filtering.

- **Pair finish table (done 2026-09-26, see progress §7b).** Next:
  - cache the built table on disk (mmap) to cut the ~3.5 s startup;
  - p10/p11 finish states from `6centers.log.xz` (memory permitting);
  - diagonal/mid orbits: done (pair-pool collapse, table beam, finish tables);
  - diagonal/mid mining from twsearch output: done (`mine`); 11-movers
    skipped (23M canonical, too many images); gains now small;
  - default beam width could scale with N (small N is cheap: 4³ at -b 256 is
    well under a second);
  - wings: mined pools + wing table beam + finish table: done (progress §7c);
    next: (depth-8 pool tested, loses at equal time, progress §7h), faster scan for big pools, cross-depth sharing, wing pairs;
  - startup (~2 s): cache eff4 per pool; fbSolver recomputes the pair pool's costs;
  - two-algorithm finishes for small m.

## B′. Fast-mode speed (next ideas)

1. **Fewer survivors:** survivor scoring is now the cost, about 38 µs of the
   62 µs per pair, for about 1,200 survivors per level. Try a positive filter:
   require at least one movement that brings the right colour into a wrong
   position.
2. **Outside the beam at 4K³ (about 1–1.5 s):**
   - build the random cube in parallel (`xcube 'R'` is single-threaded);
   - read the pair orbits in parallel;
   - allocate the 2M pair jobs up front.
3. **`-2` per orbit, not per run.** A wider beam is not a superset of a
   narrower one, so results are not monotone in width. Orbits are
   independent, so keeping each orbit's best solution across widths is never
   worse than any single width.
   - The pair states depend on the wing solution (wing moves scramble the
     centres), so the wing solution must be frozen across iterations: solve
     wings (or take their best) once, then iterate only the centre phases.
     The same applies between pairs and diagonals, since pairs have
     side effects on the diagonal orbits.
4. **Remove the comparison switches** once settled: `--topk`, `--index`,
   `--strictkeep`, `--usage`, and the `BEAMTIME` / `NOINCR` / `NOAOS`
   environment toggles.

## C. Robustness and cleanup

1. **Tests:** a regression script covering:
   - odd and even N, `--fast`, `--fallback-only`, `-E`;
   - independent verification at small N;
   - the layout tests (`layouttest`, `diaglayouttest`).

   Currently every check is run by hand.
2. **Small cubes (done):** `gullicube` accepts every N ≥ 2. Verified with the
   independent sticker check for N = 2–7:
   - 30 seeds per size in each of `-b 4`, `--fast`, `--fast --fallback-only`
     and `-b 4 -E`;
   - the no-replay fast path via `-o` + `brobcheck` at N = 2–9, 12 and 16, 20
     scrambles each.

   A formal per-size re-validation of the pools (isolation on the actual small
   cube) was not done; the end-to-end checks passed everywhere.
3. **Docs:** update `full/README.md` for `--fast`, `--fallback-only`, the
   3-cycle tables and the fast-path design. Mention in the usage notes that
   fast mode relies on `pair/fast6.d.txt`.
4. **Pool provenance:** document the generation commands for every pool file
   (`fast6.d.txt`, `p8s10.d.txt`, `eg8.d.txt`, `diag/mid/wing.pool`,
   `*3.pool`) in one place, and make regeneration a single script.
5. **Portability:** `poolcache.h` uses `st_mtimespec` (BSD/macOS); Linux needs
   `st_mtim`. The Makefile is macOS-centric (`ld -r`).

## D. Integration into brobdicube

1. **Cost model:** decide how bespoke solving enters `bestcost`, per the round-1
   plan. At small N the full solver already wins outright (about 2.4× fewer
   moves at 20³–40³).
2. **Data structures:** `gullicube` already runs on `xcube`. It needs
   brobdicube's move output conventions and its streaming path for large N.
3. **SIMD:** assess whether it's worth it in brobdicube (the NEON pair scan in
   `pair/fast.cpp` was about 4× faster than scalar).
4. **Upstream fixes:** the brobdicube issues and patch are in `brob-bugs.md`
   and `brob-fix.patch`; `gullicube` currently works around them.

## B‴. Merge phase (after seam-aware costs, progress §7e)

- one merge for pairs + diagonals + mids: done (§7f).
- C++ merge beam: done (§7f). Next: a faster beam for large merges
  (incremental candidate costs; only instances whose first axis matches the
  tail can save anything).
- choosing among near-equal per-pair solutions for mergeability.

## B″. Conjugate opportunities: smaller tables, faster scans

- Many pool entries are face-turn conjugates X A X' of cheaper entries
  (written form, a lower bound): `c12x` 75%, `wing_d6c` 74%, `mid2` 55%,
  `p8s10` 31%, `diag2` 30%, `diag_x10` 10%, `mid_x10` 19%.
- A beam with explicit setup moves (the pair beam's derated setup faces, or
  the old wing beam's setup stack) could scan a small core pool instead:
  roughly 5–10× less for wings.
- A finish table could store only unconjugated cores and try the 18 face-turn
  conjugations of a probe state.
- The semantic fraction (permutation = conjugate of a cheaper pool
  algorithm) is not measured yet.

## D′. Distribution

- **Done (2026-09-26):** `dist/gullicube`, 29 MB.
  - Contents: the solver source (`src/full`), four `pair/` headers, brobdicube's
    xcube/corner/wing/util sources unmodified (`src/brob`), 14 `.cls` data
    files, a portable `Makefile` and a `README.md`.
  - All runtime data is class files expanded in `gullicube`: the 3-cycle
    fallback tables and `wing3.cls` too, and `-E` now reads `eg8/p9/c12x.cls`.
    No separate tools or caches are needed.
  - Built from a fresh copy and verified (`--verify`) in 13 configurations
    from 2³ to 64³.
  - No user paths in the files or the binary.
- **Open:**
  - `dist/gullicube` is now the source of truth, and `full/` became `devtools/`,
    so there is nothing to keep in sync;
  - ~~licence~~: done 2026-09-27, dual MPL-2.0 OR GPL-3.0-or-later, as twsearch and cubing.js
    (`LICENSE-MPL.md`, `LICENSE-GPL.md`, README section);
  - Linux build test.

- **Done:** runtime data is symmetry-class files (≈33 MB), expanded on load;
  about 3 GB of memory.
- **Open:**
  - a `regen.sh` from `6centers.log.xz` + `conj12` + `wingbase.txt` (+ the
    wing twsearch run) to rebuild the `.cls` files;
  - drop the now-unused multi-GB pool texts and `.bin`s from the working tree
    once regeneration is scripted;
  - the legacy `-E` endgame still reads `../pair/*.d.txt`.

## F. Input and small fixes

- ~~Read the Kociemba facelet format~~: done 2026-09-27.
  - `src/brob/facelets.{h,cpp}` (brobdicube style, xcube only, meant for
    brobdicube too): `xcube_to_facelets`, `xcube_from_facelets`.
  - gullicube `-f` (read from stdin), `-F` (print the start state); `--verify`
    with `-f` loads the facelets into the simulator (`Cube::kociemba`).
  - Checks: length and letters, 4 of each colour per centre orbit, every
    corner / middle edge / wing once, centre centres an orientation, corner
    twist, middle-edge flip, and on odd cubes corner ⊕ edge ⊕ centre-centre
    parity.
  - Tested by `devtools/facelettest`: hand-checked 3³ U and R, and the xcube
    renderer equals the simulator's geometric mapping on random scrambles,
    N = 2–12.
- **Load only what a size needs:** 2³/3³ load every table (4–5 s) though they
  use only the corner solver; even N skips mid tables already.
- **Merge budget floor ∝ effort:** the fixed 1 s floor flattens quality
  beam 1–16 at 16³–32³ (all about 1 s).
- ~~The paused depth-8 wing twsearch~~: finished; `wing_d8.cls` tested and
  not adopted (a wider beam on `wing_d6c` wins at equal time, progress §7h).
- **Long-budget frontier points** (running): 512³–2048³ at beam 64/256/1024.

## E′. dwalton76 on the frontier plot

- The user has dwalton76's solver set up on this machine: Python plus C++,
  run in separate parts, probably single-threaded, and slow.
- Add real (moves, time) points for it at 4³/8³ (and wherever it runs), in
  place of the moves-only reference line. Its moves are outer-block turns:
  recount in our metric too (`full/tools/metrics.py`).

## E. Benchmark script (user request, 2026-09-26) — done

`bench/collect.py` + `bench/frontier.py` (progress §7h). Still open from the
original request: a multi-solve option in gullicube (brobdicube's `-M n`), so
small sizes don't pay startup per run.

A script that produces statistically valid move counts and execution times
for the current code:
- **Grid:** random positions, sizes N ∈ {4, 5, 6, 7, 8, 9, 10, 16, 32, 64,
  128, 256, 512, 1024, 2048} × power-of-two beam widths.
- **Order:** run the grid in increasing expected execution time, so the cheap
  cells finish first. Large-N × large-width cells may never be reached;
  that's fine.
- **Samples:** many at small N, where the variance is high; one or a few at
  large N.
- **Report:** mean ± standard error (and n) for moves, plus wall and CPU time
  per solve. Keep the raw per-solve records (N, width, seed, moves split by
  phase, times) so tables can be recomputed.
- **Startup:** at small N, startup (pool and finish-table load, ~2–3 s)
  dominates. So add a multi-solve option to gullicube, like brobdicube's
  (`-M n` or similar: do n random solves in one process with seeds s, s+1, …,
  printing one line per solve plus a summary), and time the solves apart from
  the load.
- **Fixed conditions:** record thread count and machine load with each run
  (the machine is shared); keep the run seeds so any cell can be reproduced.

## G. Pair beam: next steps from the 2026-09-27 study (progress §7j)

- **Working pool `p9` ≤ 8 moves (979k)** instead of `p8s10` (408k) in the CPU
  solver.
  - On the GPU it lowers pair moves by 0.5–0.9 at every setting.
  - Test it at equal CPU time (the scan cost grows about 2.4×); then the
    9-movers (full `p9`, 7.47M: another ~1 move, 7× the cost again).
  - Needs a length filter when expanding `p9.cls`: `devtools/gpu/gpucmp.mm`
    has one (`-m`).
- **Level 1 scores only the start's top 4k** (`rootTop` 4000, `rootLevels` 1):
  same moves at ~45% of the instructions. Decide whether it becomes the
  default in `gullicube`, after whole-cube checks.
- ~~Clean `dist`~~: decided 2026-09-27: the off-by-default experiment hooks stay in
  `solvers.h` (they do no harm); survivor lists stay on.
- **Remaining CPU per-node costs** to cut:
  - rebuilds when a correct piece shuffles within its face (22% of nodes);
  - finish-table probes on admission (~12%, cache misses);
  - the table rescans for the weakest slot (3.6M per 100 states at W 256).
- **GPU stage 2** (only if the GPU path is pursued):
  - survivor lists, duplicate removal and finish probes on the GPU;
  - batching many pair problems per launch;
  - the host side (sort, admission) is now more than half the time.
- **Bigger pools with the GPU:** `eg8`'s 10–11-movers as working algorithms.
  Also use the GPU as a fast labeller for heuristic work (width 16k in about
  1.6 s per state).

## H. Pairwise merge (progress §7k)

- `--pairmerge` is opt-in; decide on the default after more seeds (it wins
  from about 10³ up; 8³ is a tie).
- Large cubes: the merge time grows about as n² (256³: 27 s unbudgeted vs
  4.4 s greedy), so under the budget most of its 4.5% gain is lost.  Next:
  cheaper refusals (most searches end in a refusal), shorter neighbour lists
  for long pieces, or splitting the merge by layer ranges.
- Insert the diagonal / mid algorithms as a second phase (the user's plan)
  instead of merging them with the pairs under all-positions constraints.
- 1024³: building the levels (each level merged pairwise, ~126k instances
  per level) takes ~21 s and ignores the budget; improvement never runs.
  Budget the per-level merges, or a cheaper merge inside levels at that size.
- Flexible levels; insert the diagonals as a second phase.
- ~~Beam time cliff on small cubes~~: fixed 2026-10-03 (wing beam cycling;
  progress §7l).

## I. Performance and memory: worth investigating (2026-10-03)

Measured state (main, after the memory and prefetch work): peak memory about
1.0 GB at small widths, about 1.5 GB at the widest benchmark beams (was up to
6.4 GB).  Per-phase counters: `make clean; make DEFS=-DGULLI_PHASESTAT=1`,
run with `PHASESTAT=1`.  Wing beams run at IPC 4.7-5.1 on 12-14 threads; pair
and diagonal/mid beams at IPC 2.1-2.8.

**Pair / diagonal / mid beam throughput** (IPC ~2.5; with 16 threads mostly
memory-bandwidth bound):
- Pack the 32-byte per-algorithm record (three 48-bit colour planes + cost)
  into 24 bytes: 25% less data per scored algorithm.
- Level 1 scores only the start's best 4k algorithms (`rootTop`, already a
  hook, off): ~half the instructions at equal moves in the 2026-09-27 test.
- Smaller beam nodes (full state + colour masks, ~150 bytes, copied per level).
- Done: record prefetch in both survivor paths and deferred finish lookups
  (single thread -9% cycles; 16 threads -2..-8% pair solve, 0 at 512^3).
- Tried, slower: folding the seam-group index into the record.

**Thread use:**
- Small cubes: pair and diagonal beams use few threads (8^3: 3 pair orbits,
  2.9 threads busy).  Split beam levels across threads as done for wings.
- Load: 1.9 s on ~3 threads (pool expansion, beam setup); parallelize.
- Merges at large N run on one thread (512^3: 6 s): levels are independent
  (parallel per level); `std::find` on the refused-seam lists shows up in the
  profile (use a hash set or bitset).

**Memory, further:**
- Very wide wing beams use a lot of memory (2026-10-04 benchmark): 16^3 at
  -b 32768 peaked at 8.43 GB, 8^3 at -b 65536 4.59 GB, 4^3 at -b 262144
  5.68 GB (wing beam width = 4 x -b).  Likely the wing beams' node arrays
  (every node of every level is kept) and the per-thread level tables, with
  several orbits' beams at once.  Bound them (e.g. keep only what the path
  reconstruction needs, or cap the width) before wide -2 runs are routine.
- Algorithm pools (~160 MB pair + ~250 MB diagonal/mid/wing): text strings and
  per-algorithm vectors kept even when unused.
- Beam setup structures (~145 MB).
- 8-way symmetry (add a reflection) for the finish tables: another 2x, at 7
  extra keys per child instead of 3.
- Finish-table build: the sizing pass recomputes every key (half the build);
  size from class counts instead, growing in the rare overflow.

**WASM** (target: wasm32, single-threaded build plus a pthreads build for
cross-origin-isolated pages; 4 GB address limit, ~1-2 GB on phones):
- Replace `unsigned __int128` multiply-shifts with 64-bit arithmetic.
- emcc location: see ~/filelist.txt.

(J and K, orbit-bounds items, moved to upperbound5.md / twsearch.md on 2026-11-03.)

## L. Boundary passes on the GPU (orbit upper bounds, 2026-11-04)

Goal: move the boundary inner loop to the M3 Max GPU (Metal) and get about one more boundary level on each
side.  Targets:
- UD 33: ~190x the UD 34 work; CPU ~5-6 h boundx + ~1 day boundcheck.
- UD 32: ~100x more again.
- 3ax/4op 40.

Background: upperbound5.md, methods.md ("Boundary passes"), arena-plan.md.

- **Core loop.**  Per top A class with line L0 and alternatives P (its block), for each bad B state s:
  - M = P o L0^-1 (precomputed per alternative);
  - t = M(s): move the marked pieces;
  - rank t, then look up dB(t);
  - covered when |P| + dB(t) <= X.
  That is ~50 integer ops and one byte gather per check, millions of independent checks per class.
  Tables: 2ud dB 23.5 MB, 4op 24 MB, 1ud dA 113 MB.
- **Alternative search** (each class through ~980k actions, looked up in dA): the same shape, a second
  kernel.
- **First step.**  A Metal kernel for "every bad B state of this class is covered by some alternative
  in its block", run on the UD 34 data:
  - frozen stages bcert/ud34/snap (1ud = none..aUbU, 2ud = aUbU..aDbD);
  - blocks bcert/ud34/ud34.txt (boundx --out);
  - the CPU reference: 5,294 classes, 12,174,971,792 bad colourings, 0 left, 304 s on 5 threads
    (bcert/ud34/ud34.err).
  Compare counts and speed.  Then test on deliberately broken blocks: drop an alternative, drop a
  block; each must FAIL.
- **Progress (11-04).**  First step done: devtools/gpu/gpubound (boundcheck, check 4 on the GPU) matches
  boundcheck on UD 34 exactly (25,532,454,577 tries, PASS; check 4 in 1.4 s) and FAILs the broken blocks
  (journal 11-04 09:12).  devtools/gpu/gpuboundx (boundx: action search + colouring loop on the GPU)
  matches boundx on the UD 34 1/50 sample; its blocks pass gpubound.  Next: full UD 34 through gpuboundx +
  gpubound, then UD 33 (sample first).
- **Code.**  devtools/gpu (existing Metal harness: gpucmp.mm, scorebench.mm, gpubeam.h);
  devtools/boundx.cpp and boundcheck.cpp (CPU reference); axbx.cpp and axbcheck.cpp (the 3ax/4op
  versions).
- **Correctness.**  The GPU tool is a third implementation.  The certificate check stays the CPU
  boundcheck/axbcheck until a GPU checker matches it on samples.  Root-cause any mismatch before
  going on.
- **Shared-machine rules** (another session runs the 2ud tail loop on 15 cores, holding the 40 GB
  table /ud2si):
  - GPU plus at most 1-2 CPU threads;
  - never touch /ud2si or the opt2ud, opt1ud, bcert/ud34/snap files;
  - no new 40 GB jobs;
  - notes go in journal.md (append only) and twsearch.md / methods.md;
  - large temp files go in /var/tmp/orbitsolve/gpubnd;
  - disk is tight (~14 GB free; Time Machine snapshots pin deletions for ~24 h).

## M. obsolve speed-ups (oblique solver from the small certificate, 2026-11-04)

devtools/obsolve works (N=40 --bound: 171 pairs, worst 72, all checks pass).  Time is start-up: table
rebuild 232 s (8 threads), actions 28 s, then ~6 ms per pair.  Overall O(N^3): ~N^2/8 pairs x ~70
moves x O(N) per slice turn (xcube walks a row and a column); per-pair lookups and searches are O(1).
- **Rebuild over classes, not states** (8-16x less work), all 16 cores, faster rank/unrank
  (target 20-30 s).
- **Actions:** UD actions up to 8 moves as the GPU runs used (~1M, not 9.45M at prop 10), or build
  them lazily the first time a pair needs an alternative (rare: 4 of 171 at N=40, all 3ax).
- **Moves at very large N:** gullicube's CentreReplay (row-by-row batched replay, parallel) for the
  O(N^3) part; only matters for N in the hundreds.
- Pairs stay sequential (each pair's state depends on the earlier pairs' moves).
- **Independent end check:** the final "every oblique solved" check reads the same xcube that played
  the moves.  Add a --verify that replays scramble + solution on certcube.h's facelet cube (as
  gullicube --verify does with its own sticker simulator) and checks every oblique facelet.
