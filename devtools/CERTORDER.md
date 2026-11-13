# certorder: solving the oblique pairs in a better order

`certorder.cpp` is a copy of `certsolve.cpp` (unchanged) with a choice of the order in which the oblique
chiral pairs are solved.  Plain sum only (each stage plays its certificate table line; not with
`--bound`).  Experiment, 2026-11-09.

## The idea

A pair's stage lines also move the pieces of the pairs not yet solved.  The lines only have to keep
those pieces within the sets the stage requires: U, ring and D in phase 1 (1ud + 2ud); each piece on
its face in phase 2 (3ax + 4op).  Pairs that share a slice index with the pair being solved are
moved a lot; the others only by the outer face turns.  So an expensive pair can become cheap once
other pairs are solved, and the reverse.  The question is whether choosing the order exploits this.

## Options (on top of certsolve's)

    certorder N [certsolve options] --order fixed|random|greedy|greedy1|maxfirst|lazy [--cand K]

* `fixed`: certsolve's order (the default; identical results).
* `random`: a random order per phase (a control).
* `greedy`: in each phase, solve next the pair whose phase cost is least now.  Phase cost = the first
  stage's table line + the second stage's distance after that line.  Every unsolved pair is re-costed
  after each pair is solved.  The cost a pair was chosen by is checked against what it then takes.
* `greedy1`: as greedy, but by the first stage's distance alone (one table lookup, no line played).
* `maxfirst`: the most expensive pair first (a control).
* `lazy`: greedy on stale costs: one list per cost; take a pair from the least non-empty list and
  re-cost it; solve it if still no more than that cost, else move it to its new cost's list.
* `--cand K`: cost only K random unsolved pairs per step (default: all; costing all is O(pairs^2)).

The order's random numbers come from a separate generator, so the cubes are the same in every order
(checked: the `-F` facelets are identical).  Whole-cube greedy solves pass `--verify` (N = 12).

Build (from `devtools`, against the repo's brobdicube and simulator sources):

    R=~/orbitsolve/repo/solver
    c++ -O3 -std=c++20 -march=native -pthread -Wall -I. -I$R/brob -I$R/sim -o certorder certorder.cpp \
      $R/brob/{xcube,facelets,util,cornersolver,wingsolver}.cpp

## Results

Oblique moves per pair (sum of the four stage lines, no cancellation between pairs), same cubes per
row, `--obliques -t 8`, seed 2209 (2026-11-09; the machine was shared, load 20-50):

| N | pairs per cube | cubes | fixed | random | greedy | greedy, worst | fixed, worst |
|---|---|---|---|---|---|---|---|
| 12 | 10 | 1000 | 65.23 | 65.23 (seed 1109) | 62.29 (-4.5%) | 74 | 75 |
| 20 | 36 | 200 | 65.15 | 65.25 (seed 1109) | 60.12 (-7.7%) | 74 | 75 |
| 40 | 171 | 30 | 65.21 | 65.13 (seed 1109) | 57.61 (-11.7%) | 70 | 75 |
| 100 | 1176 | 5 | 65.18 | 65.22 (seed 1109) | 54.74 (-16.0%) | 69 | 75 |

`maxfirst` is worse than fixed (N = 100: 71.62), and `random` equals fixed: the gain comes from the
choice, not from reshuffling.  The more pairs there are to choose from, the larger the gain.

Whole cube (every piece; `-M` cubes, seed 3309), total moves of the cube with the most:

| N | cubes | fixed | greedy | change |
|---|---|---|---|---|
| 20 | 5 | 3,840 | 3,621 | -5.7% |
| 40 | 2 | 14,131 | 12,876 | -8.9% |
| 100 | 1 | 84,330 | 71,982 | -14.6% |

## Time

Solving the obliques themselves is cheap (N = 100: ~0.04 s for 1176 pairs, both phases); loading the
tables takes ~2.3 s.  A full phase cost takes ~4 us (it plays the first stage's line to cost the
second stage), so costing every unsolved pair at every step is O(pairs^2): at N = 100, 692,076 costs
per phase, 5.8 s on one thread, 2.4 s on 8 threads (the same choices).  Too much for very large cubes.

Sampling K candidates per step makes it O(pairs x K).  N = 100, 5 cubes (fixed 65.18):

| order | K | mean | costing time (5 cubes, both phases) |
|---|---|---|---|
| greedy | 4 | 62.37 | 0.22 s |
| greedy | 16 | 59.53 | 0.80 s |
| greedy | 64 | 57.21 | 3.1 s |
| greedy | 256 | 55.50 | 5.8 s |
| greedy | all | 54.74 | 12.0 s |
| greedy1 | 16 | 63.57 | 0.06 s |
| greedy1 | 64 | 62.80 | 0.23 s |
| greedy1 | all | 61.58 | 1.3 s |

The second stage matters: the first stage's distance alone (`greedy1`, ~0.4 us per cost) gets less
than half the gain.  N = 40 (30 cubes, fixed 65.21): K = 4 62.43, 16 59.84, 64 58.09, all 57.61.

So with K = 16 the order costs ~16 x 2 x 4 us per pair, about 4x the oblique solve's own time, for
about half of the full gain (5.7 of 10.4 moves per pair at N = 100); K = 64 gets ~75% at ~15x.

## With the P <= 71 boundary choice (`--bound --ud 33 --ax 38`)

With `--bound` a pair whose table lines fit the budget (1ud + 2ud <= 33, 3ax + 4op <= 38) plays them;
only a pair over the budget is *searched* (alternatives: actions + line, two actions + line; ~0.1 s
for a 3ax/4op search, ~2 s for a 1ud/2ud one).  Greedy order (costed by the plain table lines) solves
the pairs that fit first, and the others are often moved under the budget by then.  Seed 2209,
obliques only, 8 threads; search time = the two phases' time (the solve itself is ~0.04 s); building
the boundary actions (~18.6 s) and loading the tables (~1.5 s) are the same in both:

| N | cubes | pairs | order | searched (1ud+2ud, 3ax+4op) | phase 1 + phase 2 time | mean | worst | over a bound |
|---|---|---|---|---|---|---|---|---|
| 12 | 200 | 2,000 | fixed | 8 + 269 | 17.9 + 25.5 s | 64.26 | 71 | 0 |
| 12 | 200 | 2,000 | greedy | 5 + 98 | 10.9 + 11.3 s | 61.98 | 71 | 0 |
| 20 | 50 | 1,800 | fixed | 12 + 237 | 30.7 + 23.1 s | 64.19 | 71 | 0 |
| 20 | 50 | 1,800 | greedy | 6 + 48 | 17.1 + 4.9 s | 59.94 | 70 | 0 |
| 40 | 10 | 1,710 | fixed | 10 + 221 | 25.3 + 21.1 s | 64.27 | 71 | 0 |
| 40 | 10 | 1,710 | greedy | 1 + 16 | 2.4 + 1.9 s | 57.49 | 68 | 0 |
| 100 | 2 | 2,352 | fixed | 20 + 325 | 44.7 + 31.7 s | 64.17 | 71 | 0 |
| 100 | 2 | 2,352 | greedy | 0 + 7 | 0.04 + 1.1 s | 54.69 | 67 | 0 |

At N = 100 the searches drop from 345 to 7 and the search time from 76 s to 1.1 s (plus 1.5 s of
costing); the whole run from 97 s to 23 s, most of it now the fixed action build.  Moves also drop.

## Not tried yet

* Steering: among a pair's equal-length alternatives (stage-symmetric copies of its line), the one that
  leaves the remaining pairs cheapest; or an extra face turn when it lowers the remaining total by more
  than 1.
* Cancellation between consecutive pairs' moves (not counted here).

## Stale costs (2026-11-10): no cheap shortcut

Re-costing only the pairs that moved is no help: every pair has pieces on every face, and every stage
line has face turns, so every unsolved pair moves at every step.  How much its cost moves (greedy,
seed 2209, N = 100, 5 cubes, cost now - cost one step earlier, per pair):

| phase | pairs | 0 | +-1 | +-2 | +-3 or more |
|---|---|---|---|---|---|
| 1 | disjoint from the solved pair | 29.0% | 35.6% | 18.8% | 16.6% |
| 1 | sharing a slice index | 22.6% | 35.7% | 21.5% | 20.2% |
| 2 | disjoint | 24.3% | 35.1% | 20.1% | 20.6% |
| 2 | sharing a slice index | 22.0% | 33.9% | 21.2% | 23.0% |

Symmetric, and nearly as large for pairs that only see the face turns as for pairs sharing a slice:
the face turns alone re-randomise the cost.  N = 40 is the same (0: 26.4% / 23.1%).

So lazy greedy (`--order lazy`, lists by cost) fails: N = 40 (30 cubes) 64.02, worst 74; N = 100
(5 cubes) 63.87, worst 73 (fixed 65.2, greedy 57.61 / 54.74), with ~2.5 re-costs per step.  A pair
whose stale cost is low has mostly drifted back up by the time it is taken; worse than `--cand 4`
(62.37 at N = 100).  The greedy's gain is picking the momentary minimum of many costs that change
every step, so it needs many fresh costs per step: `--cand K` is the right knob, and the cost of a
cost (~4 us, mostly playing the first stage's line) is what to cut.
