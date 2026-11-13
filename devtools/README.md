# devtools: data generators, tests and benchmarks for gullicube

The solver itself is `../dist/gullicube`, the source of truth: source, data
files (`*.cls`), Makefile and README. Everything here supports it without
shipping with it. The tools build against `dist`'s headers and its copy of the
brobdicube sources (`make` here; see `Makefile`).

`SOLVER-NOTES.md` is the old solver README, with the earlier results and
design notes. The full history is in `../progress.md`.

## Data generators (how the `*.cls` files were made)

| tool | does |
|---|---|
| `mine` | reads twsearch output (`../6centers.log.xz`), collapses slice depths onto a diagonal or mid orbit, validates on several cube sizes, expands 96 ways: `diag_x10.pool`, `mid_x10.pool`. `mine check` re-validates any pool |
| `enumwing` | enumerates face + depth-2 slice sequences restoring corners and depth-3 wings (replaces the twsearch wing run) |
| `minewing` | reads such sequences, keeps small-support wing permutations, expands and validates: `wing_d6.pool`, `wing_d6c.pool` (with face-turn conjugates as input) |
| `mkclasses` | expanded pool → symmetry-class file (`.cls`), checked by re-expansion; `mkclasses compare` compares two pools |
| `mkpools` | `build`: the older diag/mid/wing pools; `diagfrom` / `midfrom`: collapse a pair pool |
| `mkcycles` | complete 3-cycle tables (`pair3`, `diag3`, `mid3` pools) |
| `mkbin` | binary caches for expanded pools |
| `xcount` | distinct keep-solved masks per pool (progress §7i) |
| `middiagcount` | feasibility count of joint mid + diagonal algorithms |

The pair pools themselves (`p9`, `p8s10`, `eg8`, `c12x`, `fast6`) come from
`../pair/gen` (see `../pair/strategy.md` and `../progress.md` §1).

## Tests and benchmarks

| tool | does |
|---|---|
| `pairdata` | random pair states; solve them with the pair beam under different scores and finish tables; `--path` dumps training data; `--algstats`, `--rootrank`, `--roottop M L`, `--firstwidth K`, `--grow g`; `BEAMTIME=1` prints per-level / per-correct-count counts and phase times; `--pool` takes `.cls` |
| `pairdeep` | the same states at growing beam widths |
| `wingbench` | random wing orbits: wing table beam vs the older wing beam, replayed on a real cube |
| `brobcheck` | replays a scramble plus a solution (`-o` output) on the independent sticker simulator |
| `layouttest`, `diaglayouttest`, `facetest`, `replaytest`, `cctest`, `wingtest` | layout and simulator checks from the early development |
| `maxgain` | best single-algorithm gain by correct count, for states on stdin |
| `pairtop` | best pairs of algorithms at the start; support-disjoint vs overlapping |
| `disjoint` | exact frontier (least moves per solved count) of support-disjoint algorithm sets, on growing prefixes |
| `prefixfreq` | which algorithms ever make the top-P prefix over random states |
| `seedbeam` | testbed: exact disjoint opening (P, S, N, X) seeding a beam (b, M, L, H, F); run from `dist/gullicube` |
| `tools/hillclimb.py` | hill-climbs the `seedbeam` tunables on instruction counts (macOS `/usr/bin/time -l`) |
| `facelettest` | Kociemba facelet reader/writer: 3³ hand checks, xcube vs simulator on random scrambles N = 2–12, round trips, rejection of impossible states (build: compile `../dist/gullicube/src/brob/facelets.cpp` with `brob.o`) |
| `mergebound` | merge headroom from a `MERGEDUMP` file: current greedy and merge beams, exact pairwise savings, depth histograms; with `tools/assignbound.py`: assignment bound and greedy-edge preview (progress §7k) |
| `mergeopt` | merge orders on a `MERGEDUMP` file: greedy, pairwise, levels, and local improvement from each (progress §7k) |
| `tools/metrics.py` | recounts a `-o` solution in layer, outer-block and block-turn metrics |

The whole-cube benchmark (`collect.py`, `frontier.py`, `scramble.py`) is in
`../bench`.

## Large intermediate files

Deleted 2026-10-07 to save disk (about 22 GB): the `.bin` caches (rebuilt on
demand) and the expanded pools that have verified class files
(`diag_x10.pool`, `mid_x10.pool` -> `dist/gullicube/data/*.cls`;
`wing_d8.pool` -> `wing_d8.cls`) and the superseded `diag_p9.pool`.  To get an
expanded pool back, expand its class file (`mkclasses` checks re-expansion).
Also deleted: the pair generator's text outputs (`pair/p9*.txt`, `eg8*.txt`,
`s8.txt`, `pair/chunks/`; gullicube uses `p9.cls` / `eg8.cls`) and four old
top-level files (`lengs.txt`, `centers1.txt`, `centers3.txt`,
`6centers.x96u.log.xz`).  `devtools/mkcycles` and `pair/fast` need their
inputs regenerated before they can run again.

## GPU experiments (`gpu/`, Metal, macOS only; experimental, not part of the release)

| file | does |
|---|---|
| `scorebench.mm` | micro-benchmark: node × algorithm scoring pairs per second |
| `gpubeam.h` | GPU beam: the `TableBeam` search with each level's scoring on the GPU (histogram + emit), host admission; optional 2-ply lookahead ranking |
| `gpucmp.mm` | compares it with the CPU beam on pair states: `gpucmp STATES W [-n k] [-c candMult] [-g] [-L] [-p POOL.cls] [-m maxLen] [-s seenScale]`; run from `dist/gullicube` |
| `gpubound.mm` | boundcheck with check 4 (every bad B state of every block solved by one of its lines) on the GPU: `gpubound STAGEDIR_A STAGEDIR_B FILE X [--threads T (2)] [--keep u\|ud] [--blocks K] [--cpu-sample N] [--drop-line B:I] [--drop-block B]`; stage B must have one target group per orbit (UD) |
| `gpuboundx.mm` | boundx on the GPU (actions and the check on the GPU, paths on the CPU): writes UD blocks for gpubound / boundcheck: `gpuboundx STAGEDIR_A STAGEDIR_B X --out FILE [--base POOL]... [--prop C (8)] [--slack L (5)] [--nosteps] [--deep S] [--deepprop C2] [--threads T (2)] [--batch K (256)] [--sample K] [--first N] [--only FILE] [--left FILE]` |
| `gpuaxbx.mm` | axbx on the GPU: writes AX blocks for gpuaxbcheck / axbcheck: `gpuaxbx STAGEDIR_3AX STAGEDIR_4OP X --out FILE[.zst] [--base POOL]... [--prop C (8)] [--slack L (2)] [--deep S] [--deepprop C2] [--threads T (2)] [--batch K (256)] [--sample K] [--only FILE] [--left FILE] [--order random\|long\|file (random)] [--seed N]`; the class order does not change the blocks |
| `gpuaxbcheck.mm` | axbcheck with check 4 on the GPU (reads .zst through zstd): `gpuaxbcheck STAGEDIR_3AX STAGEDIR_4OP BOUNDARYFILE X [--threads T] [--blocks K] [--cpu-sample N] [--drop-line B:I] [--drop-block B]` |

A multi-pass run: a broad pass (`--prop 8 --deep 2`, or `--deep 1` for UD), then `--only` passes on each pass's `--left` with
more slack / deeper two-action search, merged (each pass's blocks for the classes it covered; `gpu/mergeN.py`, pypy3) and checked.  Results and timings: scoreboard.md, journal.md.  Full documentation: `gpu/BOUNDARY-GPU.md`.

Build (from `devtools/gpu`):

    clang++ -std=c++20 -O3 -fobjc-arc -I../../dist/gullicube/src/brob -I../../dist/gullicube/src/full \
      -framework Metal -framework Foundation -o gpucmp gpucmp.mm ../brob.o -lpthread

The GPU code only reads a built `TableBeam`'s tables; nothing in `dist` depends on it.  gpubound, gpuboundx, gpuaxbx and
gpuaxbcheck need only certcube.h (same line for each):

    clang++ -std=c++20 -O3 -g -Wall -fobjc-arc -I.. -framework Metal -framework Foundation -o gpubound gpubound.mm -lpthread

