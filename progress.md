# Progress: session of 2026-09-25 (handover)

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


This session took the round-2 pair-orbit algorithms (see `README.md`) from raw
data to:

1. **A fast, tunable solver for one oblique centre-orbit pair** (`pair/`).
2. **A merger** that combines many independent orbit solutions into one move
   sequence, cancelling at the seams.
3. **A standalone full N×N×N solver** built on brobdicube's `xcube`
   (`full/fullsolve`): corners, wings, all centre orbits. Every result is
   verified on an independent sticker simulator.

## Conventions

- **Move metric:** one move per single-layer turn; half turns count 1.
- **Pair move counts** are always the **total for both orbits** of a pair (48
  pieces), on uniformly random states.
- **"Effective" or "merged" moves** are counts after cross-orbit merging.
- **Times are single-core CPU** unless stated otherwise.
- **The pair-solver layout** (`pair/cube.h`, `pair/pool.h`) is 48 positions:
  0–23 orbit A, then 24–47 orbit B. Position p has target colour (p % 24) / 4,
  in U L F R B D order. Algorithms are written on the (2,3)/(3,2) pair of an
  8×8×8; for pair (i,j), "2" → slice i and "3" → slice j.

---

## 1. Pair-orbit algorithm pools (`pair/`)

- **`pair/cube.h` + `pair/gen.cpp`:**
  - An independent 3-D sticker simulator for any n.
  - `gen` reads twsearch2 output (`../6centers.log.xz`, 26.5M canonical
    sequences up to 11 moves) and checks each canonical sequence on an
    8×8×8. It must restore everything except the pair and the diagonal
    orbits.
  - It then expands each 96 ways (48 symmetries × inverse) and keeps the
    shortest sequence per 48-position permutation.
  - The counts match the README exactly (432 / 864 / 15,090 at lengths
    4 / 5 / 6).
- **Pools** (format `LEN s0..s47 moves`; after the algorithm, position d holds
  what was at s_d):

  | pool | contents | size |
  |---|---|---|
  | `p6` … `p9` | all pair permutations of length ≤ 6 … ≤ 9 | 16k / 95k / 979k / 7.47M |
  | `c12x` | round-1 `conj12` single-orbit algorithms, both orbits | 194k |
  | `single8` | single-orbit 8-movers | 6.6k |
  | **`p8s6`** | p8 algorithms moving ≤ 6 pieces | **121k** |
  | **`p8s10`** | p8 algorithms moving ≤ 10 pieces | **408k** |
  | `s8` | every permutation moving ≤ 8 pieces, from the full ≤ 11-move data (built in 16 chunks) | 9.0M |
  | `eg8` | s8 plus the c12x algorithms moving ≤ 8 pieces | 9.2M |

  `*.d.txt` versions carry a trailing ` D0`/` D1` flag (see §6).
- **Structure found:**
  - At ≤ 5 moves every A 3-cycle has exactly one B partner, around the
    antipodal corner.
  - B gets real freedom only at 8 moves (about 28 partners per A 3-cycle).
  - Every single-orbit 3-cycle exists somewhere in the data.
- **Useful algorithms are the precise ones.** 8-movers that move 6–10 pieces
  beat the 16–24-piece 8-movers. p8s6 performs about as well as the full
  1.17M pool at a tenth of the size.
- **Composition of real solutions:**
  - About 8–10 algorithms per pair, 57–69% of them 8-movers.
  - Of those 8-movers, **66–87% are really 4- or 6-move cores with face-turn
    setups**.
  - Bare 4- and 5-movers are rarely used (1.5–5%); their rigid A/B coupling
    rarely fits a scramble.

## 2. Pair solver

**Scalar experiments** (`pair/psolve.cpp`):
- A level beam (level = one more algorithm, ranked by g + λ·misplaced).
- A move-count-bucketed beam (`-b`), which is better at equal width.
- An optional finish table (`-F`).
- The endgame re-optimization (`-E`), described below.
- Seam cancellation between algorithms (default on, `-x` disables). It was
  dropped from the metric at the user's request; the whole-cube merger
  replaces it.

**Fast solver** (`pair/fast.cpp`, NEON):
- The pool is stored as six per-colour 64-bit source masks, so an algorithm's
  score is `correct_after = Σ popcount(S_c & M_c)`.
- A NEON scan scores 16 algorithms per iteration, with a `vpaddq` fold and u16
  scores. The scan costs about 0.9 ns per algorithm.
- A level beam of width W, with the endgame below at every node with ≤ 8
  wrong pieces.
- Merge-aware cost weights (`-ws/-wf/-w2`, in quarter moves).
- `-o` writes solutions as lists of algorithms, for merging experiments.

**The endgame** (`pair/eg2.h`, the biggest single win):
- **Finish table:** a Zobrist hash of every state one algorithm solves, from
  eg8 + p9 + c12x: 16.9M algorithms giving 12.1M states.
- **Search:** enumerate algorithms whose support lies inside the wrong set
  plus at most one correct piece. They're found by looking up support masks
  of subsets, since a single swap needs a borrowed correct piece. Each child
  is looked up in the finish table.
- **Depth 2:** keep only the top-K children (K = 20–30).
- **Result:** the last ten pieces drop from 27.9 to 24.1 moves in 17 ms.

**Single-pair frontier** (raw moves per pair, before merging):

| time | moves |
|---|---|
| 1.4 ms | 76.2 (greedy, p8s6) |
| 5.6 ms | 70.7 (W4) |
| 11 ms | 68.9 (W8) |
| 49 ms | 64.3 (W16, p8s10) |
| 1.3 s | 60.8 |
| 6.5 s | 59.8 |

For comparison, round 1 needed about 107.6 moves per pair.

**Tried and rejected** (details in `pair/strategy.md` §3):
- fixed solve orders (weak or strict);
- a cheap-first greedy pass over the 4–6-movers;
- an orbit-imbalance penalty;
- setup-conjugated pool copies;
- a brobdicube-style colour-at-a-time reduction chain (about 100 moves);
- reordering setups at merge time.

The "cheapest last ten" hill-climb found real structure (opposite faces or
antipodal corner triples, about 12 moves against about 25 for a random set),
but forcing the solve to finish on those sets didn't pay.

## 3. Merging many pair solutions (`pair/combine.py`)

- **Legality:** algorithms of different pairs act on disjoint orbits (ignoring
  diagonals), so any interleaving is valid.
- **Dependency lattice:** within a pair, algorithm j stays after algorithm i
  only if their **moved-piece bitmaps overlap**. This matches exact
  commutation (3,444 vs 3,445 commuting pairs). Solutions are far from
  linear: 68% of algorithm pairs commute, and the median solution has 720
  valid orderings.
- **Model:**
  - Every move maps to a physical layer; pair (i,j)'s "2"/"3" slices become
    layers i and j.
  - Same-axis layers commute and merge.
  - Greedy: append whichever available algorithm adds the fewest merged moves.
  - The merge beam (`--mbeam B`) gains another 1–2.5 moves per pair at small N.
- **What cancels:** at 512 pairs, 67% of setup face turns disappear, against
  about 20% of other face turns and 2-slices and about 1% of 3-slices.
- **Hence derating:** the beam charges setup face turns at 0.25–0.5 of a move.
  Each pair's raw solution gets longer, but the merged total falls.

**Goal set by the user: ≤ 57 effective moves per pair** (single core, whole
cube):

| cube | pairs | ≤ 57 | ≤ 56 | ≤ 55 | best within 60 s |
|---|---|---|---|---|---|
| 10³ | 6 | 3.3 s | 9 s | 23 s | 54.8 |
| 20³ | 36 | 4 s | 8 s | 18 s | 52.8 |
| 40³ | 171 | 1.3 s | ~8–17 s | 17 s | 53.3 (≈52.6 with a merge beam) |

Sweep scripts: `pair/goal.sh`, `frontier.sh`, `fmerge.sh`; results in the
matching `.txt` files. `pair/strategy.md` documents the pair pipeline in
depth.

## 4. Full solver (`full/`)

Portable C++20 with no SIMD intrinsics. It links brobdicube's `xcube.cpp`,
`cornersolver.cpp`, `wingsolver.cpp` and `util.cpp` unmodified from
`~/brob-private/src`; `brobglue.cpp` supplies the globals they expect.

**Pipeline** (`full/fullsolve.cpp`):
1. **Corners:** brobdicube's `solvecorners`. On odd cubes, the centre centres
   are oriented first from `xcube::ccs`.
2. **Wings, centres free** (`wings.h`). Details in §5.
3. **Oblique pairs:**
   - Read each pair from the xcube with `layout.h`, which maps the pair-solver
     positions onto xorbits (a,b)/(b,a) with depth substitution 2 → a+1,
     3 → b+1.
   - Solve all pairs in parallel (`solvers.h` ColourSolver, pool
     `p8s10.d`, optional `-E` endgame).
   - Merge with `merge.h` (a C++ port of the greedy lattice merger on
     physical layers), apply, and check.
4. **Diagonals and mid-centres:**
   - `diag.pool` is the round-1 single-orbit set with 3 → 2 (156k permutations).
   - `mid.pool` maps one slice to the middle layer (221k permutations; both
     substitutions are valid).
   - Each orbit is solved independently, then all are merged and applied.

**Command line** (brobdicube-compatible where the options overlap):
`gullicube [-i | -r n | -R] [-o] [-b WIDTH] [-E] [-t THREADS] [--seed S] [--setupcost Q] [--verify] N`

- `-b` is one beam width for all phases; the wing beams use 4 × `-b`.
- `--verify` replays scramble + solution on `pair/cube.h`.
- `-E` needs the `*.bin` pool caches (`poolcache.h`): about 30 s the first
  time, about 6 s afterwards.

**Validation:**
- The layout mapping was checked on 3,003 (algorithm, N, pair) cases, with
  nothing else disturbed (`layouttest`).
- Every pool algorithm was validated by simulation (`mkpools`, `genwing`).
- Full solves pass the independent sticker check at N = 7, 9, 10, 11, 15, 20,
  21, 25, 40 and 41, over many seeds.

**Totals** (`-b 16 -E`, independently verified):

| cube | this solver | brobdicube |
|---|---|---|
| 20³ | **2,862** | 7,079 |
| 40³ | **10,852** | 23,373 |

With the earlier wings-last pipeline, 100³ was 65,128 against brobdicube's
106,576 (pairs merged to 50.6 per pair).

**Implementation note:** `gullicube` never calls
`xcube::normalizefacerots()`. It reads orbits in `xcube::get()`'s convention
(`layout.h: readOrbit`); see `brob-bugs.md` for why.

## 5. Wings

- **First attempt:** centre-preserving 3-cycles (brobdicube's 87 commutators ×
  48 symmetries × inverse = all 4,048, `wing.pool`), merged with the diagonals.
  About 88 moves per orbit. The commutator search `genwing` confirms that
  centre-preserving commutators on one depth give only 3-cycles.
- **Current: wings first, centres free** (the user chose this over sharing
  3-cycles):
  - **Moves:** slice turns at wing depths are free. Face turns are used only
    as setups on a stack and undone by their exact inverse, so corners and
    middle edges are preserved and no 3×3 re-solve is needed.
  - **Search** in the virtual frame (wings as if every setup were closed).
    Each action is one of: push or pop a setup; a single slice turn; a
    `[slice, face]` commutator (4 moves; always a 5-cycle or a double 3-cycle
    on wings).
  - **Parity:** fixed up front, one slice quarter turn per odd orbit.
  - **Finisher:** as the user suggested, every beam node is also completed
    with brobdicube's 3-cycles, and the best complete solution is kept.
    Relaxing the centres gives no cheaper 3-cycles (`genwing --freecentres`),
    so the brobdicube set is the right finisher.
  - **One beam per wing depth,** in parallel. A joint beam over several
    depths, applying each commutator at every depth it helps, was slower and
    worse even at scaled width. It doesn't coordinate shared commutators, so
    that result is a search deficiency rather than evidence that sharing
    can't help.
  - **Merge:** each depth's solution is split into closed blocks and merged
    across depths (about 1.5% saved).
- **Result** (moves per wing orbit, time on 12 threads):

  | cube | this | brobdicube `wingmoves` |
  |---|---|---|
  | 10³ | 42.4 (0.04 s) | 103.5 |
  | 20³ | 43.4 | 98.3 |
  | 40³ | 42.6 | 96.9 |
  | 80³ | 42.8 (0.14 s) | 93.4 |
  | 160³ | 42.1 (0.25 s) | 91.5 |

  Moves are linear in N (about 45% of brobdicube) and so is time. Test harness:
  `full/wingtest N width trials λ finishBelow maxStack chunk threads merge`.

## 6. Diagonal orbits are not preserved by pair algorithms

- **Why:** generation ignored the diagonal orbits (i,i)/(j,j).
- **How many move them** (`pair/diagflag`):
  - 0% of the 4- and 5-movers and of c12x;
  - 41–61% of the 6–9-movers;
  - 8.9% of p8s6, 32.8% of p8s10, 29% of eg8;
  - in real solutions, about 0.5–1.3 algorithms per pair.
- **Flag:** `Pool::diag` carries it (from the ` D0`/` D1` column), and
  `fast -o` marks those algorithms with `*`.
- **In `gullicube`:** the full solve applies the whole merged sequence to the
  cube, so the diagonals are simply solved afterwards from the resulting state.
- **Documented in:** the round-2 section of `README.md`.

## 7. Speed: fast mode, guaranteed completion, `-2`

**Guaranteed completion.** Pairs, diagonals and mids go beam → retries with
4× wider beams → a 3-cycle decomposition from complete tables
(`full/mkcycles`: pair 8,096; diagonal 4,048, with 48 cycles filled by
face-turn conjugation; mid 4,048).
- Each decomposition step strictly reduces the number of wrong positions, so
  it always terminates.
- Wings are guaranteed by the parity fix plus the complete wing 3-cycle
  finisher.
- `--fallback-only` exercises the fallback path; it passes the independent
  check.

**Fast mode (`--fast`).** Width-1 pair beam on the 23k pool, no merging, no
full-cube replay. The design is in `full/README.md` ("Fast path").

**Speed work on 4K³, from 74 s to 10 s wall.** The user drove most of these:

| change | effect on the pair phase |
|---|---|
| direct-mapped table beam (no top-k, sort or hash set) | (structural) |
| bit-sliced colour planes, one popcount per algorithm | 1.7× |
| keep-solved filter: never carry a solved piece off its face (user's idea) | ~95% of the pool rejected |
| that filter as bitsets over algorithms, built incrementally | 22.7 → 6.2 µs per pair |

Elsewhere in the pipeline:
- The row-parallel centre replay of wing moves took wings from 3.3 to 0.4 s.
- Skipping the full-cube replay of diagonal and mid moves took that phase
  from 2.0 to 0.1 s.
- Per-depth wing sequence checks replaced the full-cube check.

**What didn't help:**
- the movement index (scattered walks lose to a straight scan);
- a pool pruned by usage (at equal time it loses to the full pool with the
  filter);
- packing each algorithm's data into one record (scoring isn't memory-bound);
- a stricter filter that never moves a solved piece at all (the beam
  dead-ends and the slow retry takes over).

**Fastest (width 1) against brobdicube** (whole-process wall time; ours on 16
threads, brobdicube effectively single-threaded):

| N | ours: moves | ours: time | brobdicube: moves | brobdicube: time |
|---|---|---|---|---|
| 8 | 597 | 0.34 s | 1,096 | 11.65 s |
| 64 | 45,191 | 0.36 s | 50,495 | 2.13 s |
| 256 | 736,629 | 0.60 s | 540,762 | 3.11 s |
| 1K | 11.8M | 4.39 s | 6.64M | 7.78 s |
| 4K | 189.6M | 74.1 s | 86.8M | 33.7 s |

These are from before the speed work. After it, 4K³ takes 10.1 s (196.2M
moves) and 1K³ takes about 1 s. brobdicube wins on moves from 128³ up, since
its per-pair cost falls as n/lg n; ours is flat at about 90–94 per pair in fast
mode.

**`-2`.**
- Solves at widths 1, 2, 4, … from a copy of the scrambled cube (`-b` caps
  the width).
- Prints each width's moves (edges / corners + middle edges / centres) and
  time, and keeps the best.
- Ctrl-C returns the best found so far.
- The driver was restructured for this: loaded resources live in `Res` and
  are read-only; each `solveOnce()` keeps all of its state local.

**Timing caution:** the machine is shared. One 4K³ run took 23 s at load
average 26 with unchanged CPU time. Always check the load before comparing
wall times.

**`-2` refinements and verdict (2026-09-25/26):**
- Phase A (corners + wings) runs once at `--wingwidth` (default 16). Then per
  width (growth `--grow`, capped by `-b`) and per rep (`--reps`), only the
  pairs are re-solved. The best solution per pair across all runs is kept,
  and diagonals/mids are finished on the combined set.
- Every run gets its own seed (`slotSeed`, user: "vary seeds for each and
  every run always", otherwise "dupes dupes dupes"). The seed changes both the
  slot hash and a seeded tie-break between equal scores (`tieKey`: keep the
  smaller mixed key, a uniform pick among ties that no state wins across
  runs).
  - Without the tie-break, width-1 runs were identical.
  - With it, 8 reps at width 1 took best-of from 182,130 to 168,614 at 128³.
  - Pre-filters were changed from `<=` to `<` so ties reach it.
- Table size is exact (multiply-shift range reduction), so non-power-of-2
  growth factors don't duplicate widths.
- **Verdict (the user's test):** at 128³ `--fast`, best-of over widths 1…64
  gives 147,093 pair moves in about 2.7 s of pair solving. One flat width-128
  beam gives 145,692 in 2.9 s, and flat 64 gives 148,771. **One big beam
  wins.** `-2` is nice for watching solves shrink, but it doesn't pay; it is
  not the default.

**Comparison with dwalton76/rubiks-cube-NxNxN-solver:**
- His README table counts moves to reduce to 3×3×3, plus about 20 for the
  3×3: 4³ 51.6, 5³ 78.9, 6³ 135.7, 7³ 193.8, 8³ 306.4, 9³ 368.6, 10³ 523.4.
- His counter (`get_solution_len_minus_rotates`) counts outer block turns
  `kU` and skips rotations. Ours counts single-layer turns. The user
  considers the metrics roughly equivalent (ours is more flexible for
  commutators).
- `full/tools/metrics.py` recounts our solutions: in outer block turns they
  are about 40–60% longer, since each inner slice costs 2.

## 7b. Beam heuristics and the pair finish table (2026-09-26)

Tools: `full/pairdata` (random pair states; solve them with the pair beam under
different scores; `--path` dumps every intermediate state with its remaining
cost), `full/pairdeep` (one set of states at growing widths), `heur/` (data,
feature fits, a small MLP, the black-box tuner `tune.py`). Benchmarks are
average moves per pair on held-out random states (`heur/test1000.txt`); seed
noise is about ±0.3 on 200 states.

**Beam scaling:** width 1 / 4 / 16 / 64 / 256 / 1024 gives 83 / 75 / 69.6 /
66.4 / 62.4 / ~60. It had not levelled off at 1024. (The beam's rejection
bitsets now keep only two levels, and incremental sets are dropped above
64 MB per level; memory is bounded.)

**Heuristics on colour counts barely help.** The score is 3 moves per
misplaced piece. Fitted cost-to-go (from paths and from independent width-256
re-solves, which agree to ±1.5 moves) is concave in m, about 2.2 moves per
piece near the end and 1.5 at the start. Findings:
- Fitted h(m) tables, a per-orbit (mA, mB) table, a solved-orbit penalty and
  per-face correct counts (the user's suggestion; computed cheaply as
  popcount(correct-after & M_c & orbit)) all score within noise of the
  baseline, or worse. A tuner optimising beam results directly overfit its
  training states; the held-out gain was 0 at width 16 and 0.3 at width 64,
  for double the time.
- Imbalance is real but coarse: a state with one orbit fully solved costs
  about 7 moves more than its m suggests (only single-orbit 3-cycles remain).
  Per-face composition adds nothing once m is known. Most of the variance at
  fixed m (±2.7 moves) depends on which specific cheap algorithms fit.
- An MLP on the raw state learns nothing that generalises from 15k states
  (split by path; a random split leaks).
- Lesson: accuracy of h is not what matters; cost-to-go is a step function
  (algorithms cost about 8), and flat tables give the beam no gradient.

**What works: exact one-algorithm finishes (`FinishTable`, `solvers.h`).**
- Key: the Zobrist hash, with the beam's own keys, of every state that a
  single algorithm solves. The pools are the pair pool plus c12x, p9 and eg8,
  streamed from their `.bin` files (only permutation and cost are read):
  12.1M states, 33.5M slots, about 0.6 GB. Saved once and read back
  with `fread`: mmap was tried and saved only about 0.3 s of load, so the
  simpler code won (user preference: simpler unless a real difference).
- Every child the beam admits already has its hash, so one probe says whether
  one more algorithm finishes it, and at exactly what cost. Each node is also
  probed. Hits are recorded as solutions immediately.
- Held-out results (moves per pair), baseline → with the table:

  | width | 1 | 4 | 16 | 64 |
  |---|---|---|---|---|
  | baseline | 83.0 | 74.6 | 69.96 | 66.39 |
  | finish table | 77.0 | 70.3 | 66.03 | 63.02 |

  So 5–7% fewer pair moves at the same width and about the same time. Width
  16 with the table beats plain width 64.
- A table from the beam's own pool alone (408k) gives only −0.44.
  `--finishskip` (don't keep one-away children in the beam) is neutral. The
  "not in the table ⇒ at least two algorithms" floor doesn't bind under the
  3-per-piece score.
- In `gullicube`: on by default except `--fast` (`--finish` / `--nofinish`).
  Algorithms from the big pools are read on demand from the `.bin` files
  (`PoolBin`, pread) into `Res::extPool` (pool id 6), so the gigabyte pools
  are never loaded.
  - 10³: pairs 368 → 350. 16³: 1259 → 1202. 16³ fast: 1999 → 1850.
  - Verified with `--verify` and with `-o` + `brobcheck` (fast path).
  - Cost: about 3.5 s to build the table at startup and about 1 GB RSS.


**Diagonal and mid orbits from the pair pools (the bigger win).**
- The old `diag.pool` / `mid.pool` came only from single-orbit 8–12-movers
  (`c12x`), and nothing was shorter than 8 moves.
- `mkpools diagfrom IN OUT` collapses both slice depths of every pair-pool
  algorithm onto one (3 → 2). It keeps those that change nothing but the
  diagonal orbit, checked on an 8³ sticker cube. `mkpools midfrom` does the
  same with one depth sent to the middle layer (N=9 check).
- From `p9`: 3.57M diagonal algorithms (216 four-movers, 432 five, 5.8k six,
  43k seven, 387k eight) and 497k mid algorithms (162 four-movers, …).
- `diag2.pool` / `mid2.pool` = the ≤8-movers plus the old pools, deduped by
  permutation: 584k / 313k. Built with `mkpools` + `mkbin`, as are
  `diag_p9.pool(.bin)` / `mid_p9.pool(.bin)` (all of them, for the finish
  tables).
- Diagonal and mid orbits now use the table beam (`diagBeam`, `midBeam`) with
  their own finish tables (`finishdiag.cache`, `finishmid.cache`). `--topk`
  keeps the old top-k solver; `--diagpool` / `--midpool` pick the old pools.
- Diagonals at 10³: 177 → 139 (new pool, table beam) → 131 (+ finish table).
  At 16³: 302 → 210. Mids per orbit: 46–52 → 31–35.
- Whole cube, average of 5 seeds, `-b 256`:

  | N | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
  |---|---|---|---|---|---|---|---|
  | before (morning) | 92 | 149 | 232 | 335 | 427 | 573 | 690 |
  | now | 75 | 122 | 199 | 275 | 381 | 484 | 617 |
  | dwalton76 (reduction + ~20) | ~72 | ~99 | ~156 | ~214 | ~326 | ~389 | ~543 |

  (dwalton counts outer block turns; we count single-layer turns. The user
  considers these roughly equivalent.)
- 10³ split now: corners 8, wings 131, pairs 357, diagonals 113. Wings are the
  next target; wing pieces are distinct, not coloured, so the colour table
  beam doesn't apply as is.
- Verified: `--verify` at N=4–11, 16; `-o` + `brobcheck` for the fast path.
- CLI conventions (user): unknown options are errors, the invocation is
  echoed as the first stdout line (`# ...`), and no arguments prints full
  usage. Applied to gullicube, pairdata, pairdeep, mkbin.

**Fast mode and the finish table (1K³, 4 threads):**

| | pair moves per pair | pair-solve wall |
|---|---|---|
| width 1 | 93.0 | 3.9 s |
| width 1 + `--finish` | 86.0 | 5.1 s |
| width 2 | 89.1 | 6.8 s |

- The table beats widening the beam, but `--fast` keeps it off by default
  (minimum time). It adds 0.5 s of load and 0.5 GB of memory.
- Reading the finishing algorithms with `pread` was a sequential bottleneck
  (8.7 s); `PoolBin` now memory-maps the `.bin`.
- The new diagonal pool in fast mode at 1K³: diagonals 26.6k → 20.0k moves,
  with 0.5 s more load.

**Mining twsearch output directly (`full/mine.cpp`, 16 cores):**
- **Why:** `pair/gen.cpp` drops every sequence that is the identity on the
  pair, so no purely diagonal-only algorithm ever reached the pair pools.
  `mine diag|mid` reads `6centers.log.xz` (26.5M canonical sequences, ≤ 11
  moves) and collapses slice depths. It validates each sequence on several
  cubes, even and odd N, including the innermost orbits next to the middle
  layer:
  - diagonals: 8³, 9³ (orbit 1 and 3), 12³ (orbit 2 and 5), 13³ (orbit 5);
  - mids: 9³ (b = 1, 3), 11³ (b = 4), 13³ (b = 1, 3, 5).
  It requires the same permutation everywhere, expands the 48 symmetries ×
  inverse by composing permutations, merges same-axis moves, and keeps the
  cheapest sequence per permutation. Every output algorithm is then
  re-validated on the whole set.
- **Results (up to 10 moves; the 23M 11-movers were skipped for size):**
  - Diagonals: 48.9M distinct, 0 failed re-validation. By cost 4–10: 216 /
    432 / 9,273 / 58,510 / 639,598 / 4.98M / 43.2M. 4.8 min, 8.8 GB.
  - Mids: 5.30M distinct, 0 failed. By cost: 162 / 318 / 4,497 / 14,199 /
    143,310 / 489,680 / 4.65M.
- `mine check diag|mid POOL` re-validates any pool on the whole set.
  `diag.pool`, `diag2.pool`, `diagx8.pool`, `mid.pool`, `mid2.pool` and the
  new pools all pass (0 invalid, 0 permutation mismatches).
- **Bug found by this check:** `mkpools diagfrom/midfrom` stripped the
  ` D0`/` D1` flag by cutting at the first `" D"`, so algorithms containing a
  plain D turn were truncated. Only the (validated) prefix was stored, so the
  pools were valid, just incomplete. Fixed (strip only a trailing flag).
- **Effect** (8 paired seeds, `-b 256`, diag+mid moves):

  | N | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
  |---|---|---|---|---|---|---|---|---|
  | diag2/mid2 pools, p9 finish tables | 29.6 | 63.5 | 60.0 | 121.0 | 89.2 | 176.4 | 115.8 | 240.5 |
  | diag2/mid2 pools, x10 finish tables | 28.9 | 62.0 | 59.0 | 120.2 | 86.5 | 170.9 | 114.6 | 233.9 |
  | diagx/midx pools (≤8 + old), x10 finish | 29.2 | 59.8 | 60.8 | 120.1 | 85.6 | 173.8 | 115.4 | 231.5 |

  The bigger finish tables give 1–3%, and the bigger working pools nothing
  clear: diminishing returns. The defaults are now the x10 finish tables
  (`diag_x10.pool.bin`, `mid_x10.pool.bin`; `--diagfin` / `--midfin`) with
  the diag2/mid2 working pools.
  - The diagonal x10 table is 48.9M states (about 1 GB).
  - The first build takes about 10 s and needs `mkbin diag_x10.pool` (the
    8.5 GB text).
- **mmap vs plain I/O (user: prefer simpler unless a real difference):**
  - Finish-table caches: fread loads only about 0.3 s slower (1.4 vs 1.1 s at
    10³), with no difference at 1K³. Switched to fread and removed the mmap
    code.
  - `PoolBin` (the scattered fields of individual algorithms in multi-GB
    `.bin`s): pread per field was 8.7 s against 5.1 s mapped for the 1K³
    `--fast --finish` pair phase, so mmap stays there.
- The default seed is now drawn from entropy and printed (`# seed S`; user
  preference). `--seed` reproduces a run. `./gullicube 4` at the default
  `-b 16` averages about 85 moves; `-b 256` averages 74.

**`-2` is now whole-cube by default (2026-09-26).**
- **Why:** with `-2` running "forever", the weak wings from the single
  initial wing run held the total back, compared with a plain `-b` (user).
- Each step now re-solves everything (wings at 4× the width) with a fresh
  seed and keeps the best complete solve. The wing beam got a seeded
  tie-break for this: a seed-dependent offset below 1e-3 on scores that are
  at least 0.5 apart. All beams now take their seed from the run seed.
- The old mode is `--perpair`. The user agrees the per-pair best-of "just
  isn't a win after all".
- 6 states each, best of `-2` over widths 1…1024 vs one plain `-b 1024` run:

  | N | 4 | 5 | 6 | 7 |
  |---|---|---|---|---|
  | `-2` whole-cube, best of widths 1…1024 | 66.8 | 113.8 | 187.0 | 264.8 |
  | plain `-b 1024` | 72.2 | 114.8 | 192.7 | 263.0 |

  4³ is now below dwalton's ~72. Run-to-run variance is large (a 4³ width
  ladder: 107, 91, 76, 73, 83, 76, 80, 72, 72, 71, 69), and wings keep
  improving with width (57 → 30 on one 4³ state).

## 7c. Wings: mined algorithms and a wing table beam (2026-09-26)

**Enumeration with twsearch.**
- `6w.tws` = `6z.tws` (centres identical) with the depth-2 wings (`EDGES`)
  made identical too. Corners and the depth-3 wings (`EDGES2`) must be
  restored.
- Moves: faces + depth-2 slices (`--moves F,B,U,D,L,R,2F,2B,2U,2D,2L,2R`).
  twsearch solves the identity with `-c` huge and `--scramblealg ""`.
- twsearch prunes any move that leaves the position unchanged, so slices at
  the identical depth were never tried. `~/twsearch-nonull` is a fresh
  GitHub checkout (92a4810) with the three search-time checks disabled
  (`solve.cpp` ×2, `workchunks.cpp`; the pruning-table ones are kept, being
  harmless).
- As the user noted, twsearch here is essentially enumerating canonical
  sequences whose face word restores the corners; a simple program could do
  the same.
- `zerotwist` does not apply to wings: with centres free a slice's net twist
  is arbitrary (a lone `2U` is a legal wing move).
- Counts by max depth 4 / 5 / 6: 56k / 902k / 15.1M sequences (about 16× per
  level). Depth 8 (about 4B) was started in the background; it runs at about
  0.2M sequences/s.

**`full/minewing`** (stdin sequences → pool):
- Computes the depth-2 wing permutation from per-move permutations, keeps
  those moving at most `--maxsup` wings, and keeps the cheapest per
  permutation (same-axis moves merged).
- Expands 48 symmetries × inverse, then validates every algorithm on
  5³ (middle edges), 6³, 7³ (both wing depths), 8³ and 9³ (innermost):
  corners, middle edges and all other wing orbits unchanged, same
  permutation.
- `wing_d6.pool`: depth ≤ 6, ≤ 8 wings moved, 110,403 algorithms, 0 failed.
- `wing_d6c.pool`: plus face-turn conjugates `X A X'` (the user's
  suggestion): 1,219,113 algorithms up to 8 moves, 0 failed.
  `wing_c7.pool` = its ≤ 7-move part (246,987).
- There are no wing 3-cycles up to 8 moves in these pools. brob's 3-cycles
  are 8-move commutators like `[F' 2U F, U']`, which only depth 8 reaches.

**`full/wingbeam.h` (`WingTableBeam`):** the pair table beam's design for 24
distinct pieces.
- Piece ids are 5 bit-planes, so wings in place after an algorithm =
  `popcount(~(OR_b (V_b ^ T_b)) & 24 bits)`.
- Zobrist over moved slots, direct-mapped seeded table, seeded tie-break,
  seen table.
- Finish table (every state one algorithm solves), optionally from a bigger
  pool than the beam's (`finPool`; results flag such algorithms with
  `WingTableBeam::FIN`).
- Score `mul · (wings in place) − 4 · moves`, `mul` = 8 (2 moves per wing).
  4 fails to converge, and 5/6/10 are worse.
- Without the finish table it is much worse (50.0 against 38.9 at width 64).

**`full/wingbench`** (random wing orbits, 4³ layout; every solution is
replayed on a real cube), merged moves per orbit on 40 states:

| | width 16 | width 64 | width 256 |
|---|---|---|---|
| old WingBeam (setups + `[slice,face]` + brob 3-cycles; λ now 1.5) | 47.42 | 41.05 | 38.12 |
| d6 pool | 41.20 | 36.70 | 35.12 |
| d6c pool (1.2M), about 10× slower | 36.58 | 34.73 | — |
| c7 beam pool + d6c finish table | 37.42 | 35.12 | **33.73** |

At equal time the c7 beam pool with the d6c finish table is the best
trade-off.

**In `gullicube`:**
- The default outside `--fast` is `--wingpool wing_c7.pool --wingfin
  wing_d6c.pool`, one beam per depth. The per-depth checks and the
  cross-depth merge are unchanged.
- `--oldwings` gives the old solver. `--fast` keeps the old wing solver, since
  a 247k-pool scan for thousands of wing orbits would dominate at huge N.
- The old WingBeam's λ (weight on misplaced wings) went 2.0 → 1.5, measured
  best on 4³ (1.0 / 1.25 / 2 / 3 all worse; setup stack 3 no help).
- Paired 6 seeds, `-b 64` (wings at 256):

  | N | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
  |---|---|---|---|---|---|---|---|
  | wings, old | 37.0 | 38.8 | 76.7 | 75.8 | 113.2 | 113.8 | 149.7 |
  | wings, new | 34.2 | 33.8 | 66.5 | 65.2 | 97.2 | 98.0 | 128.7 |
  | total, old | 76.2 | 122.5 | 210.2 | 279.2 | 399.3 | 503.7 | 640.2 |
  | total, new | 74.7 | 119.0 | 197.5 | 274.5 | 382.3 | 482.8 | 618.8 |

- Verified with `--verify` at several N (default and `--fast`).

**Next for wings:**
- the depth-8 pool (3-cycles, and a bigger finish table);
- a faster scan for big beam pools (movement index or support filter);
- cross-depth sharing of face turns;
- wing pairs (depths k, k+1 together, as for the centre pairs).

## 7d. Symmetry-class pools, expanded on load (2026-09-26)

**Goal (user):** end users shouldn't need 40 GB; about 3 GB of memory is
fine, and the data files should be small.

**Class files (`.cls`):**
- Every pool is closed under the 48 cube symmetries × inverse (96 images) and
  keeps the cheapest algorithm per permutation, so a class is determined by
  one member and all its images cost the same.
- A class file holds one line per class, "COST moves [D0|D1]". For pair pools
  the diagonal flag is a class property, since symmetry maps diagonals to
  diagonals.
- `full/classpool.h`:
  - `OrbitModel`: per-move permutations on the orbit's positions, built from
    xcube (the pair model via `Layout::phi`). For each symmetry it derives the
    relabelling σ with perm(s(A)) = σ ∘ perm(A) ∘ σ⁻¹, seeding every
    connected component, since the pair has two orbits and a reflection can
    swap them.
  - `ClassPool`: loads classes and expands images by relabelling, with no
    replay.
  - `appendImage`: a (class, image) reference → a pool entry with its moves,
    produced by mapping faces and twists and reversing for the inverse.
  - `poolFromClasses`, `loadWingPoolFromClasses`.
- `full/mkclasses diag|mid|wing|pair IN OUT` reduces an expanded pool.
  - It checks every line's move text against its stored permutation (0
    mismatches everywhere, which also confirms the pair layout matches
    `gen`'s).
  - It re-expands the classes and compares (count + order-independent
    checksum).
  - Pools built by `mine`/`minewing`/`gen` match exactly. `diag2`, `mid2`
    (merged from several sources) and `eg8` (duplicate permutations from
    merging s8 + c12x) aren't exactly closed; their class files expand to the
    closure, which is equally valid.

| pool | algorithms | classes | factor | file |
|---|---|---|---|---|
| diag_x10 | 48,876,489 | 512,813 | 95.3× | 8.5 GB → 19 MB |
| mid_x10 | 5,298,560 | 57,704 | 91.8× | 0.9 GB → 2.1 MB |
| wing_d6c | 1,219,113 | 13,723 | 88.8× | ~200 MB → 0.34 MB |
| p9 | 7,465,840 | 79,280 | 94.2× | → 2.9 MB |
| eg8 | 9,206,320 | 110,608 | 83.2× | → 4.6 MB |
| p8s10 | 407,920 | 4,524 | 90.2× | → 144 KB |
| c12x / fast6 / diag2 / mid2 | 194k / 23k / 584k / 313k | 2,191 / 270 / 6,742 / 3,723 | | tiny |

All runtime data is now about 33 MB: the `.cls` files plus the small 3-cycle
fallback pools and `wing.pool`. The multi-GB `.bin`s and `.pool` texts are no
longer read. The legacy `-E` endgame still uses `../pair/*.d.txt`.

**Finish tables are built on every load, in parallel; no cache files:**
- Keys are computed straight from the class representative: the image's key
  is XOR_j KT_s[q[j]][j] with KT_s[x][j] = Z[σ_s(x)][colour(σ_s(j))] and q the
  representative's permutation or its inverse. The image permutation is never
  built.
- Inserts go through a 32-entry prefetch ring (`FinishTable::Batch`, the
  user's suggestion): the slot is prefetched, and the write happens 32
  inserts later.
- Slots are claimed with a CAS on the key, and the value word is replaced
  with a CAS only if cheaper (duplicate images of symmetric classes can
  race).
- The diagonal table (49.2M images → 25.6M distinct colour states) takes
  0.31 s on 16 threads; serial prefetch took 5.9 s and plain inserts 11.3 s.
  Identical to a serial build (0 keys missing or with a different cost).
- Ring sizes 8–64 made no difference.
- The first version spent about 240 instructions per image building image
  permutations; key tables cut the whole program from 31.6G to 16.6G
  instructions.

**Load time:**
- Pool move costs are now computed from the move groups directly (it used to
  be a text round trip), halving solver setup.
- The fallback solver shares the pair pool instead of loading `p8s10` twice.
- Wing pools come from `wing_d6c.cls`: beam = images ≤ `--wingbeamcost` (7),
  finish = all. `WingPool` stores bit-planes, masks, seam groups and a
  reference; moves are generated on demand.
- Load is now about 2.5–3.5 s on the busy machine (`LOADTIME=1` prints the
  phases).

**Memory:** peak RSS 2.8–3.2 GB in quality mode, 0.65 GB `--fast`, 2.3 GB
`--fast --finish`. The biggest item is the diagonal finish table itself
(25.6M states in 64M × 16 B slots, 1 GB). A table's size is set by its number
of states, so shrinking it further means canonicalising each probe (slower
probes).

**Verified:** `--verify` at N = 4, 5, 9, 10, 16; `-o` + `brobcheck` for 16³
`--fast`, `--fast --finish` and `-b 16`. 1K³ fast results are the same as
before (93.0 / 85.9 moves per pair).

## 7e. Seam-aware costs for the pair / diagonal / mid beams (2026-09-26)

**Why:** at 10³ the pair phase is about 48 algorithm instances (~375 raw
moves, ~115 of them face turns), and the merge removed only about 20 moves.
The table beam charged each algorithm its full cost, so it never sought
algorithms whose opening face turns cancel the previous algorithm's closing
ones.

**Change (`TableBeam::seamAware`):**
- Each node keeps the last same-axis group of its sequence.
- An algorithm's cost is its effective cost minus the moves cancelling
  against that group (`seamSave`, seq.h group arithmetic).
- Only the algorithm's first group can interact, and first groups come from a
  small set, so per node the saving is computed once per distinct first group
  and looked up per algorithm (`bonus4[headIdx[a]]`). The hot scans stay exact
  and cheap.
- Same idea as the wing beam's seams (§7c), which gave 4–5% there.

**Results:**
- `pairdata` (1,000 states, now also reporting the merged cost of each
  solution), merged moves per pair at width 16 / 64: 65.53 / 62.57 → 63.88 /
  61.04 (−2.5%).
- With exact seams the old half-move setup discount double-counts (64.21 /
  61.59). Setup weight ¾ move is best.
- Whole solves (6 seeds, `-b 64`), totals:

  | N | 8 | 10 | 16 |
  |---|---|---|---|
  | before | 375.2 | 608.3 | 1629.8 |
  | seam-aware, setup weight ¾ | 372.0 | 602.0 | 1615.0 |

  Most of that is on diagonals and mids; the cross-pair merge already
  recovered much of the within-pair cancellation.
- **Fast path:** each pair's own sequence is now merged too (same-axis groups
  combined at the seams; linear, exact in placeholder notation). 1K³
  `--fast`: 93.04 → 92.14 per pair from that alone, and **88.59** with
  seam-aware costs (−4.8%), for +19% pair-solve time (1.74 → 2.07 s).
- **Defaults now:** `--seam` on, `--setupcost 3`. Verified with `--verify` at
  N = 2–7, 9, 12, and `brobcheck` for 16³ `--fast`.

**Other merge ideas (not done yet):**
- one merge for pairs + diagonals + mids together (diagonal/mid instances
  touch no pair pieces; the D1 pair algorithms bound them via the lattice);
- a C++ merge beam in place of the greedy merge;
- choosing among near-equal per-pair solutions for mergeability.

## 7f. One merge and a merge beam (2026-09-26)

**One merge for pairs + diagonals + mids (`--onemerge`, default):**
- **Constraint:** diagonal algorithms must come after every pair algorithm
  that disturbs that diagonal (the D1 ones), and those must stay in the order
  the diagonal state was computed from. Mids are never touched by pairs.
- **First attempt:** chain the D1 algorithms in *solution* order and skip the
  first merge. It helped at 8³–10³ but lost 19 moves at 16³: at 16³ that's 21
  pairs over 7 diagonals, and the chains removed the merge's freedom to
  interleave pairs.
- **What's used now:**
  1. Merge the pairs first as before; `Merger::merge` now reports the order
     it placed instances in.
  2. Replay; the diagonal states come from that order. Solve diagonals and
     mids.
  3. Merge again with everything: the pairs' own lattices, the D1 algorithms
     chained per diagonal in the first merge's order (full-mask entry in a
     per-diagonal component), diagonal and mid instances in their own
     components.
  4. Keep the shorter of this and the two-stage result.
- 8 seeds, `-b 64`, totals:

  | N | 8 | 9 | 10 | 16 |
  |---|---|---|---|---|
  | two-stage | 372.2 | 472.9 | 606.5 | 1615.0 |
  | one merge | 367.4 | 467.5 | 598.4 | 1614.4 |

**Merge beam (`Merger::mergeBeam`, `--mergebeam W`):**
- Level by level, one instance placed per level, keeping the W cheapest
  partial interleavings.
- A state is the placed set, per-instance unplaced-predecessor counts, the
  last (max instance length + 1) axis groups (enough for exact costs) and its
  cost.
- Equal (placed set, tail) states are deduplicated.
- 8 seeds, `-b 64`, totals:

  | N | greedy | beam 16 | beam 64 | beam 256 |
  |---|---|---|---|---|
  | 8 | 367.4 | 363.1 | 360.0 | 356.5 |
  | 9 | 467.5 | 460.8 | 457.8 | 455.1 |
  | 10 | 598.1 | 586.4 | 582.0 | 579.0 |
  | 16 | 1613.5 | 1588.4 | 1576.0 | 1563.2 |

- **Time** grows about as instances² × width, and the gain shrinks as the
  greedy merge gets more instances to work with:
  - 32³: 6,856 → 6,694 (width 64, 11 s against 3 s);
  - 64³: 26,599 → 26,336 (width 64, 159 s).
- **Default width by merge size (`mergeWith`):** 256 up to 400 instances,
  64 up to 1,500, greedy beyond. `--mergebeam W` overrides; 0 = greedy.
- Verified with `--verify` at N = 4, 5, 7, 8, 10, 16, 24.

## 7g. Fast greedy merge, no replay, merge time budget (2026-09-26)

**Crossover benchmark (first pass)** against brobdicube (patched build,
`-p ceil(N/32)` on 16 threads; single runs; the machine was loaded):

| N | brobdicube | ours `--fast` | `--fast --finish` | quality `-b 1` | `-b 16` |
|---|---|---|---|---|---|
| 64 | 52,626 (3.6 s) | 43,896 (1.2 s) | 41,044 | 31,207 | 26,529 |
| 128 | 164,908 (2.2 s) | 179,058 (0.7 s) | 167,136 | — | 102,835 (12 s) |
| 256 | 545,381 (2.9 s) | 720,926 (0.8 s) | 672,366 | 469,287 (122 s, old merge) | 399,104 |
| 512 | 1,875,415 (4.4 s) | 2,895,023 | 2,697,870 | 1,864,871 (30 s) | |
| 1024 | 6,645,671 (6.8 s) | 11,592,495 | 10,803,053 | 7,405,542 (136 s) | |
| 2048 | 23,795,678 (13 s) | 46,417,271 | 43,242,631 | | |
| 4096 | 86,835,674 (14 s) | 185,728,029 (21 s) | 173,008,116 | | |

brobdicube's moves beat our fast mode from about 128³, and our quality mode
from about 512³–1024³. Before the fixes below, quality mode was slow at large
N.

**Found and fixed on the way:**
- **Wing table beam failure:** it could dead-end at narrow widths (`-b 1`:
  wing width 4) and aborted the whole solve ("wing solve failed", exit 0).
  It now falls back to the old wing beam, which always completes.
- **Greedy merge was quadratic:** every step scanned all available instances
  (about 130k at 256³). Now the available instances are indexed by the
  (axis, layer, twist) of each move in their first same-axis group. Only an
  instance sharing a layer with the tail's last group can save anything, so
  each step looks in the cancelling buckets (save 2), then the merging ones
  (save 1), and evaluates the exact saving for up to `mergeLim` members
  (cascades happen). If nothing saves, it takes the oldest (FIFO).
  - At 64³ this is also *better* (pairs merged 21.5% vs 20.4%).
  - The limit matters at 256³ (pair moves 469,635 / 467,719 / 462,154 at
    16 / 64 / 256 candidates); 256 matches the old full scan.
- **Quality mode replayed every merged move on the full cube** (about N per
  move: about 28 s of 60 at 1024³). Now it replays only with `--verify` or
  `REPLAY=1`.
  - Otherwise the pair effects on the diagonals are applied as permutations
    in the merge's order (`Merger::merge` returns it). That equals the
    replay, since the merged sequence is exactly the product of its
    instances in order.
  - The same moves come out (identical totals with and without replay at 9³,
    16³, 24³; `brobcheck` SOLVED).
- **Merge time budget (the user's rule: split CPU between beam and merge):**
  - budget = max(1 s, `--mergeshare` (0.5) × pair beam time);
  - the greedy merge adapts its candidate limit to the budget (halve / double
    every 1,024 steps, within [4, 256]);
  - the merge beam width is fitted to it (cost ≈ n² × width × 9e-8 s);
  - the combined merge is skipped above 20k pair instances.
- **Effect:**
  - 1024³ `-b 1`: 136 s → 12.8 s solve (7.58M moves, +2.3% against the
    unbudgeted merge);
  - 256³: 122 s → 2.0 s;
  - small cubes slightly better than before (8 seeds, `-b 64`: 8³ 355.2,
    10³ 574.5, 16³ 1553.0).

**The depth-8 wing twsearch run** was paused (SIGSTOP) at 4.07B sequences:
its pipe streaming loaded the shared machine (high system time). An
in-process enumerator would regenerate it far more cheaply if wanted.

**Benchmark collector:** `bench/collect.py` (portable Python, standard
library) runs the N × configuration grid cheapest first with a per-run cap,
excludes load time, and writes `bench/results.csv` for the
moves-per-sticker vs µs-per-sticker plots.

## 7h. Frontier benchmark: gullicube vs brobdicube (2026-09-26)

**Metric (user):** moves per sticker against µs per sticker, stickers =
6N², one log–log panel per power-of-two size.
- Our solver is a curve over effort levels; brobdicube is one point per
  setting (default = optimal planning, `-g` = greedy planning).
- dwalton76 is a moves-only reference (his README; outer-block metric, no
  times).
- Page: https://claude.ai/artifact/PxQnmqVCujZNVtCZNqAf3Y (private until
  shared). Built by `bench/frontier.py` from `bench/results*.csv`.

**Tools:**
- `bench/collect.py` (portable Python, standard library): runs the
  N × setting grid cheapest first, 8 seeds up to 16³, 4 at 32³, 2 at 64³, 1
  beyond.
  - Solve time excludes load; brobdicube's algorithm-load time is
    subtracted. CPU time and the load average are recorded.
  - A setting is dropped at larger N once a run exceeds `--cap`.
  - Filters: `--only`, `--configs`, `--sizes`; separate `--out` files merge
    in `frontier.py` (later files replace cells).
- `bench/frontier.py` + `frontier_template.html`: the page. Panels scale to
  our curve; brobdicube is drawn only when within 1.5× of our longest
  setting, otherwise listed under the panel.
- `bench/scramble.py N COUNT [SEED]`: random moves on one line for `-i`.

**Results** (moves/sticker @ µs/sticker; 16 threads, shared machine):

| N | fast | fast + finish | quality beam 1 | best quality (setting) | brobdicube | brobdicube -g |
|---|---|---|---|---|---|---|
| 4 | 1.08 @ 251 | 1.06 @ 237 | 0.88 @ 241 | 0.70 @ 5.6e4 (b256) | 2.03 @ 1.1e5 | 3.32 @ 4.1e3 |
| 8 | 1.46 @ 56 | 1.39 @ 57 | 1.10 @ 221 | 0.87 @ 1.2e5 (b1024) | 2.97 @ 2.7e4 | 4.43 @ 1.1e3 |
| 16 | 1.65 @ 15 | 1.56 @ 15 | 1.22 @ 831 | 0.96 @ 4.2e4 (b1024) | 3.23 @ 6.7e3 | 3.80 @ 259 |
| 32 | 1.75 @ 4.0 | 1.64 @ 4.3 | 1.26 @ 196 | 0.99 @ 2.0e4 (b1024) | 2.77 @ 68 | 3.11 @ 64 |
| 64 | 1.79 @ 1.4 | 1.68 @ 2.1 | 1.24 @ 23 | 0.98 @ 2.1e3 (b256) | 2.15 @ 22 | 2.49 @ 17 |
| 128 | 1.82 @ 0.70 | 1.70 @ 1.4 | 1.22 @ 19 | 0.96 @ 1.3e3 (b256) | 1.68 @ 8.3 | 2.09 @ 4.7 |
| 256 | 1.83 @ 0.50 | 1.71 @ 0.73 | 1.22 @ 7.4 | 0.97 @ 135 (b64) | 1.39 @ 4.2 | 1.68 @ 1.2 |
| 512 | 1.84 @ 0.26 | 1.72 @ 0.37 | 1.20 @ 4.9 | 0.97 @ 119 (b64) | 1.19 @ 1.9 | 1.39 @ 0.30 |
| 1024 | 1.84 @ 0.26 | 1.72 @ 0.33 | 1.20 @ 3.4 | 1.01 @ 26 (b16) | 1.06 @ 0.85 | 1.21 @ 0.089 |
| 2048 | 1.84 @ 0.26 | 1.72 @ 0.32 | 1.19 @ 3.3 | 1.07 @ 7.9 (b4) | 0.95 @ 0.33 | 1.04 @ 0.028 |

(The best-quality column at 1024³–2048³ is limited by the 2-minute cap; a
long-budget pass of beam 64/256/1024 at 512³–2048³, up to an hour a run, is
running into `bench/results_long.csv`.)

**Reading:**
- **Up to 64³ we dominate on both axes:** even `--fast` is about 2× shorter
  and faster than brobdicube. Its optimal planning costs about 10 s at 4³.
- **brobdicube's moves per sticker falls steadily with N** (3.2 at 16³ → 0.95
  at 2048³); ours is flat (fast about 1.8; beam 1 about 1.2; beam 64 about
  0.97).
  - Its default beats our fast mode from 128³.
  - It ties quality beam 1 at 512³ and beats it beyond.
- **The user's point:** we can always buy a shorter solution with more time;
  brobdicube cannot improve its single result. For huge cubes, minutes or
  hours per solve are reasonable, hence the long-budget pass.
- **Kink at small N:** quality beam 1–16 all take about 1 s at 16³–32³,
  because the merge budget has a 1 s floor. Fix: scale the floor with effort.

**Also this session:**
- **Wing table beam fallback:** it could dead-end at `-b 1` (wing width 4)
  and abort the solve; it now falls back to the old wing beam, which always
  completes.
- **Data files are read from the current directory** (a data-path option
  was tried and removed at the user's request, to keep things simple).
- **Millisecond solve times** in the TOTAL line (`wall %.3fs`).
- **2³ and 3³ verified** in all modes (8–9 and 23–27 moves). They load every
  table (4–5 s) though they need none.
- **The depth-8 wing enumeration finished** (resumed overnight): 4.61B
  sequences, 422M moving ≤ 10 wings, 10,414,407 distinct permutations (all
  validated), including 2,400 8-move wing 3-cycles. `mkclasses` reduces it to
  `devtools/wing_d8.cls` (110,993 classes, 2.9 MB, re-expansion checksum
  MATCH).
- **Tested as the wing pool, not adopted.** Wing moves per cube, `-b 64`,
  20 seeds (wing time per orbit, 8³, on a loaded machine):

  | wing setup | 6³ | 8³ | time/orbit | memory |
  |---|---|---|---|---|
  | `wing_d6c` ×4 (default) | 63.6 | 95.2 | 0.7 s | 3 GB |
  | `wing_d6c` ×8 | | 93.75 | 1.25 s | 3 GB |
  | `wing_d8` ×4 | 62.0 | 93.25 | 3.1 s | 4.0 GB |
  | `wing_d6c` ×16 | 61.35 | 90.55 | 5.2 s | 3 GB |

  At equal time a wider beam on the current table wins, and d8 breaks the
  3 GB budget, so it stays in `devtools` (usable with `--wingpool`).

## 7i. Distinct keep-solved masks per pool (2026-09-27)

Question: would a (mask, algorithm-list) organization beat the transposed
per-position bitsets for keep-solved rejection?  `devtools/xcount` counts
distinct masks (face-leave for the pair/diag/mid beams, moved slots for wings):

| pool | algorithms | distinct masks | algs/mask | bitset words (n/64) |
|---|---|---|---|---|
| `p8s10` | 407,920 | 109,533 | 3.7 | 6,374 |
| `fast6` | 23,034 | 7,939 | 2.9 | 360 |
| `diag2` | 609,683 | 58,138 | 10.5 | 9,527 |
| `mid2` | 322,492 | 49,675 | 6.5 | 5,039 |
| `wing_d6c` beam (cost ≤ 7) | 246,987 | 19,827 | 12.5 | 3,860 |

A grouped test costs one AND per distinct mask, whereas a bitset word covers
64 algorithms, so grouping needs well over 64 algorithms per mask to win.
We have 3–16: the 96 symmetry/inverse images of a class have different
supports, so symmetry spreads the masks rather than sharing them.  Not
pursued.

## 7j. Pair beam anatomy, openings, and a GPU beam (2026-09-27)

**Summary.**
- **Where the time goes:** level 1 (the start's W children) was ~43% of
  beam time and scoring ~1/6 of the instructions; per-node overhead
  (bitsets, admission, finish-table cache misses) is the rest.
- **Two CPU improvements:**
  - Level 1 scoring only the start's top 4k algorithms: same moves, ~45% of
    the instructions. It's a hook, off by default, not yet adopted.
  - Survivor lists: on by default, identical results, −16% cycles.
- **Rejected ideas** (all measured):
  - pairing algorithms with their inverses: face-leave masks differ from the
    inverse's in ~30%;
  - (mask, list) grouping: only 3–16 algorithms per mask;
  - scanning only short algorithms early: 8-movers win ~70%;
  - "never in the prefix" marking: nothing qualifies;
  - growing or shrinking the beam width per level;
  - a wide first level;
  - an exact disjoint opening seeding the beam: 0.8–2.6 moves worse at
    equal instructions after a hill-climb;
  - reusing old scores for disjoint algorithms: under 1%.
- **GPU (Metal, `devtools/gpu`, experimental, not for the release):**
  - A brute-force scoring beam is identical to the CPU beam and 16× one CPU
    thread at W 4096.
  - Width saturates near 16k.
  - 2-ply lookahead ranking reaches each quality level in 1/3–1/2 of the
    time, but the plain and lookahead curves flatten at the same ~58.7.
  - The ceiling is the algorithm set: working pool p8s10 → p9 ≤ 8 → p9 moves
    it 58.6 → 58.1 → 57.4.
  - Cheapest CPU lever found: p9 ≤ 8 moves (979k) as the pair working pool.

The details follow.

### Algorithms scored per node

`BEAMTIME=1 devtools/pairdata solve 64 --pool p8s10.cls --seam --setupcost 3
--finishpool c12x.cls --finishpool p9.cls --finishpool eg8.cls`, 200 random
pair states (TableBeam counts, by the node's correct positions out of 48; a
random state has about 8):

| correct | scored per node (keep-solved survivors of 407,920) | admitted per node |
|---|---|---|
| 1–8 (roots) | 340k → 110k | 3,100 → 930 |
| 10 | 74k | 113 |
| 14 | 31k | 55 |
| 20 | 9.7k | 45 |
| 30 | 1.1k | 44 |
| 40 | 155 | 36 |
| 46 | 42 | 42 |

Totals: 108k nodes, 5.6k scored per node on average; nodes with ≤ 14 correct
are 6.6% of nodes but 56% of scoring, ≤ 20 correct 16% / 85%.  Thread time:
keep-solved filter 2.2 s, scoring 6.9 s.

**Best single-algorithm gain** (`devtools/maxgain`, states along W=64
solution paths): always ≥ 2; 7–9 at the start, 4–6 around 30 correct, 2–3 at
the end.  At the start over 100k algorithms gain ≥ 2 and 18k gain ≥ 4.

**Winning algorithm lengths** (`pairdata --algstats`, 400 random pair states,
default scoring + finish tables).  p8s10 by length: 4:432 5:864 6:11,634
7:64,860 8:330,130 (81% are 8-movers).  Beam algorithms in the W=1024
solutions: 4:32 5:24 6:374 7:311 8:2071 — per algorithm in the pool, 4-movers
win 11× their share, 5/6-movers 4–5×, 7-movers 0.7×, 8-movers 0.9×.  The mix is
nearly the same at every width (4..1024) and every stage; 8-movers are 60–75%
of the picks even at the start.  Moves: W 4/16/64/256/1024 = 70.6/66.4/63.7/
61.7/60.6 (merged 67.8/63.5/60.7/58.5/57.1).  So scanning only short
algorithms early would lose most winners: the cheap end of the pool is too small.

**Time by beam level** (`BEAMTIME=1 pairdata`, same 400 states; level 0 =
the root, level 1 = its W children):

| W | level 0 | level 1 | level 2 | level 3 | levels ≥ 4 |
|---|---|---|---|---|---|
| 16 | 8.3% | 44.1% | 15.2% | 6.2% | 26.2% |
| 64 | 3.6% | 41.6% | 18.5% | 8.6% | 27.7% |
| 256 | 1.9% | 43.0% | 18.4% | 8.6% | 28.0% |

Scored per node: root 107k, level 1 ~35–39k, level 2 ~8–9k, level 3 ~2.2k,
then 800 → 60.  Level 1 alone is ~43% at every width: W siblings, each one
algorithm away from the same root, each rescoring ~35k algorithms.  From
level 4 on each node scores only a few hundred, and the time there is the
fixed per-node cost (the 6.4k-word bitset walk, finish probes, applying).

**Where the solution's algorithms ranked at the start** (`pairdata
--rootrank`; rank by 12·gain − eff4 at the start, 400 states):
- step 1: always in the start's top 1k (top 16 for 50–92%);
- step 2: 96–99% in the top 4k, ~100% in the top 16k; disjoint from step 1
  only 33–50%, and it gains more when applied than at the start (6.8 vs 6.3):
  step 1 sets it up;
- step 3: 80–86% in the top 4k, 90–97% in the top 16k;
- steps 4–6: spread out (most beyond 16k), almost never disjoint from the
  earlier steps.

**Experiments (hooks in `TableBeam`, off by default: `rootTop`/`rootLevels`,
`firstWidth`; `pairdata --roottop M L`, `--firstwidth K`).**  400 states,
machine load 35–44 (compare times within a block only):

| W | variant | moves (merged) | wall | score time |
|---|---|---|---|---|
| 64 | baseline | 63.737 (60.685) | 9.1 s | 30.1 s |
| 64 | level 1 scores the start's top 4k | 63.750 (60.703) | 5.7 s | 16.4 s |
| 64 | … top 4k on levels 1–2 | 63.825 (60.833) | 4.1 s | 9.8 s |
| 64 | … top 16k on levels 1–2 | 63.778 (60.715) | 5.5 s | 15.6 s |
| 64 | start keeps 256 children, top 1k | 63.528 (60.450) | 5.9 s | 16.7 s |
| 64 | start keeps 1024 children, top 1k | 63.445 (60.400) | 8.8 s | 23.2 s |
| 64 | start keeps 1024 children, top 4k | 63.333 (60.335) | 12.7 s | 38.7 s |
| 128 | baseline | 62.645 (59.410) | 17.9 s | 60.1 s |
| 128 | level 1 scores the start's top 4k | 62.645 (59.405) | 10.9 s | 31.3 s |
| 128 | start keeps 1024 children, top 1k | 62.670 (59.377) | 13.8 s | 38.5 s |
| 256 | baseline | 61.672 (58.475) | 36–39 s | 123–132 s |
| 256 | level 1 scores the start's top 4k | 61.680 (58.480) | 22.7 s | 67.4 s |

Level 1 scoring only the start's top 4k is free: same moves at every width,
~40% less time.  A wide first level (the "all pairs of the top K" idea) buys
0.2–0.4 moves, less than spending the same time on a wider beam (W 128 +
top-4k: 62.65 in 10.9 s vs W 64 + K 1024: 63.45 in 8.8 s).  The first
choice is not what limits quality.

**Support-disjoint pairs at the start** (`devtools/pairtop`, 99 states;
score 12·gain − 4·length, shown in piece units = score/12): a from the
start's top 100 (300), b from its top 4k (16k).  Best single 5.12, best pair
9.49 (9.50).  The best pair is support-disjoint in 53% (52%) of states; the
best disjoint pair (additive score, no rescoring) is 0.29 (0.30) below the
best pair and ranks ~4th among all pairs; 64% (60%) of the top 100 pairs are
disjoint.

### Survivor lists

**Survivor lists** (in `TableBeam`, on by default; `NOLIST=1` disables):
a node whose keep-solved survivors number ≤ 2·nw (≤ 12.7k for p8s10, and
≤ 64 MB per level) keeps them as a list; a child whose correct set contains
its parent's filters that list with one AND per entry (`X[a] & corr`) instead
of copying and ORing 51 KB bitsets and walking 6.4k words.  If a correct
position was lost (a correct piece shuffled within its face: 22% of nodes)
the child rebuilds its bitset.  Results identical.  One thread, 100 states,
W 256, level 1 on the start's top 4k, instructions / cycles excluding load
(load: 14.3G / 4.6G): bitsets only 70.7G / 49.1G, lists 65.8G / 41.3G
(−7% / −16%).  Tried and dropped: deriving a lost-position child from its
parent's survivors plus the algorithms that carry the lost position off its
face (per-position lists average ~51k entries): +27G instructions.

Remaining cost (W 256, level 1 on the top 4k): level 2 is 36% (9k scored per
node), level 3 16%, levels 4+ ~5% each; finish-table probes on admission
(35M, ~220 ns each) are ~12% of beam time.

### Openings: disjoint sets at the start

**Disjoint-support frontier** (`devtools/disjoint`): for a pair state, the
least total moves to reach each solved count with a set of algorithms with
pairwise disjoint supports applied at the start (they commute; gains add).
Exact branch and bound on growing prefixes (20, 40, 80, … of the candidates
sorted by moves per fixed piece), each prefix's sets bounding the next; bound =
sum of the cheapest per-position moves-per-piece.  The frontier up to ~32
solved stops changing by prefix 1,280–2,560 (≤ 1 s); prefix 5,120 takes
~50 s / 100M nodes and the full list (47–61k candidates) does not finish, so
the high end is an upper bound.  Three states (solved:moves), disjoint
frontier vs the W=256 beam path (moves without seam savings):

| state | disjoint frontier | beam path |
|---|---|---|
| A (8) | 15:8 21:14 28:25 32:32 36:43 38:53 | 15:8 21:16 28:24 32:32 36:38 40:45 43:53 48:61 |
| B (11) | 19:7 23:12 27:19 32:29 36:39 39:52 | 19:8 23:14 27:22 32:30 36:38 48:56 |
| C (8) | 14:6 22:16 28:24 33:34 37:52 | 14:8 22:16 28:24 33:32 37:40 40:47 42:55 48:62 |

Up to ~30 solved the beam is within 0–3 moves of the disjoint optimum (about
one move per piece); beyond that disjoint sets run out of room (supports
overlap) and the beam, which overlaps algorithms, is far better.  The last
~15 pieces cost about 30 moves.

**Algorithms that never make the prefix?** (`devtools/prefixfreq`, exact
top P by moves per fixed piece, then larger gain; random pair states.)
Algorithms ever in the top P after 10 / 100 / 300 / 1000 states: P 160: 0.4 /
3.6 / 9.5 / 23.8%; P 640: 1.5 / 13 / 30 / 55%; P 2560: 6 / 40 / 67 / 85%;
P 10240: 22 / 81 / 94 / 98.5% — still growing (the pool is closed under
symmetry × inverse, so symmetric states bring in every image).  The bound
length / pieces moved (the best possible moves per piece) is ≤ 8/3 for every
algorithm, below the worst-state cut, and per state it would skip only 7% (P
160), 1.4% (P 640–2560).  So nothing can be marked "never pre-evaluate";
scoring the whole pool at the start is ~400k bit-sliced scores anyway.

### Beam width schedules

**Beam width growing or shrinking per level** (`pairdata --grow g [cap]`:
the table at depth d holds W·g^d nodes, cap 65536).  One thread, 100 states,
instructions excluding load (fixed-width curve: W 16 9.4G 67.16, W 64 35.8G
63.63, W 256 150G 61.78, W 1024 663G 60.80):

| config | moves (merged) | beam instructions | fixed width at the same instructions (interpolated) |
|---|---|---|---|
| W 4 ×2 | 66.02 (62.81) | 47.9G | ~63.2 |
| W 16 ×2 | 63.43 (60.08) | 179G | ~61.6 |
| W 64 ×2 | 61.29 (58.33) | 682G | ~60.8 |
| W 16 ×1.5 | 64.81 (61.51) | 31.6G | ~63.8 |
| W 64 ×1.5 | 62.38 (59.51) | 122G | ~62.1 |
| W 256 ×0.8 | 62.62 (59.84) | 123G | ~62.1 |
| W 256 ×0.6 | 63.75 (61.25) | 111G | ~62.2 |
| W 1024 ×0.7 | 61.51 (58.41) | 509G | ~61.0 |

Both growing and shrinking lose to a constant width at equal instructions.
For reference, level 1 on the start's top 4k: W 64 16.6G 63.68, W 256 65.4G
61.78 (same moves as fixed, 44–46% of the instructions).

### Testbed: opening + beam, hill-climb

**Testbed: exact disjoint opening seeding a beam** (`devtools/seedbeam`,
`devtools/tools/hillclimb.py`; tunables P prefix, S seed threshold (solved
pieces, total), N seeds, X opening node budget, b width, M/L start-list
length/levels, H moves per misplaced piece, F finish tables).  A set is handed
to the beam as soon as it reaches S (not extended past it); seeds are ranked
by the beam's score.  One thread, 100 states, instructions excluding load
(14.6G).  Plain beam (no opening, levels 0–1 on the start's top 4k): b 8 3.8G
68.30, 16 5.6G 67.16, 32 9.4G 65.09, 64 17.2G 63.68, 128 33.0G 62.48, 256
66.0G 61.78, 512 135G 60.96.

Hill-climb from P 160 S 16 N 64 b 64 M 0 L 1 H 3 (50 configurations) moved to
the smallest opening it could (P 40, S 16) and the widest beam: best
P 40 S 16 N 64 b 512: 62.22 moves (merged 59.85) at 90.9G, **0.81 moves worse**
than the plain beam at the same instructions; the start was 2.2 worse.  Every
seeded configuration lost 0.8–2.6 moves.  Earlier variant (sets extended past
S, overshooting to ~32 solved): 2.7 worse.  Likely: disjoint sets forgo the
set-up effects (step 2 gains more after step 1 than at the start, and is
disjoint from it only 33–50% of the time); the opening ignores seam savings;
and the plain beam's first levels are already cheap with the start's top list,
so there is little time for an opening to save.

### GPU beam (experimental; devtools/gpu)

**GPU beam, stage 1** (`devtools/gpu`: `gpubeam.h`, `gpucmp.mm`; Metal,
M3 Max 40-core GPU; `scorebench.mm` micro-benchmark: 1.6e11 node×algorithm
pairs/s brute force with the CPU idle, 0.6–1e11 with it busy).  Same search as
TableBeam (seam-aware, keep-solved, finish tables, table and tie-break), but
each level scores every (node, algorithm) pair on the GPU in two passes
(score histogram, then emit candidates at or above the cut-off keeping 16×
width); the host admits them in (node, algorithm) order.  50 random pair
states, p8s10, setup cost 3, c12x/p9/eg8 finish:

| W | CPU 1 thread ms/state | GPU ms/state (GPU / host s) | moves | identical |
|---|---|---|---|---|
| 64 | 63.6 | 18.8 (20 states) | 63.45 | 20/20 |
| 256 | 277 | 51.6 (1.1 / 0.8) | 62.00 | 50/50 |
| 1024 | 1254 | 135 (2.3 / 3.2) | 60.56 | 50/50 |
| 4096 | 6062 | 384 (7.2 / 10.8) | 60.10 | 50/50 |
| 16384 | — | 1598 (29.5 / 49.4) | 58.86 | |
| 65536 | — | 8824 (134 / 305) | 59.42 | |

Identical solutions to the CPU beam wherever compared.  The GPU passes run
at ~2.3e11 pairs/s; the host (candidate sort, admission, finish-table probes)
is now the larger part.  Widths beyond ~16k stop helping: on 20 states 16384
→ 59.40, 65536 → 59.70, unchanged with a seen table scaled to the width and
with 4× the candidates (short levels 70 → 38 of ~215): the beam itself, with
this score (3 moves per misplaced piece + seam), saturates there.

**2-ply lookahead ranking on the GPU** (`gpucmp -L -c C`): the top C×width
candidates by their own score are re-ranked by their best grandchild's score
(one more GPU pass: every candidate child × every algorithm, max per child);
then admitted as usual.  50 states, ms/state on a busy machine (±2× noise):

| config | moves | ms/state |
|---|---|---|
| plain W 256 | 62.00 | 50 |
| plain W 1024 | 60.56 | 153 |
| plain W 4096 | 60.10 | 467 |
| plain W 16384 | 58.86 | 2005 |
| look W 64 C 4 | 61.76 | 30–46 |
| look W 64 C 16 | 60.18 | 77 |
| look W 256 C 4 | 60.34 | 88 |
| look W 1024 C 4 | 59.66 | 355 |
| look W 1024 C 8 | 59.26 | 652 |
| look W 1024 C 16 | 58.64 | 1177 |
| look W 4096 C 8 | 58.76 | 2411 |
| look W 4096 C 16 | 58.98 | 4392 |

Lookahead reaches each quality level at a third to half the time of a plain
beam, but both flatten near 58.7: the ceiling is not the ranking.

**Bigger working pools on the GPU** (`gpucmp -p POOL -m MAXLEN`; the same 50
states; ms/state on a busy machine):

| config | p8s10 (408k) | p9 ≤ 8 moves (979k) | p9 (7.47M) |
|---|---|---|---|
| plain W 256 | 62.00 / 50 | 61.44 / 33 | 60.32 / 123 |
| plain W 1024 | 60.56 / 153 | 60.06 / 91 | 58.52 / 456 |
| plain W 4096 | 60.10 / 467 | 59.24 / 355 | 58.16 / 2139 |
| look W 256 C 4 | 60.34 / 88 | 60.22 / 125 | 59.30 / 1079 |
| look W 1024 C 4 | 59.66 / 355 | 58.90 / 457 | 57.78 / 4723 |
| look W 1024 C 16 | 58.64 / 1177 | 58.08 / 1633 | 57.40 / 15591 |

(moves / ms.)  The ceiling moves with the algorithm set: 58.6 → 58.1 → 57.4.
p9 at plain W 1024 (58.52, ~0.5 s) beats the best p8s10 configuration.  The
p8s10 limit of ≤ 10 moved pieces costs about 0.5–0.9 moves; the 9-movers
another ~1.

## 7k. Merge headroom: assignment bound and a greedy-edge preview (2026-10-02)

Idea (user): merge pairwise like building a Huffman tree — take the best seam
anywhere, join the two pieces, repeat — instead of extending one tail
(nearest neighbour); respect order constraints; then insert the dependent
(diagonal/mid) algorithms.  Steps 1–2 measure the room for it.

Tools: `MERGEDUMP=prefix gullicube ...` (hook in `mergeWith`, off by default)
writes each merge input; `devtools/mergebound` runs the current greedy merge
and merge beams on it, computes exact pairwise savings S[i][j] (full cascade),
depth histograms and the best order's actual vs pairwise saving;
`devtools/tools/assignbound.py` gives the assignment bound (each instance ≤ 1
successor and predecessor; constraints and cycles ignored) and a greedy-edge
preview, with order constraints checked on contracted pieces (a seam is
"immediately before").  One solve each, -b 64, seed 7.

| merge | raw | greedy | beam 256 | greedy edge, constraints | greedy edge, none | bound |
|---|---|---|---|---|---|---|
| all, 10³ (63 inst.) | 501 | 459 | 453 | 441 | 423 | 413 |
| all, 16³ (197) | 1568 | 1369 | 1344 | 1296 | 1227 | 1205 |
| all, 32³ (902) | 7170 | 6051 | 6005 | 5853 | 5300 | 5219 |
| pairs, 32³ (845) | 6714 | 5548 | 5531 | 5341 | 4990 | 4901 |

- Greedy edge with the constraints beats the merge beam by 2.5–3.6% of the
  merged centre moves (32³: −152 moves of 6434), in a fraction of its time.
- The order constraints cost most of the rest (32³ all: 18.4% vs 26.1%
  saved); the assignment bound is close to unconstrained greedy edge.
- Deep merges are rare here: ≥ 2 groups consumed in 1–3% of saving seams,
  savings ≥ 3 in ~5%, no instance vanishes; every order's actual saving equals
  the sum of its pairwise savings, so pairwise scores are exact in practice.

**Pairwise merge in C++** (`Merger::mergePairwise`, `gullicube --pairmerge`;
the greedy merge and merge beam are unchanged and remain the default).
- Pieces start as single instances; the best seam anywhere joins two pieces
  (exact instance-to-instance savings: groups cancelling completely let the
  next groups inward meet; lazy best seam per piece end in a heap; buckets of
  first groups; `pairLim` 256 candidates per end).
- Order constraints: pieces keep a topological order (Pearce–Kelly style); a
  seam P→Q is refused on a path Q⇝P or P⇝X⇝Q, searched only between P and Q
  in the order; accepted seams reorder that region.  Searches past
  `pairVisitCap` (64) refuse (safe).  First version (unordered searches): 300 s
  at 128³; ordered: 1.1 s.
- Under the merge time budget the scan limit and cap halve when the projected
  time exceeds it; at twice the budget at the minimum, joining stops.
- Every order validated against all constraints (`mergebound`); whole solves
  pass `--verify` (5³, 9³, 16³, 33³; earlier 4³–16³).
- Merge alone (dumps, unbudgeted): 32³ all 6005 (beam 256, 26 s) → 5688
  (0.01 s); 64³ pairs 28336 (greedy) → 26524 (0.05 s); 128³ 115709 →
  108232 (1.1 s vs 0.7 s); 256³ 462753 → 441825 (27 s vs 4.4 s; time grows
  about as n²: large pieces carry long neighbour lists, and refused seams
  cost most of the searches).
- Whole cube, -b 64, 3 seeds: 8³ +0.3%, 16³ −2.0%, 32³ −5.7%, 64³ −5.3%
  (5 seeds earlier: 10³ −1.3%, 12³ −1.2%, 24³ −4.4%).  -b 1, seed 3: 128³
  120600 → 112987 (−6.3%, wall 3.1 → 3.8 s); 256³ 477420 → 473799 (−0.8%,
  same wall: the budget throttles it).

**Scaling the pairwise merge.**  Full constraint checks cost about n²:
at 128³, 140k checks scanned 97M neighbour entries, against 21M scored seams
(the heap part is n log n).  Refusals: 733 direct, 26k longer paths, 20k
through another piece, 56k at the search cap; and orbits are not chains (56%
of an orbit's algorithm pairs are unconstrained), so a rank-range test would
not help.  Limiting seams to pieces within W places in the order made both
quality and time worse (more refusals).

User's idea: **levels**.  Give each instance a level respecting the
constraints (as late as possible is slightly better than as early), merge
each level pairwise with no checks, lay the levels out in order: 8–10 levels.
Then **local improvement** (`devtools/mergeopt`): on the single sequence,
move runs of 1–3 instances to just after / before a partner they seam with
(partners from first- and last-group buckets, 64 each way); legal when every
predecessor stays before and every successor after (order labels; O(its
constraints)); gain = change in exact pairwise seam savings.

| pairs merge | 32³ (845) | 64³ (4,484) | 128³ (18,958) | 256³ (77,570) |
|---|---|---|---|---|
| greedy (current) | 5548 | 28336 / 0.05 s | 115709 / 0.66 s | 462753 / 4.3 s |
| pairwise, full checks | 5231 | 26524 / 0.10 s | 108232 / 1.2 s | 441825 / 27 s (capped) |
| levels | 5555 | 28216 / 0.01 s | 113345 / 0.14 s | 445089 / 0.74 s |
| levels + improve | 5317 | 27213 / 0.12 s | 110248 / 1.0 s | **434651 / 6.4 s** |
| greedy + improve | 5299 | 27511 | 113000 | 452374 / 4.8 s |
| pairwise + improve | **5219** | **26376** | **107263** / 1.7 s | — |

- At 256³ levels + improve beats everything (−6.1% against the greedy).
- Improvement cost: ~100 ns per scored candidate position, ~n × 40 × passes
  (4–5 passes); no relabels.  Larger runs / candidate limits help a little
  more at 128³ (maxRun 8, 256 candidates: 109383 in 6 s).

**Equal-time comparison and restarts** (`mergeopt --bench`, one thread,
fastest of 3; user's job paused; neither the beam nor any of these merges is
multithreaded).  Combined centre merge, moves (5 seeds at 7³–11³) / time:

| N | default (beam 256; greedy at 64³) | pairwise + improve | 16 restarts | 256 restarts | widest beam tried |
|---|---|---|---|---|---|
| 9 | 336.0 / 49 ms | 333.0 / 0.04 ms | 330.0 / 1.2 ms | 329.8 / 16 ms | 330.8 (65536, ~20 s) |
| 10 | 439.6 / 66 ms | 435.0 / 0.05 ms | 430.6 / 2 ms | 428.4 / 30 ms | 431.2 (65536, ~30 s) |
| 11 | 554.0 / 116 ms | 542.8 / 0.08 ms | 536.4 / 3 ms | 534.2 / 44 ms | 541.8 (65536, ~50 s) |
| 16 | 1344 / 0.56 s | 1305 / 0.9 ms | 1305 / 16 ms | 1297–1301 / 0.26 s | 1315 (16384, 38 s) |
| 32 | 6005 / 10 s | 5652 / 10 ms | 5637–5645 / 0.19 s | 5611–5630 / 3.1 s | 5953 (1024, 42 s) |
| 64 | 30845 / 14 ms | 27482 / 0.11 s | 27369–27395 / 1.8 s | — | 29967 (256, 192 s) |

Restarts: K seeded pairwise runs (random tie-breaks, optional noise of a
fraction of a move on seam priorities), each improved, best kept.  Noise
makes little difference.  At 9³–11³, 16 restarts beat the widest beam in a
few milliseconds; at 64³ restarts add only ~0.4%.

**New default merge on the `pairwise-merge` branch** (`Merger::mergeBest`):
pairwise (levels above 20,000 instances) + improvement, then seeded restarts
(each improved) within the merge time budget, at most `--mergerestarts`
(256); `--oldmerge` gives the previous beam / greedy merge, `--pairmerge` plain
pairwise.  Whole cube, user's job paused, 16 threads:

| N | -b | seeds | old moves / wall | new moves / wall | change |
|---|---|---|---|---|---|
| 4–6 | 64 | 5 | 71.0 / 115.6 / 188.6 | same | 0 |
| 7 | 64 | 5 | 256.4 / 1.02 s | 255.8 / 1.02 s | −0.2% |
| 8 | 64 | 5 | 357.2 / 0.79 s | 355.2 / 0.77 s | −0.6% |
| 9 | 64 | 5 | 455.0 / 1.37 s | 449.0 / 1.33 s | −1.3% |
| 10 | 64 | 5 | 571.6 / 0.87 s | 560.2 / 0.81 s | −2.0% |
| 11 | 64 | 5 | 702.4 / 1.50 s | 681.6 / 1.40 s | −3.0% |
| 16 | 64 | 3 | 1550.3 / 1.83 s | 1508.7 / 1.18 s | −2.7% |
| 32 | 64 | 3 | 6445.7 / 1.89 s | 6043.7 / 3.11 s | −6.2% |
| 64 | 64 | 2 | 24923 / 3.47 s | 23456 / 5.04 s | −5.9% |
| 128 | 1 | 1 | 120314 / 1.57 s | 111189 / 2.64 s | −7.6% |
| 256 | 1 | 1 | 477755 / 2.27 s | 441916 / 4.96 s | −7.5% |

Verified (`--verify`): 4³, 5³, 9³, 10³, 16³, 33³, 64³, and 40³ `--fast`.

**Large-cube budget and threaded restarts** (branch `pairwise-merge`):
one full-budget merge per solve (with the combined merge to come, the pairs
and diagonal/mid merges get no restarts; without it the diagonal/mid merge
gets a fifth of the budget); the improvement step stops at the merge
deadline; restarts run on the -t threads (cheapest wins, ties to the lower
seed); under -2 the restart cap is at least 4 × width.  Whole cube, user's
job paused, 16 threads:

| N | -b | old moves / wall | new moves / wall | change |
|---|---|---|---|---|
| 8 | 64 | 357.2 / 0.79 s | 354.4 / 0.76 s | −0.8% |
| 10 | 64 | 571.6 / 0.87 s | 560.6 / 0.79 s | −1.9% |
| 11 | 64 | 702.4 / 1.51 s | 681.6 / 1.37 s | −3.0% |
| 16 | 64 | 1550.3 / 1.84 s | 1505.7 / 0.99 s | −2.9% |
| 32 | 64 | 6442.7 / 1.92 s | 6047.3 / 2.11 s | −6.1% |
| 64 | 64 | 24922.5 / 3.40 s | 23478.5 / 4.08 s | −5.8% |
| 128 | 1 | 120201 / 1.58 s | 111105 / 2.30 s | −7.6% |
| 256 | 1 | 476834 / 1.80 s | 442840 / 2.23 s | −7.1% |

256 restarts at 11³: 63 ms on one thread, 8 ms on 16.  Results are the same
on 1 and 16 threads unless the budget cuts the restarts short (33³).

**512³ and 1024³** (-b 1, 16 threads, seed 1, user's job paused):
512³ 1,890,804 → 1,803,961 (−4.6%), wall 6.0 → 7.4 s, 2.7 GB; 1024³
7,484,031 → 7,158,730 (−4.3%), wall 24.2 → 32.6 s, 3.8 GB.  At 1024³ the
levels construction alone takes ~21 s (not budgeted), so improvement never
runs; unbudgeted improvement would reach 6,990,485 (another −1.9%) in 425 s
(one pass: 7,004,941 in 49 s).  `--verify` passes at 128³ and 200³; at 512³
it is impractical (the sticker simulator replays every move on every sticker:
24 GB, hours).  Merged into `main` (merge commit f473051), not pushed.

## 7l. Default time budget (2026-10-03)

Default (no -b, -2, --fast; `--time S` to set, `--time 0` off): a time budget
of 10 s + 50 s × N/1024 from program start (load included).  Solves at widths
1, 2, 4, ... while these probes stay within 10% of the solve budget, then one
final run at the widest width predicted to fit 85% of what is left; best
solve wins.  Prediction: a calibrated time-vs-width table (whole cube,
`gullicube -2`, seed 1, M3 Max 16 threads, user's job paused; log-log
interpolation over widths and sizes) scaled by the last probe.  Fitted curves
(a + b·w, a + b·w^p) were 0.3–3.8× off: the curve's shape changes with N, and
small cubes have a cliff between widths 128 and 256 (4³: 0.66 s → 6.0 s).

Wall time / target with the table (one seed each):

| N | 4 | 6 | 8 | 11 | 16 | 24 | 32 | 48 | 64 | 96 | 128 | 256 | 384 | 512 | 1024 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| % of target | 123 | 155 | 65 | 132 | 102 | 127 | 132 | 94 | 74 | 120 | 84 | 84 | 82 | 83 | 57 |

At 1024³ the width-1 run alone takes 32 s and width 2 (39 s) does not fit.
Calibration (time s / moves per width) in `progress` scratch: e.g. 16³
w64 0.63 s 1502, w512 30.3 s 1419; 128³ w1 2.24 s 111173, w64 15.0 s 91375;
512³ w1 6.9 s 1805130, w8 26.5 s 1554920; 1024³ w1 32.0 s, w2 39.0 s.

Also: the beam table's weakest slot is kept in a min-tree (`mintree.h`,
O(log W) per admission instead of a rescan; identical results; no measurable
speed change), and -2 lines print the time since start.

**Beam fixes and recalibration (2026-10-03).**  The small-cube cliff was
the wing beam cycling: steps that add no moves (an algorithm cancelling
against the previous ending) keep the cost, a fixed 16k duplicate filter
overflowed at wide beams, and the search ran to its 200-level cap.  Such
steps are now refused, duplicate filters are sized to the beam, and each
wing-beam level is split across the threads left over when there are fewer
wing orbits than threads.  4³ at -b 512: 22.4 s → 0.29 s; 10³ at -b 1024:
wings 16.4 s → 2.7 s (CPU 7× wall).  Ctrl-C now reaches the wing beam, and
in time-budget mode reports the best solve so far.  Recalibrated on an idle
CPU (gate on CPU idle, not load average); wall time as % of target:

| N | 4 | 6 | 8 | 11 | 16 | 24 | 32 | 48 | 64 | 96 | 128 | 256 | 384 | 512 | 1024 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| % | 98 | 78 | 87 | 92 | 87 | 80 | 76 | 83 | 85 | 123 | 91 | 87 | 93 | 88 | 60 |

Final widths now: 4³ 5693, 8³ 1981, 16³ 904, 32³ 351, 64³ 123, 128³ 46,
256³ 23, 512³ 6; 1024³ stays at width 1 (32 s).

## 8. Open items / next steps

See `TODO.md` for the full list. The main ones:

- **Pair working pool:** the GPU study (§7j) shows the algorithm set, not the
  search, limits pair quality: try `p9` ≤ 8 moves (979k, no piece limit) in
  place of `p8s10` in the CPU solver at equal time; then 9-movers.
- **Level 1 on the start's top 4k** (a `TableBeam` hook, off): same moves at
  ~45% of the instructions; decide whether to adopt, and whether to strip the
  other off-by-default experiment hooks from `dist` (user: the distribution
  keeps the unmodified CPU code; GPU work stays in `devtools/gpu`).
- **Large-N length:** brobdicube passes our cheap quality settings from about
  512³. Our lever is spending more time: long-budget runs, `-2`, and faster
  pair beams at large N. The per-pair beam scan and the merge are the costs to
  cut.
- **Small-N gap to dwalton76:** 4³ is ahead; 5³–10³ trail by about 3–18%.
  Wings are now about 31 per orbit; odd cubes pay for mid orbits (about 30
  each) and the 3×3 phase (about 23).
- **Merge budget floor** should scale with effort (the small-N kink above).
- **regen.py** (portable Python) to rebuild every `.cls` from the three small
  inputs; `enumwing` already replaces the twsearch wing run.
- **Kociemba facelet input**, dwalton76 timing points, and brobdicube
  integration.
