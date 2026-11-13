# Solving oblique centre-orbit pairs: strategy

This covers solving one pair of 24-position oblique centre orbits, (i,j) and
(j,i), on an n×n×n cube. It also covers merging many pair solutions into one
move sequence for a whole cube. Diagonal orbits, midges, edges and corners are
out of scope. Throughout, the algorithms are described on the (2,3)/(3,2) pair
of an 8×8×8; "2"-prefixed moves stand for slice i and "3"-prefixed for slice j.

All move counts are single-layer turns (a half turn counts as 1), summed over
both orbits (48 pieces), on uniformly random pair states.

## Results

Effective moves per pair after merging, with single-core time for the whole
cube:

| cube | pairs | ≤ 57 | ≤ 56 | ≤ 55 | best within 60 s |
|---|---|---|---|---|---|
| 10³ | 6 | 3.3 s | 9 s | 23 s | 54.8 (23 s) |
| 20³ | 36 | 4 s | 8 s | 18 s | 52.8 (56 s) |
| 40³ | 171 | 1.3 s | ~8–17 s | 17 s | 53.3 (38 s) |

For reference, round 1 (single-orbit isolating algorithms only) needed about
107 moves per pair, and brobdicube's master sequences about 156 at n = 19.

---

## 1. Algorithm pools (the seed set)

### Source

- **Where they come from:** twsearch2 enumerated, up to 11 moves, every
  sequence that restores everything except the (2,3)/(3,2) pair and the
  diagonal orbits. That is 26.5M canonical sequences (one per symmetry class),
  stored in `../6centers.log.xz`.
- **`gen.cpp`** processes them:
  1. It checks each canonical sequence on a full 8×8×8 sticker simulator
     (`cube.h`). Corners, edges and every other oblique orbit must be exactly
     restored; the diagonal orbits are ignored. The simulator reproduces the
     README's counts exactly (432 / 864 / 15,090 permutations at lengths
     4 / 5 / 6).
  2. It expands each one 96 ways (48 cube symmetries × inverse). The
     symmetries act on the move text; only the canonical form needs the full
     check.
  3. It reduces each result to its permutation on the 48 pair positions.
     Positions 0–23 are orbit A and 24–47 are orbit B; position p's target
     colour is (p mod 24) div 4, in U L F R B D order.
  4. It keeps one sequence (the shortest) per distinct permutation.
- **Single-orbit algorithms:** round 1's `conj12` pool (8–12 moves) is passed
  through the same generator, which maps it onto both orbits.

### Pools

| file | contents | size |
|---|---|---|
| `p6.txt` … `p9.txt` | all pair permutations of length ≤ 6 … ≤ 9 | 16k / 95k / 979k / 7.47M |
| `c12x.txt` | single-orbit algorithms, 8–12 moves, both orbits | 194k |
| `single8.txt` | single-orbit 8-movers from p8 | 6.6k |
| **`p8s6.txt`** | p8 algorithms that move ≤ 6 pieces | **121k** |
| **`p8s10.txt`** | p8 algorithms that move ≤ 10 pieces | **408k** |
| `s8.txt` | every permutation moving ≤ 8 pieces from the full ≤ 11-move data | 9.0M |
| `eg8.txt` | s8 plus the c12x algorithms moving ≤ 8 pieces | 9.2M |

### Diagonal orbits

The pair algorithms restore everything except the pair **and the diagonal
orbits** (i,i)/(j,j), which generation ignored. Many of them therefore permute
diagonal pieces: 8.9% of p8s6, 32.8% of p8s10, 29% of eg8, and about 61% of
the 9-movers. The 4- and 5-movers and every c12x algorithm leave the diagonals
alone.

- **The flag:** every pool has a flagged version (`*.d.txt`, trailing ` D0` or
  ` D1`, written by `diagflag`), which `Pool::diag` carries.
- **How often it matters:** solutions use about 0.5–1.3 flagged algorithms per
  pair.
- **What the full solver must do:** solve the off-diagonal pairs, apply the
  flagged algorithms (in merged order) to the diagonal state, then solve the
  diagonals.

### How the pools were chosen

- **The beam pools are p8s6 and p8s10.** The useful 8-move algorithms are the
  precise ones, moving 6–10 pieces. The 16–24-piece 8-movers fix more pieces on
  paper but disturb more, and adding them barely helps. p8s6 performs about as
  well as the full 1.17M p8 + c12x pool at a tenth of the size.
- **Structure of the cheap algorithms:**
  - At ≤ 5 moves, every A 3-cycle comes with exactly one B 3-cycle, and it runs
    around the antipodal corner.
  - B gets real freedom only at 8 moves (about 28 partners per A 3-cycle).
  - Every one of the 8,096 single-orbit 3-cycles appears somewhere in s8.
- **The endgame pools are eg8 and the finish table.** They are built from
  everything with small support, because the endgame looks for algorithms that
  lie inside the set of wrong pieces (§3).

---

## 2. Per-pair solve: SIMD level beam (`fast.cpp`)

### State and algorithm representation

- **State:** six 48-bit colour masks, M_c = the positions currently holding
  colour c.
- **Algorithm:** six 48-bit source masks, S_c = the positions whose piece moves
  to a slot of target colour c. These don't depend on the state.
- **Correct count after the algorithm:** `Σ_c popcount(S_c & M_c)`. That is 6
  AND + 6 POPCNT, with no gathers.

### Beam

- **Levels:** each level of the beam is one more algorithm applied; width W.
- **Candidates:** each state keeps its top 2W candidates by

  ```
  score = 4·λ·correct_after − effcost(alg)       (quarter-move units, λ = 3)
  ```

- **Global ranking:** candidates are ranked across the whole level by
  `score − effcost_so_far(parent)`, i.e. estimated total cost plus
  λ·misplaced, minimised.
- **Deduplication:** states are deduplicated with a hash set shared across all
  levels. This also stops width-1 greedy from cycling.
- **Answer:** the solution with the lowest effective cost wins, and its real
  move count is reported.

### Merge-aware cost (derating)

- **The observation:** face turns only rotate the centres on their own face, so
  they commute with one another on the centres. When many pair solutions are
  merged (§4), 67% of *setup* face turns cancel. (A setup face turn is one of
  the X moves of a literal conjugate `X c X'`.) Only about 20% of other face
  turns and 2-slices cancel, and about 1% of 3-slices.
- **The rule:** `effcost` charges each move by kind. Setup face turns cost
  `-ws` quarter moves each (ws = 2, i.e. 0.5 of a move, is best at these
  widths); all other moves cost 1.
- **The effect:** the beam now prefers conjugated algorithms. Each pair's own
  solution gets longer, but the merged total falls. At 512 pairs, for example,
  58.8 becomes 54.3.

### Endgame (every beam node with ≤ 8 wrong pieces)

1. **Direct finish:** look the node up in the **finish table**. The table is a
   Zobrist hash of every colour state that a single algorithm solves, built
   from eg8, p9 and c12x: 16.9M algorithms giving 12.1M states. Algorithm b
   solves state t exactly when t[src_b(d)] = target(d) for every d.
2. **One algorithm, then a finish:** enumerate every eg8 algorithm whose
   support lies inside the wrong set plus at most one correct position.
   - These are found through a support-mask index: for each subset of the wrong
     set (plus one extra position), of size ≤ 8, look up the algorithms with
     exactly that support.
   - The extra correct position is needed for cases like a single swap, where
     one 3-cycle has to borrow a correct piece of the right colour.
   - Keep only algorithms that make net progress, update the Zobrist hash
     incrementally, and look each child up in the finish table.
3. **Results:** on states with 10 wrong pieces, this finish averages 26.4 moves
   in 6 ms, against 27.9 for the beam. The full-depth version, used in
   `psolve.cpp` with a top-K cut, gets 24.1 in 17 ms; the exhaustive search gets
   23.8 in 2 s.

### Operating points (single pair, before merging)

| config | raw moves | ms/pair |
|---|---|---|
| greedy (W=1), p8s6 | 76.2 | 1.4 |
| W=4, p8s6 | 70.7 | 5.6 |
| W=16, p8s10 | 64.3 | 49 |
| W=128, p8s10 | 60.8 | 1300 |

With derating the raw numbers rise by 1–6 moves, and the merged numbers fall.

---

## 3. Why these choices: what didn't work

| idea | result |
|---|---|
| fixed solve orders (face-pair groups, the climbed "cheapest last ten"), weak or strict | 0–4 moves worse |
| a cheap-first greedy pass over the 4–6-move algorithms before the beam | 1.4–6 moves worse |
| penalising imbalance between the two orbits | worse at every weight tried |
| setup-conjugated copies of the short algorithms, kept one per (perm, first group, last group) | no measurable gain |
| brobdicube-style colour-at-a-time reduction chain with exact stages | ~100 moves; each stage wastes the multi-colour effect of the pair algorithms |
| wider beams on 7.5M pools, or a finish table without the enumeration | ~60 at 350–1,300 s per pair; the endgame was the real lever |
| reordering setup faces at merge time | +0.03; only 5.5% of the algorithms used have two or more setup faces |

The "last ten" probe found something real: the cheapest final sets lie on
opposite faces or on antipodal corner triples (about 12 moves against about 25
for a random set). Forcing the solve to finish on those sets didn't pay for
itself, though.

---

## 4. Combining pairs across the cube (`combine.py`)

### Model

- **Which pairs:** all oblique pairs (i,j) with 2 ≤ i < j ≤ n/2 — 6, 36 and 171
  pairs at n = 10, 20 and 40.
- **Layers:** a pair's "2" and "3" slices map to the physical layers i and j,
  in a random role order. That's valid, because each pair's scramble is
  uniform.
- **What can merge:** any moves that turn the same physical layer. Face turns
  are shared by every pair; slice j is shared by every pair that uses it,
  across rows as well as within one.
- **Cost:** the merged sequence is a stack of same-axis groups (the layers of
  one axis commute), and the cost is the number of nonzero layer turns.
- **Why interleaving is legal:** algorithms of different pairs act on disjoint
  orbits (ignoring diagonals), so any interleaving is valid.

### Dependency lattice

Within one pair's solution, algorithm j must stay after algorithm i only if
their moved-piece bitmaps overlap. That reproduces exact commutation almost
perfectly (3,444 vs 3,445 commuting pairs out of 5,049). Solutions are far from
linear: 68% of algorithm pairs commute, and the median solution has 720 valid
orderings.

### Merging

- **Greedy:** repeatedly append whichever available algorithm (from any pair,
  respecting the lattice) adds the fewest merged moves. It only needs to look
  at the tail of the sequence.
- **Merge beam (width B):** partial merges are ranked by moves saved so far and
  deduplicated on (placed set, last two groups). Width 16–64 is used for
  n ≤ 20. It gains 1–2.5 moves over greedy at n = 10–20, and about 0.7 at
  n = 40 with B = 4.
- **Setup-face order:** for `X c X'` with X face turns only, any order of X is
  valid. The merger picks the best order at append time, but this gains
  almost nothing (see §3).

### Savings (W=4 solutions, row model)

About 7% with 8 pairs, 14% with 128, and 17% with 512. Cancellation inside one
pair's own solution is only about 1%; everything else is across pairs. With the
whole-cube model and derated solutions, the saving is 9% at 10³, 16% at 20³ and
23% at 40³.

---

## 5. Performance tricks

- **NEON scan** (`scanTopK` in `fast.cpp`):
  - The pool is stored as six per-colour mask arrays (structure of arrays).
  - Each iteration handles 16 algorithms: `vandq` + `vcntq_u8` per colour give
    byte counts.
  - Three levels of `vpaddq_u8` fold these into one count byte per algorithm,
    in natural lane order; this is checked at startup.
  - Scores are computed in u16 with `vmlaq_n_u16`, and `vmaxvq` skips whole
    blocks that can't reach the current top-k threshold.
  - Speed: about 0.9 ns per algorithm, including the scoring.
- **Pools are sorted by cost,** so scalar scans (`psolve.cpp`) can stop once
  cost alone exceeds the bound.
- **Zobrist hashing:** child hashes are updated incrementally over the moved
  positions only, and the finish table uses open addressing (half full).
- **Support-mask index:** endgame candidates come from enumerating subsets of
  the wrong set rather than scanning 9.2M algorithms.
- **Symmetry at generation time:** only canonical algorithms go through the
  full sticker check; the 96 expansions are done on the 48-position
  permutation.
- **Parallelism:** pairs are solved independently, one per thread. Timings use
  per-thread CPU clocks, so they are single-core numbers.
- **Verification:** `fast` re-applies every solution to the starting state.
  `verify.cpp` separately checks psolve output on a full 8×8×8 (pair solved,
  all non-diagonal pieces outside the pair restored).

## 6. Files

| file | purpose |
|---|---|
| `cube.h`, `gen.cpp` | sticker simulator; algorithm validation, expansion and pool generation |
| `pool.h`, `seq.h` | pool loading, bitmask state, move-group representation |
| `fast.cpp` | the SIMD beam solver with endgame and derating (`-w`, `-ws`, `-M`, `-o`) |
| `eg2.h` | endgame: support-mask enumeration and the finish table |
| `psolve.cpp` | the earlier scalar solvers (level and move-count beams, deeper endgame) |
| `combine.py` | lattice analysis and merging (`--cube N`, `--mbeam B`, `--bitmap`) |
| `goal.sh`, `frontier.sh`, `fmerge.sh` | the sweeps behind the results table |
| `stage.cpp`, `chain.cpp`, `lastten.cpp`, `probe.cpp`, `egtest.cpp`, `eg2test.cpp` | experiments recorded in §3 |
