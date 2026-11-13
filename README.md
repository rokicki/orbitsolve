# orbitsolve — bespoke per-orbit centre solving for small n

> **Layout (2026-09-26):** the solver's source of truth is `dist/gullicube`
> (source, `*.cls` data files, Makefile, README); edit and build it there.
> The former `full/` directory is now `devtools/`: data generators, tests,
> benchmarks and large intermediate pools, built against `dist`'s headers.
> Older sections below still say `full/` for files that now live in one of
> the two.


A testbed for solving a single NxNxN centre orbit directly, by searching for a
short product of orbit-isolating algorithms, rather than by applying a
subsequence of a precomputed master sequence.

The intent is eventual inclusion in **brobdicube**, as an additional candidate
kind for its `bestcost` DP, to improve centre solves at small n. brobdicube's
master sequences are built to amortise one algorithm application across many
orbits at once; that is what makes its move count `O(n²/lg n)`. But the
amortisation needs orbits to spread the cost over, and at small n there are not
enough. At n=19 there are only 72 centre orbits and the measured sharing factor
is 0.70, so brobdicube pays close to full price for every algorithm and its
per-orbit cost is roughly twice what a bespoke solve needs.

Nothing here changes brobdicube's behaviour at large n, where master sequences
remain far ahead.

---

## Current status (2026-09-16)

Two rounds of work so far.

**Round 1 (2026-08-24)** — the single-orbit testbed: `orbitbench.py`,
`experiments.py`, `makepools.sh`, `pools/`. Established that bespoke per-orbit
solving beats brobdicube's master sequences by ~31% at n=19, with a crossover
around n ≈ 60. All of that still stands and is documented below.

**Round 2 (2026-09-05 → 09-09)** — pair-orbit algorithm generation, in this same
directory. This cleared what round 1 recorded as its main blocker, and the
result is much better than round 1 projected. See "Pair-orbit algorithms".

Companion state elsewhere:

- `~/brob-private` — the three `netgen.cpp` cost-model fixes from round 1 are
  **committed** (`6af0f9c`, `2122cb1`, `59267d3`).
- The re-run of the full optimization pass over `~/beam/algs4` (~128K
  algorithms) is **still pending**; `algs3/` and `algs4/` are unchanged since
  August. Those fixes shift small-n selection while leaving large-n
  bit-identical, so the re-run may be scopeable to the low end only.

---

## The problem

A centre orbit is 24 sticker positions holding 4 cubies of each of 6 colours.
Solved means position `i` holds colour `i//4`, in Speffz order (U L F R B D,
four positions per face). A uniformly random orbit has 20 of its 24 positions
wrong.

Slice moves couple orbits: a slice at depth *i* touches **every** orbit in
column *i*. So an orbit cannot be solved with raw slice moves without wrecking
its neighbours. The algorithms used here are therefore *orbit-isolating* —
sequences that permute stickers inside exactly one orbit and are the identity on
every other orbit, for every cube state.

That constraint is the whole story of what follows. It is also stronger than it
needs to be, which is the main avenue for further work (see below).

---

## What we measured

### Isolation has a hard price, and no economy of scale

From an exhaustive search over orbit-preserving sequences (not just
commutator-shaped ones), the cheapest algorithms by cycle structure are:

| cycle length | cheapest | typical | moves/piece |
|---|---|---|---|
| 3 | 8 | 8–12 | 2.67 |
| 5 | 8 (only 8 exist) | 14–16 | 1.6 – 2.8 |
| 7 | 14 | 16 | 2.0 – 2.29 |

A 7-cycle costs 14–16 moves, not 8. Moving more pieces per algorithm does not
get cheaper per piece: everything lands at 2.0–2.7 moves per piece placed. With
~20 misplaced stickers that puts a hard floor of roughly **40–48 raw moves per
orbit** on any method built from unconditionally-isolating algorithms.

### The bespoke solve is worth ~1/3 of brobdicube's centre cost at n=19

| | moves/orbit | n=19 centres (72 orbits) | n=4 centres (1 orbit) |
|---|---|---|---|
| brobdicube master sequences | 78.2 effective (111.6 raw × 0.70 sharing) | 5633 | 92.4 |
| bespoke beam-64, 96,832-alg pool | **53.8** | **3874 (−31%)** | **53.8 (−42%)** |
| atom-limited floor | 40–48 raw | — | — |

The bespoke figure is from the largest clean sample: 200 random orbits, beam 64,
`conj12` — **199/200 solved (99.5%)**, 53.8 moves, 6.56 algorithms, 2163 ms
per orbit. Smaller samples ran 52.6–53.0 moves; treat ~53–54 as the number and
see "guaranteeing completeness" under further work for the missing 0.5%.

brobdicube needs 11.8 algorithm activations per orbit because a fixed ordered
master sequence cannot match an orbit's actual state; the bespoke search needs
about 6.4. The atoms, the reduction chain and the master-sequence depths are all
already at their frontier — the gap is entirely the fixed-sequence constraint.

### There is a clean crossover around n ≈ 60

Bespoke cost is flat in n (no sharing to gain). brobdicube's per-orbit cost
falls as orbits multiply:

| n | 19 | 32 | 64 | 128 |
|---|---|---|---|---|
| brobdicube moves/orbit | 78.2 | 65.7 | 49.6 | 39.4 |
| bespoke moves/orbit | 53.8 | 53.8 | 53.8 | 53.8 |

So bespoke wins below n ≈ 60 and loses above it. That is exactly the shape
`bestcost` already resolves per (Mx, My), so the crossover should be chosen by
the DP rather than hard-coded.

### Conjugations matter far more than raw algorithm count

| pool | algorithms | moves/orbit (beam-64) |
|---|---|---|
| orientation expansion only, ≤17 moves | 598,806 | 56.8 |
| conjugated, ≤12 moves | **96,832** | **53.0** |
| conjugated, ≤14 moves | 592,186 | 53.0 |

96K conjugated algorithms beat 599K unconjugated ones. And ≤14 is **not** better
than ≤12 despite 6× the data and 7× the runtime — the `cost + misplaced` beam
ranking gets crowded with near-duplicates. Uncapped, the expansion chain emits
80.5M algorithms / 6.6 GB; that pool is not merely unnecessary but
counterproductive at this beam quality. **`conj12` (5.3 MB) is the recommended
operating pool.**

### Minimum pool for a 100% solve rate is ~5,000 — but quality needs far more

Solve rate and mean moves, by pool size and beam width:

| pool | W=1 | W=4 | W=16 | W=64 |
|---|---|---|---|---|
| 500 | 0% | 0% | 0% | 2% |
| 1,000 | 0% | 0% | 0% | 14% |
| 2,000 | 0% | 4% | 48% | 94% |
| 5,000 | 8% | 20% | **100%** 116.5mv | **100%** 95.2mv |
| 10,000 | 30% | 84% | **100%** 85.0mv | **100%** 76.0mv |
| 20,000 | 50% | **100%** 83.2mv | **100%** 72.6mv | **100%** 67.5mv |
| 50,000 | 88% | **100%** 67.8mv | **100%** 61.0mv | **100%** 57.6mv |
| 96,832 | 90% | **100%** 60.8mv | **100%** 56.8mv | **100%** 52.6mv |

Two separate thresholds: ~5,000 algorithms with beam ≥16 for **solvability**,
but pool size keeps buying **quality** long past that — about −13% per doubling,
and still not flattened at 96,832. Greedy (W=1) never reaches 100% at any pool
size; it plateaus around 90%.

The "100%" cells above are 50-orbit samples. Over 200 orbits the best
configuration measures **99.5%** (199/200), so read those cells as "no failures
in 50", not as a guarantee — the residual is real and needs handling.

*Caveat:* the W=1 and W=4 columns were run with a 14-algorithm depth cap, which
fails small pools that simply need more applications. The W=16/W=64 columns at
≤10,000 are from a rerun with a 40-algorithm cap and are the trustworthy ones.
`beam_solve(max_depth=...)` defaults to 40 now.

### Speed (numpy; a C++ port should be ~25× faster)

ms/orbit as measured by `experiments.py speed`:

| pool | W=1 | W=4 | W=16 | W=64 |
|---|---|---|---|---|
| 5,000 | 10.7 | 45 | 151 | 443 |
| 10,000 | 18.7 | 63 | 158 | 429 |
| 20,000 | 32 | 104 | 199 | 593 |
| 50,000 | 76 | 215 | 405 | 1,264 |
| 96,832 | 143 | 279 | 903 | 2,200–3,800 |

Cost scales about linearly in `width × pool`; quality improves sub-linearly, so
the frontier is unforgiving. The best-quality cell is ~2.2 s/orbit in numpy,
which is far too slow to ship — but the inner loop is a poor fit for numpy, not
inherently expensive (see below).

### The ">=2 fixed" prefilter: real, but small, and not realisable in numpy

Restricting candidates to algorithms whose support contains at least two
currently-misplaced positions is a valid pruning rule. Its survival curve:

| misplaced | 20 | 17 | 14 | 11 | 8 | 5 | 2 |
|---|---|---|---|---|---|---|---|
| pool surviving | 99.7% | 98.5% | 94% | 84% | 64% | 34% | 4.8% |

It prunes almost nothing early and a great deal in the endgame. A solve walks
20→17→14→11→8→5→2, so the ceiling is around **32% fewer gain evaluations** —
worth having, not transformative. There are 18,644 distinct support masks across
96,832 algorithms (5.2 algorithms each), so the test itself can be done per
bucket rather than per algorithm.

In numpy it is a net **loss**: `MOVED[rows]` / `SRCS[rows]` allocate fresh arrays
each beam state and the gather costs more than the arithmetic it avoids. The
code keeps `min_fix` as an option for a C++ port to use, where iterating buckets
and skipping is free.

---

## Pair-orbit algorithms (round 2)

Round 1 concluded that the biggest remaining lever was to stop insisting on
*unconditional* isolation: solve the (i,j) and (j,i) orbits together, using
algorithms that preserve everything **except that pair**. It recorded the
blocker as tooling — `algutil -f KEY` filters on faces, not orbits, so it cannot
express "preserves everything except orbits (i,j) and (j,i)".

That is now solved, by going to twsearch2 directly with puzzle definitions that
give **each orbit its own set**, so individual orbits can be required-solved,
ignored, or merely documented.

### Caution: the diagonal orbits are *not* preserved

The generation run (`8.txt`'s `SSSIDSSDSISI` mask) and `pair/gen.cpp` require
every orbit to be restored except the pair **and the diagonal orbits**. The
diagonals, (i,i) and (j,j) — (2,2) and (3,3) on the reference 8×8×8 — are
*ignored*, so many pair algorithms permute diagonal pieces. `pair/diagflag`
measures this on the 48 diagonal positions:

| pool | algorithms | move diagonal pieces |
|---|---|---|
| lengths 4–5 | 1,296 | 0% |
| length 6 / 7 / 8 / 9 (from `p9.txt`) | 15k / 79k / 884k / 6.5M | 41% / 44% / 49% / 61% |
| `p8s6.txt` (fast-path beam pool) | 121k | 8.9% |
| `p8s10.txt` | 408k | 32.8% |
| `eg8.txt` (endgame enumeration) | 9.2M | 29.0% |
| `c12x.txt` (round-1 single-orbit) | 194k | 0% |

In actual solutions, 6–15% of the algorithms used move diagonal pieces (about
0.5–1.3 per pair); 28–57% of pairs touch none.

**Consequence for a full solve:** the off-diagonal pairs must be solved first.
Then the diagonal state must be updated by the solution's effect before the
diagonals are solved. Algorithms flagged clean are the identity on the
diagonals, so only the flagged ones need to be applied, in final merged order.
Cancellation at seams doesn't change the net effect, so this is exact.
Reorderings allowed by the merge lattice can change the diagonal outcome but
never its correctness, because the diagonals are solved afterwards from
whatever state results.

**Flag:** `diagflag -w OUT POOL` appends ` D0` (diagonals untouched) or ` D1`
to every pool line. The flagged pools are `pair/*.d.txt`. `pair/pool.h` reads
the column into `Pool::diag`; lines without it default to 1, i.e. "assume it
moves them". `pair/fast` loads the flagged endgame pools, reports
diagonal-moving algorithms per pair, and marks them with a trailing `*` in its
`-o` output.

### The result

Algorithms affecting **both** orbits of the pair, by length:

| length | count | pieces moved (orbit 1 + orbit 2) | moves/piece |
|---|---|---|---|
| 4 | 432 | 3 + 3 | **0.67** |
| 5 | 864 | 3 + 3 | 0.83 |
| 6 | 6,096 | 3 + 3 | 1.00 |
| 6 | 354 | 4 + 4 | 0.75 |
| 6 | 5,184 | 5 + 5 | 0.60 |
| 6 | 3,456 | 6 + 6 | **0.50** |

**16,386 algorithms at ≤6 moves.** Against round 1's measured floor of 2.0–2.7
moves per piece for unconditional single-orbit isolation, these are **4–5×
cheaper per piece**. That is a much larger margin than round 1 guessed, and it
is the strongest result in this directory.

The same data cross-checks round 1: `trips.txt` rows with a zero column (one
orbit untouched) first appear at length 8 for a 3-cycle — exactly the
single-orbit floor measured independently in round 1 from `bigseed.algs`.

### The pipeline

```sh
# 1. generate, with one twsearch set per orbit
xz -d < 6centers.log.xz | ~/twsearch2/build/bin/twsearch --quiet \
    --distinguishall --nocorners \
    --omit CENTERS --omit CENTERS2 --omit CENTERS5 --omit CENTERS9 \
    --showpositions --compact 8.tws

# 2. expand each algorithm 96 ways (48 orientations x inverse)
perl x96.pl < ...            # replaces algutil -xu for this pipeline

# 3. drop known-bad entries by line number
perl good.pl                 # filters 6centers.log.xz against bad.index

# 4. summarise which faces of each of the two orbits an algorithm disturbs
perl affect.pl < ...         # reads --showpositions output, 2 x 24 positions
```

### Files

| file | what it is |
|---|---|
| `6.tws`, `8.tws` | twsearch2 puzzle definitions; one set per centre orbit |
| `8.txt` | documents `8.tws`'s sets, the `SSSIDSSDSISI` solve/ignore/document mask, and the exact twsearch invocation |
| `6centers.log.xz` | raw twsearch output (108 MB) |
| `6centers.counts` | its length distribution: 19@4, 36@5, 649@6, … 23.3M@11 |
| `6centers.x96u.log.xz` | the 96-way expanded set (1.4 GB) |
| `t.txt` | the 19 length-4 algorithms, e.g. `2B' 2D 2B 2D'` |
| `t.counts` | (count, length, total pieces moved across the pair) |
| `trips.txt` | (count, length, pieces moved in orbit 1, in orbit 2) |
| `centers1.txt`, `centers3.txt`, `lengs.txt` | derived bulk data (1.3–1.8 GB each) |
| `x96.pl`, `good.pl`, `affect.pl`, `bad.index` | expansion, filtering and analysis helpers |
| `fours.alg`, `foursx.algs`, `*.cpos` | earlier 4-move exploration |

Note the bulk `.txt` / `.log.xz` files are ~11 GB in total and are regenerable
from the `.tws` definitions; they are not worth backing up.

---

## Layout

| file | purpose |
|---|---|
| `orbitbench.py` | library: pool loading, `Pool`, `beam_solve`, `greedy_solve`, `benchmark` |
| `experiments.py` | drivers for the quality / speed / filter / compare experiments |
| `makepools.sh` | regenerates the algorithm pools from `brob-private/algutil` |
| `pools/*.costalgs` | generated pools (`conj12` is the recommended one) |

There is nothing to build — it is Python plus numpy. The pools are generated by
`algutil` from the `brob-private` tree.

## Generating the pools

```sh
./makepools.sh                      # assumes ~/brob-private
./makepools.sh /path/to/brob-private /path/to/out
```

This runs, for each cap N:

```sh
algutil -g -U < bigseed.algs | algutil -xu | algutil -m N -c -d 1 -pc
```

`-g -U` applies the 18 face-move conjugations (deduplicated globally by symmetry
class), `-xu` expands to all 48 orientation conjugations, and `-m N` caps the
move count during the cost-annotating pass. `-d 1` makes the cost field a plain
move count rather than the packed v/h/o/c fields that `-d 1000` produces.

**Always pass `-m`.** Without a cap the chain emits 80.5M algorithms and 6.6 GB,
takes minutes, and produces a pool that is worse than the 5.3 MB one.

Runtime is a few minutes per cap, dominated by the uncapped intermediate stream.

## Running the experiments

```sh
python3 experiments.py quality [POOL]   # pool size x beam width -> solve%, moves
python3 experiments.py speed   [POOL]   # pool size x beam width -> ms/orbit
python3 experiments.py filter  [POOL]   # survival curve of the >=2-fix prefilter
python3 experiments.py compare          # conjugated vs unconjugated pools
```

`POOL` defaults to `pools/conj12.costalgs`. Cells in `speed` are time-boxed, so
expensive ones report fewer orbits — the `orbits` column says how many actually
ran, and solve percentages from small samples are noisy.

Using the library directly:

```python
from orbitbench import Pool, load_pool, beam_solve, benchmark, random_orbit
import random

pool  = Pool(load_pool("pools/conj12.costalgs"))
state = random_orbit(random.Random(0))
moves, algs, solved = beam_solve(pool, state, width=64)

print(benchmark(pool, width=64, orbits=200))
```

---

## Further work

### 1. Pair-orbit algorithms (the biggest lever)

Solve the (i,j) and (j,i) orbits together, using algorithms that must preserve
everything **except those two orbits**. This attacks both factors at once:

- **Shorter algorithms.** The 8-move floor for a 3-cycle is the price of
  isolating one orbit from all others unconditionally. Allowing free disturbance
  of the partner orbit is a much weaker constraint, so the moves-per-piece floor
  should drop below 2.0.
- **Half the searches.** One search solves two orbits, halving per-orbit search
  cost independently of any move-count gain.

This is what Kociemba's solver does — his phase-1 oblique search carries four
coordinates (`ccx`, `ccy` for the orbit pair, `slx`, `sly` for the shared
slices) and prunes on the joint table `UDCentBrick256Prun[C(24,8)*slx + ccx]`,
188 MB. He requires the slice coordinate to return to solved rather than using
pre-isolated algorithms, which is why his per-orbit cost sits *below* the
unconditional-isolation floor.

**Generation is done** (round 2 above): twsearch2 with one set per orbit yields
16,386 algorithms at ≤6 moves affecting both orbits, at 0.5–0.67 moves/piece.
The moves-per-piece floor did not merely "drop below 2.0", it dropped by 4–5×.

What remains is the **solver**, and the intended shape is two-phase:

1. **Bulk phase.** Use the small pool of 4- and 6-move pair algorithms to fix as
   much of the pair as possible. Greedy is likely sufficient here — the pool is
   tiny (thousands, not 10⁵), the algorithms are cheap, and there is plenty of
   misplaced material early, so almost anything makes progress.
2. **Cleanup phase.** Finish each orbit separately with the longer single-orbit
   pool (`pools/conj12.costalgs`) and the existing `beam_solve`.

Three things follow from that split, all favourable:

- The expensive beam only ever runs on the **residue**, not on a fully scrambled
  orbit. Round 1's cost is dominated by the early steps, so shrinking the input
  to the beam attacks exactly the expensive part.
- It **rehabilitates the `≥2-fix` prefilter**, dismissed above as marginal. Its
  survival curve is 84% at 11 misplaced but 34% at 5 and 4.8% at 2 — it only
  pays in the endgame, which is precisely what phase 2 is.
- It sidesteps needing a *complete* pair-orbit set. Phase 1 may stall whenever
  it likes; phase 2 is complete by construction.

Open questions for the solver: how far down phase 1 can push the misplaced count
before stalling, whether the phase boundary should be a fixed threshold or
chosen by cost, and whether phase 1 wants the ≤4, ≤5 or ≤6 pool (more choice
versus more moves per application).

For reference, this is the same weaker-constraint idea Kociemba's solver
exploits, though he does it by search rather than by pre-generated algorithms —
his phase-1 oblique search carries four coordinates (`ccx`, `ccy` for the orbit
pair, `slx`, `sly` for the shared slices) and prunes on the joint table
`UDCentBrick256Prun[C(24,8)*slx + ccx]`, 188 MB, requiring the slice coordinate
to return to solved. That is why his per-orbit cost sits below the
unconditional-isolation floor.

### 2. A better beam heuristic

The ranking is `cost so far + misplaced count`. Misplaced count is a weak lower
bound when supports reach 6 — a single algorithm can fix 6 stickers, so the true
remaining cost is nearer `misplaced/6 × 8`. A tighter admissible bound would
improve quality at fixed width *and* let the larger pools pay off instead of
hurting, which is currently the reason `conj14` loses to `conj12`.

This is probably the cheapest remaining win.

### 3. C++ port

The inner loop is a bad fit for numpy but very cheap in C++. Represent the orbit
state as 6 colour bitmasks and each algorithm as 6 precomputed source-masks;
then per algorithm:

- `after  = Σ_c popcount(colourmask[c] & algmask[c])` — 6 AND + 6 POPCNT
- `before = popcount(correct & support)` — 1 AND + 1 POPCNT

That is ~7–10 cycles with ILP and **no memory gathers at all**, against numpy's
two `(N, S)` gathers plus comparisons. Estimated ~90 ms/orbit at W=64 / 96,832
versus the measured 2.2 s — roughly 25×. Bucketing by support mask (18,644
buckets) then adds the ~30% from the prefilter.

Indicative C++ operating points:

| config | moves/orbit | est. ms/orbit | n=19 centres | n=19 total |
|---|---|---|---|---|
| W=64, 96,832 | 53.0 | ~90 | 3816 (−32%) | ~6.5 s |
| W=16, 50,000 | 62.2 | ~11 | 4478 (−21%) | ~0.8 s |
| W=64, 20,000 | 69.3 | ~18 | 4990 (−11%) | ~1.3 s |

The full 32% costs ~65× brobdicube's current whole-solve time; the cheap end
gets 21% in under a second. Which is right depends on whether small-n solves
need to be interactive.

**Incremental gain updates — probably the largest multiplier available, and not
yet tried.** The beam currently re-evaluates the entire pool independently for
every beam state, but beam states are near-duplicates: they differ by one
algorithm, which touches only ~6 of the 24 positions. With an inverted index
from position to the algorithms whose support contains it, applying a move only
invalidates the gains of that small slice of the pool; everything else carries
over unchanged. This compounds with the support-mask bucketing and the `≥2-fix`
prefilter rather than competing with them, and unlike those two it is not capped
at ~30% — the fraction of the pool touched by any single application is small.

Note that the two-phase pair-orbit design changes the speed target as well as
the move count, since phase 2's beam runs on a much reduced problem. Worth
re-measuring the speed grid once phase 1 exists rather than optimising the
round-1 numbers.

### 4. Guaranteeing completeness

The best configuration solves 199/200. A solver that abandons one orbit in 200
is not usable, so the residual needs a fallback: widen the beam and retry, allow
a deeper plan, or fall back to the master sequence for that orbit (which is
always complete by construction). Worth characterising the failures first —
whether they are a particular kind of orbit state or just beam bad luck — since
a cheap retry with a different tie-break may be all that is needed.

### 5. Cross-orbit sharing

Bespoke solves currently forfeit sharing entirely, which is why a 2× raw
advantage becomes 32% at n=19. Two orbits in the same row that happen to want
the same algorithm could still share its `h` and `o` moves. Nothing looks for
this today; it would narrow the gap and push the crossover above n = 60.

### 6. Integration into `bestcost`

Add bespoke solving as a third candidate kind alongside master sequences, with
its own cost function — `Mx·My × moves_per_orbit`, no sharing term, no
conjugation-wrapper term. The DP then picks the crossover per (Mx, My) rather
than anyone hard-coding n ≈ 60. n=4 is the clean test case: with a single orbit
there is no sharing to lose, so bespoke must win outright there or the premise
is wrong.
