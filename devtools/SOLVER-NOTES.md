# gullicube: whole-cube solver built on brobdicube's xcube

A standalone full solver for N×N×N (any N ≥ 2). It uses brobdicube's cube data
structure, move application and corner solver unmodified, linked from
`~/brob-private/src`, and adds the pair / single-orbit beam solvers and
cross-orbit merging from `../pair`. The code is portable C++20 with no SIMD
intrinsics.

## Pipeline

1. **Corners.** On odd cubes this also covers middle edges and centre centres.
   It uses brobdicube's `solvecorners`. On odd cubes the centre centres are
   oriented first, from `xcube::ccs` (see `../brob-bugs.md`).
2. **Wings, with centres free.** Default (outside `--fast`): one table beam
   per wing depth over mined wing algorithms (`wingbeam.h`, pools
   `wing_c7.pool` + finish table `wing_d6c.pool`, built by twsearch +
   `minewing`; see `../progress.md` §7c), then the same cross-depth merge as
   below. The older solver (`wings.h`, `--oldwings`, and `--fast`):
   - Centres don't matter here, since they're solved afterwards.
   - Slice turns at wing depths are free. Face turns are used only as setups
     on a stack, undone by their exact inverse, so corners and middle edges
     are preserved.
   - Each wing depth gets its own beam, run in parallel, in the "virtual
     frame" (wings as if all setups were closed). Each action is one of:
     - push or pop a setup;
     - a single slice turn;
     - a `[slice, face]` commutator (a 5-cycle or double 3-cycle, 4 moves).
   - Parity is fixed first with one slice quarter turn per odd orbit.
   - Every beam node is also finished with brobdicube's wing 3-cycles
     (`wing.pool`, all 4,048), and the best complete solution is kept.
   - Each depth's solution is split into closed blocks (all setups undone),
     and `merge.h` interleaves blocks from different depths to cancel setups.
   - Time is linear in N. A joint beam over several depths, applying each
     commutator at every depth it helps, measured both slower and worse; it
     would need deliberate coordination of shared commutators to pay off.
3. **Oblique centre pairs** (x,y)/(y,x), 2 ≤ x+1 < y+1 ≤ N/2:
   - Each pair is solved independently, in parallel, by the table beam
     (`solvers.h` TableBeam; see "Fast path") on pool `../pair/p8s10.d.txt`.
   - **Finish table** (`FinishTable`, default except `--fast`): the Zobrist
     hash of every state that one algorithm solves, from the pair pool plus
     c12x, p9 and eg8. That is 12.1M states, built once into `finish3.cache`
     (537 MB) and read back with `fread` after that.
     - Every child the beam admits, and every node, is probed with the hash
       the beam already has, so a finish one algorithm away is seen exactly,
       at its true cost.
     - Finishing algorithms from the big pools are read on demand from their
       `.bin` files (`PoolBin`, mmap) into an extra pool; the gigabyte pools
       are never loaded.
     - Effect: 5–7% fewer pair moves at every beam width (see
       `../progress.md` §7b).
   - All solutions are merged by `merge.h`, which reorders within each
     solution's dependency lattice to maximise cancellation on shared physical
     layers.
   - The merged sequence is applied, and every oblique orbit is checked.
4. **Diagonals (a,a) and mid-centres (Mx,b):**
   - Each orbit is solved independently by a table beam (`diagBeam`,
     `midBeam`) with its own finish table (`finishdiag.cache`,
     `finishmid.cache`).
   - **Pools:**
     - `diag2.pool` (584k): every pair-pool (p9) algorithm of ≤ 8 moves with
       both slice depths collapsed onto one (3 → 2) that changes nothing but
       the diagonal orbit, plus the old 8–12-move set (`diag.pool`).
     - `mid2.pool` (313k): the same, with one slice depth sent to the middle
       layer, plus `mid.pool`.
     - The finish tables use `diag_x10.pool` (48.9M) and `mid_x10.pool`
       (5.3M): every diagonal-/mid-only algorithm up to 10 moves mined from
       twsearch's `../6centers.log.xz` by `mine` (validated on 8³–13³, even
       and odd, outer and innermost orbits). `mine check` re-validates any
       pool.
     - Built by `mkpools diagfrom|midfrom ../pair/p9.d.txt OUT` and `mkbin`.
     - The old pools had nothing under 8 moves; the new ones start at 4.
   - All solutions are merged and applied.
   - Every pool algorithm was validated by simulation to change only its
     target orbit.

Moves are one per layer turn, half turns count 1.

## Usage

Options follow brobdicube's where they overlap (`-i`, `-r`, `-R`, `-o`, `-t`).

```sh
make gullicube
./gullicube [-i | -r n | -R] [-o] [-b WIDTH] [-2] [--fast] [--finish|--nofinish] [-t THREADS] [--seed S] [--verify] N
./gullicube          # no arguments: full usage
```

The invocation is echoed as the first stdout line (`# ...`), and unknown
options are errors.

Run `gullicube` from the directory holding its data files (`*.cls`).
`python3 ../bench/scramble.py N COUNT [SEED]` prints COUNT random moves for
`-i`.

| option | meaning |
|---|---|
| `-i` | read scramble moves from stdin |
| `-r n` | scramble with n random moves |
| `-R` | scramble with 50·N random moves |
| (none of the above) | uniformly random state (brobdicube 'R' mode) |
| `-o` | write the solution, on lines starting with a space |
| `-b` | beam width for every phase (default 16; 1 with `--fast`). With `-2` it is the maximum width |
| `-2` | solve the whole cube at widths 1, 2, 4, … (up to `-b` if given; wings at 4× the width, a fresh seed per run), printing each result: width, total moves split into edges / corners + middle edges / centres, time. Keeps the best complete solve. Ctrl-C stops (the width-1 solve always completes) and the best is reported, written with `-o`, and checked with `--verify`. Watching the count fall is the fun part |
| `--grow f` | `-2`: width growth factor per step (default 2) |
| `--reps n` | `-2`: runs per width, each with its own seed (default 1). Seeds change table slots and tie-breaks in every beam (wings, pairs, diagonals, mids), so even width-1 runs differ |
| `--perpair` | `-2`, older variant: wings solved once (at `--wingwidth`, default 16), then the best solution per oblique pair is kept across runs. Measured worse than both whole-cube `-2` and one flat beam |
| `--finish` / `--nofinish` | pair/diagonal/mid finish tables (default on, except with `--fast`). The first use builds `finish3.cache`, `finishdiag.cache` and `finishmid.cache` (about 1 GB total); later runs read them back (`fread`) |
| `--diagpool f`, `--midpool f` | override the diagonal / mid pool (defaults `diag2.pool`, `mid2.pool`; the old ones are `diag.pool`, `mid.pool`) |
| `--wingpool f`, `--wingfin f`, `--oldwings` | wing beam pool and finish-table pool (defaults `wing_c7.pool`, `wing_d6c.pool`; `--fast` uses the older wing solver unless `--wingpool` is given) |
| `--diagfin f`, `--midfin f` | finish-table pools (defaults `diag_x10.pool`, `mid_x10.pool`; `f.bin` is read) |
| `-t n` | threads (default: all cores) |
| `--seed s` | random seed for the scramble (default: drawn from entropy and printed as `# seed S`, so any run can be repeated) |
| `-E` | older pair endgame search (loads the eg8/p9/c12x pools in full, several GB; superseded by the finish table) |
| `--setupcost Q` | cost of a setup face turn, in quarter moves (default 3 = ¾ move) |
| `--onemerge` / `--twomerge` | merge pairs + diagonals + mids together (default; `../progress.md` §7f) or pairs first |
| `--mergeshare f` | merge time budget = max(1 s, f × pair beam time) (default 0.5; 0 = unlimited). The greedy merge adapts how many candidates it evaluates, and the merge beam width is fitted to the budget |
| `--mergebeam W` | merge beam width (default by merge size: 256 up to 400 algorithm instances, 64 up to 1,500, else greedy; 0 = greedy) |
| `--seam` / `--noseam` | seam-aware costs in the pair/diagonal/mid beams: an algorithm's cost minus the moves cancelling with the previous algorithm (default on; `../progress.md` §7e) |
| `--verify` | replay scramble + solution on an independent sticker simulator (`../pair/cube.h`) and check every sticker; needs `-i`, `-r` or `-R` |
| `--fast` | speed over moves: greedy width 1 on the 23k pair pool (`../pair/fast6.d.txt`), no merging, and no full-cube replay (see "Fast path"). Quadratic time: 1K³ ≈ 1 s, 4K³ ≈ 10 s on 16 threads |
| `--fallback-only` | testing: solve every pair, diagonal and mid with the guaranteed 3-cycle fallback |
| `--topk`, `--index`, `--nokeep`, `--strictkeep`, `--pairpool f`, `--usage f` | comparison and diagnostic switches (see the usage text) |

**Guaranteed completion:**
- **Pairs, diagonals and mids:** after the beam and wider-beam retries, they
  fall back to a 3-cycle decomposition from complete tables (`pair3.d.pool`,
  `diag3.pool`, `mid3.pool`, built by `mkcycles`). It always terminates.
- **Wings:** always completed by the parity fix plus the complete 3-cycle
  finisher.

**Data files:** symmetry-class pools (`*.cls`, one line per class of the 48
symmetries × inverse, expanded on load; see `../progress.md` §7d). The
defaults are `p8s10.cls` / `fast6.cls` (pair beam), `c12x.cls` + `p9.cls` +
`eg8.cls` (pair finish table), `diag2.cls` / `mid2.cls` (beam),
`diag_x10.cls` / `mid_x10.cls` (finish), `wing_d6c.cls` (wings), plus
`pair3.d.pool`, `diag3.pool`, `mid3.pool`, `wing.pool`. About 33 MB in total.
`mkclasses diag|mid|wing|pair IN OUT` makes a class file from an expanded pool
and checks it.

Pools are regenerated with `mkpools` (diagonal, mid and wing pools, validated
by simulation; `mkpools diagfrom|midfrom` for the collapsed pair-pool sets),
`mkbin` (binary caches) and `genwing` (commutator wing search).

**Experiment tools:**
- `pairdata`: random pair states; solve them under different beam scores and
  finish tables; `--path` dumps states with their remaining cost.
- `pairdeep`: fixed states at growing widths.
- `tools/metrics.py`: recount a `-o` solution in layer / outer-block /
  block-turn metrics.
- `../heur/`: heuristic data, fits and the tuner. `layouttest` validates
the pair-solver ↔ xcube position mapping and the depth substitution on several
cube sizes.

## Fast path (`--fast`)

Most of the work is the oblique pair solves: there are (N/2)² / 2 of them.

- **Pair beam** (`solvers.h: TableBeam`):
  - **Structure:** a level-by-level beam whose next level is a direct-mapped
    table indexed by the child's Zobrist hash, computed incrementally. There's
    no top-k, sort or collision resolution, and width 1 is greedy.
  - **Keep-solved filter:** an algorithm is never used if it would carry a
    currently-solved piece off its face. The filter is 48 per-position
    bitsets over the algorithms; the rejected set is the OR over solved
    positions, built incrementally from the parent's set. About 95% of the
    pool is rejected per level.
  - **Scoring the survivors:** colours as 3 bit-planes. The correct count
    after an algorithm is
    `popcount(~((C0^T0)|(C1^T1)|(C2^T2)) & 48 bits)`, one popcount per
    algorithm.
  - **Visited states:** blocked by a second direct-mapped table of hashes.
- **No full-cube replay:**
  - Each pair, diagonal and mid solution is checked in its own orbit layout.
  - Pair algorithms' only later side effect, on the diagonal orbits, is
    applied directly as permutations. Those algorithms are flagged in the pool
    (` D1`), and the diagonal layout was validated for every (a,b)
    (`diaglayouttest`).
  - Wing moves are replayed on the centres only, row by row in parallel
    (`replay.h`, validated byte-for-byte against `domove`).
  - Each wing depth's sequence is checked to solve its own orbit, with face
    turns cancelling as a word modulo same-axis commutation.
- **Completion is guaranteed:** beam → wider retries → 3-cycle fallback.

`-o` builds the move list without replaying it; `--verify` forces the full
replay plus the independent sticker check.

**4K³ single run** (16 threads, quiet machine):

| | wall time | moves |
|---|---|---|
| start of the speed work | 74 s | 189.6M |
| now | **10.1 s** | 196.2M |
| brobdicube (effectively single-threaded) | 33.7 s | 86.8M |

The pair solve is about 62 µs of CPU per pair (6 µs filter, 38 µs survivor
scoring).

## Results

**Small cubes** (average of 6 random states, `-b 256`, all defaults as of
2026-09-26 evening, single-layer turn metric):

| N | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|
| moves | 67.8 | 115.5 | 181.0 | 253.2 | 342.2 | 439.0 | 562.7 |
| dwalton76 NxNxN solver (outer block turns; reduction + ~20 for the 3×3) | ~72 | ~99 | ~156 | ~214 | ~326 | ~389 | ~543 |

**Against brobdicube** (moves and time per sticker, 4³ to 2048³): see
`../progress.md` §7h and the frontier page built by `../bench/frontier.py`.

**Earlier runs** (single run each, `-b 16 -E`, before the new diagonal pools
and finish tables, independently verified):

| N | wings (per orbit) | total | brobdicube total |
|---|---|---|---|
| 20 | 388 (43.1) | 2,862 | 7,079 |
| 40 | 797 (41.9) | 10,852 | 23,373 |

- **Wing phase time:** 0.03–0.08 s, linear in N (about 0.5 s at 80³ with a
  wider beam).
- **Wing cost:** brobdicube spends about 94–98 moves per wing orbit. A
  width-256 per-depth beam reaches about 38.5.

## Open items

- **Coordinated wing sharing:** have each depth's beam produce several
  near-optimal alternatives, then choose combinations that use the same
  commutators at several depths (brobdicube's parallel trick, done
  deliberately).
- **Wings:** now the largest part at small N (131 of 617 at 10³). Shorter
  wing algorithms with centres free, plus a beam and finish table over
  distinct pieces.
- **Startup (~2 s):** eff4 is computed per pool at load (and twice for
  p8s10, via fbSolver); it could be cached in the `.bin` files.
- **Merge beam:** only the greedy merge is ported to C++; the merge beam (used
  for small N in the experiments) is not.
