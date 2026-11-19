# Journal of the orbit upper-bound work

Everything as it happened, verbatim: runs, measurements, approaches tried and dropped, old versions of the
summary files.  The brief files are upperbound5.md (results), methods.md (how) and twsearch.md.  New entries go
at the end, under "Running log".

Contents:
- Parts 1-3 and the results log: the former history.md (old upperbound.md, upperbound2.md, upperbound5.md
  as of 2026-10-29, and the dated results log that followed).
- Part 4: upperbound3.md as of 2026-11-03 (the P 80 record with the rings, UD stages and methods).
- Part 5: upperbound4.md (the parked front + horseshoe split, 3fr/4h).
- Part 6: upperbound5-notes.md as of 2026-11-03 (3ax/4op working details, 2026-11-02..03).
- Part 7: upperbound5.md as of 2026-11-03 (the short summary before upperbound5.md).
- Running log (from 2026-11-03).

# Former history.md

# History of the orbit upper-bound work

Superseded notes, kept verbatim.  Current results: upperbound3.md (the record: whole cube, the oblique
chiral pair P 80), upperbound5.md (working notes for the axis split 3ax/4op), upperbound4.md (the
parked front + horseshoe split).

Contents:
1. The original approach (upperbound.md, 2026-10-09 to 2026-10-22: certified P 87).
2. Relaxed stages (upperbound2.md, from 2026-10-23).
3. The axis split, 3ax/4op (upperbound5.md as of 2026-10-29 22:10): run logs and discussion.

# 1. The original approach (was upperbound.md)

## Upper bounds for solving one centre orbit

Question: with everything else solved, how many moves (single-layer turn
metric: every turn of any one layer, quarter or half, counts 1) can it take to
solve one centre orbit using only moves that leave everything else alone?
Same-coloured centre pieces are interchangeable.  The easy bound is 144
(at most 12 three-cycles, each a commutator of at most 12 moves).

### Whole cube: best bounds (2026-10-22)

| n | bound (single-layer turn metric) |
|---|---|
| even | **(87 n^2 + 302 n - 864) / 8** = 87 (n-2)(n-4)/8 + (n-2)/2 (D + W) + 11 |
| odd | **(87 n^2 + 284 n - 1475) / 8** = 87 (n-3)(n-5)/8 + (n-3)/2 (D + M + W) + 20 |

Checks: n = 4 gives 44 + 162 + 11 = 217; n = 5 gives 44 + 39 + 162 + 20 = 265; n = 6 gives 510;
n = 7 gives 597 (P = 88 gave 511 and 598; P = 95 gave 518 and 605).
Both polynomials are now fully certified.

**Centres only** (obliques, then mids, then diagonals; edges and corners ignored), the same
certified constants without the corner, midge and wing terms:

| n | bound (single-layer turn metric) |
|---|---|
| even | **(n-2)(87 n - 172) / 8** = 87 (n-2)(n-4)/8 + (n-2)/2 D |
| odd | **(n-3)(87 n - 103) / 8** = 87 (n-3)(n-5)/8 + (n-3)/2 (D + M) |

In general P (n-2)(n-4)/8 + D (n-2)/2 (even) and P (n-3)(n-5)/8 + (D + M)(n-3)/2 (odd), with P = 87,
D = 44, M = 39.  Checks: n = 4: 44; n = 5: 83; n = 6: 175; n = 7: 253; n = 8: 393; n = 9: 510.

Order: oblique pairs (centres first), mids, diagonals, then corners (and midges on odd n), then
wings.  With k = (n-2)/2 (even) or (n-3)/2 (odd) there are k(k-1)/2 oblique pairs, k diagonal
orbits, k mid orbits (odd), k wing orbits.

| constant | value | basis | status |
|---|---|---|---|
| P, oblique pair (centres first) | 87 | (19 + 24 = 43, boundary pass 41) + (21 + 29 = 50, boundary pass 46), stages of 2026-10-22 (88 = 42 + 46 on 2026-10-17); rings 45 dropped 2026-10-23 for the relaxed stages (see upperbound2.md) | **certified** (cert/obl23pair/cf-xfree: certcheck PASS on the four stages, boundcheck PASS on the two boundaries; pairdepth: valid for every pair (p, q)) |
| D, diagonal (centres first) | 44 | D 10, U 16, LFRB 18 | **certified** (cert/diag/centres-first, diagcheck PASS on all three stages, every N 4..12 and every diagonal depth) |
| M, mid (centres first) | 39 | UD 18, LFRB 21 | **certified** (cert/mid/centres-first, midcheck PASS on both stages, every odd N 5..11 and every mid depth; no line even changes a diagonal colour) |
| W, wing orbit keeping centres | 162 | 32 + 32 + 40 + 58 | **certified** (cert/wing/centres-kept, wingcheck PASS on all four stages) |
| corners (even n) | 11 | exact | cornerbfs: full BFS of the corner group under face turns, every position within 11 |
| corners + midges (odd n) | 20 | God's number of the 3x3x3 | the leftover is always a legal 3x3x3 position (see below) |

P, D, M and W are certified, so both polynomials are certified (given the exact corner search
and God's number).

**Wings keeping centres (W = 162).**  Wings are solved last, so a wing algorithm must keep every
centre colour-solved and leave corners, midges and the other wing orbits in place.
`devtools/wingcentres` filters a wing pool for exactly that on every size 6..12 and every slice
depth (independent facelet model): all 49 wing3 classes pass (every wing 3-cycle, 8-13 moves); of
wing_d6c's 13,723 classes only 2 pass, of wing_d8's 110,993 only 8 (short wings-first algorithms
scramble centres).  The parity algorithm `2R2 B2 U2 2L U2 2R' U2 2R U2 F2 2R F2 2L' B2 2R2`
(15 moves) passes too, so odd wing permutations are reachable.  wingbound, chunks 4,4,5,11:
32 + 32 + 40 + 60 = 164; with face-turn conjugates of every algorithm 32 + 32 + 40 + 58 = **162**.
No merges counted.

Wing certificate (2026-10-12): `cert/wing/centres-kept/` (4.7 GB), made by `wingbound4` with
`CERT=DIR` (pools and run log in `certruns/wing-centres-kept/`).  Wings are distinct pieces, named
'a' + k in certcube.h's `wingFacelets` order; stage directories are `<placed>..<target>` wing
letters, segments `wNN.txt` by target wings away from home; one solving sequence per line for
every stage position (no symmetry reduction).  `devtools/wingcheck STAGEDIR` (certcube.h only)
checks each line's moves, that placed wings stay home, the segment, that every stage position
occurs exactly once, and side effects on 6x6x6..10x10x10 at every wing depth (14 cases): centres
keep their colours, corners, midges and other wing orbits stay in place, and the depth-d wings
move as on the 6x6x6 reference.  Negative controls fail as they should (a corrupted and a dropped
line; 200 wings-first algorithms all flagged).

| stage | positions | worst | 
|---|---:|---:|
| none .. abcd | 255,024 | 32 |
| abcd .. efgh | 116,280 | 32 |
| abcdefgh .. ijklm | 524,160 | 40 |
| abcdefghijklm .. nopqrstuvwx | 39,916,800 | 58 |
| **total** | | **162** |

Joining adjacent turns of one layer leaves only 9 positions of the last stage at 58 and 2,334 at
57 (cert/wing-57plus.seq.txt): a target for the ML agent.

(The wings-first wing bound 77 in the table below is a different regime:
there centres may be disturbed.)

**Parity and the last stage.**  Wing parity is absorbed in W (the parity algorithm is in the pool).
Even n: no OLL/PLL parity, corners are any face-turn position, within 11.  Odd n: the centre and
wing stages turn only faces and non-middle slices, apart from the mid algorithms, which restore
the middle centre; so corners and midges are always a legal 3x3x3 position, within 20.  Damage
to edges, corners and diagonals by the centres-first pair sequences is free: later stages accept
any arrangement.

#### Wings by 3-cycles: W = 144 + 5 per orbit, + 4 once (sketch, 2026-10-13; not yet certified)

A simpler wing reduction beats the staged 162; it needs no state-space certificate.

1. **Bulk parity fix (4 + 5x moves).**  For the set X of wing orbits whose permutation is odd
   (x = |X|), play `S D2 S D2 S D2 S D2 S`, where S is a quarter turn of every slice dF, d in X
   (x moves).  For one orbit this is `2F D2 2F D2 2F D2 2F D2 2F` (9 moves).  It keeps every
   centre colour-solved (checked 4..12), leaves corners and midges in place, and flips the
   parity of exactly the orbits in X (checked on 7x7x7 and 8x8x8).  It scrambles those wings
   arbitrarily, which costs nothing: the wings come next.
2. **Even permutation of 24 wings: at most 12 three-cycles.**  Peel each cycle with 3-cycles that
   each fix two more wings: a k-cycle takes (k-1)/2 if k is odd; even cycles come in
   pairs (the permutation is even), and a pair of lengths k1, k2 takes (k1 + k2)/2 (peel each
   down to a swap, then two 3-cycles across the two swaps).  The total is
   (24 - number of odd-length cycles)/2 <= 12; 12 disjoint swaps need all 12.
3. **Every wing 3-cycle within 12 moves, keeping centres.**  twsearch (6x6x6, moves F 2F B 2B
   U 2U D 2D L 2L R 2R, centres identical within a face) found optimal versions of all 49
   symmetry classes: 4,048 3-cycles at 8:2400 9:912 10:640 11:48 12:48 (brob's pool had two
   13-move classes; both drop to 10).  All 148 solutions pass wingcentres on 6..12.  Pool:
   certruns/wing-centres-kept/pools/wing3opt_cp.cls.  Dropping the corner requirement
   (`--nocorners`) does not shorten the worst class.

So each wing orbit costs at most 5 + 144 = 149, plus 4 once per cube: **W = 149**, constant + 4.

| n | bound with W = 149, + 4 |
|---|---|
| even | (87 n^2 + 250 n - 728) / 8 = 87 (n-2)(n-4)/8 + (n-2)/2 (D + 149) + 15 |
| odd | (87 n^2 + 232 n - 1287) / 8 = 87 (n-3)(n-5)/8 + (n-3)/2 (D + M + 149) + 24 |

Checks: n = 4 gives 208, 5 gives 256, 6 gives 488, 7 gives 575, 8 gives 855, 9 gives 981.

To certify: the 3-cycle list (one line per 3-cycle, or per class), a checker that replays each
on 4..12 (wing 3-cycle, centres, corners, midges, other orbits), the fixer checked for every
subset X on 4..12, and the counting argument above.  Things that only change the constant are
set aside: double swaps (one class at 14, all within 24 as two 3-cycles), the 15-move single
swap of partner wings (the other ten single-swap classes: swap class 0, wing (-3,5,-5) with
(5,5,-3), has no solution within 15 moves; the run was stopped at depth 16), stage searches.

### Results so far (2026-10-12)

| orbit | bound | stages (worst case each) | status |
|---|---|---|---|
| diagonal (X-centres; all centres of 4x4x4) | **54** | D 12, U 18 (D solved), LFRB 24 (U, D solved) | certificate complete (136 + 84 + 98 classes); stages 1-2, purity on 4..9 and 12 example solves re-checked in JS; page https://claude.ai/artifact/LhXB83116s4PajiuAsLXPC |
| mid (T-centres, odd N) | **54** | UD together 26, LFRB 28 (U, D solved) | orbitbound result; superseded by the certified centres-first 39 |
| single oblique, partner kept solved | **81** | UD together 41, LFRB 40 (U, D solved) | orbitbound result; no certificate yet |
| single oblique, diagonals may be disturbed, with merging | **71** (re-confirmed with the purified obl11p.single) | UD together 35 (16 positions; all 188,640 actions), LFRB 36 (352 positions; all 19,856 actions) | orbitbound16, whole single pool incl. obl11.single; both stages exact for this pool |
| oblique, partner and diagonals may change ("free") | **42** (re-confirmed on the purified obl11p.all: LFRB 24, boundary 43: 1 canonical UD state, 42: 92, 437 carries; 200 random checks, worst 40) | UD together 20 (4 positions after merging refinement; cost <= 7, 94,764 actions), LFRB 24 (4 positions; pool adds all of obl11.all up to 11 moves: 639M images, 5.1M keep U and D; cost <= 9 search, refine with 2.96M actions); then the boundary pass proves UD + LFRB <= 42 (43: 4 UD states, 42: 716, about 2 carries each; 200 random hard colourings checked directly, all <= 41) | orbitbound16 for UD, orbitbound20 `--planstart UD --plan LFRB` for LFRB (3.8 h, nearly all loading; `obl11.all.cls@UD` now filters on load); orbitbound25 `--boundary 0` with `LOADDIST` (6 min) |
| wing (edge) orbit, wings first | **77** | 4 wings 12, 4 wings 15, 5 wings 20, last 11 wings 30 | wingbound; wing3 + wing_d6c only |
| chiral pair of obliques (strict: edges, corners, other centres kept) | **112** | aUbU 21, aDbD 31, then the boundary pass proves aUbU + aDbD <= 51 (26 canonical aUbU states, 80 carries; 100 random hard colourings checked directly, worst 50); a ring 25 (b ring free), b ring 36 (single-orbit algorithms) | orbitbound17/19 `--merge --refine --diagok --budget 1e13`; boundary orbitbound29 with `LOADDIST`; plans compared below; pools pair3, p8s10, p9, eg8, c12x (all re-checked pure, see below) |

#### Centres first (user's focus from 2026-10-10 afternoon)

Edges and corners are solved afterwards and the centre orbits one at a time, so
an algorithm only has to keep the other centre orbits colour-solved.  Outer face
turns are then cost-1 actions, and every pool algorithm also counts without
any of its leading and trailing outer face turns (`--faceturns`, orbitbound30).
Diagonals are solved after the obliques (`--diagok`).

| orbit | bound | stages | without face turns | status |
|---|---|---|---|---|
| chiral pair of obliques | **87** = 41 + 46 (certified 2026-10-22; 88 = 42 + 46 and 89 = 42 + 47 on 2026-10-17); plain sum **93** = 19 + 24 + 21 + 29 (2026-10-19; 94 earlier that day, 95 on 2026-10-16); boundaries on the 2026-10-22 stages: aUbU + aDbD <= 41, a ring + b ring <= 46 (boundcheck PASS); rings 45 dropped (relaxed stages instead: upperbound2.md) | aUbU 19; aDbD 24; a ring 21; b ring 29, each a certificate of explicit solving sequences (below); boundary certificates prove aUbU + aDbD <= 42 and a ring + b ring <= 46 ("Boundary passes on the certificate"). aDbD 28 -> 27 -> 26 -> 25 and b ring 33 -> 32 -> 31 -> 30 -> 29 came from ML-agent sequences (57 + 7 + 196 + 1,168) and earlier hardest-state DFS solutions, merged with certmerge | 21, 31, 25, 36 | **certified**: certcheck PASS on all four stages |
| oblique, "free" | **40** | UD 19, LFRB 22; boundary pass proves 40 (4 canonical UD states; 39 has 90,356, too slow) | 20, 24 (42 with the boundary pass) | proved; 39 not attempted |
| single oblique | **63** | UD 33, LFRB 34; boundary pass proves 66, 65, 64, 63 (5, 17, 45, 1,035 canonical UD states); 62 started (28,851 states) | 35, 36 (71) | proved; the 62 pass is paused |
| diagonal | **44** = 10 + 16 + 18 (certified 2026-10-13) | D 10, U 16, LFRB 18 (LFRB 70 lines at 18); orbitbound53 --faceturns --merge --refine, pools diag3 + diag2 + diag_x10 | 12, 17, 20 with the current tool (49); 54 strict | **certified**: diagcheck PASS (coverage, one line per class, side effects on 4..12 at every depth) |
| mid | **39** = 18 + 21 (certified 2026-10-13) | UD 18 (2,431 lines at 18), LFRB 21 (44 lines at 21); orbitbound53 --faceturns --merge --refine, pools mid3 + mid2 + mid_x10 | 26, 28 (54) | **certified**: midcheck PASS (coverage, one line per class, side effects on odd 5..11 at every depth, face centres kept) |

##### The certified diagonal 44 (2026-10-13)

Certificate: `cert/diag/centres-first/` (187 MB; `MANIFEST`; run log, checker output and the
normalizer in `certruns/diag-centres-first/`).  Stages `none..D` (1,381 classes, longest 10),
`D..U` (635, 16), `UD..LFRB` (3,950,059, 18).  Moves are outer faces and depth 2 (the diagonal
orbit's depth d).  The search writes the diagonal model's depth-3 placeholder (the same layer
as 2 in that model); 9,951 LFRB lines had it, and diagnorm.py rewrote 3 as 2 and combined
adjacent turns of one layer, which also took the 14 lines at 19 to 18.  `devtools/diagcheck
STAGEDIR` checks one line per class, coverage, and side effects on every N in 4..12 at every
diagonal depth: the orbit moves as on 6x6x6 and every other centre facelet (other diagonals,
obliques, mids, face centres) keeps its colour.  This replaces the strict 54 (JSON algorithm
table and page), which was not a sequence-per-class certificate.

##### The certified mid 39 (2026-10-13)

Certificate: `cert/mid/centres-first/` (353 MB; `MANIFEST`; run log and checker output in
`certruns/mid-centres-first/`).  Same layout
as the pair: `stages/none..UD/w0..w8.txt` (3,226,306 classes, longest 18) and
`stages/UD..LFRB/w00..w16.txt` (3,957,199 classes, longest 21).  Moves use depth 1, depth 2 (the
mid orbit's depth d) and depth 3 (the middle slice).  `devtools/midcheck STAGEDIR` re-derives the
position of every line, checks one line per symmetry class and full coverage, and checks side
effects on every odd N in 5..11 at every mid depth: the mid orbit moves as on 7x7x7, and every
other centre facelet, face centres included, keeps its colour.  Edges, corners and wings may
move; they are solved later.  Both stages PASS (64 s and 86 s).

##### Live status (auto-updated by certruns/feed/ubloop.sh; 2026-10-22 17:38)

Plain sum **93** = 19 + 24 + 21 + 29.  At the worst length: aUbU 36,652 at 19, aDbD 272 at 24, a ring 1,665 at 21, b ring 127 at 29.
Boundary work at the current worsts: rings at 46 check the a-ring classes longer than 17 (1,849,635); UD at 42 checks the aUbU classes longer than 18 (36,652).  b ring classes at 25+: 904,529.

- a ring products (run30, 7pairud-2): round 1 of 1, slice 12.0..12.0 GB of 12.0, chunk 1 of 1 done; overall 100.0% after 21.9 h; about 0.0 h left, finish ~Thu 17:38
- b ring: == end: b ring at 29 181 classes (10-21 12:03); 7pairudb-2 fed to 1,279,537,719 bytes (udbloop, hourly products)
- aDbD: 7pairup-2 fed to 52,347,976,564 of 52,347,976,564 bytes (run25); 7pairupu-2 to 1,410,987,175 of 1,410,987,175 (run37)

##### The certified stages, 93 as a plain sum (2026-10-19, 94 earlier that day; 87 = 41 + 46 with the boundary certificates of 2026-10-22 on the stages of that day, obl23pair-cf-xfree-P93-2026-10-22 in ~/orbitsolve-certsafe; 88 with those of 2026-10-17; 95 on 2026-10-16; 96 on 2026-10-15, 97 and 98 on 2026-10-14, 99 on 2026-10-12, 100 before that)

Certificate: `cert/obl23pair/cf-xfree/` (1.3 GB; `MANIFEST` lists every segment).  Every
certificate directory under `cert/` holds only `MANIFEST` and `stages/`: plain text, one solving
sequence per line, single spaces; the solved position's line in w0 is empty. One file per
stage, `stages/<solved>..<target>/wNN.txt`, NN = target pieces off their home face; one solving
sequence per line (no positions, no header), one line per symmetry class.  The position of a line
is the sequence run backwards from the solved stage.  `devtools/certcheck STAGEDIR` re-derives the
stage symmetries, checks that every state is covered and solved within the stated length, and
checks side effects on 6x6x6..10x10x10 (no centre outside the pair and the diagonals leaves its
colour).  `devtools/certmerge STAGEDIR FILE...` merges shorter sequences.  `devtools/pairdepth`
shows the same sequences work for every oblique pair (p, q) (checked on 25x25x25, all 110 pairs).

| stage | solved .. target | classes | worst |
|---|---|---:|---:|
| 1 | none .. aUbU | 14,119,851 | 19 |
| 2 | aUbU .. aDbD | 2,936,935 | 24 |
| 3 | aUaDbUbD .. aLaFaRaB (a ring) | 7,891,339 | 21 |
| 4 | aUaLaFaRaBaDbUbD .. bLbFbRbB (b ring) | 7,891,339 | 29 |
| | | **total** | **93** |

Length distribution of the certificate lines (number of symmetry classes whose stored solving
sequence has that many moves, after merges and the ML/DFS improvements):

| length | aUbU | aDbD | a ring | b ring |
|---:|---:|---:|---:|---:|
| 0 | 1 | 1 | 1 | 1 |
| 4 | 26 |  | 12 |  |
| 5 | 56 |  | 56 |  |
| 6 | 389 |  | 58 |  |
| 7 | 1,183 | 16 | 475 | 46 |
| 8 | 5,732 | 150 | 1,802 | 58 |
| 9 | 25,029 | 210 | 5,804 | 107 |
| 10 | 70,853 | 526 | 12,637 | 823 |
| 11 | 244,157 | 2,924 | 49,832 | 2,783 |
| 12 | 630,748 | 15,652 | 138,609 | 4,878 |
| 13 | 1,500,590 | 29,917 | 312,485 | 9,174 |
| 14 | 2,465,019 | 71,140 | 785,106 | 38,339 |
| 15 | 3,363,576 | 123,524 | 1,242,654 | 96,950 |
| 16 | 3,118,018 | 144,961 | 1,747,860 | 155,208 |
| 17 | 2,060,671 | 213,180 | 1,744,313 | 205,846 |
| 18 | 597,151 | 311,857 | 1,233,235 | 368,805 |
| 19 | 36,652 | 450,413 | 535,277 | 670,649 |
| 20 |  | 611,867 | 79,458 | 906,934 |
| 21 |  | 620,251 | 1,665 | 1,055,239 |
| 22 |  | 271,869 |  | 1,298,556 |
| 23 |  | 68,205 |  | 1,260,688 |
| 24 |  | 272 |  | 911,726 |
| 25 |  |  |  | 563,216 |
| 26 |  |  |  | 243,901 |
| 27 |  |  |  | 79,473 |
| 28 |  |  |  | 17,812 |
| 29 |  |  |  | 127 |
| **classes** | 14,119,851 | 2,936,935 | 7,891,339 | 7,891,339 |
| **worst** | 19 | 24 | 21 | 29 |

How the last two moves came: aDbD had 57 classes at 28; the ML agent solved them in 24-27 moves
(~/mlcube/adbd28_improved.txt).  b ring: the certificate run (orbitbound53 with `CERT`, refined
table) left 32 classes at 33; 6 were in the ML agent's first 77, 19 had been shortened by the
earlier hardest-state DFS (cert/bring-hardest.seq.txt, 29-32 moves) and 7 were new; the ML agent
solved those 7 in 28-32 moves (~/mlcube/bring33_remaining_improved.txt).  Then all 196 classes
at 32 (cert/bring-32.seq.txt) were solved by the ML agent in 27-31 moves
(~/mlcube/bring32_improved.txt; 4 at 27, 14 at 28, 42 at 29, 68 at 30, 68 at 31), giving 99.
Then all 1,168 classes at 31 (cert/bring-31.seq.txt; 1,166 in w16, 2 in w15) were solved by the
ML agent in 24-30 moves (~/mlcube/bring31_improved.txt; 1 at 24, 9 at 25, 16 at 26, 33 at 27,
149 at 28, 355 at 29, 605 at 30), merged with certmerge (all 1,168 replaced, none disturbs another
centre); certcheck PASS on the b ring at 30 (2026-10-14), giving 98.  Next targets: the b ring's
440 classes at 30 (cert/bring-30.seq.txt).  As of 2026-10-14 the ML
agent was given aDbD's then 2,710 at 27 (~24 h; later cut to 159 and solved, see below); its tests on the a ring's 22s found
nothing shorter, and the b ring's 17,719 at 30 then looked out of reach.  The a ring was rebuilt
(2026-10-14) with tcwalk's zero-twist cores to depth 10 added (tw10.cls@aUaDbUbD, 6.8M new
permutations): worst still 22 (304 canonical at 22); merged with certmerge (119,679 lines
shortened, 362 -> 304 at 22), certcheck PASS at 22.  Rebuilt again with arena's twsearch
U/D-preserving pair algorithms (7pairud.log, lengths <= 11, 14.3M new permutations): worst
still 22 (303 canonical); merged (178,328 lines shortened, 304 -> 274 at 22), certcheck PASS.
The b ring likewise (2026-10-14) with arena's 7pairudb.log (orbit a kept, b's U/D kept, only b's
ring moves; 233,595 classes to length 12, 3.49M new permutations): the table alone has 848 at
30, 618 at 31, 52 at 32, 3 at 33 (was 16,986 / 1,058 / 284 / 32); merged (2,682,498 lines
shortened, 17,719 -> 1,063 at 30), certcheck PASS at 30.  Again with the larger 7pairudb.log
(775,357 classes to length 13, 13.9M new permutations; streamed loader orbitbound55): table
alone 676 at 30, 463 at 31, 18 at 32; merged (1,384,547 lines shortened, 1,063 -> 711 at 30),
certcheck PASS at 30.  And aDbD with arena's 7pair.log
(any pair effect; lengths <= 10, 2.9M classes, filtered @aUbU, 2.47M new permutations): table
alone 157 at 27, 3 at 28; merged (786,569 lines shortened, 2,710 -> 159 at 27), certcheck PASS
at 27.  Then the ML agent solved all 159 classes at 27 in 19-26 moves
(~/mlcube/adbd27_improved.txt; 1 at 19, 4 at 21, 7 at 22, 5 at 23, 29 at 24, 47 at 25, 66 at
26), merged with certmerge (all 159 replaced, none disturbs another centre); certcheck PASS on
aDbD at 26 (2026-10-14), giving 97.
Overnight (2026-10-15), with orbitbound55 (streamed pools): aDbD with all of 7pair.log (28.1M
classes to length 11, @aUbU; 21.4M new permutations from 742K contributing classes): table alone
54 at 27, 1,584 at 26; merged (885,555 lines shortened, 4,830 -> 1,247 at 26), certcheck PASS at
26.  a ring with 7pairud.log to length 12 (9.9M classes, @aUaDbUbD; 124.5M new permutations, 168.6M
in all): table alone 24 at 22; merged (421,665 lines shortened, 274 -> 20 at 22), certcheck PASS.
Then devtools/certimprove (streams a pool through the hardest lines: each algorithm image as the
first block, the certificate's own lines of <= 15 moves for the rest) with all of 7pairud.log (49.4M
classes to length 13): 4 of the 20 at 22 solved in 21; merged, certcheck PASS (16 at 22).
The ML agent then solved 13 of the 16 (~/mlcube/aring22_improved.txt; 1 at 20, 12 at 21), merged,
certcheck PASS: 3 at 22 remain; then one more (21), 2 remain.  The ML agent solved the last 2
(2026-10-15, ~/mlcube/aring22_improved.txt, both 21 moves, even on every wing orbit; certimprove
with near <= 16 had found nothing); merged, certcheck PASS on the a ring at 21, giving 96.
b ring: certimprove with the 7pairudb.log of 2026-10-15 (1.65M classes to length 14, near <= 18)
shortened 55 of the 711 at 30 (28 to 28, 27 to 29); merged, certcheck PASS: 656 at 30.
The b ring was then rebuilt (2026-10-15, orbitbound55 streaming) with the 7pairudb pool to
length 14 (udb2, 1,654,323 lines, 31.1M new permutations): 328,577 lines shortened by
certmerge, 656 -> 606 at 30, certcheck PASS at 30.
Final arena logs (2026-10-15; 7pairudb 2.63M lines to length 14, 7pairud 62.4M to length 13, all
valid): certimprove with all of 7pairudb (near <= 18) shortened 66 of the 606 (540 at 30).  Then
devtools/certextend (incremental: the certificate as a dense table; only the 977K new 7pairudb
lines streamed, each image skipped unless its stage effect is new; seeds = lines <= 10, band = the
lines at 30 against the whole table; every shortened class propagated through the cheap actions)
shortened 291,961 classes in 7 minutes; merged, certcheck PASS: 440 at 30.
The a ring likewise with the 13.1M new 7pairud lines (length 13; 27 minutes): 25,785 classes
shortened, merged, certcheck PASS: 11,301 -> 11,082 at 21.  A copy of the certificate as it was
before these certextend merges into the a ring (all four stages certcheck PASS: 19, 26, 21, 30)
is kept read-only in ~/orbitsolve-certsafe/, with the verified P = 95 certificate after the full
7pairup-2 feed (19, 25, 21, 30; 2026-10-16) and its worst lists beside it.  The certified P = 89 (stages after run6/run7, aDbD 598 at 25, plus
boundaries/) is obl23pair-cf-xfree-P89-2026-10-17 with lists-P89-2026-10-17.  P = 88 (the same stages, plus x46.txt):
obl23pair-cf-xfree-P88-2026-10-17.  The plain sum P = 93 (19, 24, 21, 29; no boundary files: those belong to the 2026-10-17 stages) is obl23pair-cf-xfree-P93-2026-10-19 with lists-P93-2026-10-19 (certcheck PASS on all four stages of the copy, certruns/cert-P93.log).  The stages of 2026-10-22 (feeds stopped 17:40; aUbU 36,652 at 19, aDbD 272 at 24, a ring 1,665 at 21, b ring 127 at 29) are obl23pair-cf-xfree-P93-2026-10-22 with lists-P93-2026-10-22 (certcheck: certruns/cert-P93-2026-10-22.log); their boundaries are being regenerated (certruns/boundary/ud22.sh).  Logs are now fed with devtools/pairfeed.sh  Boundaries on these stages (2026-10-22): UD 42 and UD 41 boundx cover every aUbU class (633,803 longer than 17 for 41) and boundcheck PASS (41: 43.0G colourings, 616 s); both are in boundaries/none..aUbU+aUbU..aDbD/ of the 2026-10-22 snapshot.  Rings 45 1/500 sample: 0 of 7,188 uncovered.  Rings 46 boundx covered all 1,849,635 a-ring classes longer than 17 (2 h 04 m), boundcheck PASS (306G colourings, 3,646 s), in boundaries/aUaDbUbD..aLaFaRaB+aUaLaFaRaBaDbUbD..bLbFbRbB/x46.txt: **P = 41 + 46 = 87, certified** (obl23pair-cf-xfree-P93-2026-10-22).  Rings 45 in full (certruns/boundary/night22.sh) was stopped 2026-10-23 at about 88% (all 17s and 18s covered) in favour of the relaxed stages.
(pairlogcheck --new skips lines seen before, so a grown log costs only its new lines; certextend,
certmerge and certcheck per stage, the stage restored on any failure).
certextend was then made much faster (2026-10-15/16): images are generated only up to the stage
symmetries (one per coset, the targets and seeds expanded to their 8 symmetric copies instead),
each image's map is computed once and conjugated, targets are kept as per-colour bit masks that an
image permutes with byte tables, and `--worst` tries each image against the seeds or the
worst-length targets, whichever is fewer.  With it (worst lengths only): 7pair.log (28.1M lines,
to length 11) shortened aDbD 188 classes (6 min) and the a ring 1,400 (15 min); 7pairup-2.log
(51.8M aDbD lines to length 13, 92M new effects, 1.5 h) took aDbD from 1,057 classes at 26 to 9;
7pairud.log on the a ring shortened 146,320 classes (51 min): 21 now 3,340 (cert/aring-21.seq.txt).
The ML agent solved the last 9 at 26 in 23-25 moves (2026-10-16, ~/mlcube/adbd26_improved.txt);
merged, certcheck PASS on aDbD at 25, giving 95.  The full 7pairup-2.log (8.5 GB, 2026-10-16:
136.2M more lines, all length 13) went in 20M lines at a time (CHUNK in pairfeed.sh, each chunk
merged and checked), 3.2 h with --worst: aDbD's 25 went from 31,626 classes (252,500 positions)
to 1,610 (12,836 positions), most in the first chunk; the last chunk shortened nothing.
Then (run6) 7pair.log fed again against that improved certificate (its 10-11 move lines can now
use the many new 13-14 move lines): aDbD 984 classes shortened in 2 minutes, 25 now 622 classes
(4,932 positions); the a ring 139 shortened, 21 now 3,177.  A two-level test (certextend --miss 1:
the 8,269 positions that missed a 25 by one move were made targets in a second 7pair stream)
shortened nothing; 7pair is closed under itself here, as the table build with it suggests.
7pairud.log fed again to aDbD (62.4M lines, 86 minutes with the GPU busy): nothing.
7pairup-2.log grown to 9.1 GB (13.3M more lines, all length 13; 5 minutes): 24 shortened, 25 now 598.
Run8 (2026-10-17, certruns/feed/run8.sh, smallest log first, 20M-line chunks): 7pairudb-2.log
(5.46M lines, lengths 6-14, one orbit colour-solved; 4 minutes): aDbD nothing; the b ring 191
classes shortened, 30 now 208 classes (1,400 positions; was 393 / 2,786).
7pairud-2.log (94.6M lines, lengths 4-14, 24.3M at 14; 62 minutes on three stages): aDbD nothing
(36 minutes); the a ring 72,804 classes shortened, all in the last two chunks, 21 now 2,985
classes (23,636 positions; was 3,177 / 25,172); the b ring nothing (2 minutes).
The grown 7pairup-2.log (19.8 GB; 224.4M new lines, 64.8M at 13 and 159.5M at 14; 75 minutes
in 12 chunks) on aDbD: 46 classes shortened, spread over the chunks, 25 now      552 classes (4,372
positions; was 598 / 4,740).  Run8 total 2.6 h.
Run9 (re-feed against run8's gains, fresh seen sets via -r2 links): 7pairud-2 on the a ring
(15 minutes) 51 classes shortened, all in the first chunk, 21 now     2934 (23,228 positions);
7pairudb-2 on the b ring and aDbD: nothing.  Run10 (the older logs again, fresh seen sets: 7pairudb on the b ring,
7pair on both rings, 7pairud on the a ring; 18 minutes): nothing.  The re-feed cascade is
exhausted; the next gains need new lines.
Run11 (2026-10-18, the grown logs, new lines only): 7pairudb-2.log (+106,203 lines, all length 14):
the b ring 1 class shortened, 30 now 207 (1,392 positions); aDbD nothing.
7pairud-2.log (+67.3M lines, all length 14; 36 minutes): the a ring 36,852 classes shortened
(mostly in the first three chunks), 21 now     2887 (22,852 positions); the b ring and aDbD nothing.
Run12 (7pairup-2.log grown to 32.3 GB: 249.7M new lines, all length 14) on aDbD (4.1 h):
3 classes shortened, all in the first 4 of 13 chunks, 25 now 549 (4,348 positions).  A 14 only helps
aDbD when its image lands on a line of at most 10 moves (797 classes), about 2e-7 per line.
Last-move merges (certextend --merge G, 2026-10-18): a seed or band candidate up to G moves too
long by plain length is still tried when the image's last turn and the line it continues with
start on the same layer (face and depth); the joined sequence is simplified (cancellations,
same-layer turns combined) and counted exactly.  On the b ring with the already-fed
7pairudb-2.log, b-ring positions at 30 from 1,392: G = 2 1,240, 4 712, 6 608, 8 604; G = 6 used.
On aDbD with the 7pairup 14s no candidate was within 12, so no gain there.
Run13 (--merge 6, fresh seen sets): 7pairudb.log on the b ring 66 classes shortened (positions
at 30: 880), 7pairudb-2.log 55 more: 30 now 113 classes (664 positions; was 207 / 1,392).
7pair.log: the b ring nothing; the a ring 195 shortened, 21 now 2,692 (21,324 positions).
7pairud on the a ring with G = 6 was far too slow (one 20M chunk unfinished after 3 h: the a ring's
band at 21 - 6 is millions of classes); stopped, rerun as run14 with G = 2.
13s with merges on aDbD (test, 5M 13s of 7pairup-2): G = 0 nothing, G = 6 203 classes shortened
(positions at 25 4,348 -> 2,724); the 7pair* logs hold every sequence shorter than their longest,
so these are all the 13s there are.  Run15 fed all of 7pairup-2 (675M lines, 34 chunks, 2.9 h) to
aDbD with G = 6 (and, from its second chunk, axis merges): 25 from 549 classes to **4** (24 positions;
cert/adbd-25.seq.txt).  Run18 (7pairud, the grown 7pairud-2 and 7pairup-2 again to aDbD, G = 6, fresh seen sets): nothing;
the last 4 are out of reach of one 13 plus a line.
Run14 (the a ring, G = 2, 7pairud then 7pairud-2 in 5M chunks, 6 h): 149,105 classes shortened,
21 from 2,692 to 1,904 classes (15,104 positions).  Run19 (7pairudb-2's 3,242 new 14s, b ring): nothing.
Run20 (the a ring with G = 4): far too slow (a 10M chunk over 8 h); stopped after 2 chunks.
Two-level search for aDbD's last 4: --miss 3 over the whole of 7pairup-2 in one stream (run21) and
--deepband 4 (run24: every worst position moved by each of the 99 actions of at most 4 moves, 1,576
targets, then an image plus a line; over every 7pairup-2 line of at most 13 moves) found nothing in
the slices that ran; both hit a heap-corruption abort in certextend's second pass that ASan,
TSan and a bounds-checked build do not reproduce (open; the stage is restored each time).
**The ML agent solved the last 4 (2026-10-19, ~/mlcube/adbd25_improved.txt, 21-24 moves): merged,
certcheck PASS, aDbD worst 24, plain sum 94.**
Logs since 2026-10-19 come from arena as tails only (certruns/feed/pulltail.sh: seek to the
processed byte count in certruns/feed/processed.tsv, read the rest, cut at the last complete line;
certruns/feed/logs.md).  7pairud-2.log (12,007,884,518 bytes) and 7pairup-2.log (52,347,976,564)
were stopped on arena; 7pairud-2 is fully fed (a ring G = 2, b ring G = 6); 7pairup-2's last slices
go to aDbD with G = 2 (at worst 24 aDbD's band is 1M positions and G = 6 tries every seed).  Each 5M
chunk shortens about 66,000 aDbD classes (positions at 24: 1.01M -> 768K) but takes ~25 min; the feed
is paused (SIGSTOP, pids in certruns/feed/run25.paused) while the b ring has the CPU, about 18 h left.
7pairudb-2.log has finished length 14 and now produces 15s; all of arena works on it, and its tail
is fed to the b ring hourly (certruns/feed/udbloop.sh).
**The b ring push (2026-10-19).**  Only the 7pairudb logs have ever moved the b ring (7pairud,
7pair: nothing; 7pairup cannot touch orbit b).  From 107 classes at 30 (628 positions):
- --deepband 4 on 5.56M lines of 7pairudb-2: 31 classes, 30 to 92; over the whole log with
  C1 = 4 and 6 nothing more (one class dropped during the C1 = 6 pass); C1 = 8 too slow, stopped.
- The ML agent works on cert/bring-30.seq.txt and appends solutions to ~/mlcube/bring30_live.txt;
  certruns/feed/mlloop.sh merges new lines every 15 minutes under the stage lock (certmerge,
  certcheck) and refreshes the list.  Three batches so far: 8, 7, 7 classes.
- The full pair product (run27): --worst runs never used seeds on the b ring (the band is
  smaller), so positions solved by single new 7pairudb-2 algorithms were never recorded and pair
  products A1 A2 only used the old pool as A2.  Pass 1 without --worst (--near 2: A^-1 of solved,
  then propagation): 89,119 classes shortened across the table, positions at 30 456 -> 376;
  pass 2 (--worst --merge 6): 30 now 284 positions.
- 49 classes at 30 (284 positions) after run27; scorecard then: merges 36, ML 22.
- Run28: whole-table products, every 7pairudb-2 image A in front of every certificate line of at
  most K moves (seeds, no --worst), each pass propagated and re-reading the table: K = 10, 5M
  lines: 295,064 classes shortened, positions at 30 164; after K = 10: 25 classes; K = 11: 24;
  K = 12: 19.  Its final --worst pass aborted with the certextend heap crash (stage restored).
- The hourly 7pairudb-2 tails (15s, udbloop.sh, G = 6 with --worst): 19 -> 10 (16:15) and,
  after ML lines took it to 4, 4 -> **1 class at 30 (4 positions)** (17:23, processed to
  604,603,712 bytes).
- Scorecard since 107 (approximate where runs overlapped): merges about 74, ML about 32.  The
  last class (cert/bring-30.seq.txt):
  `2U 2D' 3U' B' 3D2 3L2 B 2D R 3F2 3L2 3R2 F' 3R2 3D2 F 2B2 2U2 R' 3U R 2B2 2U2 2D2 L' 3F2 L 2D2 R' 2U'`
- The ML agent solved that last class in 28 moves (~/mlcube/runs/obl8_ring4, round 3 of n30dr),
  merged by hand: `2D' R 3B2 3U2 R' 2U' 2D 3U2 R 3D2 3F2 R' B 2U 3B2 3D2 F' 2U' U' 3R2 U B' F2 2U 3F2 2U' F2 2U`;
  certcheck PASS at **29**, plain sum **93** (17:40).  Final scorecard 107 -> 0 at 30: merges about
  74, ML about 33.
**The a ring push (2026-10-19).**  Every a-ring feed had used --worst (band, never seeds).  Run29
(--near 2, no --worst, merge 2, 7pairud-2 in 3 GB slices from arena): its first 5M chunk found 0
seed candidates (single algorithms were all recorded by the early non --worst 7pairud feeds) and
55 band classes (21: 1,824 -> 1,769) in 29 min, nearly all spent on the band of 14K classes; stopped.
Run30: products only (--near 10 --band 0 --merge 6) over all of 7pairud-2 in 3 GB slices from
arena, in progress (restarted 19:47 on the lock-free certextend below; chunk 1 of the first try
ran 80+ min on the old binary without finishing).
**The b ring's 29s (run32, 17:43-19:36).**  run28 again on 13,168 classes at 29, with the local
7pairudb-2 copy refreshed to 604,603,712 bytes (the 15s fed so far; 12.4M lines, 3 chunks): K = 10:
310 classes; K = 11: 267; K = 12: 262; --worst --merge 6: **260 classes at 29 (1,852 positions)**,
tail 25: 4,729,402, 26: 2,639,672, 27: 1,075,552, 28: 332,880 positions.  Nearly all of it from
K = 10.  (Run31, the re-run of run28's --worst pass at 30, found nothing more.)

**2026-10-20.**  b ring: --deepband 6 (run35, bnight) 260 -> 241 -> 196; K = 10/11 and the
hourly 7pairudb-2 tails then gave nothing at 29; --near 13 (bnight k13) 196 -> 195 so far.
aDbD: the rest of 7pairup-2 (run25, slices from 42.1 GB) and 7pairupu-2 (run37, never fed
before) took its 24s from 96,148 to 2,330 classes by 16:21; aDbD at 23 (plain sum 92) is in reach.
a ring: run30 (products, --near 10 --band 0) 1,824 -> 1,665 at 21 at 64%.

**2026-10-21.**  run25 finished 7pairup-2 (05:31) and run37 7pairupu-2 (10-20 18:44): every arena
log is now fed; aDbD at 24: 1,271 classes.  b ring: bnight cycles 2-3 (db6, K = 10, K = 13) 195 ->
181 at 29.  run38 (aDbD's 24s: --deepband 6, then --near 10 products on the 7pairudb-2 pool) started.  run30 (a-ring
products over all of 7pairud-2) ended 07:50: a ring 1,824 -> 1,665 at 21 (classes > 17: 1,854,113).

**--worst skipped singles (found 2026-10-21).**  With --worst an image tries the seeds (and so
its own single, A^-1(solved) by A) only when the classes within W - 1 - |A| are fewer than the
band; pairfeed's seen set then never offers the line again.  Every 7pairup-2 feed to aDbD used
--worst, so aDbD's lower lengths were never complete against its own log (complete through 13):
since P93, 58 classes went 12 -> 11 (6 of those lines are verbatim in 7pairup-2) and 1,235 13 -> 12,
from run37 (7pairupu-2, a new log name, fed while the band was large).  run40 (7pairup-2 lines
<= 12 as singles: --near 0 --band 0, 7 min): 10: 420 -> 526, 11: 3,026 -> 2,924, 14: 69,272 ->
62,168 (classes move down; the top unchanged).  The a ring (run30) and the b ring (run32, bnight)
had full non-worst passes over their own logs; aUbU has only seen 7pair and 7pairudb.

**Deep merges (devtools/deepmerge, 2026-10-21).**  Joins F' B' of F = F' S and B = S^-1 B' (B any
of the 96 images), K = |S|, keyed by K-move prefixes (every subset of the same-axis run the K-th
move falls in).  On 7pairudb-2's lines <= 12: 24.5M image keys; distinct keys 1.0M / 4.2M / 8.0M
and joins 196M / 40M / 8.6M for K = 4 / 5 / 6.  Length 16 from 13 + 13, K = 5: 121,536,017
distinct lines (29 min); a 1/24 sample: 712K break U/D, 1.73M leave the pair colours unchanged.
run41 fed the sample to the b ring (products, K = 10): 3,600,109 lines passed bonly, 18,640 classes
shortened in 142 s (26: 278,229 -> 277,303, 27: 93,499 -> 92,029, 28: 19,818 -> 19,550; 29 still 181).
run43 feeds all 121.5M.  Sources <= 14 (5,566,329 lines, 590M image keys, 9.4 GB), K = 5: joins by
length 15: 320M, 16: 1.03G, 17: 2.86G, 18: 6.64G; certruns/deepmerge/dmloop.sh makes and feeds the
17s in 24 slices (deepmerge --part), each slice made while the previous one feeds.
Through slice 16 of the 17s (10-22 00:32): b ring 26: 277,303 -> 250,050, 27: 92,029 -> 80,574,
28: 19,550 -> 17,921, 29: 181 -> 133 (each slice about 58M lines that pass bonly, 30 min).  The same
merges on aDbD (run44: the 16s; dmu.sh: 17s slices 4-17) shortened almost nothing (20: 617,622 ->
617,589): merges of 7pairudb-2 lines pass the uonly filter but reach no new aDbD states.  run45: aDbD
products without --worst (K = 10) over 7pairup-2's lines <= 13.  The <= 12 part (34.7M lines, 54 min, done
10-22 01:43): aDbD 21: 744,047 -> 678,937, 22: 340,087 -> 308,866, 23: 95,032 -> 74,757, 24: 1,270 -> 316.
The 13s (231M lines; done 10-22 08:09): 21: 622,622, 22: 272,868, 23: 68,430, 24: 272.
The 17s merges finished 10-22 09:15: b ring 25: 563,345, 26: 244,565, 27: 79,640, 28: 17,869, 29: 127
(slices 17-23 about one 29 each).  A 1/64 sample of the 18s (6.64G joins) measures their yield first.  run47 then feeds all of 7pairudb-2 (to 1,226,375,198 bytes)
to aDbD as non-worst products: aDbD only ever had it with --worst (or K = 2 deepband).  Done
10-22 10:42 (35 min): only ~1,000 classes, none at 22-24 (24 still 272).  Big temporary files (slices,
pulled log copies, seen sets) now live in /var/tmp/orbitsolve, excluded from Time Machine.
run48 (--deepband 6 on aDbD at 272): the first pass found nothing; the near-miss pass (1.09M
queued positions against every new pool image) was still running after 2 h 50 min and was stopped,
as run38 was.
**Lock-free certextend (v8, 2026-10-19).**  On the a ring with --near 10 --merge 6 the process
showed 45% system time and 240K context switches/s: `sample` put ~38% of samples in mutex waits,
from lineOf (which locked the global mutex on every seed candidate to look up this run's
shortened lines) and improve.  lineOf now checks a bitset of shortened positions first and only
locks when its bit is set (a reader missing a fresh bit uses the older, longer line, still valid;
improve re-checks under the lock); improve returns without the lock when the candidate is not
shorter than its class (all images of a class share one length).  Same output single-threaded as
before (300K-line test); now 3.9% system time and ~18K switches/s.
Run16 (the b ring again with G = 8): 1 class, 30 now 112.  Then merges by axis blocks: turns of one
axis commute, so simplify now combines same-layer turns anywhere within a run of same-axis turns,
and a merge is tried when the axis run ending A and the axis run starting line(q) share a layer
(was: A's last turn and q's first on the same layer).  Run17 (b ring, G = 6): 3 classes, 30 now
109 (640 positions); G = 10 adds nothing.
aUbU has not yet seen 7pair, 7pairud or 7pairup
(about 9 new effects per line against 292K worst positions: too slow as is).

**1u lower bound: 15 (2026-10-23; relaxed stages are in upperbound2.md).**  The aUbU class aubu19-00007 (devtools/tws/aubu19all.scr,
one of the 36,652 classes at 19 in the 2026-10-22 1f certificate) needs 15 moves under 1u (every
other oblique orbit keeps only its U face): twsearch on devtools/tws/aubu19u.tws (-M 7000)
exhausted depth 14 and found

    scramble: 2R2 3B' 2R2 3B D 2D' 3R 2F 3L' 2D 3R' 2F' 3L 3D 2L 3D' 3U' 2L' 3U
    1u solve: 2B2 U' 3L 3R 2D R' 2U 3R' D 2L2 2R' 3D2 3U' 3L' 2R   (15, optimal)
    1f line:  3U' 2L 3U 3D 2L' 3D' 3L' 2F 3R 2D' 3L 2F' 3R' 2D D' 3B' 2R2 3B 2R2   (19)

so 1u's worst is at least 15 (the first seven of those classes: 14, 14, 13, 13, 14, 14, 15;
/var/tmp/orbitsolve/aubu19u-sample.out).  For comparison, the stage with nothing kept has worst 9.

**Boundary passes on the certificate (2026-10-16/17; certified P = 88.  2026-10-22 stages: UD 42 and 41 PASS, rings 46 PASS, so P = 87 certified; rings 45 dropped 2026-10-23; see the snapshot note above).**
devtools/boundx loads two consecutive stage certificates as dense tables.  For every A class
whose line L0 is longer than X - worst(B), the colourings of the rest that L0 carries onto B
states beyond X - |L0| are listed and tried against alternative solving sequences of the A state
(an action of at most 8 moves, then the certificate line of where it leads; with --deep/--slack
also two actions, and sequences up to 5 moves longer than L0), cheapest first; A + B <= X holds
when none is left.  Results with the P = 95 certificate (aDbD 598 at 25):

| boundary | X | A classes checked | left | time |
|---|---:|---:|---:|---:|
| a ring + b ring | 50 | 3,177 | 0 | 7 s |
| a ring + b ring | 49 | 104,125 | 0 (one action: 36; two: 13; slack 5: 0) | 2.5 min |
| a ring + b ring | 48 | 689,344 | 0 (slack 5) | 15.5 min |
| a ring + b ring | 47 | 1,928,090 | 0 (slack 5; 1.47e11 colourings) | 2.6 h |
| a ring + b ring | 46 | 3,692,331 | 4 (slack 5); 0 with deep 3, slack 7 on those 4 | 12.5 h |
| aUbU + aDbD | 43 | 36,652 | 0 | 5 min |
| aUbU + aDbD | 42 | 633,803 | 0 | 82 min |

The two boundaries do not overlap, so P <= 42 + 47 = **89**, certified 2026-10-17.  The
boundary certificates are in cert/obl23pair/cf-xfree/boundaries/ (MANIFEST lists them after the
stage files; stages/ is unchanged, so certcheck alone still verifies 95):
`none..aUbU+aUbU..aDbD/x42.txt` (633,803 blocks, 1.68M lines) and
`aUaDbUbD..aLaFaRaB+aUaLaFaRaBaDbUbD..bLbFbRbB/x47.txt` (1,928,090 blocks, 7.66M lines).  Format:
one block per A class, its stage line L0 first, then the alternative solving sequences it uses,
one per line, blocks separated by an empty line.  Written by boundx --out (ring 47: 1 h 51 min;
UD 42: 80 min).  Checker: devtools/boundcheck STAGEDIR_A STAGEDIR_B FILE X, independent of
boundx (own ranking, own dense tables over all symmetric images, the same side-effect check as
certcheck on N = 6..10): every line legal and solving its block's A state, every A state with
dA > X - worst(B) has a block, and every colouring of the rest is carried by some line onto a B
state within X.  PASS on both: UD 42 (5,063,512 A states, 8.1e10 colourings, 21 min), rings 47
(15,404,276 A states, 3.1e11 colourings, 72 min).  Tests: removing one alternative or one block
makes it FAIL.  A boundary certificate holds only for the stage files it was made with; after
any certmerge into one of its two stages, regenerate it with boundx --out and re-run boundcheck.
**Rings at 46, P = 42 + 46 = 88 (certified 2026-10-17).**  A 1/500 sample (boundx --sample 500)
covered all 7,385 sampled classes, so the full run went overnight (certruns/boundary/prod2.sh,
deep 1, slack 5: 12.5 h, the last 1% of the classes, the longest lines, taking 4 h of it).  It
left 4 of the 3,692,331 classes with 7 colourings uncovered; boundx --only FILE (check only
the listed lines' classes) on the 4,971 classes with 40+ alternatives found them (2 h):
`B 2U' 3F2 3B2 2U 2D 3F2 3B2 3U' 3D 2D' 2F2 2B2 3U 3D' 2F2 2B2`, the same with `F' B` in front,
and two `F' R` / `L B'` + `2U' 2D 3L2 3R2 2U 2D' 3L2 3R2 3U' 2B2 3D 2F2 3U 2B2 3D' 2F2`.
--deep 3 --slack 7 covered all four (94 s; --prop 10, 384K actions of up to 10 moves, did not);
--check replayed 24,548 of their colourings, none failed.  Their four blocks were spliced into
the certificate: boundaries/aUaDbUbD..aLaFaRaB+aUaLaFaRaBaDbUbD..bLbFbRbB/x46.txt (3,692,331
blocks, 19.2M lines, 1.3 GB).  boundcheck PASS: 29,508,704 A states, 1.64e12 colourings,
6.0 h.  x47.txt is kept beside it.  aUbU + aDbD at 41 (2.69M classes, 979K actions) is far
slower: its 1/500 sample had not finished after 2 h; a ring 45 would need dA > 15, more again.
--check replays covered colourings on the full colouring (both stages' pieces, move by move):
10,600 at the ring boundary at 50 and all 44,031 deep covers at 49, none failed.

Which pools each stage has seen (B = built into the stage table by orbitbound, E = certextend via
pairfeed.sh, I = certimprove on the hardest lines; - = not applicable to the stage):

| pool (arena twsearch logs) | lines, lengths | aUbU | aDbD | a ring | b ring |
|---|---|---|---|---|---|
| base pools (pair3, p8s10, p9, eg8, c12x) | | B | B | B | B |
| tw10.cls (zero-twist cores) | 6.8M perms | | | B | |
| 7pairudb.log | 2.63M, <= 14 | E | E | - | B, I, E |
| 7pair.log | 28.1M, <= 11 | not yet | B, E, E again* | E, E again* | E |
| 7pairud.log | 62.4M, <= 13 | not yet | E, E again* | B (<= 12), I, E | E |
| 7pairup-2.log | 201M, <= 13 | not yet | E (all) | - | - |

*Re-fed 2026-10-16 (run6, fresh seen sets via the -r2 links): the shorter pool lines can now use
the many certificate lines of 13-14 moves that 7pairup-2 created.  Records: certruns/feed/run*.sh
and run*.log; certruns/feed/<log>.seen holds the lines each log has fed.

Purity (sequences that move no other centre piece at all, not just keep colours): on 6x6x6 every
b-ring sequence is pure; on 7x7x7 and up 99,520 of the b ring's 7,891,339 are pure (the rest permute
same-coloured pieces of other orbits, which is harmless).  `certcheck` prints the counts per stage.

Stage tables are saved (scratchpad pairF.*, freeF.*, singleF.*, diagF.*) so
boundary passes can reload them.  Pools: pair3, p8s10, p9, eg8, c12x, and the
purified obl11p.all (free) / obl11p.single (single); diag3, diag2, diag_x10;
mid3, mid2, mid_x10.

Every bound is the sum of exact per-stage worst cases (each stage searched
exhaustively over all its positions), so it is rigorous given the algorithm
pools: each pool algorithm changes only its orbit.  For the diagonal pools that
was re-checked independently on full 4x4x4 to 9x9x9 cubes in JavaScript (214
algorithms of the certificate, all pure, all lengths exact).  The pair pools
carry a flag `D1` for algorithms that also move the diagonal orbit; those are
excluded (46,872 of p9's 79,280 classes, 33,299 of eg8's, 1,480 of p8s10's).

Diagonals afterwards (`--diagok`): when the diagonal orbit is solved after the
obliques, an oblique stage may also use the D1 algorithms, since they change
nothing but the oblique pair and the diagonals, and the diagonal bound (54)
holds from any starting state.  So for a whole-cube centre bound the oblique
numbers to use are the `--diagok` ones.  Single-orbit algorithms: 139,936 with
D1 against 112,474 without; the extras are mostly 10 and 11 movers (10:
24,448 vs 17,032; 11: 40,914 vs 18,474) plus a few 8 and 9 movers, so the
expected gain is modest for single and possibly larger for free.

Unified reduction (the "bonus" question): the best plans found for all three
orbit types have the same shape, **first the U and D faces, then the four side
faces together**, keeping U and D.  For the diagonal the first part is done as
D then U (12 + 18 = 30) only because the joint UD stage was too expensive at
the budget used (with 4-5 move algorithms alone it does not reach every
position); for the mid orbit the joint UD stage gives 26.  This is the familiar
"U/D centres first, then the ring" order, applied orbit by orbit.

### Obliques first (user, 2026-10-09: obliques dominate on big cubes)

- devtools/obliquepool.cpp extracts symmetry classes from 6centers.log.xz
  (twsearch: 26,486,548 sequences of up to 11 moves on 8^3 changing only one
  oblique pair and the diagonals): 7,179,911 classes (4:6 5:10 6:183 7:894
  8:9,577 9:68,623 10:727,723 11:6,372,895), saved as devtools/obl11.all.cls;
  2,443 single-orbit classes (8:76 9:47 10:766 11:1,554), obl11.single.cls.
- For one orbit this adds little: 6,720 new permutations, almost all 10 and
  11 movers; still only six 6-movers.  Short single-orbit algorithms are
  rare, so single-orbit gains must come from merging and budgets.
- For the pair / partner-ignored case the full pool up to 10 moves (~77M
  images) is the big addition over p9 (complete to 9) + eg8.
- Running in order: single `--merge --diagok --plan UD,LFRB`; free, same;
  pair `--merge --diagok --plan aUaD,bUbD,aLaFaRaB,bLbFbRbB` (joint: stage 2
  keeps only a's U and D, so it may disturb a's sides and has many more
  algorithms than a single orbit; only the last stage is single-orbit);
  free with obl11.all up to 10 moves.

`--refine` (orbitbound4): after a stage, the positions at the worst distance
are re-solved with every usable algorithm (no budget) as the first step and
the stage table for the rest, level by level while all improve.  It only
helps where the budget cut algorithms (it cannot beat an exact stage search
with the full pool); the single-oblique stages ran at cost <= 9 and <= 10.

Search order (orbitbound7+): discovery order by target distance (the user's
suggestion): for each target d, sources at a < d with algorithms of cost
exactly d - a (less merge saving); a position first reached is final, and the
search stops when the last position is found, so pairs whose sum exceeds the
worst case are never tried.  Identical histograms to the old level-synchronous
Dijkstra (kept as `--oldsearch`), 57x faster on the diagonal D stage (cost <=
7), 6x with merging, about 2.3x on the single-oblique UD stage.
Each stage line now prints the distance distribution (after refinement).

Full-set seeding (user's suggestion, orbitbound10, `--fullwork W`, default
2e11): a source level whose (positions x all usable actions) <= W uses every
usable action, not just the budget set.  The low levels are tiny (the goal is
one position), so this is nearly free and gives the best short paths.  Still
an exact shortest-path search in that graph, so still valid.  Test (diagonal,
diag3 + diag2, merge, budget 2e7 = cost <= 5): plain search cannot solve stage
D; with seeding 12 + 19 + 13 + 20 + 12 = 76, the same as the unrestricted
search.

Band refinement (orbitbound9+, `--refine`): every position in the top levels
(as many as fit `--refinework`) gets dist = min(dist, cost(A) + dist(A(x)))
over every usable algorithm, repeated until nothing improves, so gains
propagate up through the band.

Performance round (2026-10-09 evening), measured with /usr/bin/time -l on the
diagonal LFRB stage (63M states, cost <= 7, --merge, no seeding):
| step | time | instructions | per pair |
|---|---|---|---|
| level-synchronous (old) | 325 s | 2.80e13 | 657 |
| half-word ranking (user's idea: base-k half words, rank = offHigh[hi] + idxLow[lo], tables 3^12 / 4^8) | 216 s | 1.52e13 | 357 |
| + stage symmetry (16; canonical sources only, discoveries and new merge bits written to all images) | 21 s | 1.42e12 | 430 (13x fewer pairs) |
| + actions bucketed by (cost, first move) in merge mode | ~9 s | 1.09e12 | 330 |
Prefetching gave nothing (IPC 4-5: instruction bound, not memory bound).
Fixes found on the way: ClassPool::expand(distinct) dropped images with the
same permutation but different first/last moves (merge action set was not
symmetric; now every image is loaded); small stages ran on one thread (chunk
size); refine skipped single algorithms to the goal (prune used cost + 1), which
also made its result order dependent.  Symmetric and plain searches and refines
now give identical histograms (checked with and without merging), and
`--checkrefine` verifies a witness for every refined value.

Pair plans (2026-10-09 night): finishing one orbit and then the other always ends
with a single-orbit-only stage (the 36).  aUaD then bUbD (keeping only aU, aD)
gave 20 then 35: keeping a's U and D is as restrictive as keeping all of a.
Results: face by face aUbU,aDbD,aLbL,aRbR,aFaBbFbB = 21+31+22+29+18 = 121;
aUbU,aDbD then aLbL, aRaFaB, bRbFbB = 52+22+28+30 = 132; aUbU,aDbD then a ring,
b ring = 52+25+36 = 113 (best), now **112** with the boundary pass on
aUbU|aDbD (51).  Conjugation by face turns (`--conj 1`,
2.6M single-orbit entries, many more short ones) left single at 35+36 = 71:
the hardest positions are not short of algorithms.
More plans from aUbUaDbD (52): aLaR (b ring free) 22, bLbR (a's U,D,L,R kept)
34, aFaBbFbB 18 -> 126.  Keeping most of an orbit fixed is nearly as costly
as keeping all of it.

Why single-orbit stages stick at 35/36 (2026-10-10 night): the worst UD positions
(16) have all eight U and D pieces on two side faces; the worst LFRB positions
(352) have none of the 16 pieces home (e.g. two faces exchanged).  Single-orbit
algorithms are mostly 8-move 3-cycles, which place at most 2-3 tracked pieces
each, so these need about 4.5 algorithms whatever the pool.  Pools tried, all
with the same 35/36 and the same worst counts: original (205k entries), face
conjugation (`--conj 1`, 2.6M), pair-to-single compositions (`--pairsingle 8`:
A then C^-1 for pair algorithms leaving the partner in the same colour
arrangement, 98.8M compositions, 9.58M entries up to 16 moves).
So avoid stages that keep a whole orbit fixed: running aLaR (b ring free) then
one joint stage b ring + a's F,B (63M x 70 = 4.4e9 states, ~40 GB).
Result: aLaR 22, then the joint stage = **55** (52+22+55 = 129, worse).  At
4.4e9 states the budget allowed only cost <= 7 actions (464), and the top of
the distribution (46..55, ~280M states) is a separate bump of positions that
the short actions reach badly; refinement with 63k actions got none below 55.
A joint stage this large needs a far bigger pool than fits the budget.

Earlier idea: advance both orbits together face by face, so every stage, including
the last, can use genuine pair algorithms: aUbU (113M states), aDbD (23M),
aLbL (3.3M), aRbR (245k), aFaBbFbB (4,900).

### Pool purity (2026-10-10)

The twsearch log behind obl11 (`6centers.log.xz`) contains known-bad lines,
which the README pipeline drops with `bad.index` / `good.pl`; obliquepool read
the log directly and did not.  Re-checked every class representative on a full
8x8x8 sticker simulation (corners, wings and the other obliques exactly
restored; the (2,3)/(3,2) pair and the diagonals may change):

- obl11.all: 1,490 of 7,179,911 classes impure (8: 6, 9: 6, 10: 255, 11: 1,223);
- obl11.single: 2 of 2,443 impure (both 11 moves);
- pair3, p8s10, p9, eg8, c12x: all pure.

The purified pools are devtools/obl11p.all.cls and obl11p.single.cls.  The
pair results never used obl11.  Free with obl11p.all: LFRB again 24, and the boundary
pass again proves 42.  Single with obl11p.single (plus slice conjugates) is again 35 + 36.

### Centres first: free face turns (2026-10-10, user's ring idea)

If centres are solved before edges and corners, a pool algorithm only has to
preserve the other centre orbits.  An outer face turn keeps every centre on
its face, so it is a legal cost-1 action in every stage (`--faceturns`; no
merges are counted with these one-move actions, which is conservative).  This
assumes the orbits are solved one after another.

Ring order by itself gives little: orbit (i,j) is moved only by slices i, j
and the faces, and under those every other orbit containing depth i acts the
same way, as does every other orbit containing depth j.  So it does not
matter whether such an orbit lies in an inner or outer ring; only the first
orbit solved at a given depth gets that slice for free.

Diagonal (diag3 + diag2 + diag_x10, merging, current tool): D 12, U 17,
LFRB 20 = 49 without face turns; 10 + 16 + 18 = **44** with them.

### Either order for the last two pair stages (`--eitherorder`, orbitbound34)

After aUbU and aDbD both rings remain, and either may be solved first.  A
reflection swapping the orbits maps "b ring first, then a ring" onto the forward
stages, so the same two tables serve both orders.  In the boundary pass, for an
a-ring state whose forward carries leave some b-ring states uncovered, each such
full colouring is mapped through the swapping symmetry and solved forward
(a-ring paths plus the b-ring table); the bound holds if one order is within X.
The random check (`BOUNDARYCHECK`) also tries both orders.  Result: a ring + b
ring <= 55 (forward alone: 56), and 54 with the seeded b-ring table.  The certified pair bound
no longer needs a boundary pass (each stage's worst case is lowered directly).

### New algorithms and table seeding (2026-10-11)

**tcwalk** (devtools/tcwalk.cpp): every algorithm for a centres-first oblique solve, by
depth-first search over the 54 moves (faces, depth-2, depth-3) on the (2,3) pair.  A
sequence keeps every other oblique orbit colour-solved iff both projections (drop the
depth-3 turns; drop the depth-2 turns and rename 3 to 2) return the 5x5x5 T-centres to
their faces (checked on 26,000 sequences and 167,618 re-interleaved pool algorithms against
an 8x8x8).  Pruning: exact distances for every 5^3 T-centre colouring within 6 moves, and
the slice twists (every pool class, 7.4M, has all slice twists 0).  Cores only (no face turn
in the first or last same-axis run), one per symmetry class; diagonals, edges and corners
ignored.  Complete against the pools up to 8 moves (every pool class, stripped, is present).
Classes of (2,3)-pair effects: 4:5 5:14 6:142 7:1,456 8:12,477 9:156,076 10:1,655,934
(length 10: 2.2 h).  Reports every cycle structure with its shortest sequence (136 up to 8;
the one-orbit 3-cycle first at 8).

**Pairing seeds** (orbitbound `PAIRSEED=I`): bucket every algorithm by where each solved
group's pieces go (64-bit key, sorted); within a bucket A C^-1 keeps the solved groups.  The
bucket of algorithms already legal for the stage is skipped (their products are
compositions the search has).  Products run at about 4e7/s; a product is a seed when its
state's table distance exceeds its cost.  Seeds are propagated (dist(b(s)) <= cost(b) +
dist(s) over all actions, no merges counted), `PAIRTARGET=k` also tries every product on
the states at the top k distances, `PAIRROUNDS` repeats until nothing changes, and
`PAIRSAVE` writes the improved table.  With tcwalk up to 9 moves:
- b ring: 1.7e9 products, worst still 34 but 34: 24 -> 8, 33: 1,762 -> 1,370; then with tcwalk
  up to 10 moves (174.7M algorithms, 7.4e10 products, 36 GB): the last 8 fell by 5, worst 33
  (1,314 states);
- a ring: 8.75e10 products, worst 22 (2,874 states at 22);
- aDbD: 2.1e11 products, worst 28 (264 states at 28).
A seeded table serves as stage B of a boundary pass (only its values are used); stage A
needs action paths, so it keeps its original table.

### Deeper seeds: conjugation by slices (`--conjslice C`)

X A X^-1 is as pure as A for every move X (moves keep each orbit in place, so
the support stays in A's orbit).  `--conjslice C` adds these for every pool
algorithm of cost >= C and all 54 moves, slices included.  Single oblique with
C = 8: 205k entries become 7.65M (mostly new 12-14 movers); the UD stage
stays 35.

### Looking across a stage boundary (2026-10-10 night)

The staged bound adds each stage's worst case, but the worst cases rarely meet.

**Orientation (`--firstmin N`, no gain).**  The first stage can be applied with any
face or axis as "U", so its effective bound is the largest k such that some full
colouring is at >= k in every orientation (found by forward-checking search over
the bad states, with the witness re-checked through the forward symmetry maps).
Every case so far has a symmetric witness: free UD 20 (4 states; witness a
face pattern), pair aUbU 21 (208 states), single UD 36 on the pool without
obl11.single (witness: whole faces permuted).  So nothing is gained.

**Boundary (`--boundary I`).**  Stages A = I and B = I+1.  A path P solving the
A-state u (cost c) carries the remaining pieces onto B's positions by a
permutation pi_P.  The rest colourings r with c + dB(pi_P r) > X are
pi_P^-1(B states beyond X - c): an explicit small set for the cheapest path,
which is then filtered by every other path found for u (up to 3,000 distinct
carries, best-first depth-first search over A's actions, merges counted).  A + B
<= X holds once that set is empty for every u with dA(u) > X - max(B).
Check (`BOUNDARYCHECK=N`): N random hard u with random rest colourings, solved
directly on the coloured positions without the carry bookkeeping.  On a small
diagonal pool, D 12 + U 17 = 29 became 27 (300 random checks, worst 27).
`DUMPDIST=prefix` saves the tables, and `LOADDIST=prefix` reloads them, skipping
the search.

### Cancellation between algorithms (`--merge`, 2026-10-09)

Per state, besides the one-byte distance, keep a 54-bit mask of the last
moves of its cheapest paths (8 bytes per state, not 36x the states).  An
algorithm whose first move is the same layer as one of those last moves
cancels (-2, amounts summing to 0 mod 4) or merges (-1).  Actions become
(free-position permutation, first move, last move), cheapest per triple, kept
when at most one move above the permutation's cheapest; composition pruning
is off.  Paths kept only at a longer distance are dropped, deeper cascades and
commuting-move merges are not counted, and the start of each stage assumes no
previous move, so this is still a valid upper bound.
Test, diagonal with diag3 + diag2, plan D,U,L,R,FB: 12+22+14+24+12 = 84
without, 12+19+13+20+12 = 76 with merging.

`--worst FILE` (orbitbound, wingbound) writes every position at each stage's
worst distance when there are at most 1e6, for later analysis (e.g. re-solving
only those with a deeper search to lower a stage bound by one).  Diagonal with
3-cycles only, D,U,LFRB: 32 + 32 + 64 = 128; 4,845 / 1,820 / 14,697 worst
positions.

### Approach

Staged worst-case analysis (devtools/orbitbound.cpp):

- The 24 positions of an orbit form six groups of four (one per face colour).
  A stage takes the set S of already-solved faces and solves a further set T,
  using only algorithms that keep every face of S solved (they may permute
  pieces within a solved face).
- Stage state: for each unsolved position, which face of T its piece belongs
  to, or "other".  Sizes: one face from scratch C(24,4) = 10,626; two faces
  with one solved 8,817,900; UD from scratch 51,482,970; the four side faces
  with U, D solved 16!/(4!^4) = 63,063,000.
- Exact worst case M(S, T): Dijkstra from the goal (pools are closed under
  inversion, so distance to and from the goal agree), each algorithm costing
  its move count, level-synchronous and multithreaded with atomic min on a
  uint8 distance array.  No cancellation between consecutive algorithms is
  counted, so the result is an upper bound.
- Algorithms are reduced per stage to their action on the unsolved positions
  (cheapest per action), compositions of a short algorithm and another
  algorithm of no greater total cost are dropped, and an edge budget
  (states x actions, default 4e11) drops the costliest whole cost classes for
  big stages.  Any subset still gives a valid bound if it reaches every
  position; the result is just possibly weaker.
- B(S) = min over T of M(S, T) + B(S + T), memoised over symmetry classes of
  (S, T) under the 48 cube symmetries (24 for single obliques); B = 0 once one
  face is left.  `--maxstates` caps the stage size considered.
- Modes: `diag`, `mid` (24 positions), `single` (pair-pool algorithms that keep
  the partner orbit colour-solved, restricted to one orbit), `free` (partner
  ignored), `pair` (48 positions, 12 groups; with `--plan` to evaluate a given
  stage sequence instead of searching).
- Pools: diag3 + diag2 + diag_x10 (every diagonal algorithm up to 10 moves,
  49M images); mid3 + mid2 + mid_x10 (5.4M); pair3 + p8s10 + p9 + eg8 + c12x,
  D0 only (single: 112,474 one-orbit algorithms, shortest 6 moves; free:
  2.42M, shortest 4).

Certificates and the write-up (devtools/orbitcert.cpp, bench/orbitbound/):

- orbitcert recomputes the chosen stages, then shrinks each stage's algorithm
  list: Dijkstra keeps, among equal distances, the parent of best priority;
  priorities are re-ranked by tree usage over three passes; the classes the
  tree uses reach every position at the same distance, and a final Dijkstra
  with every image of just those classes confirms the worst case.  It writes
  JSON: per stage the classes (representative move text; depth placeholder 3
  written as 2 for the diagonal), histogram, a hardest position with its
  solution, and example solves (scramble = 40 random 3-cycles).
- page.html + build.py make a single self-contained HTML page: the claim,
  the puzzle and method for a cuber, per-stage histograms and algorithm lists,
  hardest positions, step-through example solves on a cube net, and in-browser
  checks that use an independent N x N x N simulator: (1) every listed
  algorithm changes only the orbit on several cube sizes; (2) exhaustive
  stage searches in a Web Worker.  Tested in node: purity on 4..9 and stages 1
  and 2 reproduce the certificate's worst cases and histograms exactly
  (13 s and 1 s).  Stage 3 (63M positions) is too slow for a browser at about
  100 ns per edge; it is offered as a slow optional check.

### Still to do

0. Superseded by "Wings by 3-cycles" (W 149 + 4 once), pending its certificate.  Old plan, kept
   for reference: wing parity (W 162 -> 156): the last wing stage's worst is 51 on even wing permutations and
   58 on odd ones, and every earlier wing-stage line is even.  If every centre-certificate line
   is even on every wing orbit (an even number of quarter turns of each inner slice), the wing
   parity entering the wings is the scramble's, and one slice quarter turn per odd orbit at the
   very start (before the centres, whose bounds are worst-case) makes it even: W = 32 + 32 + 40
   + 51 + 1 = 156.  All diagonal, mid and pair lines are even except 35 b-ring lines (lengths
   28-31), listed in cert/bring-oddwing.seq.txt; each needs a replacement of at most 31 moves
   with an even number of quarter turns at depth 2 and at depth 3 (devtools/wingparity checks
   this; that list predates the 2026-10-18 merges).  Only this 156 plan needs even lines: W = 162
   takes odd wing permutations in its last stage, and W = 149 fixes parity after the centres
   (bulk fix, 4 + 5x), so merges need no parity check.
1. Chiral pair (certified 88 = 42 + 46 with boundary certificates, see "Boundary passes on the
   certificate"; after merging new logs into a stage, regenerate its boundary file: UD 42 ~1.5 h
   + 20 min check, rings 46 ~12.5 h + 6 h check, or 47 ~2 h + 1.2 h): the stages are at 19 / 24 / 21 / 29 (the boundary files in cert/obl23pair/cf-xfree/boundaries/ belong to the 2026-10-17 stages).  Next: the b ring's 260 at 29 (cert/bring-29.seq.txt), the a ring's 1,769 at 21 (cert/aring-21.seq.txt), aDbD's 96,148 at 24,
   aUbU's 36,652 at 19 (needs a faster certextend for aUbU).  Merge with certmerge (or
   devtools/pairfeed.sh for logs), re-run certcheck.
2. Page sections for mid (certificate done: cert/mid/centres-first) and a certificate for single oblique (build.py needs
   their text; the page already handles kind `mid` and `oblique`, with the
   oblique (1,2) orbit at depths 2 and 3 checked on 6x6x6 and up).
3. Tighten the bounds:
   - diagonal: a joint UD stage with cost <= 7 algorithms (51M x 68k actions,
     a few hours) could beat D 12 + U 18 = 30 (mid gets 26 jointly);
   - all orbits: the side-face stage ran at the edge budget (cost <= 7 for the
     diagonal, 1,074 actions for mid); a bigger budget or a symmetry-reduced
     state space (16 symmetries keep U and D) should lower 24 / 28 / 40;
   - single oblique has few short algorithms (6 six-movers): longer pools
     restricted to one orbit (e.g. from twsearch) would help most here.
4. In-browser stage-3 check: a symmetry-reduced search or a compact
   certificate (one action index per canonical state) would make it fast.
5. Edge (wing) orbits.  Wings are solved first, so a wing algorithm only
   has to preserve corners, middle edges and the other wing orbits; centres
   may end up anything (user, 2026-10-09).  The wing pools are exactly that:
   wing3 (all 3-cycles, 8 moves) and wing_d6c (1,223,161 permutations up to
   13 moves, mostly <= 8; single slice turns cost 1); devtools/wing_d8 has
   10.4M more.  Tool: devtools/wingbound.cpp (wings are distinct pieces; a
   stage places a chunk of specific wings, state = their positions, n!/(n-k)!
   states; order and chunk sizes are options, `--random R` tries random
   orders).
   - First test, chunks 4,4,4,4,4,4 in model order: 12 + 15 + 18 + 18 + 23
     for 20 wings, then the last 4 wings are unreachable: with 20 wings fixed
     the pools hold only 3-cycles, so odd arrangements of the last 4 cannot be
     reached.  Wing parity is free (a slice quarter turn is a 4-cycle), so the
     final stage must be big enough to contain odd algorithms.
   - chunks 4,4,4,4,8: 12 + 15 + 18 + 18 + 30 = 93.
   - chunks 4,4,5,11: 12 + 15 + 20 + 30 = **77** (last 11 wings jointly,
     39,916,800 states, 1,016 usable algorithms, 201 s).
   - chunks 4,5,4,11: 12 + 17 + 18 + 30 = 77 as well.
   - Next: add wing_d8, choose the wing order (edge partners together vs
     spread; `--random`), certificate and page section.
6. Bound for an N x N x N cube's whole centre set: sum over orbits, or better,
   a reduction that does the UD stage for every orbit at once.


# 2. Relaxed stages (was upperbound2.md)

## Upper bounds with relaxed stages (from 2026-10-23)

The certified results of the original approach (every stage keeps every other oblique orbit
colour-solved; P = 87 = 41 + 46 with boundary certificates, 2026-10-22) are in upperbound.md.
This file records the relaxed stages.

### Idea

The chiral-pair stages kept every other oblique orbit colour-solved ("f").  The whole-cube order
needs less:

- if every pair's stage 1 runs before any pair's stage 2, stage 1 need only keep the other
  orbits' U faces;
- run with stage 2 pair by pair, stages 1 and 2 need only keep the other orbits' U and D faces;
- the a ring would need only keep U, D and the a-chirality orbits, but that is no relaxation: an
  orbit (2,s) (a chirality) and (s,2) (b chirality) both move exactly as MID2 (layer s never turns),
  through face-preserving correspondences, so keeping the a-chirality orbits forces MID2 and MID3
  colour-solved and so keeps every other orbit (likewise layer 3 and MID3).  3uda = 3f.  The b
  ring keeps everything (4f).  Only stages 1 and 2 relax.

Since 3f and 4f keep everything, the rings can always be done pair by pair (3f then 4f), keeping
the ring boundary (46).  Only the UD half depends on the choice: 1u for every pair, then 2ud for
every pair, has no UD boundary (plain sum 1u + 2ud); 1ud then 2ud pair by pair keeps the UD boundary
(boundary pass over 1ud + 2ud).  P = min of those two, + 46.

### Names

The digit is the stage; the letters say what the other oblique orbits must keep (f: everything,
the original stages).

| name | stage | other oblique orbits keep |
|---|---|---|
| 1u | aUbU | U face |
| 1ud | aUbU | U and D faces |
| 1f | aUbU | everything |
| 2ud | aDbD | U and D faces |
| 2f | aDbD | everything |
| 3uda | a ring | U, D, and the a-chirality orbits (the same as 3f: see above) |
| 3f | a ring | everything |
| 4f | b ring | everything |

### Tools

- devtools/tws: aubu19u.tws, aubu19ud.tws (the aUbU stage with MID2, MID3, which stand in for every
  other oblique orbit, labelled by face set: U / other, or U / ring / D); aubu19all.scr (all 36,652
  classes the 2026-10-22 certificate solves in 19, as ScrambleAlg inverses of their lines in
  aubu19all.sol).
- Arena generation: 7pair-1u.tws and 7pair-1ud.tws (pair pieces all identical, MIDs labelled by
  face set) piped through -U 7pair-1u-uniq.tws / 7pair-1ud-uniq.tws (pair effects only), go1u.sh
  and go1ud.sh (one socket each, numactl).
- pairlogcheck, certcheck, certmerge and certextend take --keep u|ud for the relaxed side
  condition.  Checked: the twsearch 1u solutions pass pairlogcheck --keep u on 6..12 and fail
  without it and with --keep ud.  certextend --keep uses only the symmetry images that map U to U
  (u) or U to U or D (ud): for stage 1 every one of the 48 symmetries keeps the stage, and one that
  takes U elsewhere turns "keep U" into "keep F" (certmerge caught this: without certextend --keep,
  4.0M of 4.04M 1u lines were rejected).
- certruns/relaxed/rfeed.sh NAME KEEP STAGE LOG [PAIRFLAG]: pairlogcheck, certextend (CEARGS),
  certmerge, certcheck, all with --keep, on a copy of a 2026-10-22 stage in /var/tmp/orbitsolve/rNAME.

### Results

**1u lower bound: 15 (2026-10-23).**  The aUbU class aubu19-00007 (devtools/tws/aubu19all.scr,
one of the 36,652 classes at 19 in the 2026-10-22 1f certificate) needs 15 moves under 1u (every
other oblique orbit keeps only its U face): twsearch on devtools/tws/aubu19u.tws (-M 7000)
exhausted depth 14 and found

    scramble: 2R2 3B' 2R2 3B D 2D' 3R 2F 3L' 2D 3R' 2F' 3L 3D 2L 3D' 3U' 2L' 3U
    1u solve: 2B2 U' 3L 3R 2D R' 2U 3R' D 2L2 2R' 3D2 3U' 3L' 2R   (15, optimal)
    1f line:  3U' 2L 3U 3D 2L' 3D' 3L' 2F 3R 2D' 3L 2F' 3R' 2D D' 3B' 2R2 3B 2R2   (19)

so 1u's worst is at least 15 (the first seven of those classes: 14, 14, 13, 13, 14, 14, 15;
/var/tmp/orbitsolve/aubu19u-sample.out).  For comparison, the stage with nothing kept has worst 9.

**1u, twsearch on this machine** (aubu19u.tws, -M 7000, optimal): the first seven of the 36,652
aUbU 19s solve in 14, 14, 13, 13, 14, 14, 15 moves.  So 1u's worst is at least 15.  Stopped
(depth 15 took 43 min per position); /var/tmp/orbitsolve/aubu19u-sample.out.

**1ud, twsearch on this machine** (aubu19ud.tws, -M 30000, optimal): the first six of the 36,652
aUbU 19s solve in 15, 14, 14, 14, 15, 15 (aubu19-00001..6), so 1ud's worst is at least 15 too
(1u gave 14, 14, 13, 13, 14, 14 on the same six).  Stopped 2026-10-23 17:45 to give the machine to
the arena feeds; /var/tmp/orbitsolve/aubu19ud.out.

**2ud worst 22 (2026-10-24 16:19).**  The target-side passes (certextend --proptop: every class
at the top lengths through every relaxed action of up to 9, 10, 11 moves) took 2ud's 23s from
40,627 to 112; the arena 12-13 lines streamed against those (--band 1) left one class.  Its line
(devtools/tws/adbd23ud.sol, 23 moves; adbd23ud.scr and adbd23ud.tws for the stage) was cut to 17
by a separately trained ML solver (the other agent, ~/mlcube):

    2F 2B' U 3F' 2R' 2D 3F R' 3B2 3U 2R 3D2 3B2 U R 2B 2F'

fed through rfeed.sh (pairlogcheck --keep ud, certmerge, certcheck --keep ud PASS at 22);
propagation from it shortened 418 more classes.

**2ud worst 21, 1u and 1ud worst 18 (2026-10-24 23:25).**  certextend --slices adds 2U, 3U, 2D,
3D (every amount) as one-move actions: they never touch U-face or D-face pieces, so they keep the
relaxed side condition (they do move the pair's own pieces).  With them: 2ud's 22s cleared, and
--proptop 21 at 11 moves then two blocks (--proptop2 4, first block to 9 and 10) took its 21s to
710; 1u's last 12 19s cleared by --proptop 19 at 10 moves.  1ud's 54 19s: --proptop 19 at 10 moves
left 11 (devtools/tws/aubu19ud-r2.scr, .sol), which the ML solver cut to 15 and 16
(~/mlcube/aubu19ud_improved.txt); fed through rfeed.sh (certcheck --keep ud PASS at 18),
propagation from them shortened 2,328 classes.  So the plain sum 1u + 2ud is 18 + 21 = 39.

**2ud worst 20 (2026-10-24 23:52).**  The ML solver solved all 710 2ud 21s (devtools/tws/adbd21ud-r.scr,
.sol) in 16 to 20 moves (~/mlcube/adbd21ud_live.txt); fed through rfeed.sh (certmerge 0 bad,
certcheck --keep ud PASS at 20), propagation from them shortened 112,269 classes.  Plain sum
1u + 2ud = 18 + 20 = 38.

**1u and 1ud worst 17 (2026-10-25 09:04).**  The ML solver solved 1ud's 124,320 18s (devtools/tws/
aubu18ud.scr, shuffled with seed 20261025) overnight, fed every 30 minutes by certruns/relaxed/
rmlfeed1ud.sh; the last 91 (aubu18ud-left.scr) came in at 14 to 17 (~/mlcube/aubu18ud_left_live.txt):
certcheck --keep ud PASS at 17.  The 1ud certificate fed into 1u (its lines are 1u-legal) shortened
486,898 1u classes: certcheck --keep u PASS at 17.  Plain sum 1u + 2ud = 17 + 20 = 37.

**2ud worst 19 (2026-10-25 15:57).**  2ud's 240,042 20s (devtools/tws/adbd20ud.scr, shuffled with
seed 20261025) went to the ML solver; its lines (~/mlcube/adbd20ud_live.txt, mostly 17-19) were fed
every 30 minutes by certruns/relaxed/rmlfeed2ud.sh, which also rewrote adbd20ud-remaining.scr (the
classes still at 20) for the solver to work from.  Each line shortened about 300 classes through
propagation (one-move face and slice actions and pool actions to 8), so about 2,800 lines cleared
all 240,042: 240,042 -> 169,613 (348 lines) -> 68,465 (615) -> 547 -> 227 (1,170) -> 42 (with the
arena 14s, below) -> 0 (661 lines).  Meanwhile the arena 2ud run resumed after a long fill (no
output 10-24 15:29 to 10-25 morning) and certruns/relaxed/rloop3.sh fed its 14s (--near 6 --prop 8
--slices): one round of 99M lines shortened 316,587 classes.  certcheck --keep ud PASS at 19 with
180,013 classes at 19.  Plain sum 1u + 2ud = 17 + 19 = 36.

**Rings 45 (a ring + b ring <= 45; running in parts).**  The rings are the same
3f and 4f stages as in upperbound.md (P93-2026-10-22 snapshot), so a rings 45 boundary combines with
any UD bound here: P = UD + 45.  The 10-22 run (one boundx over every class) was stopped after 14 h
with nothing written, so the run is now split into 20 resumable parts:
certruns/boundary/ring45.sh takes the 3,593,948 a-ring lines longer than 16
(/var/tmp/orbitsolve/bcert/ring45/alines.txt; only-00 .. only-19 take every 20th line) and runs
boundx 45 --deep 1 --slack 5 --only on each part, keeping part-NN.txt only when every class is
covered (otherwise part-NN.tmp); when all 20 are in it joins them into
/var/tmp/orbitsolve/bcert/ring45.txt and runs boundcheck.  Part 00 (179,698 classes) took about 6 h
with about a third of the machine (the 2ud feeds had the rest), so the whole run is 2 to 5 days.
- From part 02 on (2026-10-25 night) the parts run in four lanes: certruns/boundary/ring45p.sh LANE 4
  takes the parts NN with NN mod 4 = LANE (log ring45.laneL.log), and ring45join.sh then joins the
  parts and runs boundcheck (log ring45.log).  Each part uses about 2.4 cores and 3.7 GB.
- Pause after the current parts: `touch certruns/boundary/ring45.stop` (each lane exits before its next part).
- Resume: `rm certruns/boundary/ring45.stop; for l in 0 1 2 3; do nohup certruns/boundary/ring45p.sh $l 4 >> certruns/boundary/ring45.lane$l.log 2>&1 & done`,
  then `certruns/boundary/ring45join.sh >> certruns/boundary/ring45.log 2>&1` once every lane is done
  (finished parts are skipped; an interrupted part starts over).
- Progress: ring45.log lists the parts started; inside a part, the blank lines in part-NN.tmp count
  the classes done; part-NN.err has a "class ...: N deep alternatives, ... -> 0 colourings left" line
  for each hard class (anything but "-> 0" means a class not covered); part-NN.out has boundx's
  verdict line ("X=45: 0 of N classes not covered").
- Part 00 done 2026-10-25 15:19: 179,698 of 179,698 classes covered (part-00.txt).
- Part 01 done 2026-10-25 22:47: 179,698 of 179,698 covered (6.6 h, at nice 20).  Parts 02-05 started 22:47 in four lanes.
- 2026-10-26 08:30: paused after parts 02-05 (ring45.stop) so the 2ud arena backlog can drain; /var/tmp/orbitsolve/ring45resume.sh
  restarts the lanes once a rloop4 round pulls under 3 GB of 2ud log, then joins and runs boundcheck
  (now ring45resume2.sh, which first runs ring45fix.sh 03 04 05).
- Parts 02-05 done 2026-10-26 09:15-09:24: part 02 fully covered; parts 03, 04, 05 left 2, 1, 1 classes
  (5-7 colourings each) and are kept as part-NN.tmp.  ring45fix.sh reruns those classes with --deep 3
  --slack 7 and splices the blocks in (bsplice.py); ring45p.sh no longer reruns a part with a verdict.
- Parts 14-19 run on armory.local (M1, 8 cores, 16 GB; ssh -i ~/.ssh/id_gizmo), two at a time from 2026-10-26 12:13:
  ~/ring45 there holds boundx, copies of the two ring stages and the five pools; armlane.sh 14 16 18 and
  armlane.sh 15 17 19 (logs lane0.log, lane1.log; stop: touch ~/ring45/stop); results in ~/ring45/parts.
  Here those parts have part-NN.remote, which ring45p.sh skips; copy part-NN.txt/.out/.err back before ring45join.sh.

**Distance tables (classes; positions are about 8 times as many).**  1f and 2f: the 2026-10-22
certificates (obl23pair-cf-xfree-P93-2026-10-22).  1u, 1ud, 2ud: copies of those stages in
/var/tmp/orbitsolve/rNAME, each certcheck --keep PASS after every feed.  Passes so far: the arena
lines directly (each solves its own position), the shortest-path pass (certextend --propall
--prop 6), and each arena line in front of every certificate line of at most K moves
(certextend --near K: K = 4 with the lines to 11, K = 6 with the later ones).  Exact (the arena
log complete through that depth): 1u and 1ud through 11, 2ud through 12; longer lengths are upper
bounds.  Since 2026-10-23 18:00 certruns/relaxed/rloop.sh feeds each log's new lines every round
(processed offsets in certruns/relaxed/processed.tsv) and rebuilds this table.

<!-- rtable start -->
Updated 2026-10-26 13:32 (certruns/relaxed/rtable.py).

| length | 1f | 1u | 1ud | 2f | 2ud |
|---:|---:|---:|---:|---:|---:|
| 0 | 1 | 1 | 1 | 1 | 1 |
| 3 |  | 4 | 2 |  |  |
| 4 | 26 | 29 | 26 |  |  |
| 5 | 56 | 187 | 136 |  |  |
| 6 | 389 | 850 | 620 |  | 28 |
| 7 | 1,183 | 3,849 | 2,580 | 16 | 106 |
| 8 | 5,732 | 15,270 | 12,814 | 150 | 330 |
| 9 | 25,029 | 48,997 | 38,714 | 210 | 422 |
| 10 | 70,853 | 167,529 | 122,474 | 526 | 2,346 |
| 11 | 244,157 | 497,891 | 380,568 | 2,924 | 14,772 |
| 12 | 630,748 | 1,231,851 | 993,315 | 15,652 | 26,672 |
| 13 | 1,500,590 | 2,346,068 | 2,028,815 | 29,917 | 56,558 |
| 14 | 2,465,019 | 3,405,406 | 3,169,592 | 71,140 | 148,682 |
| 15 | 3,363,576 | 3,487,606 | 3,776,410 | 123,524 | 348,606 |
| 16 | 3,118,018 | 2,492,805 | 2,905,185 | 144,961 | 504,168 |
| 17 | 2,060,671 | 421,508 | 688,599 | 213,180 | 963,539 |
| 18 | 597,151 |  |  | 311,857 | 861,959 |
| 19 | 36,652 |  |  | 450,413 | 8,746 |
| 20 |  |  |  | 611,867 |  |
| 21 |  |  |  | 620,251 |  |
| 22 |  |  |  | 271,869 |  |
| 23 |  |  |  | 68,205 |  |
| 24 |  |  |  | 272 |  |
| **classes** | 14,119,851 | 14,119,851 | 14,119,851 | 2,936,935 | 2,936,935 |
| **worst** | 19 | 17 | 17 | 24 | 19 |
<!-- rtable end -->

Arena lines are pair permutations, not positions: 9.5M 1u lines of length <= 10 reach only about
110K classes.  The shortest-path pass gains little on stage 1 (1u 19s 36,652 -> 29,615): the
twsearch solutions of 13-15 are single long legal sequences, not chains of short legal blocks.
Next: each arena line to 11 as a first block in front of every certificate line of at most K
moves (certextend --near K; certruns/relaxed/near1.sh: 1u and 1ud K = 4, 2ud K = 7).  Logs:
certruns/relaxed/r*-10.log, r*-11.log, r*-pa6.log; 2ud's direct pass and --worst pass took 24 -> 23.

**Arena 7pair-1u.log** through length 10: 9,528,300 pair effects (4: 46, 5: 104, 6: 1,203,
7: 7,711, 8: 88,271, 9: 806,781, 10: 8,624,183).  Fed into a copy of the 2026-10-22 aUbU stage
(/var/tmp/orbitsolve/r1u): see the table above.


# 3. The axis split, as of 2026-10-29 22:10 (was upperbound5.md)

The working notes as they stood before the clean-up, verbatim: run logs (op1, t1-t39, smoke, A, B,
ax-t1-ax-t10), the 3ax alternatives A and B, pools, the ML batches.

## Axis split: stages 3ax and 4op (working notes, from 2026-10-28)

Working memory for the third 3/4 split of the oblique chiral pair.  Certified results stay in
upperbound3.md (currently P 80 = 1ud 17 + 2ud 18 + rings 45); 3fr/4h is parked in upperbound4.md.

### Status (2026-10-29 22:10)

- **4op <= 23** (t39, 2026-10-29 22:10), 25,745 classes at 23.  The 24s went 15,256 (t27) -> 12,291 ->
  1,952 t31 -> 946 t33 -> 631 t35 -> 10 t37 -> 0 t39 (arena's reduced-move-set optima, ML lines, the
  merge pass).  Table /var/tmp/orbitsolve/op/4op-t39.tab (also armory ax3/).  List
  devtools/tws/4op23-t39.* (4op-t39-ML.md).  Arena (user) continues over 4op2324-t33.scr.
- **Merge pass (2026-10-29, axbound/opbound, default; --nomerge to skip).**  The engine scores a
  chained line as the sum of its parts, so turns that cancel at the seams were never counted (the
  merged 25s, 24s and 23s found by hand).  Now every class's line is rebuilt and simplified, shorter
  ones are loaded and propagated, until nothing changes.
- **3ax <= 25** (ax-t9, merge pass), **67 classes at 25**, 525 at 24, 8,400 at 23.  Table
  /var/tmp/orbitsolve/ax/ax-t9.tab (also armory ax3/).  ML: devtools/tws/3ax-t9-ML.md (3ax25-t9.*, 67,
  goal <= 24; then 3ax24-t9.*, 525, goal <= 23).
- **Certificate (2026-10-29 17:55): ~/orbitsolve-certsafe/obl23pair-axis-2026-10-29** (MANIFEST +
  stages/3ax/lNN.txt, stages/4op/lNN.txt: one file per sequence length, one line per class).  3ax from
  ax-t8, now ax-t9 (<= 25), 4op from t29, now t39 (<= 23, PASS 22:10).  devtools/axcheck (new: lines legal and at most NN, each a
  stage start; one line per class under the 16 stage symmetries and every stage start covered; side
  effects on 6..10, diagonals free): 4op PASS (24,010,000 starts, 1,508,387 classes, 7 s), 3ax PASS
  (165,636,900 starts, 10,366,284 classes, 12 s on armory).  Negative tests (a line removed, a class
  repeated, a line too long for its file, a non-start, a line moving another oblique orbit) all FAIL
  as they should.  Every ML and arena line folded so far was also replayed on obl8op/obl8ax.tws
  (linecheck.py): 0 problems.  Update: devtools/axcertupdate.sh CERTDIR 3ax|4op TABLE rewrites only
  the length files that change, then rerun axcheck.
- **Plain sum 48** (3ax 25 + 4op 23), against the rings' 50 plain / 45 with boundary.
- **Diagonals are free (2026-10-28 22:50).**  The diagonals are solved last, so pool lines that move
  them (pairlogcheck flag D1) are valid actions.  Every table up to t6 / ax-t4 used only D0 lines
  (axbound skips D1 without --diagok).  With --diagok, the 4op actions of cost <= 10 grow from 3,378 to 11,024.
  The ML puzzles (obl8op/obl8ax.tws) only have the six oblique sets, so ML lines were never restricted.
- **Arena 4op log in batches.**  /var/tmp/orbitsolve/op/arena/pull.sh N pulls the new tail of
  twsearch2/7pair-4op.log from the saved offset, checks it with pairlogcheck --ud --emit and writes
  the pool.  Batch 1 (the first 1 GB, all 12s and 13s, part of the 14s) gave 20,657,411 lines, 1.13M of them D0; batch 2 (to byte 1,078,799,956) 1,587,578 14s.  pull.sh keeps D0 and D1.
  Running on armory: t7 = t6 + --diagok + batch 1 (actions and loaded lines), --prop 10, band 1.
- **Tools (2026-10-28):** opbound and axbound are one engine (axbound.cpp; opbound.cpp = OP4 wrapper):
  actions once per conjugacy class (4op: 202,172 instead of 3.1M), class lowering, dirty bits, byte
  tables for every action when they fit (--tabmb), --band 255 = full band (about a day for 4op: not
  used), --rounds, --from L, --save/--load, and --base (load a saved table as closed: an ML fold-in
  takes ~20 s).  Cancellation counting tested and not worth it (0.5% of lines shorten).
- **Arena:** 7pair-4op.log generation stopped by the user (3 batches pulled, to the 14s).  Since 2026-10-29 15:00 arena runs reduced-move-set optimal solves (4op24-t26.scr, then 4op2324-t33.scr), the strongest source for the 4op top.
- **Pergola** (10.0.0.8, same ssh as arena): twsearch/7pair-4op-2.log, 4op in the reduced move set F R B L 2U2 3U2 2D2 3D2 (all to 16 and 7.7M 17s pulled and folded, t22-t24, t32, t36, t38; nothing at the top except 7 band classes).  For 3ax: devtools/tws/7pair-3axs.tws + 7pair-3axs-uniq.tws + go3axs.sh (side pieces only; pair two colours, mids four), copied to pergola 18:10, not started.  /var/tmp/orbitsolve/op/pergola/pull.sh N, same as arena's.
- **Open:** t3 (--prop 10) found 32 more positions at 13 than t2 (120,300 against 120,268), so the
  base + 7pairud pools are not quite complete at 13 as 4op actions (D1-flagged lines are skipped
  without --diagok?).  To check before calling the table exact through 13.

### Why this split

The three 3/4 splits, after U and D are solved for both orbits (stages 1ud, 2ud):

| split | stage 3 | stage 4 | plain sum | with boundary |
|---|---|---|---|---|
| rings | a ring 21 | b ring 29 | 50 | **45 certified** |
| front + horseshoe (parked) | 3fr 19 (certified) | 4h <= 40 (<= 38 with 4hb optimal) | 57-59 | not done |
| **axis + opposite** | 3ax <= 25 (B) | 4op <= 26 | 51 | not done |

The last stage is always the killer (b ring 29, 4h 38-40).  4op is small: 24M positions and ~1.5M
classes, against 63M for a ring and 1.2G for 4h.  It also keeps the full 16-way symmetry.  User's
expectation (2026-10-28): **3ax ~20, 4op ~25, so 45 plain**, before any boundary pass.

### Stages

| name | stage | solves | positions | stage symmetries | classes |
|---|---|---|---|---|---|
| **3ax** ("axis") | aUaDbUbD..axis | both orbits' ring pieces sorted by axis: F/B pieces onto F or B, L/R pieces onto L or R | 165.6M | 16 | ~10.4M |
| **4op** ("opposite") | axis..aFaLaRaBbFbLbRbB | F vs B and L vs R, in both orbits at once | 70^4 = 24,010,000 | 16 | ~1.5M |

- **16 symmetries:** the 8 rotations that keep the U-D axis, times the mirror.  The mirror swaps
  orbit a and orbit b.
- **Lower bound:** 4op >= 15, because it contains 4hb (aL aR bL bR with F and B solved), and 4hb is
  exactly 15 (twsearch optimal, 2026-10-27).
- **Kept orbits:** both stages keep every other oblique orbit fully, as the rings do; see the 3ax
  alternatives below.

**4op state** (devtools/opbound): 32 bits.  Per orbit there are 8 F/B slots (bit set = F piece; F face
bits 0-3, B face bits 4-7) and 8 L/R slots (bit set = L piece; L face bits 8-11, R face bits 12-15).
Orbit b is in the high 16 bits.  SOLVED = 0x0F0F0F0F; rank = r16[low16]*4900 + r16[high16].

### Pools (the fair comparison)

Any 3ax or 4op solution keeps the pair's U and D and both mid orbits (MID2, MID3), so it is a
7pairud sequence.  Arena's 7pairud.log is complete to length 13, so it is the fair pool for both
stages, as it was for 3fr.  The same argument makes the tables exact through 13.

- **3ax pool:** the generator is 7pairud.tws as is.
- **4op generator** (arena, started 2026-10-28), devtools/tws/:
  - `7pair-4op.tws`: pair labelled U 1, F B 2, D 3, L R 4, so each sequence keeps U, D and the axis
    split; MID2, MID3 colour-solved.
  - Rotations: the 8 that keep the U-D axis, with labels permuted (generators y = quarter turn, z =
    half turn swapping U and D).  The mirror half of the 16 swaps PAIRA and PAIRB and can't be
    expressed as a twsearch rotation.
  - `7pair-4op-uniq.tws`, and `go4op.sh` -> twsearch2/7pair-4op.log.
  - 4op sequences are a subset of 7pairud, so this should get past 13.
  - Smoke test (capped at 3,000 lines): the 18 outer turns at length 1 and 4-move slice commutators
    such as `2D2 3F2 2D2 3F2`.  With 8 rotations, 5-move lines drop from 200 (y2 only) to 68.

### The two alternatives for 3ax

2U2, 3U2, 2D2 and 3D2 never touch the U or D faces, and only swap side pieces F<->B and L<->R.  They
map the 3ax solved set to itself (and the 4op position space to itself), for every oblique orbit.
They do move MID2 and MID3 between faces, though.

**A. Slice moves allowed in phase 3, no boundary.**
- Run phase 3 on *all* obliques before any phase 4.
- Then the four half twists are extra cost-1 moves in 3ax: one never takes a phase-3-solved orbit out
  of solved.
- 3ax gets a richer move set, so its worst should drop.
- Cost: phase 3 and phase 4 can't be merged, so there is no 3ax/4op boundary pass, and the bound is the
  plain sum.
- To settle if we pursue A: what exactly phase-3 sequences must keep in the other orbits (fully
  solved, or only their axis state).  That decides the 3ax pool.

**B. No slice moves, boundary pass.**
- 3ax keeps every other orbit fully, so a 3ax line can be followed directly by a 4op line.
- A 3ax/4op boundary pass then merges the two stages, as for the rings.
- Cost: the boundary pass, which is about the size of the rings check (13.4 h on armory).

**Evidence and decision (2026-10-28):**
- The boundary was worth 5 on the rings (50 plain -> 45).
- Slice moves only shorten the phase-3 term.  A plausible gain is 1-2 at the top of 3ax, and nothing
  for 4op, the stage that matters.
- **Prefer B.**  When the 3ax table is built, run it both ways (with and without the half twists) to
  measure A's gain; it costs little.  A competes only if it saves 3 or more.

**Not valid as conjugation.**  X A X (X a half twist, A a pool line) keeps the mids only if A also
keeps the X-swapped mid colouring, which is not true in general (A may permute F's four MID2 pieces
among themselves).  Under B it could only be used as a per-line test of each conjugate, and it adds
nothing below 14 (the pool is complete to 13).

### 4op results

Tool: devtools/opbound (dense 24M-state table).
- **Actions:** outer F/L/R/B turns (cost 1) plus every symmetry image of every pool line that keeps
  the pair's U and D and the axis split; cheapest per distinct effect, closed under inverse.
- **Seeds:** dist(a(solved)) <= cost(a).
- **Propagation:** Dijkstra with the actions of cost <= --prop C.
- **Band:** every action is tried on the top --band B lengths, then propagation again, until nothing
  changes.
- **Replay check:** every worst state and --verify N random states are rebuilt as move sequences and
  replayed.
- **Limitation:** no cancellation between consecutive actions is counted, so every value is an upper
  bound.
- **Dirty bits:** propagation re-expands only states lowered since they were last expanded (op2,
  without them, spent hours re-expanding everything after its band round).
- **Persistent table:** `--save FILE` writes one replayed solving line per class ("LEN<TAB>moves",
  1,508,387 classes); `--load FILE` (repeatable) starts from such files or any 4op lines (e.g. ML
  output, NAME<TAB>moves; lines that are not 4op lines are skipped).  Every run loads the best table
  so far and saves a new one, so nothing is recomputed.
- **Classes:** lengths are reported by position and by class; `--worst FILE` writes one line per
  worst class (the input for the ML packages).
- Usage: `opbound [--prop C] [--band B] [--verify N] [--worst FILE] [--load FILE]... [--save FILE] [--diagok] [--threads T] POOL...`
- Logs: /var/tmp/orbitsolve/ring34/op1.* .. op4.*; runs on both machines (armory: ~/ax3/runop.sh).

| run | pools | actions | worst | notes |
|---|---|---|---|---|
| op1 | base (pair3, p8s10, p9, eg8, c12x) + 7pairud to 11; --prop 8 --band 3 | 187,562 | **28** | 8.5 min, 195 MB; replay 1,848 lines, 0 bad |
| op2 | base + 7pairud to 13 (ud.cls); --prop 10 --band 1 | 3,142,146 (46,266 of cost <= 10) | **28** | 352 positions at 28 (27: 17,248, 26: 120,090); 4.7 h on the laptop (re-propagation without dirty bits); replay 1,352, 0 bad |
| op3 | op2's pools, --load the 54 ML lines; --prop 8 --band 2 --save (laptop, 36 min) | 3,142,146 | **27** | 339 classes (5,104 positions) at 27, 26: 5,734 classes; first saved table /var/tmp/orbitsolve/op/4op-t1.tab (1,508,387 lines, all replayed); 27s to the ML agent as 4op27.* |
| op4, op5 | t1 (+ ML lines), band 3, old engine (laptop) | | stopped | band rounds with 3.1M bit-loop actions would have taken hours; replaced by t2 on the new engine |
| t2 | new engine: --load t1 + ML 28s + ML 27s; --prop 8 --band 3 --save (armory) | 202,172 (one per conjugacy class) | **26** | 4,171 classes at 26 (25: 19,218, 24: 60,055); band round 0 lowered 48,872 classes, round 1 none; 2 h 21 min; saved /var/tmp/orbitsolve/op/4op-t2.tab |
| t3 | --load t2 + 31 merged 26s; --prop 10 --band 1 --rounds 1 (armory, 10 min) | 202,172 (3,378 of cost <= 10) | **26** | 3,668 classes at 26 (25: 17,945, 24: 58,830, 23: 145,840); the middle moved down too |
| t4 | --base t3 + the first 332 ML lines for the 26s (armory, 22 s) | | **26** | the lines set 280 classes; propagation took ~1,200 more off 26: 2,202 at 26 |
| t5 | --base t4 + all 359 ML lines so far (armory, 21 s) | | **26** | 27 new lines set 18 classes; 1,980 at 26 (25: 17,636) |
| t6 | --base t5 + all 1,135 ML lines so far (armory, 22 s) | | **26** | 758 classes set; **577 at 26** (25: 17,195); ML batch devtools/tws/4op26-t6.* |
| t7 | --diagok, --load t6 + arena batch 1 (20.66M lines, as actions and loaded), --prop 10 --band 1 (armory, 46 min) | | **26** | 8.4M actions (11,024 of cost <= 10); 26: 37, 25: 5,986, 24: 24,660, 23: 68,897 |
| t8 | --base t7 + op-b1 table (armory, 2 min) | | 26 | 3,513 classes set; 23: 67,113 |
| t9 | --base t8 + 71 merged 25s (adjacent moves merge to 23-24) | | **26** | **26: 33**, 25: 5,808; ML batches 4op26-t9.*, 4op25-t9.* (devtools/tws/4op-t9-ML.md) |
| t10 | --diagok, --base t9 + arena batch 2 (1.59M 14s), --band 2 (25-26) --rounds 1 (armory, 34 min) | | 26 | band lowered 3,399 classes: 26: 14, 25: 2,706 |
| t11 | --base t10 + all ML lines so far (1,427: 26s and 25s) (armory, 2 min) | | **25** | 72 classes set; **no 26s**; 25: 2,479; ML batch 4op25-t11.* (4op-t11-ML.md) |
| t12 | --diagok, --base t11, --band 2 (24-25) --rounds 1 (armory, 2.6 h) | | 25 | band lowered 12,866 classes: 25: 2,198, 24: 15,907 |
| t13 | --base t12 + 692 ML 25 lines (armory, 2 min) | | **25** | 586 classes set; **25: 1,073**; ML batch 4op25-t13.* (4op-t13-ML.md) |
| t14 | --base t13, --band 2 (24-25) --rounds 1 (armory, 1.5 h) | | 25 | nothing lowered (t13 was already band-closed apart from the ML lines) |
| t15 | --base t13 + all 1,129 ML 25 lines (armory, 2 min) | | **25** | 237 classes set; **25: 738**; refreshed list 4op25-t15.* |
| t16 | --base t15 + arena batch 3 (1.15M 14s), --band 1 (25) --rounds 1 (armory, 6 min) | | 25 | nothing at 25; 141 classes to 14 (exact-depth lines) |
| t17 | --base t16, --band 3 (23-25) --rounds 1 (armory, 8.2 h) | | 25 | band lowered 26,861 classes (on t16: 23: 76,327 -> 52,507, 24: 15,654 -> 13,610); merged in t29 |
| t18 | --base t16 + all 1,228 ML 25 lines (armory, 4 min, beside t17) | | **25** | 98 classes set; **25: 542**; refreshed list 4op25-t18.*; merge with t17 when it lands |
| t19 | --base t18 + 112 new ML 25 lines (lines 1,229-1,340) (armory, 4 min) | | **25** | 111 classes set; **25: 340** |
| t20 | --base t19 + pergola's 399 optimal 4hb lines (12-15 moves) as actions and loaded lines, --band 1 (armory, 9 min) | | 25 | loaded lines set nothing (already at or below); as actions ~17 classes to 15; band at 25: 0.  ML list 4op25-t20.* (4op-t20-ML.md) |
| t21 | --base t20 + 196 new ML 25 lines (lines 1,341-1,536) (armory, 4 min) | | **25** | 189 classes set; **25: 118**; ML list 4op25-t21.* (4op-t21-ML.md) |
| t22 | --base t21 + pergola batch 1 (twsearch/7pair-4op-2.log, moves F R B L 2U2 3U2 2D2 3D2: all lines to 15, 1,005,204), as actions and loaded, --band 1 (armory, 6 min) | | 25 | loaded lines set nothing; as actions ~50 classes into 14-16; band at 25: 0.  Pergola's 16s and 17s come next |
| t23 | --base t22 + 71 new ML lines (1,537-1,607) + pergola batch 2 (1,216,063 16s) as actions and loaded, --band 1 (armory, 5 min) | | **25** | ML: 71 classes set; **25: 37**.  Pergola 16s: ~400 classes to 16, nothing at the top; band 0.  ML list 4op25-t23.* (4op-t23-ML.md) |
| t24 | --base t23 + pergola batch 3 (651,682 more 16s), --band 1 (armory, 5 min) | | 25 | ~45 classes to 16; nothing at the top (25: 37) |
| t25 | --base t24 + 23 new ML lines (1,608-1,630) (armory, 4 min) | | **25** | 22 classes set; 25: 13 |
| t26 | --base t25 + 149 24s that merge adjacent moves to 22-23 | | **25** | 149 set; **25: 11**, 24: 15,273.  ML lists 4op25-t26.* (11) and 4op24-t26.* (15,273) (4op-t26-ML.md) |
| t27 | --base t26 + 15 new ML lines (1,631-1,645) (armory, 4 min) | | **24** | 12 classes set; **no 25s: 4op <= 24**; 24: 15,256 |
| t28 | --base t27 + arena's first 72 reduced-move-set optima for 4op24-t26 (20: 1, 21: 51, 22: 19, 23: 1) (armory, 3 min) | | 24 | 72 classes set, and propagation lowered 1,087 more (12 to 20, 425 to 21, 650 to 22); **24: 15,256 -> 14,264** (992 off, ~14 per line) |
| t29 | --base t28 + the t17 table (armory, 3 min) | | 24 | 28,966 classes set: **24: 12,291**, 23: 53,883 (from 77,776) |
| t30 | --base t29, --band 2 (23-24) --rounds 1, every pool (laptop, started 17:25) | | running | |
| t31 | --base t29 + arena's 1,846 reduced-move-set optima (18: 7, 19: 43, 20: 203, 21: 1,143, 22: 434, 23: 16) + 1,331 ML 24 lines (armory, 3 min) | | 24 | arena set 1,770 classes, ML 784 more; propagation lowered 17,056 (3,329 to 20, 10,178 to 21, 3,549 to 22): **24: 12,291 -> 1,952**, 23: 48,507.  ML list 4op24-t31.* (4op-t31-ML.md); certificate updated (l18-l24), axcheck PASS |
| t32 | --base t31 + pergola batch 4 (the rest of its 16s, 2,110,304 lines; pergola has printed nothing since 14:57, searching 17) as actions and loaded, --band 1 (24) (armory, 17 min) | | 24 | loaded lines set nothing; ~600 classes to 16; band: 5 classes off 24 (**24: 1,947**) |
| t33 | --base t32 + arena (2,392 optima, 546 new) + ML (2,046 lines, 715 new) (armory, 3 min) | | 24 | arena set 422 classes, ML 521; propagation lowered 2,790 (440 to 20, 2,011 to 21, 339 to 22): **24: 946**, 23: 46,865.  ML list 4op24-t33.* (4op-t33-ML.md); certificate updated, axcheck PASS |
| t34 | --base t33 + 540 23-lines that merge adjacent moves (to 19-22) (armory, 3 min) | | 24 | 540 set; 24: 946 -> 768, 23: 45,302 |
| t35 | --base t34, **merge pass** (new, default: rebuild every class's line, cancel turns of one layer also across commuting same-axis turns, load the shorter ones, propagate, repeat) (armory, 5 min) | | 24 | passes: 10,369 / 300 / 48 / 2 / 0 classes (15,486 moves saved); **24: 768 -> 631, 23: 45,302 -> 36,011**, 22: 192,303 -> 179,358.  ML list 4op24-t35.* (4op-t35-ML.md); certificate updated, PASS |
| t36 | --base t35 + pergola batch 5 (3,075,813 17s) as actions and loaded, --band 1 (laptop, 55 min) | | 24 | loaded lines set nothing; ~2,000 classes to 17; band: 2 classes off 24 (631 -> 629) |
| t37 | --base t35 + arena's first 560 optima on 4op2324-t33 (20: 37, 21: 233, 22: 269, 23: 21) + ML (2,854 lines, 808 new), merge pass (armory, 3 min) | | 24 | arena set 537 classes, ML 187; propagation and 4 merge passes (190 / 22 / 23 / 1): **24: 631 -> 10**, 23: 33,333.  List 4op24-t37.* (4op-t37-ML.md, also on arena twsearch2/); certificate updated, PASS |
| t38 | --base t37 + the t36 table + pergola batch 6 (4,650,086 more 17s), --band 1 (armory, 3.5 min) | | 24 | band 0; merge 0; ~1,500 classes to 17; 24: 10 |
| t39 | --base t38 + arena (1,668 optima on 4op2324-t33, 1,108 new; 18: 4, 19: 40, 20: 319, 21: 719, 22: 525, 23: 61) + 60 new ML lines, merge pass (armory, 4 min) | | **23** | arena set 955 classes, ML 0; merge passes 1,405 / 451 / 54 / 1; **no 24s: 4op <= 23**; 23: 25,745.  Certificate updated (l24 removed), axcheck PASS at 23.  List 4op23-t39.* (4op-t39-ML.md) |

ml1 (op2's pools + the ML lines as a pool, on armory) was stopped as redundant once op3 finished.

**Lesson:** the band (every action straight into the table, from the top lengths) is the strong
step; propagation with cheap actions alone leaves a lot.  op2 (band 1) had 17,248 positions at 27;
op3 (band 2, + ML lines) 5,104 after the 28s were cleared.

**Structure of the 4op 24s (2026-10-29 15:30).**  Home count = pieces of an axis already on their own
face (F pieces on F, which equals B pieces on B; L pieces on L), per orbit and axis, 0-4 each, 0-16 in
all; a random 4op position averages 8.  The 15,256 24s average 5.8 (mostly 5-7), and 12,014 of them
(79%) have exactly one orbit-axis fully swapped (home 0: all four F pieces on B and all four B pieces
on F, or the same for L/R), 491 have two; random classes: 6% one, 0.2% two; the 23s: 37% one.  So
like the ring tails, the 4op tail is the derangements: a 3ax ending that leaves no orbit-axis fully
swapped avoids most of it, which a 3ax/4op boundary pass would use.  (feat4op.py in
/var/tmp/orbitsolve/op.)  Arena (user, 2026-10-29 15:00): twsearch optimal in the reduced move set
F R B L 2U2 3U2 2D2 3D2 on 4op24-t26.scr finds 22 for every class so far (twsearch2/4op24-t26-2.log).

**Current 4op table: t39** (diagonals free, merge pass; 2026-10-29 22:10, /var/tmp/orbitsolve/op/4op-t39.tab, armory ax3/), by length:

| length | classes | positions |
|---|---|---|
| 0 | 1 | 1 |
| 4 | 4 | 16 |
| 5 | 6 | 48 |
| 6 | 8 | 32 |
| 7 | 37 | 400 |
| 8 | 125 | 1,482 |
| 9 | 264 | 3,616 |
| 10 | 630 | 8,870 |
| 11 | 1,440 | 21,044 |
| 12 | 4,328 | 65,614 |
| 13 | 11,960 | 185,276 |
| 14 | 30,742 | 482,724 |
| 15 | 52,376 | 829,220 |
| 16 | 73,732 | 1,170,985 |
| 17 | 117,170 | 1,866,616 |
| 18 | 190,236 | 3,027,772 |
| 19 | 257,618 | 4,107,904 |
| 20 | 287,757 | 4,590,140 |
| 21 | 278,778 | 4,446,694 |
| 22 | 175,430 | 2,794,754 |
| 23 | 25,745 | 406,792 |

Earlier tables at the top: t1 27: 339, 26: 5,734; t2 26: 4,171; t3 26: 3,668; t4 26: 2,202; t5 26: 1,980 (25: 17,636); t6 26: 577 (25: 17,195); t9 26: 33 (25: 5,808); t10 26: 14 (25: 2,706); t11 25: 2,479; t12 25: 2,198; t13 25: 1,073; t15 25: 738; t18 25: 542; t20 25: 340; t21/t22 25: 118; t23/t24 25: 37; t26 25: 11; t27 24: 15,256 (no 25s); t28 24: 14,264; t29 24: 12,291; t31 24: 1,952; t32 24: 1,947; t33 24: 946; t34 24: 768; t35 24: 631; t37/t38 24: 10.

op1 (the first pilot) had 54 classes at 28, 1,536 at 27 and 10,226 at 26.

The peak is at 21, against the b ring's 24-25.

**ML agent.**  op1's 54 classes at 28 went to the ML agent
(devtools/tws/4op28.scr, .sol, 4op28-ML.md, obl8op.tws; validated with /var/tmp/orbitsolve/op/twsval.py).

ML results (2026-10-28 13:24, ~/mlcube/4op28_live.txt): 55 lines, all valid; all 54 classes solved,
best lengths 21: 10, 22: 12, 23: 7, 24: 9, 25: 10, 26: 6 classes (/var/tmp/orbitsolve/op/4op28-ml.tsv,
matched by op/mlmatch.py).  So no class at 28 is really 28.  The lines keep U, D and the axis split, so
they are 4op actions; op3 loads them.

Second batch: op3's 339 classes at 27 (devtools/tws/4op27.*).  ML results (~/mlcube/4op27_live.txt,
338 lines, all valid, /var/tmp/orbitsolve/op/4op27-ml.tsv): 338 of 339 solved, best lengths 21: 5,
22: 16, 23: 30, 24: 49, 25: 104, 26: 134 classes; the 339th was already 26 by merging.  So **4op <= 26**.
134 of 338 came down only to 26, so the 26s will be much harder.

Third batch (with the agent since ~18:40): t2's classes at 26, 4,140 after dropping 31 that shorten
by merging adjacent moves (devtools/tws/4op26.*, goal <= 25, output ~/mlcube/4op26_live.txt).
First results: 359 lines (19: 1, 20: 1, 21: 10, 22: 17, 23: 45, 24: 97, 25: 161 among the first 332),
folded in as t4 and t5.  Refreshed lists: devtools/tws/4op26-t3.* (3,668) and 4op26-t5.* (1,980).

**Band cost:** t2's band over the top three lengths (~83K classes, 202K actions, 16 images each)
took 82 min per round on armory; a full band over all 1.5M classes would be about a day.  Use
--rounds 1, a band only over the top lengths, and a higher --prop to lower the middle.

op2, actions by cost:

| cost | actions |
|---|---|
| 1 | 12 |
| 4 | 32 |
| 6 | 192 |
| 7 | 404 |
| 8 | 2,850 |
| 9 | 8,452 |
| 10 | 34,324 |
| 11 | 140,848 |
| 12 | 622,628 |
| 13 | 2,332,404 |

That is 3,142,146 in all (op3 the same).  The save/load round trip was tested on small pools: load
reproduces the table, every saved line replays, and loaded ML lines lower it further.

### 3ax

Tool: devtools/axbound (2026-10-28).
- **State:** 32 bits.  Per orbit there are 16 side slots (L F R B, 4 each; bit set = F/B piece), with
  8 set, so C(16,8)^2 = 165,636,900 positions.  SOLVED = 0xF0F0F0F0.
- **Storage:** a distance byte for every position (a class's images share it), back-pointers for
  the least image only, and a dirty byte per position so that propagation re-expands only what was
  lowered.  About 1 GB plus the actions.
- **Actions:** stored once per conjugacy class under the 16 stage symmetries, deduplicated by sorting.
  Applying every stored action to every position reaches all conjugates.  `--slices` adds 2U2, 3U2,
  2D2, 3D2 as cost-1 moves (alternative A).
- **Rebuild:** a line is rebuilt by trying the 16 symmetry images of the class's stored action, then
  replayed on the 48 pair facelets.
- Usage: `axbound [--prop C] [--band B] [--slices] [--verify N] [--worst FILE] [--from L] [--load FILE]... [--save FILE] [--diagok] [--threads T] POOL...`
- **Where it runs:** armory, ~/ax3 (axbound.cpp, certcube.h, pools/ with base + ud.cls; runax.sh TAG
  ARGS).  Armory is ~10x faster than the laptop while the ML agent runs (smoke test: 24 s against
  235 s).

| run | pools | worst | notes |
|---|---|---|---|
| smoke | pair3 + p8s10, --prop 6 --band 1 (1,464 actions) | 29 | 38 classes at 29; 1 GB; same result on both machines; replay 0 bad |
| B | base + 7pairud to 13, --prop 8 --band 1 (no slices; 7,987,363 actions, 2,490 of cost <= 8) | **26** | 2 classes at 26; 20 min, 2 GB; replay 1,002 lines, 0 bad |
| A | the same + --slices (2U2 3U2 2D2 3D2: 2 more cost-1 actions mod symmetry) | **25** | 346 classes at 25; 22 min; replay 1,346 lines, 0 bad |
| B3 | B with --band 3 (lengths 24-26) | **26** | band lowered 476 classes (24: 1,049, 25: 486), but not the 2 at 26; 38 min |
| A3 | A with --band 3 | stopped | after 2 h 46 min (band 3 over ~22K classes at 23-25 with 8M bit-loop actions); A is not the chosen route |
| ax-t1 | B3's settings + the ML line for one 26; --save (laptop, 29 min) | **26** | first saved 3ax table (10,366,284 classes, all replayed); 1 class at 26, 486 at 25 |
| ax-t2 | --base ax-t1 + both ML lines (56 s) | **25** | 487 classes at 25 (2 shorten to 24 by merging); 3ax25 batch for the ML agent |
| ax-t3 | --load ax-t2 + the 2 merged lines, --prop 9 --band 2 (laptop, 2.9 h) | **25** | 485 at 25; middle lowered (22: -8,625 classes); band found nothing |
| ax-t4 | --base ax-t3 + 8 ML lines at 24 (armory, 57 s) | **25** | 477 at 25; ML batch devtools/tws/3ax25-t4.* |
| op-b1 | --load 4op-t5, --prop 10 --band 4 (23-26) --rounds 1, no ML since t5 (laptop, 1.9 h) | 26 | 26: 1,980 -> 1,972; band lowered 22,308 classes (23: 146,449 -> 128,057); table 4op-b1.tab, to merge with --load |
| ax-t5 | --diagok, --load ax-t4, pools + arena batch 1 (35.0M actions), --prop 9 --band 1 (laptop, 5.4 h) | **25** | **25: 97** (from 477), 24: 1,072, 23: 16,126, 22: 67,621; band lowered 315; ML batch 3ax25-t5.* (3ax-t5-ML.md) |
| ax-t6 | --base ax-t5, --band 2 (24-25) --rounds 1 (laptop, 46 min) | 25 | band lowered 429: **24: 702** (from 1,072), 25: 97 |
| ax-t7 | --base ax-t6, --band 3 (23-25) --rounds 1 (laptop, 10.8 h) | 25 | band lowered 4,520: 23: 16,432 -> 12,076, 24: 702 -> 636; 25: 97 unchanged |
| ax-t8 | --base ax-t7 + 15 new ML 3ax 25 lines (laptop, 5 min) | **25** | 15 classes set; **25: 77**, 24: 646 |
| ax-t9 | --base ax-t8, **merge pass** (armory, 37 min) | **25** | passes: 69,736 / 2,344 / 14 / 0 classes (101,767 moves saved); **25: 77 -> 67, 24: 646 -> 525, 23: 12,082 -> 8,400**, 22: 71,847 -> 57,594, 21: 451,090 -> 376,252.  ML lists 3ax25-t9.*, 3ax24-t9.* (3ax-t9-ML.md); certificate updated, PASS |
| ax-t10 | --base ax-t9, --band 2 (24-25) --rounds 1, merge pass (armory, 18 min) | 25 | band: 5 classes (24: 525 -> 520); merge 0; 25: 67 |

3ax ML: the two B 26s (devtools/tws/3axb26.*, puzzle obl8ax.tws) came down to 24 and 25 (ML
lines in /var/tmp/orbitsolve/ax/3axb26-ml.tsv).  Next batch (ready): the 485 classes at 25
(devtools/tws/3ax25.*, goal <= 24, output ~/mlcube/3ax25_live.txt).

**Current 3ax table: ax-t9** (diagonals free, --prop 9, merge pass; 2026-10-29 19:40, /var/tmp/orbitsolve/ax/ax-t9.tab, armory ax3/), by length:

| length | classes | positions |
|---|---|---|
| 0 | 1 | 1 |
| 4 | 4 | 32 |
| 5 | 22 | 352 |
| 6 | 58 | 672 |
| 7 | 67 | 832 |
| 8 | 310 | 4,154 |
| 9 | 1,452 | 22,208 |
| 10 | 6,998 | 109,830 |
| 11 | 20,703 | 328,272 |
| 12 | 61,464 | 974,256 |
| 13 | 186,729 | 2,976,808 |
| 14 | 391,243 | 6,243,548 |
| 15 | 646,302 | 10,323,444 |
| 16 | 1,293,534 | 20,669,445 |
| 17 | 1,961,665 | 31,360,328 |
| 18 | 2,344,039 | 37,470,920 |
| 19 | 1,920,201 | 30,695,800 |
| 20 | 1,088,654 | 17,393,084 |
| 21 | 376,252 | 6,005,956 |
| 22 | 57,594 | 915,228 |
| 23 | 8,400 | 132,848 |
| 24 | 525 | 7,898 |
| 25 | 67 | 984 |

Change from ax-t2 (classes): 22: 141,165 -> 132,540; 23: 35,358 -> 34,719; 24: 1,050 -> 1,025; 25: 487 -> 485 (ax-t3); ax-t4 adds 8 ML lines at 24: 25: 477.  ax-t5 (diagonals free, arena batch 1, 35M actions): 22: 67,621, 23: 16,126, 24: 1,072, 25: 97.

Earlier: run B had 26: 2, 25: 512 (both 26s now solved by ML lines).

A against B (classes): 20: 752,178 / 1,246,936; 21: 517,951 / 751,315; 22: 68,633 / 141,129;
23: 20,908 / 34,970; 24: 973 / 1,447; 25: 346 / 512; 26: 0 / 2.  The slices are worth about 1 at the
top (as estimated: 1-2), short of the boundary's 5 on the rings.

B's two classes at 26 resist the band (no single action plus the table shortens them); they and
the 486 at 25 are the candidates for the ML agent or deeper pools.  The 2 26s are packaged for the ML agent:
devtools/tws/3axb26.scr, .sol, 3axb26-ML.md, puzzle obl8ax.tws (pair solved to the 3ax goal: U, D,
F=B, L=R); goal <= 25; output ~/mlcube/3axb26_live.txt.  axbound now has --save/--load (one
replayed line per class, 10,366,284 classes; save ~1 min) and --from L (--worst writes every class
of length >= L); round trip tested.  The next 3ax run should --save, and write --from 25.

Propagation took almost all of the 20 min (lengths 16-20 dominate); the band (every one of the 8.0M
actions on the worst classes) took seconds, so a wider band is cheaper than a higher --prop.

3ax plays the role of the a ring (21) and 3fr (19), so ~20 is a reasonable expectation.

**Relaxed 3ax (curiosity only, not useful for the bound: user, 2026-10-28; devtools/axbfs, exact BFS):** in the group <F, R, B, L, 2U, 3U, 2D, 3D>, preserving
nothing but every orbit's U and D faces: **12** (25 s on armory, 168 MB).  Classes: 0-6: 5,194;
7: 421,753; 8: 2,434,240; 9: 5,538,996; 10: 1,900,079; 11: 17,523; 12: 28.  These moves break other
orbits' axis sort (quarter slices) and the mids, so they cannot seed the strict 3ax; they are exact
only for a variant where nothing else needs to survive phase 3.  Table: armory /tmp/axbfs.dist.

### Plan

Done: 4op 28s and 27s by ML (4op <= 26); 3ax 26s by ML (3ax <= 25); persistent tables for both
stages; conjugacy-class opbound; --base fold-ins.

1. 4op: fold the ML 26 lines in as they come (--base the latest table, ~20 s on armory); point the
   agent at the current list when its old one is mostly cleared.  A band down to 23 (~4 h) is the
   next middle-lowering step; the full band (~1 day) is too long.
2. 3ax: t3 (--prop 9, laptop) -> refreshed 25s if any change.
3. ML: 3ax25 (485 classes, goal <= 24) and 4op26 (goal <= 25); fold results in with --base.
4. Arena 4op log: when the 13s finish, pull (<= 3 GB), check every line up to 13 is already a 7pairud
   action, time the 14s; feed the 14s into 4op.
5. A 3ax/4op boundary pass, like the rings 45 check (boundcheck on armory).

## Runs after the clean-up (2026-10-29 22:10 on)

| run | what | result |
|---|---|---|
| ax-t10 | --base ax-t9, --band 2 (24-25), merge (armory, 18 min) | 24: 525 -> 520; 25: 67 |
| ax-t11 | --base ax-t10 + 30 new ML 3ax lines (3ax25_live 24-53; 25 at 24, 5 at 23), merge (laptop, 30 min) | 30 classes set: **25: 67 -> 35**, 24: 547; certificate updated, PASS |
| t40 | --base t39 + pergola batch 7 (2,229,567 17s; all 9.95M 17s as actions), --band 1 (23), merge (armory) | 5.1 h; 0 set by the load; band: 309 lowered, **23: 25,745 -> 25,436**; certificate updated, PASS |
| t41 | --base t40 + pergola batch 8 (5,103,743 17s), --band 1 (23), merge (armory) | stopped at ~95% of the band (5.2 h): superseded by t43 |
| t42 | --base t41 + pergola batch 9 | cancelled (folded into t45) |
| t43 | --base t40 + arena optimal solves arena2324-c (56,758 lines, 4op2324-t33 log to 43,435) + laptop t30 table, merge (laptop, 24 min) | arena set 35,980 classes, t30 none; merge 5,630; **23: 25,436 -> 448**, 22: 122,279 |
| ax-t11b | --base ax-t11 + 37 new ML 3ax lines (3ax25_live 54-82, 3ax24_live 1-8), merge (laptop, 33 min) | 35 classes set: **3ax 25: 35 -> 8**, 24: 545; certificate updated, PASS |
| t44 | --base t43 + pergola p8, pg9 as loaded lines (laptop) | stopped; done on armory as t45 |
| t45 | --base t43 + last arena solves (5,728 new lines) + pergola p8, pg9 loaded and as actions, --band 1 (23), merge (armory, 9 min) | arena 1,875 classes set, pergola 0, band 0; merge 103; **23: 448 -> 273**, 22: 119,836; certificate updated, PASS |
| t47 | --base t46 + pergola 3axs log as pool (s3ax1, 257,525 lines), --band 1 (23) (armory, 4 min) | band 0 lowered; only a few classes at 18-21 changed |
| ax-t14 | --base ax-t11b + pergola 3axs pool (s3ax1), --band 2 (24-25) (armory, 17 min) | 0 lowered; 25: 8, 24: 545 |
| t46 | --base t45 + arena 22s batch 1 (5,471 lines) (armory, 6 min) | 3,800 set, merge 3,748; **22: 119,836 -> 91,592** |
| t48 | --base t47 + arena 22s batch 2 (10,935 new lines) (armory, 4 min) | 6,316 set, merge 2,225; **22: 91,592 -> 71,142** |
| ax-t15 | --base ax-t14 + 1 ML line (3ax25_live 83: a 25 at 23) (armory, 4 min) | **25: 8 -> 7**; no neighbour lowered; certificate PASS |
| (analysis) | 3ax classes with one orbit fully axis-swapped: 1,703, all at 20-25 but one; devtools/tws/3axswap.scr/.sol for ML training |
| ax-t16 | --base ax-t15 + 25 ML lines (3ax25_live 84-91, 3axswap_live 1-17; net retrained on 3axswap) (armory, 4 min) | 17 classes set; **3ax 25: 7 -> 0, 3ax <= 24**, 24: 512; certificate PASS; plain sum 47 |
| ax-t12 | --base ax-t11, --band 3 (23-25), merge (laptop, 7.0 h) | 0 lowered: bands over the 3ax top are exhausted |
| a1, a2 | arenaloop: t48 + arena 22s solves (26,398 and 24,772 new lines), hourly on armory | **4op 22: 71,142 -> 49,643 -> 39,257; 23: 273 -> 271**; certificate PASS each |
| (analysis) | 4op classes with an orbit-axis fully swapped: 85,388, including all 271 23s and 25,178 of 39,257 22s; devtools/tws/4opswap.scr/.sol for ML training |
| a3 | arenaloop (25,856 new lines) | 4op 22: 39,257 -> 29,921; 23: 271 -> 269 |
| ax-t17 | --base ax-t16 + 5 ML lines (3axswap_live 18-22) (armory, 4 min) | 3ax 24: 512 -> 503; certificate PASS |
| t49, ax-t18 | pergola 3axs batches 1+2 (s3ax1, s3ax2: to 15) as pools: 4op band over 23 on a3; 3ax band over 24 on ax-t17 (armory, 4 + 16 min) | 0 lowered in both |
| (note) | opbound/axbound print the "classes by length" summary before the final merge passes; the saved table (and certificate) can differ slightly at mid lengths. md tables now from the next run's load line. |
| a4-a6 | 2026-10-30 14:21-16:33 | arena loop folds of the rest of 4op2223-t45n (all 111,890 solved) | 4op 22s 29,921 -> 24,598 -> 19,168 -> 16,723; 23s 269 |
| t50 / ax-t19 | 2026-10-30 16:45 armory | final folds after arena: 4op a6 + ML 4op23/4opswap (89 lines) + pergola pg10 (491,616 17s, end of 7pair-4op-2.log); 3ax ax-t17 + ML 3ax24/3axswap (68 lines) + pergola 3axs s3 (560,476 15s); --rounds 0 | 4op 23s 269 -> 218 (22s 16,760); 3ax 24s 503 -> 420 (23s 8,550); pergola set nothing at the top |
| t51 / ax-t20 | 2026-10-30 21:00 armory | ML lines folded again (4op 109 lines on t50, 3ax 101 lines on ax-t19), --rounds 0 | 4op 23s 218 -> 156 (22s 16,800); 3ax 24s 420 -> 365 (23s 8,601) |
| b22 -> t52 | 2026-10-30 13:18-21:12 laptop, merge 21:25 armory | 4op band over 22-23 on a2 (base pools + arena b1all/b2/b3 + 4hb optima), 7.9 h; table loaded into t51 | band: 4,714 lowered on a2; merged: 1,229 classes set, 4op 22s 16,800 -> 16,188, 23s 156 |
| ax-t21 | 2026-10-30 21:40 laptop | 3ax band over 24 on ax-t20, base pools + 7pairud + arena b1all + pergola 3axs s1-s3, 25 min | 0 lowered (3ax 24s stay 365) |
| t54 | 2026-10-30 22:00 laptop | 4op band over 23 on t52, base pools + 7pairud + arena b1all + every pergola 17s batch (p1-p8, pg9, pg10), 10 min | 0 lowered (23s stay 156) |
| 4op1921-t52 | 2026-10-30 23:05 | arena list: t52 4op 19-23 less the 161,465 classes arena already solved optimally (reduced set) | 855,212 classes (21: 217,171, 20: 350,811, 19: 287,230); every 22 and 23 already tried |
| t53 | 2026-10-30 21:35 armory | 4op band over 22-23 on t52 (base pools + arena b1-b3 + 4hb optima + pergola pg10), 1.7 h | 0 lowered |
| b1 (4op1921) | 2026-10-30 23:25 armory | first fold of arena's 4op1921-t52 run (5,186 solved, 8,224 lines) into t52, --rounds 0 | 4,173 set, propagation thousands at 16-19; 21s 280,422 -> 260,448; 22s 16,188 -> 16,186; 23s 156 unchanged |
| b2 (4op1921) | 2026-10-31 armory | second fold of arena's 4op1921-t52 run (15,643 solved, 15,520 new lines) into b1 | 7,743 set; 21s 260,448 -> 235,667; 22s 16,186, 23s 156 unchanged |
| foldloop c1 | 2026-10-31 00:09 armory | first pass of op/foldloop.sh: arena 4op1921 (17,812 new lines with ML) and ML 3ax (23 lines) | 4op 23s 156 -> 131, 21s 218,514; 3ax 24s 365 -> 323 |
| foldloop c2-c8 | 2026-10-31 01:19-07:37 armory | hourly folds (c8 run at once on request): arena 4op1921-t52 (158,502 solved), arena 12-move 23s (4op23-b2r: 38 solved, 24 at 21, 14 at 22), ML | 4op 23s 131 -> 65, 22s 16,239, 21s 136,691; 3ax 24s 323 -> 254 |
| c9 + ML lists | 2026-10-31 07:50-08:05 | 7,845 4op + 1,147 3ax ML results found only in ~/mlcube/runs/*/*.json folded (foldloop3 extra files); new ML lists axis-c9-ML.md; loop now foldloop4.sh | 4op 23s 65 -> 60; 3ax 24s 254 |
| ax-cj | 2026-10-31 08:50 laptop | 3ax band over 24 on ax-c9 with a conjugate pool S A S (S side inner-slice half turn, A pergola 3axs line; 2,931,878 pass pairlogcheck; ~876K new action classes at 15-17) | 0 lowered |
| foldloop5 | 2026-10-31 08:40 | fold loop now reads every ~/mlcube/4op*_live / 3ax*_live, and after each pass postfold.sh writes new ML lists of the open tops (devtools/tws/axis-current-ML.md) and mdupdate.py rewrites upperbound5.md's status, hourly-fold block and tables | c11: 4op 23s 56 -> 45 |
| c12 / m1 | 2026-10-31 09:37-10:30 | fold loop c12 with the suffix lines (4op: 23s 45 -> 29), then laptop fold m1 on 4op-c12 with 32 newer arena/ML lines | 4op 23s 22; list 4op23-m1 (22) for arena's restart |
| ax-qc | 2026-10-31 10:00 laptop | 3ax band over 24 on ax-sx with conjugates Q A Q^-1 (Q one or two U/D slice quarter turns; 2,143,210 pass pairlogcheck) | 0 lowered |
| cmerge / 4op-cm | 2026-10-31 10:45 laptop | commutation merge (outer faces commute across axes, same-depth inner slices commute across axes, on the centres): cmerge.py over the certificate (4op 1,623 shorter, 3ax 19,640); axbound.cpp merge pass extended; 4op-cm = m1 + those | 4op 22s 15,924 -> 15,793, 23s 22; 3ax top unchanged |
| shortenseqs part 0 | 2026-10-31 10:30-10:55 laptop | twsearch --shortenseqs --distinguishall --maxdepth 8 -M 20000 -t 16 on the first 2,558 4op 22/23 lines | 45 shortened (1 to 19, 26 to 20, 18 to 21) |
| 2026-10-31 13:45 | shortenseqs run2 (md 8, 12x1 thread): 4op 138/13,308 shortened, 3ax 103/8,318 (9 24s); run3 md 10 tops / md 9 next started |
| 2026-10-31 14:05 | correction: 3ax shortenseqs md 8 = 94 shortened, no 24s (parser misalignment); 4op 23s at md 10: 0 of 12 |
| 2026-10-31 14:15 | run3 stopped (md 8 is the cap); run4 md 8 on 3ax 22s then 4op 21s |
| 2026-10-31 14:16 | fold c16 (ML 4op23 + arena + shortenseqs): **4op <= 22** (15,204 at 22); 3ax 24: 246, 23: 6,659; plain 46 |
| 2026-10-31 14:30 | arena: full 4op1921-m1 list at maxdepth 20 (user); fold loop now foldloop7 (reads every 4op1921-*.log), from c16 |
| 2026-10-31 15:45 | fold c18 (ML 3ax24 lines): 3ax 24s 246 -> 118 (23s 6,771); 4op 20s 412,312 (arena L21 + others) |
| 2026-10-31 20:20 | folds c19-c21: 3ax 24s 118 -> 6; 4op 21s 116,604, 20s 327,742 (arena L21 done, L20 running); run4 shortenseqs: 3ax 22s 599, 4op 21s 1,042 shortened; run5 started |
| 2026-10-31 20:52 | full P81 certificate (1ud 17 + 2ud 18 + 3ax 24 + 4op 22) built after c22, all four stages PASS, copied to armory ~/certsafe, checksums match; c22: 3ax 22s 45,557, 23s 5,959 (shortenseqs run4) |
| 2026-10-31 21:20 | fold c23: 3ax 24s 6 -> 1 (holdout 3ax24-c20-003: orbit a fully swapped, orbit b 4 off-axis pieces, one per side face); 4op 20s 314,204 |
| 2026-10-31 22:46 | final arena/pergola pull (22:11), fold c24: 4op 22s 15,057, 21s 113,674, 20s 313,001; 3ax 24: 1; P81 certificate rebuilt from c24, all PASS, armory copy checksums match |
| 2026-11-01 03:25 | **clean plain P80**: last 3ax 24 fell (ML, fold c25 23:35); certificate obl23pair-axis-P80-2026-11-01 from c29 (3ax 23s 3,116), all four stages PASS, on armory, checksums match |
| 2026-11-01 03:40 | upperbound3.md: P 80 record now the plain axis-split certificate obl23pair-axis-P80-2026-11-01 (user) |
| 2026-11-01 03:50 | boundary outlook redone from the plain 80 (ax-c29, 4op-c24): 44 ~0.5 h, 43 ~2-7 h, 42 ~7-40 h, 41 ~30-170 h (laptop) |
| 2026-11-01 07:30 | run6 shortenseqs on ML 3ax23 misses: 17/1,091 shortened, 5 to <=22; 3ax 23s: one orbit 14-16 of 16 off axis in nearly all |
| 2026-11-01 07:40 | ML agent: own shortener for x W x2 -> x' W* (exact permutation check); 27 of 4op22-c24 lines shortened (14 to 20, 13 to 21), 5 of its 3ax23 lines; reruns on 3ax23_live every 20 min; new 3ax23-c29 ML run 07:32 (12 of first 20 to 22) |
| 2026-11-01 08:00 | ruleshort.py (half-turn rewrite + merges): 84/87 of shortenseqs' 3ax-23 hits; whole certificate run started (11.9M lines, 14 procs, ~5 h) |
| 2026-11-01 08:55 | travel: laptop only; fold loop moved local (foldloop9, from 4op-c24 / ax-c29, c30); stuck armory c30 killed |
| 2026-11-01 09:00 | axbx/axbcheck (new 3ax/4op boundary tools): 44 all 3,116 classes covered, axbcheck PASS (1.48e9 positions, 7 min); negative tests: L0-only blocks FAIL, dropped block FAIL (16 states without a block); 43 sample (1/20) all covered; full 43 started |
| 2026-11-01 09:50 | 43 dry run: one action 30 classes uncovered (140,233 positions); on c30 snapshot with --deep 2: 22 uncovered (3,117 positions), all 3ax 22s; deeper retry (--prop 10 --slack 4 --deep 4) running; ML list 3ax22-b43 (22) |
| 2026-11-01 10:05 | 43 dry run: the last 22 covered with --prop 10 --slack 4 --deep 4 (83 s); merged blocks to axbcheck |
| 2026-11-01 09:50 | user: all CPU work paused with kill -STOP (pids in /var/tmp/orbitsolve/stopped-pids.txt): axbcheck x43m, fold c31 (axbound, 3ax half), ruleshort run (11 procs + runrs.sh) |
| 2026-11-01 19:05 EST | 43 axbcheck PASS; fold c31: 3ax 23s 2,143; ruleshort whole-certificate pass done (3ax 650, 4op 7 shortened); 42 dry run started (run42.sh); pypy for Python scripts from now on |
2026-11-01 19:29: boundary dry run 42 (c32 snapshot): broad pass leaves 81 of 350,629 classes; --prop 10 --slack 4 --deep 4 covers all 81.
2026-11-01 20:40: axbcheck started on the 42 blocks (bnd/x42m.txt = x42a minus its 81 left + x42b); est. 15-20 h.
2026-11-02 ~02:00: 42 axbcheck PASS on bnd/x42m.txt (c32 snapshot): 3.83e11 positions, 0 not solved, 19,046 s.  P 77 = 1ud 17 + 2ud 18 + 3ax/4op 42 (dry run; certificate when home).
2026-11-02 07:06: 41 boundary dry run started (bnd/run41.sh, c41 snapshot; 3ax 23s 765).
2026-11-02: P77 evidence copied to ~/orbitsolve-certsafe/obl23pair-axis-P77-evidence-2026-11-02 (snap-42 stages, x42m blocks + check, axbx/axbcheck sources, SHA256SUMS).
2026-11-02: move-set census of the 3ax/4op lines (devtools/msclass; tables in upperbound5.md).  4op: 48% H only; 3ax: every line needs Q, 75% HQ23; quarter turns of vertical slices in 11% (4op) and 19% (3ax) of lines.
2026-11-02: devtools/tws/movesets.txt: --moves strings for H, H2, H3, H23, HQ, HQ2, HQ3, HQ23, HQ23V (checked on obl8ax/obl8op: rotation group 8 each, canonical move states 9/49/49/105).
2026-11-02 09:25: axbx --status S (default 600 s): progress line on stderr (classes done, covered, not, positions left, ETA).  bnd/watch41.sh adds 10-minute status lines to run41.log for the running 41 (broad pass at 09:19: 1,087,323 of 1,326,357 classes).
2026-11-02 12:11: boundary dry run 41 (c41 snapshot): broad pass 18,231 s leaves 1,563 of 1,326,357 classes; --prop 10 --slack 4 --deep 4 covers all (128 s).  Not yet axbchecked.  (watch41.sh logged no status lines; future runs: axbx --status into the run log.)
2026-11-02 12:46: 3ax optimality pass started: all moves, -si, -M 40000 (41.9 GB), -t 16, table saved to /var/tmp/orbitsolve/opt3ax/cache, 10,366,283 lines in length order (opt3ax/snap = stages/3ax at c41+; fold loop paused).  Uses a patched twsearch2 copy (/var/tmp/orbitsolve/twsi): ~/twsearch2's processlines never sets globalinputmovecount, so -si there searches nothing ('No solution found in 0').
2026-11-02 12:49: 3ax -si restarted on INVERTED lines (first run fed solving lines as scrambles: false improvements; log kept as opt3ax/3ax-all-uninverted-bad.log).  First real improvement: line 174, 8 -> 7 (3D 2F 2B' U2 2F' 2B 3D', all-moves only).  Found that 47,800 3ax lines end in outer face turns, which keep the 3ax goal colouring: trimmed copies (ax/trim-outer.txt; 179 22s -> 21, 504 21s -> 20, ...) appended to fold-extra-ax.txt for the next fold.
2026-11-02 13:30: cheap scans (scan/cheapscan.py: merge + drop final no-op moves; 3ax/4op outer turns, 1ud/2ud also U/D-axis slices): 4op 0, 3ax 47,800, 2ud 45,345 (9,194 of the 18s; certcheck PASS on the trimmed stage, no colour side effects on 6-10), 1ud 1,444,340 (58,817 of the 17s; certcheck running).  axbound merge patch narrowed to dropping final outer turns: a general loop cut is unsafe (the cut stretch can move other centres; axcheck checks them).
2026-11-02 13:45: 1ud trimmed stage certcheck PASS (248 s).  Test fold (patched axbound) on c45: merge pass 47,800 + 159 + 2 classes; 23: 696->682, 22: 46,069->43,649, 21: 302,815->298,324.  Patched axbound/opbound installed in devtools.
2026-11-02 16:38: 3ax -si pass moved to shared memory: owner (twsi2 --shmhold obl8ax, depth-8 table) + 16 workers (-t 1 --prefixslack 0) over 1,000-line chunks from line 58,900 (opt3ax/shm/run.sh).  Tuning on 6,400 length-12 lines: default -t 16 687 lines/min, -t 16 --prefixslack 0 1,993, shm 16 x -t 1 --prefixslack 0 2,051; identical 639 improvements.  Main run stopped at line 52,499; 52,500-58,899 from the test (3ax-all-52500-58899.log).
2026-11-02 19:30: merge m1 (3ax): the twsearch all-moves -si improvements so far (7,133 lines, lengths up to 13) plus trim-outer and the new extra lines, folded into ax-c45. 23s 696 -> 564, 22s 46,069 -> 41,597, 21s 302,815 -> 251,267, 20s 976,777 -> 868,631 (opt3ax/merge/ax-m1.tab; certificate not yet updated). The shm workers were stopped and the table freed.
2026-11-02 20:44: --distinguishall all-moves 3ax table built to depth 8 (40 GB; shorten/cache). shortenseqs on 50 3ax 23s: maxdepth 10 and 11 shortened none. All 564 23s now run at increasing maxdepth (shorten/top23.sh).
2026-11-03 07:05: merge m2 (3ax): hq23 --newcanon 3 -si results (13s, 14s, part of the 15s; 32,960 lines) plus the shortenseqs 23 -> 22, folded into m1: 23s 564 -> 552, 22s 41,597 -> 35,717, 21s 251,267 -> 208,902, 20s 868,631 -> 828,636 (opt3ax/merge/ax-m2.tab; certificate not yet updated).
2026-11-03 10:40: 4op all-moves -si quick pass (up to 12 complete; 13+ too slow) folded into c32 as 4op-m1: 22s 14,975 -> 14,965, 21s 113,737 -> 113,539 (opt4op/merge/4op-m1.tab; certificate not yet updated).

# Part 4. upperbound3.md as of 2026-11-03

# Upper bounds for solving the centres (current best results)

This file holds only the current best results and how they are certified; it replaces
upperbound.md and upperbound2.md (kept for history: the approaches tried and dropped, and the run
logs behind every number here).

Metric: single-layer turn metric (every turn of one layer, quarter or half, counts 1).  Centres
are solved first, one orbit (or chiral pair of oblique orbits) at a time, then corners (and midges
on odd n), then wings.  An algorithm for one orbit must keep the other centre orbits as the order
below requires; edges and corners may be scrambled, since they come later.  Same-coloured centre
pieces are interchangeable.

## Whole cube

| constant | value | basis | status |
|---|---|---|---|
| P, oblique chiral pair | **80** | UD half 1ud + 2ud = 17 + 18 = 35 (plain sum, relaxed stages, pair by pair); axis split 3ax + 4op = 23 + 22 = 45 (plain sum, no boundary pass, since 2026-11-01 03:25; the rings' 45 needed a boundary certificate) | certified: ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01, copy on armory ~/certsafe (1ud, 2ud certcheck --keep ud PASS; 3ax, 4op axcheck PASS) |
| D, diagonal | 44 | D 10, U 16, LFRB 18 | certified (cert/diag/centres-first) |
| M, mid (odd n) | 39 | UD 18, LFRB 21 | certified (cert/mid/centres-first) |
| W, wing orbit keeping centres | 162 | 32 + 32 + 40 + 58 | certified (cert/wing/centres-kept) |
| corners (even n) | 11 | exact (full BFS under face turns) | proved |
| corners + midges (odd n) | 20 | God's number of the 3x3x3 | proved |

With k = (n-2)/2 (even) or (n-3)/2 (odd) there are k(k-1)/2 oblique pairs, k diagonal orbits,
k mid orbits (odd n) and k wing orbits.

| n | bound with P = 80 |
|---|---|
| even | **(80 n^2 + 344 n - 920) / 8** = 80 (n-2)(n-4)/8 + (n-2)/2 (D + W) + 11 |
| odd | **(80 n^2 + 340 n - 1580) / 8** = 80 (n-3)(n-5)/8 + (n-3)/2 (D + M + W) + 20 |

Checks: n = 4: 217; 5: 265; 6: 503; 7: 590; 8: 869; 9: 995.  (With P = 81: 6: 504, 7: 591.)

**Centres only** (edges and corners ignored):

| n | bound |
|---|---|
| even | **(n-2)(80 n - 144) / 8** = 80 (n-2)(n-4)/8 + (n-2)/2 D |
| odd | **(n-3)(80 n - 68) / 8** = 80 (n-3)(n-5)/8 + (n-3)/2 (D + M) |

Checks: n = 4: 44; 5: 83; 6: 168; 7: 246; 8: 372; 9: 489.

**Wings by 3-cycles (W = 149, + 4 once; sketch, not yet certified).**  Fix the parity of every
odd wing orbit at once with `S D2 S D2 S D2 S D2 S` (S = quarter turns of those orbits' slices:
4 + 5x moves for x orbits; keeps centres, flips exactly those parities), then at most 12 wing
3-cycles per orbit, each within 12 moves keeping centres (twsearch-optimal for all 49 classes:
certruns/wing-centres-kept/pools/wing3opt_cp.cls).  So 5 + 144 = 149 per orbit, + 4 once:

| n | bound with P = 80, W = 149 + 4 |
|---|---|
| even | (80 n^2 + 292 n - 784) / 8 |
| odd | (80 n^2 + 288 n - 1392) / 8 |

To certify: the 3-cycle list, a replay checker on 4..12, the fixer checked for every subset of
orbits, and the counting argument (an even permutation of 24 wings needs at most 12 3-cycles).

**Parity and the last stage.**  Wing parity is absorbed in W (the parity algorithm is in the
pool).  Even n: no OLL/PLL parity; corners are any face-turn position, within 11.  Odd n: the centre
and wing stages turn only faces and non-middle slices apart from the mid algorithms, which restore
the middle centre, so corners and midges are a legal 3x3x3 position, within 20.

## The oblique chiral pair: P = 80 = 35 + 45

A chiral pair is two oblique orbits a = (p, q) and b = (q, p).  Four stages: 1 aUbU (none ..
aUbU), 2 aDbD (aUbU .. aDbD), 3 the a ring (aUaDbUbD .. aLaFaRaB), 4 the b ring (.. bLbFbRbB).
The same sequences work for every pair (p, q) (devtools/pairdepth, all 110 pairs on 25x25x25).

### Order and side conditions

Stage names: the digit is the stage; the letters say what the other oblique orbits must keep.

| name | stage | other oblique orbits keep |
|---|---|---|
| 1u | aUbU | U face |
| 1ud | aUbU | U and D faces |
| 2ud | aDbD | U and D faces |
| 3f | a ring | everything |
| 4f | b ring | everything |

- Stage 1 for **every** pair first, then stage 2 for every pair: stage 1 need only keep the other
  orbits' U faces (1u) and stage 2 their U and D faces (2ud).  No boundary between them: P's UD
  half is the plain sum 1u + 2ud (or 1ud + 2ud: both 17 + 18).
- Alternatively 1ud then 2ud pair by pair, which allows a UD boundary pass (1ud + 2ud <= X).
- The rings can't relax: an a-chirality orbit (2,s) and a b-chirality orbit (s,2) both move
  exactly as MID2 (layer s never turns), so keeping U, D and the a orbits forces MID2 and MID3
  colour-solved and so keeps everything (3uda = 3f).  So the rings run pair by pair (3f then 4f)
  after every pair's UD half, and keep their boundary.
- So P = (1u + 2ud, or the 1ud + 2ud boundary) + (rings boundary) = 35 + 45 = **80**.

### Stages 1 and 2 (relaxed): 1u 17, 1ud 17, 2ud 18

Live stage copies in /var/tmp/orbitsolve/rNAME/stages, each certcheck --keep u|ud PASS after
every feed.  Table: classes (symmetry classes; positions are about 8 times as many) by the length
of their certificate line.

<!-- rtable start -->
Updated 2026-10-27 09:39 (certruns/relaxed/rtable.py).

| length | 1u | 1ud | 2ud |
|---:|---:|---:|---:|
| 0 | 1 | 1 | 1 |
| 3 | 4 | 2 |  |
| 4 | 29 | 26 |  |
| 5 | 187 | 136 |  |
| 6 | 850 | 620 | 28 |
| 7 | 3,849 | 2,580 | 106 |
| 8 | 15,270 | 12,814 | 330 |
| 9 | 48,997 | 38,714 | 422 |
| 10 | 167,529 | 122,474 | 2,346 |
| 11 | 497,891 | 380,568 | 14,772 |
| 12 | 1,231,851 | 993,315 | 26,672 |
| 13 | 2,346,068 | 2,028,815 | 56,558 |
| 14 | 3,405,406 | 3,169,592 | 154,887 |
| 15 | 3,487,606 | 3,776,410 | 368,713 |
| 16 | 2,492,805 | 2,905,185 | 546,711 |
| 17 | 421,508 | 688,599 | 948,909 |
| 18 |  |  | 816,480 |
| **classes** | 14,119,851 | 14,119,851 | 2,936,935 |
| **worst** | 17 | 17 | 18 |
<!-- rtable end -->

Lower bound: 1u needs at least 15 (twsearch, optimal, on one of the 1f 19s:
`2B2 U' 3L 3R 2D R' 2U 3R' D 2L2 2R' 3D2 3U' 3L' 2R`), and 1ud likewise at least 15.

### Stages 3 and 4 (rings, unchanged since 2026-10-22): a ring 21, b ring 29, boundary 45

Certificate: ~/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22 (also cert/obl23pair/cf-xfree).

| length | a ring | b ring |
|---:|---:|---:|
| 0 | 1 | 1 |
| 4 | 12 |  |
| 5 | 56 |  |
| 6 | 58 |  |
| 7 | 475 | 46 |
| 8 | 1,802 | 58 |
| 9 | 5,804 | 107 |
| 10 | 12,637 | 823 |
| 11 | 49,832 | 2,783 |
| 12 | 138,609 | 4,878 |
| 13 | 312,485 | 9,174 |
| 14 | 785,106 | 38,339 |
| 15 | 1,242,654 | 96,950 |
| 16 | 1,747,860 | 155,208 |
| 17 | 1,744,313 | 205,846 |
| 18 | 1,233,235 | 368,805 |
| 19 | 535,277 | 670,649 |
| 20 | 79,458 | 906,934 |
| 21 | 1,665 | 1,055,239 |
| 22 |  | 1,298,556 |
| 23 |  | 1,260,688 |
| 24 |  | 911,726 |
| 25 |  | 563,216 |
| 26 |  | 243,901 |
| 27 |  | 79,473 |
| 28 |  | 17,812 |
| 29 |  | 127 |
| **classes** | 7,891,339 | 7,891,339 |
| **worst** | 21 | 29 |

**Rings 45 (certified 2026-10-27 23:10).**  devtools/boundx covered all 3,593,948 a-ring classes
longer than 16, in 20 parts (00-11 here, 12-19 on armory.local; each boundx 45 --deep 1 --slack 5
--only; the 1-2 leftover classes in parts 03, 04, 05, 07, 08, 09, 12, 18 by --deep 3 --slack 7,
spliced with certruns/boundary/bsplice.py).  boundcheck on armory: PASS (1.82e12 colourings, 0 not
solved; 48,178 s, 1.6 GB; certruns/boundary/check45.log).  Certificate:
boundaries/aUaDbUbD..aLaFaRaB+aUaLaFaRaBaDbUbD..bLbFbRbB/x45.txt (3,593,948 blocks, 19,057,562 lines).

**Structure of the deep ring classes (2026-10-28).**  "Home" = ring pieces of the stage's orbit
already on their own face (the segment number wNN is 16 - home).  Every b-ring 29 (127 classes) and
1,653 of the 1,665 a-ring 21s have no piece home (the other 12: one home); among all classes only
1.2% have none home (94,494, segment w16).  Of the 690 classes with none home and none on the opposite
face (every piece one face off), 583 are b-ring 28s or 29s (74 of the 127 29s).  Worst by home
count: a ring 0: 21, 1: 21, 2: 20; b ring 0: 29, 1: 28, 2: 28.  So the ring tails are the
derangements: an earlier stage that left a piece or two of each ring home would cut them.  Tools:
devtools/stagedump (colourings of a stage's classes, local analysis), /var/tmp/orbitsolve/ringdeep.

### Boundary passes (method)

devtools/boundx loads two consecutive stages as dense tables.  For every A class whose line L0 is
longer than X - worst(B), it lists the colourings of the rest that L0 carries onto B states beyond
X - |L0|, and tries alternative solving sequences of the A state on them (an action of at most 8
moves then the certificate line of where it leads; --deep: two actions; --slack: sequences up to
that many moves longer than L0), cheapest first.  A + B <= X holds when none is left.  --out writes
the certificate: per A class a block of L0 and the alternatives used, blocks separated by an empty
line.  devtools/boundcheck STAGEDIR_A STAGEDIR_B FILE X [--keep u|ud] checks it independently
(own ranking and tables; every line legal and solving its A state; every A state beyond
X - worst(B) has a block; every colouring carried onto a B state within X).  A boundary certificate
holds only for the stage files it was made with: after any merge into either stage, regenerate it.

**UD 34 (1ud + 2ud, next).**  UD 35 is now the plain sum (2ud 18, 2026-10-27), so the boundary
target is 34: only 1ud 17 x 2ud 18 fails, and the bad colourings per A class equal the 2ud
positions at 18 (about 6.8M, so about 190 + 800 CPU-h by the model below unless the pattern idea
in "Next: characterize the 2ud 18s" cuts it).  The model, from the UD 35 sample (bad = 2ud 19s):  Cost: boundx about 4e-11 x nA x nB CPU-hours, boundcheck about
1.7e-10 x nA x nB (nA = 1ud classes at 17, 688,599; nB = 2ud positions at 19).  So each 2ud 19
removed cuts the time in proportion; per class a 2ud 19 is worth about 3.8 1ud 17s.  A 1/500
sample (bcert/ud35) covered all 1,378 classes; boundx needs no --keep (its alternatives keep U and
D), boundcheck takes --keep ud.  Needs frozen copies of both stages.  With rings 45, UD 34 would
give P = 34 + 45 = 79.

### The hard 2ud classes: structure (2026-10-26)

devtools/posstat --lines STAGEDIR [LO HI [MAXLEN]] tallies every class of the 2ud stage by
{lo, hi} = the number of the eight target pieces already on D in each orbit (in this stage the D
pieces of one orbit are interchangeable, so on D means home):

- **{0, 1}** (one orbit with nothing home, the other with exactly one: 7 of 8 pieces off D) is
  the hard pattern: 1,019,200 of the 2,936,936 classes, but 79% of the 18s and 98% of the 19s, and
  only 871 classes of at most 15 moves.  The cheapest {0, 1} classes take 14 (9 classes).
- {0, 2} is much easier: of the 39,642 original 19s with that pattern only 76 lasted (0.2%), while
  7.7% of the {0, 1} 19s did.  {0, 0} (nothing home) is easier than {0, 1}: 101 at 19, 98,601 at 18
  of 407K.  No class with a piece home in both orbits ever reached 19 (one original exception).
- The a/b asymmetry in posstat's per-orbit counts on .scr files is the class normalization (a
  U/D-preserving reflection swaps a and b; the representative puts the emptier orbit first), so
  use the unordered pair.
- **The short {0, 1} lines are setups on a few shared cores**: of the 23,896 {0, 1} lines of
  14-16 moves, the 14s are 2-move setups and the 16s 4-move setups in front of a shared 12-move
  ending (the most common, `D' 3R 2B' 3L 2D' F2 3R 2B 3L U 3F2 3B2`, ends 304 of them; the
  endings mostly finish with a paired slice half turn such as 3F2 3B2).  That is the footprint of
  certextend's propagation (cheap actions in front of short lines), so templating these cores
  with setups adds nothing new; arena's 14s land the same way, which is why they now rarely reach
  a 19.  The ML agent's solutions of the 19s do not share endings (18,703 distinct 10-move endings
  in 20,303 lines; 401 end in one of the shared 12-move endings): it finds new routes.

**Neighbour hubs (2026-10-26).**  devtools/nbrcount STAGEDIR L [--slices] counts, for each class at
L, its one-move neighbours (outer face turns, and 2U 3U 2D 3D with --slices: the legal one-move
actions of 2ud) that are also at L.  Solving a hub to L-2 brings every such neighbour to L-1.  At
5,961 2ud 19s: 2,379 isolated, 3,524 neighbour pairs, the largest hub 12.
devtools/tws/adbd19ud-hubs.scr is the remaining list sorted by neighbour count (a snapshot).

### Next: characterize the 2ud 18s by a simple pattern (plan, 2026-10-26)

With the 19s cleared (2026-10-27), push the structure further: show that **every 2ud 18 has one or two
simple characteristics**, such as {0, 1} (exactly one target piece home, in one orbit).  Then the
UD 34 boundary (1ud 17 + 2ud 18) need not enumerate the 2ud 18 positions for each of the 688,599
1ud 17 classes.  A pattern like {0, 1} depends only on which D-face positions each line L0 carries
onto D, so the bad colourings of an A class are a count from L0's permutation, and an alternative
line only has to move those colourings out of the pattern.  That would make the check close to
linear in nA rather than nA x nB (about 190 + 800 CPU-h by the general model).

2ud 18s at 19s = 5,149 (posstat --lines, 2026-10-26 ~15:00): 855,467 in all, {0, 1} 674,425 (79%);
the rest is 181,042: {0, 0} 98,487, {0, 2} 57,303, {1, 1} 25,246, {0, 3} 6.  The ML agent would then
work on the 18s outside the chosen pattern(s), smallest groups first ({0, 3}, then {1, 1}, then
{0, 2}), in the hope of eliminating them; {0, 0} might be kept as a second pattern if it resists.
Simplest candidate (user): **{0, x}**, i.e. some orbit has none of its target pieces home.  That
already covers all 18s but {1, 1} (25,246), so the ML agent would only need the {1, 1} 18s, and the
test per colouring is still one count per orbit.

### Front + horseshoe (3fr/4h): see upperbound4.md

## How the UD stages are being lowered

- **Arena** (10.0.0.155, ssh -i ~/.ssh/id_gizmo): twsearch generators 7pair-2ud and 7pair-1ud
  (pair effects only, other orbits labelled by face set) write growing logs; the user runs them.
  certruns/relaxed/rloop4.sh pulls each log's new tail (offsets in processed.tsv) and feeds it
  through rfeed.sh every round (2ud then 1ud; 1u's log stopped 2026-10-24).  The 1ud generator was
  killed (memory pressure on arena) by 2026-10-26 11:48; only 2ud runs now.  rtail.sh pulls at most 3 GB
  per call (MAXBYTES) so the arena feed never holds the 2ud lock for hours ahead of the ML feed.
- **rfeed.sh NAME KEEP STAGE LOG [PAIRFLAG]**: under the stage lock (/var/tmp/orbitsolve/rNAME/lock),
  pairlogcheck --keep (legality), certextend (CEARGS; --slices adds 2U/3U/2D/3D as one-move actions,
  --prop 8 propagates shortened classes through face, slice and pool actions), certmerge,
  certcheck --keep.  The stage is restored on any failure.
- **ML agent** (~/mlcube, its own processes): works from devtools/tws/adbd19ud-remaining.scr
  (ScrambleAlg scrambles of the 2ud classes still at 19), appends solving lines to
  ~/mlcube/adbd19ud_live.txt; certruns/relaxed/rmlfeed2ud.sh feeds new lines every 10 minutes and
  rewrites the remaining list.  Each ML line shortens hundreds of classes through propagation.
- certruns/relaxed/rtable.py rebuilds the table above after each feed.

## Certificates and checkers

Every certificate directory holds only `MANIFEST` and `stages/` (plus `boundaries/` where
approved): plain text, `stages/<solved>..<target>/wNN.txt`, NN = target pieces off their home
face, one solving sequence per line, one line per symmetry class, no positions (a line's position
is the sequence run backwards from solved).  Checkers re-derive everything from the lines:

- devtools/certcheck STAGEDIR [--keep u|ud]: coverage, one line per class, stated length, side
  effects on 6x6x6..10x10x10 (with --keep: the other orbits keep their U, or U and D, faces).
- devtools/boundcheck: boundary certificates (above).
- devtools/diagcheck, midcheck, wingcheck: the D, M and W certificates (side effects on every
  size 4..12 or odd 5..11 and every depth).
- devtools/pairdepth: the pair sequences work for every pair (p, q).

| result | where |
|---|---|
| **P 80**: 1ud 17 + 2ud 18 + 3ax 23 + 4op 22, plain sum | ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01 (stages/none..aUbU is 1ud, stages/aUbU..aDbD is 2ud: certcheck --keep ud, byte-identical to the 2026-10-27 record; stages/3ax, stages/4op: devtools/axcheck 3ax|4op; all four PASS 2026-11-01 03:25; copy on armory ~/certsafe/obl23pair-axis-P80-2026-11-01, SHA-256 match; working notes upperbound5.md) |
| P 80 with the rings (superseded, kept): 1ud 17 + 2ud 18 + rings 3f, 4f with boundary 45 | ~/orbitsolve-certsafe/obl23pair-relaxed-P80-2026-10-27 (boundaries/.../x45.txt boundcheck PASS 2026-10-27 23:10) |
| rings 3f, 4f (source) | ~/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22 |
| 1u, 1ud, 2ud (live, still being lowered) | /var/tmp/orbitsolve/r1u, r1ud, r2ud |
| D 44 | cert/diag/centres-first |
| M 39 | cert/mid/centres-first |
| W 162 | cert/wing/centres-kept |

## Open

- 2ud 19s: done 2026-10-27 04:35 (ML agent; arena and propagation).
- Next: reduce the 2ud 18s to one or two simple patterns (see "Next: characterize the
  2ud 18s"), e.g. {0, x} with the ML agent on the 25,246 {1, 1} 18s; then a pattern-based UD 34.
- 1ud 17s: the arena 1ud generator was killed (memory pressure); restart is the user's call.  Fewer
  17s also cut the UD 34 cost.

# Part 5. upperbound4.md as of 2026-11-03

# Front + horseshoe: stages 3fr and 4h (exploring, 2026-10-27)

Only the 3fr/4h numbers (3ax/4op: upperbound5.md).  The certified results (P 80) and everything else are in upperbound3.md.

**Stopping point (2026-10-28):** 3fr/4h is parked at **3fr 19 (certified) + 4h <= 40** (composed certificate;
<= 38 with 4hb optimal), plain sum 59 against the rings' 50.  Not pursued further; attention moves to
3ax/4op (upperbound5.md).  The last 4h certextend feed (4h-12 pool) was stopped while still reading pools;
the 4h certificate is unchanged.  Arena's 7pair-4h generation finishes its current length, then
switches to 4op.

## Stages

After U and D are solved for both orbits of the chiral pair (stages 1ud, 2ud), in place of the
a ring (3f) and the b ring (4f):

| name | stage | solves | positions | stage symmetries | classes |
|---|---|---|---|---|---|
| **3fr** ("front") | aUaDbUbD..aFbF | the 8 front pieces of both orbits | C(16,4)^2 = 3,312,400 | 4 | 829,206 |
| **4h** ("horseshoe") | aUaFaDbUbFbD..aLaRaBbLbRbB | the remaining L, B, R pieces (they wrap round the front like a horseshoe) | (12!/4!^3)^2 = 34,650^2 = 1,200,622,500 | 4 | ~300M |

Both keep every other oblique orbit fully (like 3f/4f: ring stages can't be relaxed).  For
comparison each ring is 16!/(4!)^4 = 63,063,000 positions.

**Target:** the plain sum.  The rings are 21 + 29 = 50 as a plain sum (46 certified with their
boundary, 45 being checked), so **3fr + 4h <= 49 as a plain sum is already worthwhile** (user,
2026-10-27); a 3fr/4h boundary pass comes after that and may take it further.

**Why 4h might win (user's intuition, 2026-10-27).**  The b ring is long because its algorithms
must restore all of orbit a: the a ring and the b ring have the same 63M positions, yet the a ring
is 21 and the b ring 29, and the ML agent could not shorten any of the b ring's 127 classes at 29.
4h moves both orbits' L, R, B pieces freely (only U, D, F are kept), so there is much more freedom
and conjugates can fix things up.  Early signs: 4h has 4-move algorithms (e.g. `2F2 3D2 2F2 3D2`;
the b-ring generator's shortest are 6).

## 3fr

| date | pools | worst | distances (classes... positions) |
|---|---|---|---|
| 2026-10-27 | base (pair3, p8s10, p9, eg8, c12x) + obl11p.all@aUaDbUbD; --merge --refine | **21** | 16: 618,174, 17: 207,024, 18: 28,776, 19: 1,744, 20: 36, 21: 2 (positions) |

| 2026-10-27 | + 7pairud to 11 moves; CERT | **20, certified** (certcheck PASS; ring34/cert3) | 17: 197,094, 18: 25,056, 19: 1,520, 20: 2 |
| 2026-10-27 | + 7pairud 12s and 13s through certextend (39,676 classes shortened) | **19, certified** | 17: 186,472, 18: 21,268, 19: 1,090 (positions); 273 classes at 19 |

Current certificate (ring34/cert3, certcheck PASS 2026-10-28), classes by line length:

| length | 0 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| classes | 1 | 20 | 68 | 132 | 454 | 1,246 | 5,604 | 13,063 | 32,487 | 76,895 | 136,605 | 171,605 | 190,456 | 148,238 | 46,719 | 5,340 | 273 |

Expected exact (optimal) through 13: a 3fr solution keeps the pair's U and D sets and MID2, MID3,
so it is one of the 7pairud.log sequences, which are complete to 13 and all went through the
certificate (ud11 in the build, the 12s and 13s by certextend).  From 14 on, lengths are upper
bounds only.  The 273 classes at 19: devtools/tws/3fr19.scr (ML: 3fr19-ML.md; twsearch: 7pair-3fr.tws).

First run: orbitbound55 --faceturns --merge --refine --refinework 2e11 --diagok --budget 1e13 --planstart
aUaDbUbD --plan aFbF (34,978,454 algorithms -> 1,226,368 actions of cost <= 9; 66 min).
Log /var/tmp/orbitsolve/ring34/s3.log.  The a ring with similar pools was 22.

Next: arena's 7pairud.log (complete to length 13) is exactly 3fr's pool (same kept faces as the
a-ring generator): build the certificate (CERT=DIR), feed 7pairud.log through certextend, then the
ML agent on the worst classes.

## 4h

Pilot 1 (2026-10-27, 29 min, 6.6 GB): **57**, meaningless as a bound: the base pools and obl11p hold
only 195,258 algorithms that keep U, D and F of both orbits (637 actions after reduction).  4h
needs its own pools.  Pilot 2 (running, ring34/run4b.sh, s4b.log): the same plus 7pair-4h.log so
far (1,163,076 lines to 12, pairlogcheck --ud clean) and 7pairud lines to 12, both @aUaFaDbUbFbD.

Pilot 2 (53 min, 8.8 GB): **56**, still not a measure of 4h: 12,795,766 usable algorithms, but the
edge budget (2e12 / 1.2e9 states) kept only the cheapest cost classes, 1,214 actions.  The tail is
blocky (46: 179M, 49: 112M, 50: 7.1M, 53: 43.0M, 56: 24,010,000 = 70^4 positions): states that the
cheap actions only reach by long chains.  Pilot 3 (running, run4c.sh, s4c.log): budget 6e12 (about
5,000 actions; search about 3.5 h).

Pilot 1 command: orbitbound55 --faceturns --diagok --budget 2e12 --maxstates 2e9
--planstart aUaFaDbUbFbD --plan aLaRaBbLbRbB, base pools + obl11p.all@aUaFaDbUbFbD (28,057,678
algorithms), no --merge/--refine (per-state merge sets at 1.2e9 states cost tens of GB); the edge
budget keeps only the cheaper actions, so its worst is an overestimate.  Gives timing and a first
number.  Log /var/tmp/orbitsolve/ring34/s4.log (script ring34/run4.sh).

Pool generator (arena, started by the user 2026-10-27): devtools/tws/7pair-4h.tws (pair labelled
U 1, F 2, D 3, L R B 4; MID2, MID3 colour-solved; rotation z2 only, group size 2),
7pair-4h-uniq.tws, go4h.sh -> twsearch2/7pair-4h.log.

## 4h split: 4ha + 4hb (2026-10-27)

4ha = aB bB keeping U, D, F (495^2 = 245,025 positions, 61,560 classes); 4hb = aL aR bL bR keeping
U, D, F, B (70^2 = 4,900, 400 classes, 16 symmetries).  Pools: base + obl11p + 7pair-4h to 12
(complete) + 7pairud to 12, --merge --refine (ring34/run4ab.sh, s4ab.log; 21 min, 9.6 GB):

| stage | actions | worst | tail |
|---|---|---|---|
| 4ha | 7,202,243 | **23** | 21: 20,784, 22: 1,672, 23: 48 |
| 4hb | 2,667,782 | **17** | 15: 292, 16: 54, 17: 16 |
| 4hb optimal (twsearch, 2026-10-27) | | **15** | 13: 116, 14: 18, 15: 12 (classes) |

So **4h <= 40** (plain sum of the split); with 4hb optimal, **4h <= 38**.  The single-stage 4h pilots (57, 56, 56) were limited by
the edge budget (whole cost classes: only the cheapest ~1,214 actions fit at 1.2e9 states), not
real 4h numbers.  Next: twsearch optimal on the 4hb tail; a 4ha/4hb boundary pass; real 4h by
certextend starting from 4ha+4hb sequences.

4hb optimal (2026-10-27): the user ran twsearch -M 56000 on all 399 classes on pergola (4hb.log;
longest 1,874 s): worst **15**.  Merged into ring34/cert4hb (46 lines shorter, the rest equal), certcheck
PASS at 15 (the 17 version in /var/tmp/orbitsolve/backup/cert4hb-17).  The lines (ring34/4hb-opt.lines)
keep U, D, F, so they also go into the next 4h feed.

4hb certificate (ring34/cert4hb, certcheck PASS at 17, now 15) and the optimal-solve files for the user:
devtools/tws/7pair-4hb.tws (all 24 rotations; pair faces labelled U 1, F 2, D 3, B 4, L 5, R 6),
4hb.scr (the 399 non-trivial classes, inverses of the certificate lines, shortest first), 4hb.sol.

### Single-stage 4h certificate (2026-10-27)

devtools/stagecompose A B OUT composes two certified stages into one: for every class of
S..(TA+TB), the A line of its A state (a symmetry image), then the B line where that leads,
joined and simplified, each replayed before writing.  4h from 4ha + 4hb (cert4ab): 300,187,350
classes (= orbitbound's canonical count), 0 failed, 2 min, 28 GB of text
(ring34/cert4h/stages/aUaFaDbUbFbD..aLaRaBbLbRbB).  Lengths: peak 29-30; 36: 1,677,665, 37: 410,168,
38: 69,431, 39: 5,462, 40: 128.  certcheck can't hold 300M lines (vectors per line, ~135 GB;
killed): a streaming certcheck (dense bitmap) is needed before 4h can be certified.  Improving it
with certextend (ring34/feed4h.sh).  The 4ha/4hb boundary passes were dropped (user): they would
tie 4h to the split and get in the way of 3fr/4h boundary passes.

## 3ax + 4op

Moved to upperbound5.md (working notes for the axis split).

## Open

- 4h pilot result and timing; then decide on a full 4h table (memory: dist 1.2 GB; certificate
  ~300M lines, ~15-20 GB of text).
- 3fr certificate with 7pairud.log.
- Feed 7pair-4h.log into 4h as it grows.

# Part 6. upperbound5-notes.md as of 2026-11-03

# 3ax/4op working details (move sets, twsearch optimality-pass planning)

Details behind upperbound5.md (which keeps the summary and the current tables).

## Move sets used (2026-11-02, current stages, devtools/msclass)

Each line's smallest move set among: H = F,B,L,R (all powers) + 2U2,3U2,2D2,3D2 (16); +Q = 2U,3U,2D,3D quarter turns (8); +2 = 2F2,2B2,2L2,2R2 (4); +3 = 3F2,3B2,3L2,3R2 (4).  +V = also uses quarter turns of the vertical slices (2F, 3F', ...); +UD = also uses U or D.  These are the moves each line uses, not the smallest set that could solve the state at that length.

4op:

| len | H | H2 | H3 | H23 | HQ | HQ2 | HQ3 | HQ23 | +V | +UD | total |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 1 |  |  |  |  |  |  |  |  |  | 1 |
| 4 |  | 2 | 2 |  |  |  |  |  |  |  | 4 |
| 5 |  | 2 | 4 |  |  |  |  |  |  |  | 6 |
| 6 |  | 4 | 4 |  |  |  |  |  |  |  | 8 |
| 7 |  | 10 | 10 | 1 |  | 6 | 6 |  | 4 |  | 37 |
| 8 | 2 | 18 | 24 | 11 |  | 28 | 29 |  | 13 |  | 125 |
| 9 |  | 27 | 28 | 43 |  | 63 | 61 |  | 41 | 1 | 264 |
| 10 |  | 53 | 61 | 78 | 4 | 158 | 156 | 1 | 119 |  | 630 |
| 11 |  | 139 | 156 | 472 | 8 | 184 | 197 | 145 | 135 | 4 | 1440 |
| 12 |  | 377 | 452 | 937 |  | 589 | 626 | 826 | 493 | 28 | 4328 |
| 13 | 21 | 587 | 674 | 3388 | 9 | 1365 | 1492 | 2760 | 1547 | 138 | 11981 |
| 14 | 304 | 1458 | 1594 | 9146 |  | 2162 | 2461 | 9855 | 3778 | 272 | 31030 |
| 15 | 2202 | 1621 | 1526 | 11633 |  | 2738 | 2938 | 23981 | 7744 | 136 | 54519 |
| 16 | 12920 | 582 | 647 | 9296 |  | 3174 | 3440 | 42074 | 13325 | 26 | 85484 |
| 17 | 54360 | 424 | 441 | 12645 |  | 4628 | 4853 | 59759 | 27086 | 201 | 164397 |
| 18 | 132394 | 1624 | 1593 | 28434 |  | 3344 | 3442 | 92467 | 41623 | 149 | 305070 |
| 19 | 235816 | 2430 | 2227 | 25202 |  | 2498 | 2433 | 98597 | 38025 | 104 | 407332 |
| 20 | 206271 | 1235 | 1177 | 15078 |  | 771 | 807 | 62807 | 24799 | 74 | 313019 |
| 21 | 75473 | 512 | 241 | 5716 |  | 130 | 122 | 24498 | 7042 | 3 | 113737 |
| 22 | 9460 | 83 | 62 | 842 |  |  |  | 4003 | 517 | 8 | 14975 |
| all | 729224 | 11188 | 10923 | 122922 | 21 | 21838 | 23063 | 421773 | 166291 | 1144 | 1508387 |

3ax:

| len | H | H2 | H3 | H23 | HQ | HQ2 | HQ3 | HQ23 | +V | +UD | total |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 1 |  |  |  |  |  |  |  |  |  | 1 |
| 4 |  |  |  |  |  | 2 | 2 |  |  |  | 4 |
| 5 |  |  |  |  |  | 10 | 10 |  | 2 |  | 22 |
| 6 |  |  |  |  |  | 39 | 17 |  | 2 |  | 58 |
| 7 |  |  |  |  | 5 | 37 | 31 |  | 12 |  | 85 |
| 8 |  |  |  |  |  | 188 | 134 | 24 | 91 |  | 437 |
| 9 |  |  |  |  | 18 | 446 | 423 | 448 | 281 |  | 1616 |
| 10 |  |  |  |  |  | 1935 | 1791 | 2594 | 609 | 8 | 6937 |
| 11 |  |  |  |  | 42 | 5565 | 4993 | 9271 | 1556 | 43 | 21470 |
| 12 |  |  |  |  | 238 | 12350 | 11458 | 35800 | 6828 | 163 | 66837 |
| 13 |  |  |  |  | 132 | 28081 | 26224 | 119233 | 28421 | 232 | 202323 |
| 14 |  |  |  |  | 38 | 40043 | 37796 | 268930 | 61491 | 191 | 408489 |
| 15 |  |  |  |  | 11 | 51359 | 46697 | 474175 | 157983 | 71 | 730296 |
| 16 |  |  |  |  |  | 50985 | 45802 | 962389 | 324494 | 208 | 1383878 |
| 17 |  |  |  |  | 3 | 48634 | 44500 | 1565156 | 388354 | 188 | 2046835 |
| 18 |  |  |  |  | 25 | 35050 | 33103 | 1793058 | 488260 | 271 | 2349767 |
| 19 |  |  |  |  |  | 16374 | 15173 | 1471951 | 317306 | 68 | 1820872 |
| 20 |  |  |  |  |  | 4584 | 4312 | 805551 | 162315 | 15 | 976777 |
| 21 |  |  |  |  |  | 681 | 535 | 240797 | 60794 |  | 302807 |
| 22 |  |  |  |  |  | 217 | 267 | 36837 | 8695 |  | 46016 |
| 23 |  |  |  |  |  | 6 | 10 | 635 | 105 | 1 | 757 |
| all | 1 | 0 | 0 | 0 | 512 | 296586 | 273278 | 7786849 | 2007599 | 1459 | 10366284 |

## Optimality pass (plan, 2026-11-02; starts after 41 and when the GPU is free)

- One obl8ax.tws / obl8op.tws for every set, restricted with --moves (devtools/tws/movesets.txt); no per-set .tws files, so -si
  parses every certificate line.  No rotations in --moves (user): twsearch keeps every rotation the set preserves (8 for
  each named set; ALL: 24 on 4op, 8 on 3ax).  ~/twsearch2/build/bin/twsearch, --nowrite.
- First ALL (absolute): -si on the stage lists, shortest lengths first; a length with no improvement is proven optimal.
  When it slows, add --maxdepth to sweep the rest.  Then reduced sets (H, H23, HQ23, ...) to reach deeper; there a miss
  only means "no shorter line in that set".
- Snapshot the stage lists first (the hourly folds rewrite them).
- **Bug in ~/twsearch2:** -s mode (processlines) never sets globalinputmovecount, so -si stops at depth 0 and every line
  says "No solution found in 0".  Fixed in a copy, /var/tmp/orbitsolve/twsi (counts the line's moves); ~/twsearch2 untouched.
- **Run 1 (2026-11-02 12:46): 3ax, all moves, -si, -M 40000, -t 16**, table saved (opt3ax/cache), all 10,366,283 lines in length
  order (opt3ax/in.txt from opt3ax/snap).  Log /var/tmp/orbitsolve/opt3ax/3ax-all.log: one "Solving" per line; an improvement is
  the indented line before "Found"; "No solution found in N" = optimal.
  Inputs kept read-only (user: keep them to understand the improvements): opt3ax/snap (stages/3ax at the start), in.txt
  (certificate lines, solving, length order), in-inv.txt (twsearch -i of in.txt: generating, the actual -si input; line n
  matches the n-th "Solving"), inputs.sha256, and 3ax-all-uninverted-bad.log (the first, wrong run on uninverted lines).
- Branching factor (twsearch -C30, asymptotic; /var/tmp/orbitsolve/br/brbench.log; setup time, peak memory):
  plain canonical vs --newcanon N.  1 and 2 are worse than plain on every set; 3 is the knee.  Gain at depth 10 = plain / newcanon
  cumulative canonical count at 10.  Fill depth at -M 40000 (1.6e11 2-bit entries): twsearch fills 0-2, then depth k+1 while
  T_k^2/T_{k-1} <= entries (T = distinct entries so far); modelled as T = -C cumulative / rotations (8; ALL 24), which
  overcounts (duplicates), so real fills can go a level deeper.  Fill depth is the same for plain and nc3-5 on every set
  (nc1-2 lose a level on h2, h3, h23, hq23, hq23v).

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


## Later: clean the whole certificate (user, 2026-11-02)

- 1ud and 2ud too: an all-moves -si pass like 3ax's (unlikely to change the 35, but the certificate should not carry
  easily shortened lines).  Certificate lines are solving sequences: invert before -si.  Check which solving .tws
  matches each stage's goal first.
- Then shortenseqs over the whole certificate (all four stages): -t 1, -M 1000, many processes, --maxdepth at most 8.

## twsearch -si throughput (2026-11-02 13:53; run 1 kept running, user: think more before changing it)

- Rate at lengths 11-12: 334 lines/min, ~108% CPU of 16 threads.  sample profile: per line and depth, solve() makes
  work chunks, spawns and joins threads; with d - initd <= 1 there is one chunk (one thread), above it the chunks
  are milliseconds of work, and the time goes to chunk setup (slowmodm2 in innersetup ~25%) and the work lock (~25%).
  No threshold helps: one short search has too little work for 16 threads.
- Options: (a) parallel over lines: a batch mode, N threads each running a plain single-threaded IDA* per line
  (read-only table; fill to depth 8 first with --startprunedepth); ~100 lines, must match twsearch's lookup and
  canonical-state semantics.  (b) No code: split the input over several processes with smaller tables (-M 5000 still
  fills depth 7: predicted 5.1e9 <= 2e10), e.g. 8 x -M 5000 -t 2 --nowrite for lengths <= 13-14, and the single 40 GB
  process (depth 8 later) for 15+, where its threads have work.

## twsearch tuning and shared memory (2026-11-02 evening)

- Cause of the slow -si on short lines: sharding to a 3-move prefix (~36k chunks) for searches only ~3 levels past the
  table (makeworkchunks: target 40 x threads x microthreads, loop while prefix + 3 < depth).  Fix in the experiment
  build (/var/tmp/orbitsolve/twsi2): --prefixslack K caps the prefix at depth - (table depth) - K - 1 (table depth =
  baseval + 1), automatic from the search depth and the table; K = 0 best.  Also --chunkfactor, --shardmargin,
  --smallchunks (no better), and a TIMING line after the table load (temporary).
- On 6,400 length-12 lines (depth-8 table): default -t 16 687 lines/min; -t 16 --prefixslack 0 1,993; shared memory,
  16 clients x -t 1 --prefixslack 0, 2,051.  Same 639 improved lines in all three.
- Shared memory (twsi2): --shmhold NAME (owner: creates /NAME exclusively, loads or fills the table in place, sets the
  magic last, prints a ready line, holds until SIGINT/SIGTERM/SIGHUP, which remove it), --shmattach NAME (read-only
  client, refuses a table without the magic, never extends or writes), --shmremove NAME (after SIGKILL).  macOS
  accepts the 41.9 GB object; load from the saved file 12 s.  Each new client faults in its page tables (~2.6M
  soft faults touching the whole table), so short client runs pay a start-up cost.
- The run: opt3ax/shm (run.sh, worker.sh): owner + 16 workers over 1,000-line chunks from line 58,900; out/cNNNNN.log
  per chunk, in order by chunk number; touch STOP to stop after current chunks; kill -INT the owner (owner.pid).

### Shared-memory run status (2026-11-02 18:45)

- Owner /obl8ax plus 16 workers since 16:38. 38 chunks done (38,000 lines) and about 6,650 more in flight: 44,656 lines, 2,728 improved.
- Length 12 (38,567 lines from line 58,900) is done: 7.5 min per 16-chunk round, about 2,100 lines/min. 2,465 of 38,000 improved (6.5%).
- Length 13 has run since about 17:02 at about 55 lines/min. Each line takes about 17 s at depth 12 (31M lookups, 1.85M lookups/s per worker).
- Projection: the 202,323 length-13 lines take about 61 h. That breaks the 16 h rule, so a --maxdepth is needed from length 13 on.
- Memory: 63 GB used, 4.7 GB of 5 GB swap used. No other heavy processes.
- 18:55: STOP set; the workers stop after their current chunks (c00038-c00053, about 3 h). When they have stopped, opt3ax/merge/merge.sh 1
  folds every -si improvement so far plus the new extra/ML lines (including trim-outer) into ax-c45.tab, giving merge/ax-m1.tab.
  The certificate and the fold loop are untouched. The owner still holds /obl8ax.

### Merge m1 (2026-11-02 19:04-19:30)

- Workers killed at 19:00 (STOP set; partial chunks c00039-c00053 are in out/*.tmp; their unfinished rows need a redo). The shm owner was torn down at user request and is to be reloaded from cache/tws9-obl8ax-39G.dat.
- opt3ax/merge/merge.sh 1 folded into ax-c45.tab: 7,133 -si improvements, the new extra/ML lines and trim-outer (54,963 new; 70,638 with suffixes; linecheck 0 problems). Output merge/ax-m1.tab. Certificate not updated.

| table | 19 | 20 | 21 | 22 | 23 |
|---|---|---|---|---|---|
| c45 | 1,820,872 | 976,777 | 302,815 | 46,069 | 696 |
| loopt (patched engine and trim only) | 1,812,185 | 966,382 | 298,324 | 43,649 | 682 |
| **m1** | 1,670,771 | 868,631 | 251,267 | 41,597 | **564** |

### shortenseqs (--distinguishall), 2026-11-02 evening

- --distinguishall is a different puzzle (about 10^142 states, 24 rotations), so it needs its own table: the obl8ax all-moves cache does not apply.
- An 8 GB table fills to depth 6. On 20 top lines (5 each of 20-23, from the c45 stage), all moves, -t 16:
  - maxdepth 9: 118 s, 0 improved;
  - maxdepth 10: 19 lines in about 230 s, 0 improved (stopped).
- 19:40: one m1 23 (`2F2 2L2 3D' ... 2B' 3R`) at maxdepth 11 with -M 40000 -t 16 --cachedir shorten/cache. This builds and writes the 40 GB distinguishall table. Log: /var/tmp/orbitsolve/shorten/top1-d11.log.
- Next after this: an hq23 -si pass on the 3ax 13s only. Lengths 12 and below are done for all moves.
- 19:38: the maxdepth-11 run finished in 157 s with no improvement. It wrote the depth-7 table (cache/tws9-obl8ax-39G-owhqsevykfta.dat, 5.0 GB compressed). Window searches at depth 11 took about 2 s each (80M probes).
- 19:45: the same line restarted with no maxdepth (user), to run until the table is full. Log: shorten/top1-full.log.
- 20:35: shorten/bench.sh is queued. When top1-full logs "written in" (the depth-8 table), it stops that run, then times --shortenseqs on b23.txt (50 m1 23s) at maxdepth 10, 11, ... It stops raising maxdepth after the first run over 30 min. Results go to shorten/bench.log.
- Bench on 50 m1 23s with the depth-8 table (24.1 GB file, filled 20:44 in 59 min, 7.9e10 at depth 8):
  - maxdepth 10: 123 s, 0 shortened;
  - maxdepth 11: 322 s (about 6 s/line), 0 shortened;
  - stopped during maxdepth 12.
- 20:55: shorten/top23.sh runs all 564 m1 23s at maxdepth 10, 11, 12, ... Each depth's output is the next depth's input. Log: shorten/top23.log; STOP23 stops it after the current depth.
- 21:00: shared-memory shortenseqs: the owner holds /obl8axd (shorten/shm, owner.pid); 16 workers run -t 1 --prefixslack 0, one chunk per worker per depth (4-line chunks spent 75% sys re-attaching: about 2.6M soft faults per attach).
  - maxdepth 10: **38 s** for all 564 23s (the -t 16 single-process estimate was about 20 min). **1 23 -> 22**: an 11-move window was replaced by an equivalent 10-move one.
  - maxdepth 11: 572 s, nothing new.
  - maxdepth 12 started 21:11 (shm23c.sh, log shm23.log). Each depth starts with about 25 s of sys while the workers map the table.
  - maxdepth 12 killed at 21:12 (user). The owner still holds /obl8axd. Result: t23-d11.txt (564 lines: 563 23s, 1 22).
- 21:25: hq23 --newcanon 3 -si on all m1 3ax 13+ (10,259,674 lines; opt3ax/hq23: in.txt solving, in-inv.txt generating, inputs.sha256). Single process first to build the table (single.sh, -M 40000 -t 16 --prefixslack 0). Log: opt3ax/hq23/hq23-single.log. Shared-memory workers take over once the table is full.
- The depth-8 hq23 table was written as cache/tws9-obl8ax-39G-oissgpuh850ck.dat. hq23/shm/handoff.sh waits for the depth-9 write, stops the single process, and runs the remaining lines with an owner (/obl8axh) plus 16 workers in 10,000-line chunks. Log: hq23/shm/run.log.
- 22:13: depth-9 hq23 table written (3.5e10 at depth 9; fill 2,574 s, 22:13). The single process stopped after 33,559 lines. 22:14: owner /obl8axh ready (baseval 8); 16 workers on 1,023 chunks from line 33,560 (hq23/shm/run.log, out/).
- 2026-11-03 06:47: hq23 progress since 21:25. The 13s and 14s are done; the 15s run at about 85-100 lines/min (about 6 days for all 842,838).

  | input | lines | shortened | rate | to |
  |---|---|---|---|---|
  | 13 | 204,347 | 4,900 | 2.4% | 12: 4,775, 11: 125 |
  | 14 | 439,154 | 25,854 | 5.9% | 13: 24,762, 12: 1,092 |
  | 15 (part) | 17,366 | 2,179 | 12.5% | 14: 2,116, 13: 62, 12: 1 |

- 06:55: merge m2 (opt3ax/merge/merge2.sh): the hq23 results so far plus the shortenseqs 22, folded into ax-m1, giving ax-m2.tab. It runs alongside the shm workers. Log: merge/merge-m2.log.
- 07:05 merge m2: 32,960 hq23 lines + the 22 (33,338 new; 38,564 with suffixes; linecheck 0 problems), folded into m1 in 16 min:

  | table | 19 | 20 | 21 | 22 | 23 |
  |---|---|---|---|---|---|
  | m1 | 1,670,771 | 868,631 | 251,267 | 41,597 | 564 |
  | **m2** | 1,633,820 | 828,636 | 208,902 | 35,717 | **552** |
- 07:39: hq23 15s stopped (user). Partial chunks are in hq23/shm/out/*.tmp. The owner still holds /obl8axh.
- 11-03 07:43: tails of the m2 23s (opt3ax/tails; gL.txt = the first L moves of each inverted 23 = the inverse of its last L solving moves). All-moves table in shm /obl8ax (owner-all.pid). L13 running; summary in tails.log.
- tailtest.py (opt3ax/tails): a new line is the old head plus twsearch's solution. It must start from the same position as the old 23 (inverse line run on Solved, every set) and pass linecheck. Control: the solution spliced onto the head instead must not match. First 3 finds at L13: all ok, controls all fail as expected, linecheck 0 problems.
- L13 all-moves (07:55): 552 tails in 692 s, 3 shortened 13 -> 12, so 3 23s -> 22 (tested: tailtest ok 3; t13-obl8ax/new.txt). L14 all-moves started 07:55 (est. 30x, about 6 h). Then 4op: all-moves table, -si on full lines up to some length, tails for longer ones, then hq23 (user: about a day on 4op).
- 4op prep (opt4op): in.txt = all 1,508,386 c32 lines (solving, length order; linecheck 0 problems), in-inv.txt = generating, inputs.sha256; -si sanity test (U U, F F') ok. single.sh (all moves, builds the table) runs after 3ax L14 stops.
- 11-03 08:26: 3ax L14 stopped (30 min, about 33 of 552 done, 0 shortened); 3ax table freed. 4op all-moves single.sh started. Log: opt4op/all-single.log.
- 4op shm/handoff.sh queued: after the depth-8 write it stops the single run (pid 62766) and continues with owner /obl8op plus 16 workers in 10,000-line chunks (opt4op/shm/run.log, out/).

### Plan (user, 2026-11-03 08:50)

1. 4op all-moves -si: a quick pass (a few hours), then fold it into c32.
2. 1ud and 2ud: a quick all-moves -si pass (all 54 moves, a few hours each).
3. **Boundary 40** (P 75): the main goal. Run on stages from the newest tables (3ax m2 plus the tail finds; 4op after step 1).
   Estimate: broad pass 10-17 h; the leftovers need excess 5 (--slack 5 --deep 5, never timed); axbcheck about 5 days, so at home.
4. Then the 1ud/2ud boundary, for another move.

Sequence clean-up is secondary from here on.
- 08:50: 4op at 5,448 lines done, 189 shortened (3.5%); filling depth 8.
- Idea (2026-11-03), the user decides later whether to spend the time: a dense, cube20-style coset solver for 3ax+4op. Each 3ax class is a coset; keep a 24M-bit 4op bitset per class; take alternatives from the action graph; close under the 4op actions; lower the target per coset. It pays off below about 39, where most of each coset is bad. Prototype on the hardest classes first. Same shape for 1ud+2ud. Not started.
- Scope (user): P 76 is nearly in hand (verification at home); the stretch goals are 75 (40) and maybe 74, then wrap up.
- Naive solver from the tables (one line per stage per pair, no merging): mean 66.6 moves per oblique pair (1ud-t 14.30 + 2ud-t 16.51 + 3ax m2 17.30 + 4op c32 18.47; class means equal position means to 0.01 for 3ax and 4op), worst 80. For comparison, gullicube's beam search plus merging averages about 48 (user).

### Classes one stage move from a shorter class (2026-11-03)

A solver need not store these lines: make the move, then use the shorter class's line, recursively.
- 3ax/4op: axbound/opbound --onemove (new option; stage moves = the cost-1 actions).
- 1ud/2ud: devtools/udonemove (new; certcheck's cube, keys and symmetries).

| stage | stage moves | classes | one move from shorter | must store |
|---|---|---|---|---|
| 1ud (1ud-t) | outer turns + 2U 3U 2D 3D (30) | 14,119,851 | 13,239,245 (93.8%) | 880,606 |
| 2ud (2ud-t) | the same 30 | 2,936,935 | 2,826,206 (96.2%) | 110,729 |
| 3ax (m2) | outer L F R B only | 10,366,284 | 8,473,806 (81.7%) | 1,892,478 |
| 3ax (m2) | + 2U2 3U2 2D2 3D2 (alternative A) | 10,366,284 | 9,404,410 (90.7%) | 961,874 |
| 4op (c32) | outer L F R B | 1,508,387 | 1,212,619 (80.4%) | 295,768 |

By length: scan/1ud-onemove.out, scan/2ud-onemove.out, merge/onemove-m2.out, onemove-m2s.out, op/onemove-c32.out.

# Moved from upperbound5.md (2026-11-03): status history, work log and background

## Old status block (as of 2026-11-03 07:05)

- **Boundary dry runs: P 77 checked, P 76 covered** (1ud 17 + 2ud 18 + 3ax/4op 42 or 41).  42: axbcheck PASS,
  evidence in ~/orbitsolve-certsafe/obl23pair-axis-P77-evidence-2026-11-02.  41: every class covered (c41
  snapshot), not yet axbchecked (~35 h here; better at home).  No boundary certificate until home.
- **3ax all-moves optimality pass running** (twsearch -si, -M 40000, table saved; upperbound5-notes.md).  By 13:08:
  lengths 4-7 optimal, 7% of the 8-11 lines shortened (1,541 of 22,179).  Needs a patched twsearch (-si bug,
  TODO.md J) and inverted input (certificate lines are solving sequences).
- **Cheap scans** (merge + drop final no-op moves; scan/cheapscan.py): 4op 0; 3ax 47,800; 2ud 45,345 (9,194 of the
  18s); 1ud 1,444,340 (58,817 of the 17s).  Trimmed 1ud and 2ud stages pass certcheck --keep ud (no colour side
  effects on 6-10); not yet swapped into a certificate.
- **Fold engine patched** (axbound/opbound merge pass drops final outer turns; installed).  Test fold on c45: 47,800
  lines trimmed, then propagation: 23s 696 -> 682, 22s 46,069 -> 43,649, 21s 302,815 -> 298,324, 20s 976,777 ->
  966,382 (ax-loopt.tab; not yet in the certificate; fold loop paused while twsearch runs).
- **3ax merge m1 (19:30):** the twsearch -si improvements (7,133, inputs up to length 13), the trimmed lines and new extras folded
  into c45: **23s 696 -> 564**, 22s 46,069 -> 41,597, 21s 302,815 -> 251,267, 20s 976,777 -> 868,631
  (opt3ax/merge/ax-m1.tab; not yet in the certificate).  Details: upperbound5-notes.md.
- **3ax merge m2 (11-03 07:05):** hq23 -si on the 13s and 14s (and 17k 15s), plus the shortenseqs 23 -> 22, folded
  into m1: **23s 564 -> 552**, 22s 41,597 -> 35,717, 21s 251,267 -> 208,902, 20s 868,631 -> 828,636 (ax-m2.tab;
  not yet in the certificate).

- **4op <= 22** (4op-c32): **14,975 classes at 22**, 113,737 at 21, 313,019 at 20.
- **3ax <= 23** (ax-m2): **552 classes at 23**, 35,717 at 22, 208,902 at 21.
- **Plain sum 45** (3ax 23 + 4op 22), against the rings' 50 plain / 45 with a boundary pass.
- **Certificate: ~/orbitsolve-certsafe/obl23pair-axis-2026-10-29**, both stages, axcheck PASS (3ax from ax-c45; ax-m1/m2 not yet written into it; 4op
  from 4op-c32).  Frozen copy for ML training: ~/orbitsolve-certsafe/obl23pair-axis-snap-2026-10-30
  (14:15: ax-t17 / a3; devtools/tws/axcert-ML.md).
- **Full certificate, P 80 plain** (1ud 17 + 2ud 18 + 3ax 23 + 4op 22, no boundary pass; 2026-11-01 03:25; the
  last 3ax 24 fell to the ML at fold c25, 23:35): ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01 (1ud/2ud
  identical to the P80 record, 3ax/4op from c29); all four stages PASS on the copy (certcheck --keep ud 17, 18;
  axcheck 23, 22).  Copy on armory ~/certsafe/obl23pair-axis-P80-2026-11-01, SHA-256 of all 60 files match.
- **Full certificate, P 81 plain** (1ud 17 + 2ud 18 + 3ax 24 + 4op 22, no boundary pass; rebuilt 2026-10-31 22:46
  after fold c24, the final arena/pergola pull): ~/orbitsolve-certsafe/obl23pair-axis-P81-2026-10-31 (1ud/2ud
  identical to the P80 record, 3ax/4op from c24); all four stages PASS on the copy (certcheck --keep ud 17, 18;
  axcheck 24, 22).  Copy on armory ~/certsafe/obl23pair-axis-P81-2026-10-31, SHA-256 of all 61 files match
  (op/fullcert2.sh c24).
- **Last 3ax 24 (c23):** 3ax24-c20-003 (devtools/tws/3ax24-c20.scr/.sol).  Orbit a fully swapped (every L/R slot
  holds an F/B piece and vice versa, as in all six of the last 24s); orbit b has 4 pieces off axis, one per side
  face (the solved 004 has the same counts, so the counts alone do not single it out).
- **Laptop only from 2026-11-01** (travel, ~10 days; armory, arena, pergola unreachable): the hourly fold now runs
  locally (op/foldloop9.sh, work dir /var/tmp/orbitsolve/fold with the same pools symlinked; postfold2.sh).
- **Boundary dry run (2026-11-01; no boundary certificates until home, user):** devtools/axbx (search) and
  devtools/axbcheck (independent check) for 3ax + 4op.  On the P80 stages: 44 covers all 3,116 classes and
  axbcheck PASSes (1.48e9 positions, 7 min; negative tests fail as they should); 43 (c30 snapshot): one action leaves
  30 classes, --deep 2 leaves 22 (all 3ax 22s, 3,117 positions), --prop 10 --slack 4 --deep 4 on those 22 covers
  them all: **43 axbcheck PASS** (merged blocks on the c30 snapshot).  **42 covered** (bnd/run42.sh, c32 snapshot, 19:29): broad pass
  --prop 8 --deep 2 leaves 81 of 350,629 classes, --prop 10 --slack 4 --deep 4 covers all 81 (replay check 0 failed).  **42 axbcheck PASS** (bnd/x42m.txt, 350,629 blocks, 3.83e11 positions, 0 not solved; 5.6M 3ax states with d3 > 20 all have a block; 19,046 s): **P 77 = 17 + 18 + 42, checked, no certificate yet** (home).  Evidence (stages, blocks, check log, sources, SHA256SUMS; read-only) in ~/orbitsolve-certsafe/obl23pair-axis-P77-evidence-2026-11-02.  **41 covered** (bnd/run41.sh, c41 snapshot, 2026-11-02 12:11): broad pass (5.1 h) leaves 1,563 of 1,326,357 classes (1.18e12 bad positions), --prop 10 --slack 4 --deep 4 covers all 1,563 (2 min); replay checks 0 failed; no axbcheck yet.  **P 76 = 17 + 18 + 41 if it checks.**  A class that cannot be covered is a showstopper for that level.  Main
  effort stays on the base tables: the tops, the averages and the 21s.
- **Automatic:** this Status block's first lines, the Hourly folds block and both table sections are rewritten
  after every hourly fold (op/foldloop5.sh -> postfold.sh -> mdupdate.py); ML lists of the open tops are rewritten
  at the same time (devtools/tws/axis-current-ML.md).
- **Arena (10:30):** the 4op23-b2r run (74 of 156 done: 45 at 21, 28 at 22) started a day-long fresh fill and
  is being stopped by the user.  Laptop fold m1 (c12 + 32 newer arena/ML lines + suffixes): **4op 23s 22**;
  devtools/tws/4op23-m1.scr / .sol (on arena): exactly those 22, for the restart.  The paused 4op1921 run was
  killed by mistake (its 158K+ solves are folded); the CONT watchers are stopped.  Next for arena after the 23s:
  devtools/tws/4op1921-m1.scr / .sol (on arena), reduced set: the top 400,000 4op classes of m1 not yet solved by
  arena (320,154 tried classes from all six arena logs subtracted): 21: 26,242, 20: 279,826, 19: 93,932 (of
  363,894 untried 19s).  Every 22 and 23 has been tried in the reduced set.  ML agent: on 4op until the 4op 23s are resolved (user, 09:00).
- **Goal before travel (user, 2026-10-31):** a clean plain 80 (3ax <= 23 and 4op <= 22, P = 35 + 45) before any
  boundary work; boundary work later, on the laptop only.  Runs must give results within ~16 h.
<!-- folds start -->
**Hourly folds** (op/foldloop.log, last 12; columns: new lines, then classes by length at the top, then the certificate check):

```
c34 21:13 3ax +     333: 22:45578 23:1344 PASS
c35 22:38 3ax +     352: 22:45769 23:1078 PASS
c36 00:04 3ax +     207: 22:45826 23:1021 PASS
c37 01:30 3ax +     167: 22:45840 23:1007 PASS
c38 02:55 3ax +     180: 22:45934 23:845 PASS
c39 04:01 3ax +      81: 22:45982 23:791 PASS
c40 05:08 3ax +      43: 22:46000 23:773 PASS
c41 06:15 3ax +      44: 22:46008 23:765 PASS
c42 07:22 3ax +      33: 22:46016 23:757 PASS
c43 08:51 3ax +      41: 22:46034 23:737 PASS
c44 10:22 3ax +      37: 22:46046 23:724 PASS
c45 11:50 3ax +      57: 22:46069 23:696 PASS
```
<!-- folds end -->

- **Work in progress:**
  - Laptop: ax-cj (done, 19 min: **0 lowered**), 3ax band over 24 on ax-c9 with a conjugate pool S A S (S in 2F2 2R2 2B2 2L2 3F2 3R2 3B2 3L2, A
    every pergola 3axs line; 10.7M made, 2,931,878 pass pairlogcheck: ax/conj/conj.cls), from 08:50.  Why: the
    ML agent's 3ax 24 solutions are reduced-set lines with bracketed inner-slice half-turn pairs.
  - **3ax experiments (laptop, 2026-10-31):**
    - ML 3ax 24 solutions, traced (axis-home pieces per orbit after each move): two U/D slice quarter turns
      (e.g. 2D' 3D') take the swapped orbit 0/16 -> 8/16; ~18 moves, mostly half turns, keep both orbits at 8/16;
      two more slice quarter turns end at 16/16.  Each slice row (2U 3U 3D 2D) holds one piece of each orbit per
      face, so a row quarter turn flips the axis of 4 pieces of both orbits at once.
    - Suffixes (op/suffixes.py): wherever a solving line passes through another stage start, its rest solves
      that one.  From the ML lines and the certificate's 20-24 lines: 1,297,853 3ax suffixes (and 657,469 4op from
      the 4op 19-23 lines, arena and ML).  ax-sx (ax-c9 + 3ax suffixes, 36 min): classes moved down at every
      length, even 7-8 (the table's short lengths are not optimal: they come from compositions of pool actions,
      and a direct full-move solving line need not be one); top: 24 254 -> 252, 23 8,697 -> 8,066,
      22 57,634 -> 54,593.  Both suffix sets are queued in the fold loop's extra files.
    - Optimal twsearch of short 3ax classes (full move set, -M 8000): ~150 s per length-12 class (and the
      sample was already optimal), too slow to do in bulk.
    - ax-qc: band over 24 on ax-sx with conjugates Q A Q^-1 (Q one or two U/D slice quarter turns, A every
      pergola 3axs line; 42.7M made, 2,143,210 pass pairlogcheck), running.
  - twsearch --shortenseqs (user's idea; --distinguishall, needed since identical pieces are not invertible,
    which also makes every replaced window exact; --maxdepth 8, --nowrite, no newcanon).  First 2,558 4op 22/23
    lines (16 threads, 20 GB): 45 shortened (22 -> 19: 1, -> 20: 26, -> 21: 18).  Now op/shorten/pshort.sh: 12
    single-thread processes with 1 GB tables (user's advice; ~4x the throughput).  run2 (maxdepth 8, done
    11:27): 4op 13,308 22/23 lines -> 138 shortened (all 22 -> 20/21); 3ax 8,318 23/24 lines -> 94 shortened
    (23 -> 22: 89, -> 21: 5; none of the 24s; parse.py misaligned one line per chunk there, which first looked
    like 9 24s shortened; parse2.py fixed).  Queued for the 13:xx fold (fold-extra files).  run3 (13:45):
    maxdepth 10 on the tops (4op 12 23s, 3ax 246 24s), then maxdepth 9 on 4op 22s and 3ax 23s.  4op 23s at maxdepth 10 (~11 min a line): none shortened.  Deeper passes stopped (user: maxdepth 8 is the most worth running; it catches missed
    obvious things).  run4 (14:15): maxdepth 8 on 3ax 22s (49,847), then 4op 21s (126,192).  Done: 3ax 22s 599 shortened (-> 21: 481, -> 20: 118); 4op
    21s 1,042 (-> 20: 670, -> 19: 351, -> 18: 19, -> 17: 2); queued for the 20:xx fold.  run5 (20:20): untried 3ax
    23/24 (199) and 4op 22 (12) lines, then 3ax 21s (310,308, ~11 h).
  - 3ax 23s (2026-11-01): md 8 has seen nearly every current 23 line.  run6: the ML's 3ax23_live misses (1,091
    lines of 23+ moves): 17 shortened, 5 of them to 21-22 (queued for the fold); slow on long lines (3.5 h).
    Shape of the 3,116 23s: one orbit has 14 or 16 of its 16 side pieces off axis in nearly all ((14,6) 1,028,
    (14,4) 939, (16,6) 358, (16,8) 333 ...; counts of off-axis side pieces per orbit).
  - **Half-turn rewrite** x W x2 -> x' W* (W* = W with some quarter turns reversed, same permutation as x2 W x2;
    the 8 -> 7 shortenseqs hits; the ML agent found it too and runs it on its lines): op/ruleshort.py, exact
    permutation check on every tws set, plus the commutation merges.  On the old 3ax 23s: 84 of shortenseqs' 87
    hits, ~48 lines/s per core.  op/rs/runrs.sh (08:00, 14 processes, ~5 h): every 3ax and 4op certificate line of
    6+ moves (10.37M + 1.51M); results go to the fold-extra files.  Done: 3ax 650 lines
    shortened (mostly 17-19 -> 16-18), 4op 7; folded in the next passes.
  - **Commutation merge** (found from the shortenseqs diffs): on the centre sets (edges, corners and diagonal
    centres are free here) outer face turns commute with each other on any axes, and inner slices of the same
    depth commute across axes (2F with 2D, 3R with 3U...).  opbound's merge pass only used same-axis
    commutation.  op/cmerge.py over the whole certificate: 4op 1,623 lines shorter (22s: 21), 3ax 19,640 (22s:
    73; none at 23-24); all pass linecheck.  axbound.cpp's merge pass now uses the full rule (laptop and armory
    rebuilt 10:49).  4op-cm (m1 + those, laptop): 22s 15,924 -> 15,793, 23s 22.
  - (A deep band, two actions, as on the rings: not useful here.  The tables are closed under propagation with
    the cheap actions everywhere and the band at the top, so two actions add nothing a wider band would not,
    and wider bands have been tried.)
  - /var/tmp/orbitsolve/op/foldloop.sh (laptop, folds on armory, hourly): arena 4op1921-t52 and 4op23-b2r logs,
    pergola 4op23-b2 log, ML 4op23/4opswap and 3ax24/3axswap; tables 4op-cN / ax-cN, certificate each time;
    log op/foldloop.log (now foldloop4.sh from c9: adds ML 4op22_live/3ax23_live and the extra files
    op/fold-extra-op.txt, ax/fold-extra-ax.txt).  c9: ~9,000 ML results that were only in ~/mlcube/runs/*/*.json
    (never in the live files) folded: 4op 23s 65 -> 60, 3ax unchanged.
  - Arena (08:20): the 4op1921 run paused by the user; the 12-move 23s run is at 4op23-b2-106 (50 done),
    but only 58 of the 106 left in its queue are still 23s at c9.  devtools/tws/4op23-c9a.scr / .sol (on arena):
    exactly those 58 (not used: the user keeps the run going rather
    lose hours rebuilding the 1.1 TB table).  Pergola's 56 GB copy was still on its first scramble after 8 h:
    stopped 08:30 (pergola idle).
  - ML lists at c9: devtools/tws/axis-c9-ML.md (4op23-c9 60, 3ax24-c9 254, 4op22-c9 16,237, 3ax23-c9 8,697).
     4op 23s by pass: 131, 125, 108, 105, 96, 86,
    67, 65 (c8); 3ax 24s 323 ... 254.  Arena's 12-move run on the 23s (from the end of the list): 38 solved by
    07:40, 24 at 21 and 14 at 22 -- every one lowered; pergola still on its first scramble.
  - Arena (user): devtools/tws/4op1921-t52.scr / .sol (on arena twsearch2/): 855,212 4op classes of t52 at
    21 (217,171), 20 (350,811), 19 (287,230), longest first, less every class arena's reduced-set optimal
    solver has already done (161,465 classes from 4op2223-t45n, 4op2324-t33 and 4op24-t26; matched by
    axcheck --keys).  All 16,188 22s and 156 23s of t52 had already been tried, so the list starts at 21.
    First fold (b1, 23:25, 5,186 solved): 21s 280,422 -> 260,448, but 22s only 16,188 -> 16,186 and the 23s
    unchanged: the 21s' optima do not reach the top through propagation.  Second fold (b2, 15,643 solved):
    21s 235,667, 22s and 23s unchanged.  Certificate on 4op-b2.
  - twsearch optimal over the 156 4op 23s in the reduced set + side-slice half turns (F R B L 2U2 3U2 2D2
    3D2 2F2 2R2 2B2 2L2; rotation group 8): pergola on devtools/tws/4op23-b2.scr (forward, -M 56000,
    twsearch/4op23-b2.log, from 00:02; its 3axs generator stopped) and arena on 4op23-b2r.scr (reverse order,
    -M 1100000 -t 40, twsearch2/4op23-b2r.log, from 00:05, beside the 4op1921 run).  Armory's -M 12000 copy
    swapped and was killed.  (t53, 4op band over 22-23 on t52: 0 lowered.)
  - Laptop: idle.  Null today: ax-t21 (3ax band over 24 on ax-t20, pergola 3axs s1-s3 as pools, 25 min)
    and t54 (4op band over the 156 23s on t52, every pergola 17s batch as pools, 10 min): 0 lowered.
    Pools and bands no longer move either top; only ML lines (and arena with a wider move set) do.
  - ML agent: 3ax 24s and 4op 23s (devtools/tws/3ax-t11b-ML.md, 3axswap-ML.md, 4opswap-ML.md).
  - Earlier null results (history.md): pergola 3axs batches 1-2 as pools (t47, t49, ax-t14, ax-t18),
    pergola 4op 17s as loaded lines, ax-t12 (3ax bands are exhausted).


## Progress at the top

| stage | top level, classes |
|---|---|
| 4op | 28: 54 (op1) -> 27: 339 (t1) -> 26: 5,734 (t1) -> 577 (t6) -> 37 (t7) -> 0 (t11); 25: 2,479 -> 0 (t27); 24: 15,256 -> 12,291 -> 1,952 -> 946 -> 631 -> 10 -> 0 (t39); 23: 25,745 (t39) -> 25,436 (t40) -> 448 (t43, arena) -> 273 (t45) -> 269 (a3) -> 218 (t50, ML) -> 156 (t51, ML) -> 131 (c1, ML) -> 65 (c8: ML + arena 12-move optima) -> 60 (c9, ML results from runs/); 22: 119,836 (t45) -> 91,592 (t46) -> 71,142 (t48) -> 49,643 (a1) -> 39,257 (a2) -> 29,921 (a3) -> 24,598 (a4) -> 19,168 (a5) -> 16,723 (a6) -> 16,760 (t50: 51 23s came down) -> 16,800 (t51) -> 16,188 (t52, laptop band b22) |
| 3ax | 26: 2 -> 0 (ML); 25: 487 (ax-t2) -> 97 (ax-t5) -> 77 (ax-t8) -> 67 (ax-t9) -> 35 (ax-t11) -> 8 (ax-t11b) -> 7 (ax-t15) -> 0 (ax-t16); 24: 545 (ax-t11b) -> 512 (ax-t16) -> 503 (ax-t17) -> 420 (ax-t19, ML) -> 365 (ax-t20, ML) -> 323 (ax-c1, ML) -> 254 (ax-c8, ML) |

What moved the top, in order of effect: the ML agent's lines and arena's reduced-move-set optima
(each line also pulls neighbours down through propagation, ~5-15 classes per line); freeing the
diagonals (577 -> 37 4op 26s in one run); the merge pass; bands over the top lengths.  The twsearch
generation logs (arena 13s/14s, pergola 15s-17s) mostly lower the middle.

## Stages

| name | solves | positions | classes (16 symmetries) |
|---|---|---|---|
| **3ax** ("axis") | from the pair's U and D solved: every F/B piece onto F or B and every L/R piece onto L or R, both orbits | C(16,8)^2 = 165,636,900 | 10,366,284 |
| **4op** ("opposite") | from the 3ax goal: F vs B and L vs R, both orbits at once | 70^4 = 24,010,000 | 1,508,387 |

- **Symmetries:** the 8 rotations that keep the U-D axis, times the mirror (which swaps orbits a and b).
- **Side conditions:** both stages keep every other oblique orbit colour-solved; the diagonals may
  move (they are solved last).  So a 3ax line can be followed directly by a 4op line, and a 3ax/4op
  boundary pass is possible (alternative B; history.md has the comparison with A, slice half twists
  in 3ax, which saved about 1 at the top).
- **Lower bound:** 4op >= 15 (it contains 4hb, exactly 15).
- **State encodings** (devtools/axbound.cpp): 3ax, per orbit 16 side slots (L F R B, 4 each; bit set =
  F/B piece), SOLVED 0xF0F0F0F0; 4op, per orbit 8 F/B slots (bit set = F piece) and 8 L/R slots (bit
  set = L piece), SOLVED 0x0F0F0F0F.

## Method

- **Engine:** devtools/axbound.cpp (3ax), opbound.cpp (the same engine built with OP4).  Dense table,
  one distance byte per position.  Actions: every pool line that keeps the stage, stored once per
  conjugacy class.  Propagation with the actions of cost <= --prop; bands (every action on the
  classes of the top --band lengths); a **merge pass** (default since 2026-10-29: rebuild every
  class's line, cancel turns of one layer also across commuting turns of that axis, load the shorter
  lines, propagate, repeat).  Every value is the length of a replayed solving line.
- **Persistent tables:** --save writes one line per class (LEN<TAB>moves); --base loads a saved table
  as closed, --load adds lines (ML, arena) or other tables.  A fold-in of new lines takes ~3 min on
  armory.
- **Pools:** base (pair3, p8s10, p9, eg8, c12x), 7pairud to 13 (ud.cls), arena's 4op log (12-14),
  pergola's reduced-move-set 4op log (to 17), pergola's 4hb optima; all with --diagok.  Pulled in
  batches by /var/tmp/orbitsolve/op/arena/pull.sh and .../pergola/pull.sh.
- **External lines:** ML (obl8op.tws / obl8ax.tws, 8x8, only the six oblique sets) and arena's
  optimal solves.  Every one is replayed with /var/tmp/orbitsolve/op/linecheck.py before folding
  (0 problems so far).  ML packages: mkml.py (scr/sol), twsval.py.
- **Certificate:** devtools/axcertupdate.sh CERTDIR 3ax|4op TABLE writes stages/<stage>/lNN.txt (one
  file per length; only changed files are rewritten) and the MANIFEST; devtools/axcheck 3ax|4op DIR
  checks lines (legal, at most NN, a stage start), one line per class with every stage start covered,
  and side effects on 6..10.  4op 7 s, 3ax 12 s (armory) / 37 s (laptop).  axcheck --keys FILE writes each
  line's canonical class key (used to subtract classes already sent to arena).

## Structure of the tails

The 4op 24s (t27) were mostly derangements: per orbit and axis, the number of pieces already on their
own face (0-4) was low (5.8 of 16 on average, against 8 for a random position), and 79% had exactly one
orbit-axis fully swapped (all four F pieces on B and all four B pieces on F, or L/R), against 6% at
random.  A 3ax ending that leaves no orbit-axis fully swapped avoids most of the 4op tail; a boundary
pass would use that.

## Extra grip for the reduced move sets (2026-10-30, from the certificate)

Lines at the top lengths (4op >= 21: 438,061; 3ax >= 22: 66,585), counted by the grips they use
outside the reduced set (4op: F R B L, U/D slice half turns; 3ax: F R B L, U/D slice turns):

- 4op: 74,558 lines use none (arena's optima); 2,358 use exactly one, always a side slice (3B 442,
  2B 398, 3F 357, 3L 307, 2F 285, 2L 225, 3R 207, 2R 137: third-layer slices 1,313, second-layer
  1,045); never a U/D slice quarter turn alone; 355k use four or more.
- 3ax: no line uses only one extra grip (2: 70, 3: 2,762, 4+: 63,753).
- Per grip the order is mostly orientation noise (each class's line is one arbitrary conjugate).  A
  single side slice breaks the y-rotation symmetry of the reduced set, so to use it on a class, solve
  the scramble in its four y-rotations (or add all four grips of the type).  The certificate's lines
  come from full-move pools, so this is weak evidence; a direct test is reduced + 3F against
  reduced + 2F on a sample of the hard scrambles.

**3ax tails (ax-t15, 2026-10-30).**  All 7 3ax 25s and 516 of the 545 24s have **one orbit fully
axis-swapped**: its 8 F/B pieces all on L and R, its 8 L/R pieces all on F and B (at random 1 in
12,870 per orbit); the other orbit is mostly in place (25s: 10 or 12 of its 16 pieces on their own
axis).  The 23s mostly have one orbit with a single pair in place.  Every class of that shape (1,703;
about 25,740 positions) is long: 16: 1 (both orbits swapped), 20: 36, 21: 112, 22: 433, 23: 599,
24: 515, 25: 7.  So the 3ax tail is the same thing as the 4op tail (79% of the 4op 24s had one
orbit-axis fully swapped).  All 1,703 with their current lines: devtools/tws/3axswap.scr / .sol (for
ML training).  (Keys from axcheck --keys: slot order is k order, nibble 2*face+orbit for faces
L F R B; bit set = F/B piece; solved ff00ff00.)

**4op tails (a2, 2026-10-30).**  All 271 4op 23s and 25,178 of the 39,257 22s have an orbit-axis
fully swapped (85,388 classes of that shape in all, 12-23; 43,882 at 21).  All of them with their
lines: devtools/tws/4opswap.scr / .sol (4opswap-ML.md), the 4op counterpart of 3axswap.

## Boundary outlook (estimate, 2026-11-01 03:50; tables ax-c29 and 4op-c24; laptop-hours)

From the plain 80 (3ax 23 + 4op 22 = 45).  A 3ax class at length a needs work for target X when a + 22 > X; its
bad colourings are the 4op positions beyond X - a (4op positions: 22: 235,552, 21: 1.81M, 20: 4.99M, 19: 6.50M
of 24.01M).  Excess = a + b - X, at most 45 - X.  Scaled on rings 45 (8.3e11 bad pairs; boundx ~290 core-h,
boundcheck 107 core-h for 1.82e12 colourings) and on the earlier rows of this table; laptop = 16 cores.

| X | P | 3ax classes needing work | bad pairs | boundx (laptop h) | boundcheck as written / bad-only (laptop h) |
|---|---|---|---|---|---|
| 44 | 79 | 3,116 (the 23s) | 7.3e8 | ~0.1-0.3 (excess 1) | ~0.3 / minutes |
| 43 | 78 | 48,716 | 1.7e10 | ~1.5-3 (excess up to 2) | ~4 / ~0.1 |
| 42 | 77 | 350,846 | 1.9e11 | ~6-12 (excess up to 3) | ~30 / ~0.7 |
| 41 | 76 | 1.33M | 1.2e12 | ~25-50, success uncertain (excess up to 4) | ~120 / ~4.5 |

Plus the one-time adaptation of boundx and boundcheck (stage encodings, 16 symmetries, side conditions on
6..10); bad-only boundcheck needs a change to check only the bad colourings.  Every 3ax 23 the ML lowers
(5,964 -> 3,116 since c24) shrinks the 44 row directly and the lower rows a little.

## Plan

1. 3ax 24s (503, nearly all one orbit fully swapped) to 23: ML agent (3axswap-trained net); plain sum 46.
2. 4op 23s (273) to 22: ML agent (full move set); then the 22s (119,836) with arena's optimal solves.
3. Pergola: 4op 17s as they come; then the 3ax reduced-move-set generator.
4. A 3ax/4op boundary pass (adapt boundx/boundcheck), aiming at 44 or below.
5. Deferred (user, 2026-11-01): an optimality pass with twsearch -si over the certificate's own lines (they are
   generator sequences), to see what shortens; neither table is proven exact at any depth (4op unchanged
   through 12 since c12, 3ax only through 8; the 7pairud pool, complete to 13, was never loaded).
6. Open: t3 found 32 more 4op positions at 13 than t2, so the pools were not quite complete at 13 as
   4op actions; check before calling the table exact through 13.

### Prefix plus lookup (2026-11-03)

A class that still needs a line could store only a prefix of its line, if the prefix ends at a stage position whose class is no longer than the rest of the line.

| stage | prefix found | stored moves (full lines) |
|---|---|---|
| 4op | 97,883 of 295,767 (33.1%) | 4.32M (5.42M) |
| 2ud | 1,481 of 110,728 (1.3%) | 1.60M (1.62M) |
| 1ud | 642,121 of 880,605 (72.9%) | 7.06M (11.90M) |

The 3ax run was stopped. In all, about 35M moves become about 29M, and the user judged that not worth implementing; about 28 MB is fine for the oblique centres. Revisit for edges and diagonals.

### 4op all-moves -si (2026-11-03 08:25-10:25)

- Lengths up to 12: all 6,853 lines done, 230 shortened.
- Lengths 13 and up: about 1 line/min/worker, too slow; 1,040 done, 119 shortened (113 13s).
- Fold 4op-m1 (opt4op/merge): 351 lines, 374 with suffixes, linecheck 0 problems.
  - 22s 14,975 -> 14,965; 21s 113,737 -> 113,539; 20s 313,019 -> 311,675.

# Part 7. upperbound5.md as of 2026-11-03

# Oblique chiral pair, axis split (summary)

Summary and the current table only.  Development, approaches tried and run details: upperbound5-notes.md;
logs: history.md; the certified record: upperbound3.md.

## Bounds (single-layer turn metric; D 44, M 39, W 162, corners 11 / 20)

| P | status | whole cube, even n | whole cube, odd n | centres only, even n | centres only, odd n | n = 6, 7, 8 (whole) |
|---|---|---|---|---|---|---|
| **80** | certified (upperbound3.md) | (80 n^2 + 344 n - 920) / 8 | (80 n^2 + 340 n - 1580) / 8 | (80 n^2 - 304 n + 288) / 8 | (80 n^2 - 308 n + 204) / 8 | 503, 590, 869 |
| 77 | checked, dry run (42 axbcheck PASS); certificate at home | (77 n^2 + 362 n - 944) / 8 | (77 n^2 + 364 n - 1625) / 8 | (77 n^2 - 286 n + 264) / 8 | (77 n^2 - 284 n + 159) / 8 | 500, 587, 860 |
| 76 | covered (41), axbcheck at home | (76 n^2 + 368 n - 952) / 8 | (76 n^2 + 372 n - 1640) / 8 | (76 n^2 - 280 n + 256) / 8 | (76 n^2 - 276 n + 144) / 8 | 499, 586, 857 |
| 75 | target (40) | (75 n^2 + 374 n - 960) / 8 | (75 n^2 + 380 n - 1655) / 8 | (75 n^2 - 274 n + 248) / 8 | (75 n^2 - 268 n + 129) / 8 | 498, 585, 854 |

P = 1ud + 2ud + (3ax + 4op): 17 + 18 + 45 plain = 80; with a 3ax/4op boundary pass 42 -> 77, 41 -> 76, 40 -> 75.
Whole cube: P (n-2)(n-4)/8 + (n-2)/2 (D + W) + 11 (even), P (n-3)(n-5)/8 + (n-3)/2 (D + M + W) + 20 (odd);
centres only: drop W and the corner term.

## Status (2026-11-03 10:23)

- **4op <= 22** (4op-m1): **14,965 classes at 22**, 113,539 at 21, 311,675 at 20.
- **3ax <= 23** (ax-m2): **552 classes at 23**, 35,717 at 22, 208,902 at 21.
- **Plain sum 45** (3ax 23 + 4op 22); boundary dry runs: 42 checked (P 77), 41 covered (P 76).  No boundary
  certificate until home.  Evidence for 77: ~/orbitsolve-certsafe/obl23pair-axis-P77-evidence-2026-11-02.
- **Certificate: ~/orbitsolve-certsafe/obl23pair-axis-2026-10-29**, both stages, axcheck PASS (3ax from ax-c45; ax-m1/m2 not yet written into it; 4op
  from 4op-c32).  Full P 80 certificate: ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01.
- **Now:** quick 1ud/2ud -si pass, then boundary 40 (P 75).
- **Table size for a solver:** about 28 MB (2.25M stored lines; the rest are one stage move from a shorter class).

## Current tables (classes by length; P <= 80 plain)

P = 1ud + 2ud + 3ax + 4op, each stage from the previous one's goal, classes up to the stage symmetries.  1ud and 2ud: ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01 (1ud, 2ud identical to the P80 record of 2026-10-27); 3ax: /var/tmp/orbitsolve/ax/ax-m2.tab (merge m2: hq23 -si), 4op: /var/tmp/orbitsolve/op/4op-m1.tab (4op merge m1: all-moves -si, 2026-11-03).

| length | 1ud | 2ud | 3ax | 4op |
|---|---|---|---|---|
| 0 | 1 | 1 | 1 | 1 |
| 3 | 2 |  |  |  |
| 4 | 26 |  | 4 | 4 |
| 5 | 136 |  | 22 | 6 |
| 6 | 620 | 28 | 58 | 8 |
| 7 | 2,580 | 106 | 101 | 37 |
| 8 | 12,814 | 330 | 506 | 125 |
| 9 | 38,714 | 422 | 2,022 | 268 |
| 10 | 122,474 | 2,346 | 8,088 | 636 |
| 11 | 380,568 | 14,772 | 24,726 | 1,611 |
| 12 | 993,315 | 26,672 | 77,071 | 4,469 |
| 13 | 2,028,815 | 56,558 | 225,254 | 12,142 |
| 14 | 3,169,592 | 154,051 | 444,068 | 30,650 |
| 15 | 3,776,410 | 368,574 | 829,220 | 54,619 |
| 16 | 2,905,185 | 547,197 | 1,492,054 | 86,332 |
| 17 | 688,599 | 949,232 | 2,191,771 | 165,453 |
| 18 |  | 816,646 | 2,363,691 | 305,543 |
| 19 |  |  | 1,633,820 | 406,304 |
| 20 |  |  | 828,636 | 311,675 |
| 21 |  |  | 208,902 | 113,539 |
| 22 |  |  | 35,717 | 14,965 |
| 23 |  |  | 552 |  |
| **classes** | **14,119,851** | **2,936,935** | **10,366,284** | **1,508,387** |
| **longest** | **17** | **18** | **23** | **22** |

# Running log (from 2026-11-03)

2026-11-03 10:45: the .md files were reorganized (user): upperbound5.md (brief results), methods.md, twsearch.md, and this journal (all earlier notes verbatim above).  Backups of the old files: /var/tmp/orbitsolve/md-bak-20261103.
11-03 10:44: 1ud all-moves -si started (opt1ud; devtools/tws/1ud-si.tws = goal aUbU with other obliques keeping U and D; input scan/1ud-t, 14,119,850 lines, length order, inverted). Test on the first 400 short lines: 20 shortened, all with the same start position and no side effects (certcheck --show --keep ud).
2026-11-03 11:45: bounds.md renamed back to upperbound5.md (user: upperbound5.md is the main landing page).
2026-11-03 12:15: 1ud test fold.
- Input: the first 215,236 1ud -si finds (input lengths up to 11; pairlogcheck --keep ud: all pass; only the diagonals move).
- Folded into a copy of scan/1ud-t with certextend --near 0 --band 0 --keep ud --slices --prop 6, then certmerge: 5,592,417 classes shortened, 0 bad, 0 disturb another centre. /var/tmp/orbitsolve/r1ud-si.
- 1ud classes at 17: 629,782 -> 161,600 (-74%); at 16: 2,749,636 -> 1,696,255; at 15: 3,597,754 -> 3,243,530.
- Lessons: certextend defaults to --prop 8 with a top band; with 215k new algorithms both blow up, so use --band 0 --prop 6 (413 s).
- certcheck of the copy running.
11-03 12:54: twsearch unpack bug found and fixed (see twsearch.md); 1ud resumed on the fixed build: shm owner /ud1si, opt1ud/shm2 (8 workers full lines from 510,644; 8 workers on 811,469 distinct 12-move tails of the r1ud-si 17s and 16s).

11-03 15:50: 1ud tail splice (opt1ud/splice/splice.py).  187,050 12-move tail finds (shm2/tails so far) applied to EVERY line of the r1ud-si stage (14,119,851 lines, not just the 16s/17s), and under the y rotations (tail conjugated, solution conjugated back; x2 is not a 1ud symmetry here since Solved is "U pieces on U").  537,189 lines shortened (61k of them only via a rotation: 16,982 / 17,673 / 26,572), every one checked on 1ud-si.tws: same start, shorter; 0 bad.  Control (head replaced instead) matches only on the 7,822 length-12 lines, where head is empty (expected).  By length: 17->16 39,415, 17->15 2,139, 17->14 29; 16->15 248,915, 16->14 8,575; 15->14 141,748; ...
11-03 15:53: second 1ud test fold started (r1ud-si2/fold.sh): certmerge of the spliced lines, then all -si solutions so far (full + tails) as a pool, certextend, certmerge, certcheck.
11-03 16:00: second 1ud test fold done (r1ud-si2, 7 min): spliced lines 537,189 classes; then pool of 846,081 (all 817,214 full-line finds pass pairlogcheck --keep ud; 158,183 of the 187,050 tail solutions "move another oblique", expected, since a tail need only match its own effect; those are used through the splice) -> certextend/certmerge 2,599,631 classes, 0 bad, 0 disturb.  certcheck --keep ud PASS at 17.  1ud 17s 161,600 -> 107,164; 16s 1,696,255 -> 1,442,664; 15s 2,989,379.
11-03 17:41: 1ud shm2 tails done: 187,132 of 811,469 12-move tails shortened; full lines 1,610,000 of 13.6M done, 1,027,054 shortened (64%).
11-03 17:45: tails2 started (opt1ud/tails2, 8 workers): new 12-move tails of the r1ud-si2 stage, distinct up to y rotation and not searched before: 17s 0 (all done), 16s 32,735, 15s 1,290,776 (16s first).
11-03 17:56: switched to 2ud (user: "switch to 2 after fold three ... we can resume any stage at any time").  1ud stopped resumably: STOP files in opt1ud/shm2/full, shm2/tails, tails2; claims of unfinished chunks removed (full caaagh..caaago, tails2 caaaaai..caaaaap); partial .tmp logs kept (fold 3 uses their finds).  Owner /ud1si torn down cleanly (SIGTERM).  Table on disk: opt1ud/cache/tws9-1ud-si-39G.dat.  tails2: 16-move tails done (4 chunks).  2ud single.sh (table build, -M 40000 -t 16) started 17:57, inputs sha256 OK.
11-03 17:56: fold 3 (r1ud-si3.sh) started: splice (string + y rotations), pool of 1,060,090 full finds (all pass), certextend --threads 12, certmerge, splice again, certcheck.
11-03 18:10: user asked about symmetry and the rejected tails.  The rejected tails (moving another oblique standalone) cannot be certextend actions: the table holds pair states only and side conditions are a property of actions, so a tail is usable only after a head that repairs the other obliques, i.e. by splicing.  But the string splice missed (a) the 4 mirror symmetries (certcheck: 8 stage symmetries; y rotations x L/R mirror) and (b) other lines whose suffix reaches the same labelled position by a different string or a different length.  splice3.py: find images under all 8 symmetries (every one of 191,793 x 8 checked on 1ud-si.tws: 0 fail), keyed by labelled position (PAIRA PAIRB MID2 MID3), every suffix of every line looked up.  Test, 200k lines of the r1ud-si2 w8: 1,322 more lines shortened (suffix lengths 11, 12, 13), all re-checked.  Chained after fold 3 (r1ud-si3b.sh).  First tail run g12 was distinct strings only (811,469 strings = 678,388 up to y rotation, 16% duplicate work); tails2 is distinct up to y rotation (the mirror not yet).
11-03 18:10: fold 3 done (r1ud-si3, certcheck --keep ud PASS at 17): splice 7,655, full-find pool 1,060,090 -> certextend/certmerge, splice again, then splice3 (position-keyed, 8 symmetries, every suffix).  1ud 17s 107,164 -> 107,137 -> 106,989; 16s 1,442,664 -> 1,440,697 -> 1,439,885.
11-03 18:15: disk.  31 GB free of 1.8 TB.  Deleted 7pair-2ud-12to13.log (32G), shorten/cache distinguishall table (22G), 7pairup-2-13s.log (9.9G), seen/7pairup-2-a6.seen (6.1G) (user OK; one link each, not open): free space unchanged.  Cause: APFS local Time Machine snapshots (22, hourly since 11-02 17:46) are whole-volume; the /var/tmp exclusion only governs what is copied to the backup drive (not attached while travelling).  Space returns as the snapshots age out (~24 h).  User declined deleting snapshots or a separate APFS volume.
11-03 18:15: twsi4 = twsi2 + a free-space guard in writept (pruneio.cpp): writept truncates the old file and then error()s on a full disk, losing both the file and the in-memory table; now it skips the write (table stays in memory) unless bitcost/8 + 2 GB fits (statvfs).  Tested: 32 MB RAM disk -> "Not writing ... free"; normal disk -> written and read back.  Use twsi4 for all new runs (the running 2ud single.sh is on twsi2; its depth-8 write ~16-22 GB expected, 31 GB free).
11-03 18:20: upperbound5.md brought up to date: status time, 3ax m3 (549 at 23, 35,720 at 22), 1ud column from r1ud-si3 (test fold 3), Now = 2ud, Open list (1ud resume, twsearch upstream state).  Backup md-bak-20261103/upperbound5.md.1820.
11-03 18:45: twsearch: wasm build of shm-tables tried (user): builds and links; build-wasm64 fails on main too (MEMORY64 vs ASYNCIFY, pre-existing; user: not used yet).  The wasm API reaches processargs, so the shm options are now stubbed for WASM like Windows (one shmunsupported() message; README).  Lint clean, test-cpp-samples pass, native hold/attach T-perm identical to a plain load, wasm builds.  Committed c767564 on shm-tables, fast-forward merged into main and pushed (user asked).
11-03 18:50: 2ud depth-8 table written (cache/tws9-2ud-si-39G.dat, 20.7 GB, bitcost 165.9G, compression 2.02; disk then 11 GB free, snapshots pin the old 3.9 GB file).  Single run: 27664 of 58689 lines shortened.
11-03 19:37: 2ud handoff (opt2ud/shm/handoff.sh, twsi4): owner /ud2si ready (read 13.7 s), 16 workers on 288 chunks from line 58,690.
11-03 20:06: first 2ud test fold (r2ud-si.sh -> /var/tmp/orbitsolve/r2ud-si, 1 min): 45,207 finds (single run + first shm chunks, input lengths <= 13; all pass pairlogcheck --keep ud) -> certextend 316,694 classes shortened (31,371 direct, 287,164 by propagation), certmerge 0 bad 0 disturb, certcheck --keep ud PASS at 18.  2ud 18s 816,646 -> 771,238; 17s 949,232 -> 914,722; 16s 547,197 -> 577,596.
11-04 06:55: 2ud shm full-line run too slow past 13: 4 chunks (40k 13s) in 11 h (~2.5 s/line/worker; ~10x per extra move: 14s ~3 days, 15+ out of reach).  Should have been caught last night.  Done: every 2ud line up to 13 (103,490) but half of chunk caaaae (still running, 1 worker), plus ~10k 14s (partial .tmp logs kept; 7,592 finds in them).  Stopped the 14-chunk workers (STOP; claims caaaaf..caaaat released for resume).
11-04 07:00: 2ud 12-move tails (opt2ud/tails): tails of the r2ud-si 18s and 17s distinct up to the 8 stage symmetries: only 41,211 + 34,238 = 75,449 (2ud lines share tails heavily).  15 workers on /ud2si, 2,500-line chunks; ~22% shortened so far.  Fold r2ud-si2.sh chained after (splice3 --tws 2ud-si.tws, pool, certextend, splice again, certcheck).
11-04 07:05: user asked: lines that end in a face turn or 2U 3U 2D 3D?  Any outer turn, and (keep u/ud) any inner U/D layer, leaves the goal as it is, so such a last move is wasted.  scan/1ud-t and scan/2ud-t: 0 such lines.  Our folds: r1ud-si3 52, r2ud-si 46 (all droppable: same start without the last move, checked on 1ud-si.tws / 2ud-si.tws); none end in two.  Traced: all from certextend output, none from the -si finds (0 end in a free move; ~45% START with one) or the splices.  Cause: with --near 0 a seed line is an algorithm image on its own; images include inverses, which end with what the find started with; certextend's strip only made extra variants without trailing OUTER turns (depth 1), never inner slices.  Fix (devtools/certextend.cpp, improve()): drop trailing moves that fix the goal on the stage pieces (outer turns; with --slices, 2U 3U 2D 3D); every line goes through improve.  Old binary: certextend.old-20261104.  Test: 2ud fold 1 redone: 324,590 classes shortened (was 316,694), 18s 764,473 (was 771,238), 0 lines ending free, certcheck PASS.  1ud fold 3 should gain similarly when redone.
11-04 07:08: 2ud 12-move tails done: 16,962 of 75,449 shortened (22.5%).  Fold 2 (r2ud-si2.sh) running (its certextend step uses the fixed binary).  13-move tails started (opt2ud/tails13: 45,364 + 41,510 = 86,874, distinct up to 8 symmetries, from the r2ud-si 18s/17s; 1,000-line chunks, 15 workers).
11-04 07:11: 2ud fold 2 (r2ud-si2, fixed certextend): splice3 of the 12-move tail finds (16,931 x 8 images, 0 fail the check) shortened 231,240 lines (suffix lengths 10-14), pool 74,278 full finds -> certextend 361,323 classes, splice again 78, then 11 trailing free moves dropped.  certcheck --keep ud PASS at 18.  2ud 18s 771,238 -> 685,893; 17s 914,722 -> 808,953.  upperbound5.md 2ud column now from r2ud-si2.
11-04 07:20: user: "keep working all day on this; tails or work on the 14s or whatever makes sense".  Started:
- r1ud-si4.sh: 1ud refold from scan/1ud-t with the fixed certextend (all 1ud full finds; extend, splice3, extend, splice3, drop trailing free moves, certcheck).
- opt2ud/loop.sh: after tails13, rounds of fold (r2ud-si3, -si4, ...) -> new tails not searched before up to symmetry (12-move of 18/17/16s, 13-move of 18/17s; mktails.py) -> 15 workers -> fold, until a round has < 2,000 new tails.  After fold 2 there were already 39,801 new 12-move and 13,502 new 13-move tails.
- Full 2ud 14s stay out: ~25 s/line/worker (157k lines ~3 days).  14-move tails of the 18s: maybe overnight, after the loop.
11-04 07:50: user asked whether a boundary-code bug could make it run fast and wrong.  42 has an independent axbcheck PASS (+ 3 negative tests); 41 was only covered by axbx, never checked.  Common-mode risk: certcube.h geometry shared by axbx/axbcheck/axcheck/certcheck (an end-to-end replay on the .tws definitions would rule that out).  Started axbcheck on a 1/50 sample of the 41 blocks (bnd/s41.txt: every 50th complete x41a block (x41a minus its 1,563 left classes) = 26,496, plus all 1,563 x41b blocks = 28,059; step 3 will report the unsampled classes as missing, expected) with /usr/bin/time -l, 4 threads.  Disk 18 GB free.
11-04 07:58: 1ud refold r1ud-si4 (fixed certextend, from scan/1ud-t, all 1.28M full finds, then splice3): certcheck PASS at 17; 1ud 17s 106,989 (fold 3) -> 23,835, 16s -> 1,208,847.  Its second certextend found nothing: certextend propagates only from classes it shortened in the same run, so the 305k spliced classes were never expanded (true of every fold so far, 1ud and 2ud).  --proptop 16 pulls instead (each class >= 16 through all 3,671 cheap actions): 277,895 more classes (251,805 pulled + 28,073 propagated), 4.5 min, 3.9 GB.  certcheck PASS: **1ud 17s 5,296**, 16s 976,039.  More pull rounds running (r1ud-si4p.sh).  2ud loop.sh restarted with up to 3 --proptop 16 pulls per fold (a stale copy, pid 26117, kept running after the first pkill and the script was edited under it while it slept in its wait loop; killed before it read on).
11-04 08:15: --proptop 16 on 2ud fold 2 (r2ud-si2, in place; the loop copies it next): round 1 474,118 pulled + 132,426 propagated = 586,755 classes; round 2 98; certcheck --keep ud PASS at 18.  **2ud 18s 685,893 -> 287,575**; 17s 1,055,318, 16s 673,843, 15s 427,438.  Why: certextend's propagation pushes only from classes shortened in the same run; splices/merges never pushed, so the table was not closed under the cheap actions; proptop pulls (every class >= 16 through every cheap action), then pushes from what it lowered.
11-04 08:17: 1ud pull round 2: 4 more; certcheck PASS: 1ud 17s 5,294, 16s 976,039.  Frozen copies of both stages for the UD boundary: bcert/ud34/snap (r1ud-si4, r2ud-si2 after pulls).  08:20 UD 34 dry-run sample started (bcert/ud34/sample.sh: boundx 34 --deep 1 --slack 5 --sample 50 --threads 4).  upperbound5.md tables: 1ud and 2ud columns from bcert/ud34/snap.
11-04 08:19: UD 34 sample (1/50): 106 of 106 classes covered, 243,775,408 bad colourings, 25 s on 4 threads, 1.7 GB; alternatives per class 1:34, 7-13 the rest.  (2ud positions at 18 in the snapshot: 2,299,768.)  Full UD 34 dry run started (bcert/ud34/full.sh: boundx over all 5,294 1ud 17-classes, then boundcheck --keep ud).  41 axbcheck sample: stage checks passed (3ax 165.6M states, 4op 24.0M, 0 not covered), blocks being checked.
11-04 08:25: arena-plan.md written (user): deep in-memory tables on arena (1.5 TB, no disk: --startprunedepth + --shmhold + --nowrite), move sets per stage, campaign order; linked from upperbound5.md Open.
11-04 08:24: UD 34 boundx (dry run): all 5,294 1ud 17-classes covered, 12,174,971,792 bad colourings, 0 left, 304 s on 5 threads.  boundcheck --keep ud running.  2ud 13-move tails: 11,335 of 86,874 done, 4,163 shortened (37%); ~150/min, ~9 h total.
11-04 08:35: arena-plan.md: arena is 2 sockets, 20 cores / 40 threads, old high-latency memory, 1.5 TB; deep tables 12-24 h each, so: full work queues ready before each fill, fold while held, one table per puzzle, numactl --interleave, 40 workers.
11-04 08:47: **four-stage certificate obl23pair-axis-2026-11-04** (user: lock in the results): 1ud and 2ud from bcert/ud34/snap, 3ax from ax-m3.tab, 4op from 4op-m1.tab (certbuild.sh; MANIFEST rebuilt for all four stages, as for P80).  certcheck --keep ud 1ud PASS (17) and 2ud PASS (18); axcheck 3ax PASS (23) and 4op PASS (22).  New independent check devtools/udindcheck.py (pypy, uses only the .tws puzzles, nothing from certcube.h): each line's start on 1ud-si/2ud-si.tws, other obliques kept, 2ud start, coverage bitmap under the 8 symmetries (rotation images cross-checked against renamed lines): 1ud 112,911,876 of 112,911,876 states, 0 twice, PASS (5.4 min); 2ud 23,474,025 of 23,474,025, 0 twice, PASS (2 min).  Negative tests on 2ud copies: a dropped line (8 states unmarked), a duplicated line (8 marked twice), a non-start line: all FAIL.  Copied to ~/orbitsolve-certsafe/obl23pair-axis-2026-11-04 (read-only, 60 files, SHA-256 match); checks + SHA256SUMS in certruns/cert-2026-11-04.  Not yet: an independent check for 3ax/4op; a copy off this laptop (armory when home).
11-04 09:00: TODO.md item L: boundary passes on the GPU (Metal): core loop, first step (UD 34 validation against the CPU numbers), correctness and shared-machine rules; written as a handoff brief for a second session.
11-04 08:38: **UD 34 boundcheck PASS** (independent, --keep ud): 25,532,454,577 colourings tried, 0 not solved, frame errors 0; 42,244 A states with dA > 16, 0 without a block; 872 s, 1.2 GB.  So 1ud + 2ud <= 34 (dry run), and P <= 76 checked (UD 34 + 3ax/4op 42), P <= 75 covered (with 41).  Evidence ~/orbitsolve-certsafe/obl23pair-UD34-evidence-2026-11-04 (blocks, logs, boundx/boundcheck sources, snapshot SHA-256 = the 2026-11-04 certificate's 1ud/2ud stages, read-only).
11-04 09:05: arena-plan.md: Running Claude Code on arena via Remote Control (from the docs).
11-04 09:10: 41 axbcheck 1/50 sample (bnd/s41.txt, 28,059 blocks): blocks all valid: 65,173,930,310 positions tried, 0 not solved, 0 bad lines, 0 side effects, frame errors 0.  Step 3 FAILs as expected for a sample (21,177,590 3ax states with d3 > 19, 20,729,966 without a block in the sample).  5,252 s on 4 threads, peak 3.3 GB.  Full 41 check ~50x: ~18-20 h on 16 free cores.
11-04 09:12: **TODO L, GPU boundary check: UD 34 validated.**  devtools/gpu/gpubound.mm = boundcheck with check 4 on the GPU (Metal): B's target pieces tracked by position only (L0 solves A's state, so a piece at state facelet p sits at D0^-1(p) and line P carries it to M(p), M = DP o D0^-1, 40 bytes per line); one GPU thread per (block, bad B state) tries the lines in order, ranks (4-subset colex per orbit via ctz) and reads dB.  Checks 1-3 on the CPU as boundcheck (stage lines streamed in 512k-line chunks; --threads 2).  UD 34 on bcert/ud34 (snap + ud34.txt): 12,174,971,792 pairs (= boundx bad colourings), 25,532,454,577 colourings tried (= boundcheck exactly), 0 not solved, 42,244 A states with dA > 16, 0 without a block, PASS; **GPU check-4 loop 1.4 s (8.6 G pairs/s, 18 G tries/s)** vs boundcheck 872 s on 16 threads; whole run 112 s (table builds on 2 threads), peak 335 MB.  --cpu-sample 50 (106 blocks re-run the boundcheck way): 0 differ (also all 106 blocks of sample50.txt).  Negative tests (/var/tmp/orbitsolve/gpubnd/neg.sh): drop line 1 / line 9 of block 0 -> FAIL (2 / 3 colourings left); drop block 0 -> FAIL (8 A states without a block); X = 33 on the 34 blocks -> FAIL (2,691,638,501 left in 5,251 blocks; 56.8 G pairs, 10 s on the GPU).  Drop line 20 of block 345 -> PASS, correctly (tries +47: a later line of that block covers the same states; boundx blocks are not minimal; the block was in the CPU sample, same counts).  Bug found and fixed in passing: the kernel used radix C(nF0,4) for the second group where the rank uses C(nF1,4) (equal on UD, 20/20, so no effect here).
11-04 09:18: devtools/gpu/gpuboundx.mm = boundx with both inner loops on the GPU: kernel 1 every (action, class): y = a(u), kept when |a| + dA(y) <= |L0| + 2 (979,282 actions); CPU builds paths a + line(y) (simplified, + symmetric copies of L0 fixing u), cheapest first, drops duplicates by M; kernel 2 every (bad B state, class) through the paths in order, marks the first that solves it.  Pass 2 (one action, --slack) for classes with states left; no two-action pass (boundx --deep never fired at UD 34: every class was covered by its first pass).  A table built in two passes so each state's stored line has length exactly dA (boundx stores srcLine racily).  Self-tests: symmetric copies of a random sequence vs conjugated facelet maps (48 syms); unrank vs rank on every B state.  UD 34 --sample 50: 106 of 106 covered, 243,775,408 bad colourings (= boundx), 1,293 alternatives per class (boundx 1,294), 76 s (73 s of it the table loads on 2 threads), peak 1.76 GB; gpubound --cpu-sample 1 on its blocks: checks 2 and 4 pass, CPU = GPU on all 106 (check 3 misses the unsampled classes, as expected).  Full UD 34 + check + UD 33 1/1000 sample chained (gpubnd/chain1.sh).
11-04 09:24: gpuboundx full UD 34 (gpubnd/x34.txt): 5,294 of 5,294 covered, 12,174,971,792 bad colourings (= boundx), 110 s total (class work: CPU paths 11 s, GPU check 1 s, GPU actions < 1 s; the rest is table loading on 2 threads) vs boundx 304 s on 5 threads; peak 2.0 GB.  gpubound --cpu-sample 50 on its blocks: **PASS** (25,447,779,900 tried, 0 left, 0 A states without a block; CPU = GPU on 106 blocks).
11-04 09:24: **UD 33 sample (gpuboundx --sample 1000, gpubnd/x33s1000.txt): 982 of 982 classes covered, 0 left**; 2,292,124,944 bad colourings (B states at 18: 2,299,768, at 17: 8,438,192); 6,223 alternatives per class; 126 of 982 classes needed pass 2 (one action, slack 5); 77 classes use >= 63 paths.  So UD 33 looks feasible.  Cost: ~38 s of class work for 982 classes, nearly all the CPU pass 2 (~979k candidate paths per pass-2 class) -> ~10 h at full size on 2 threads; peak 3.3 GB (pass-2 paths held move vectors).  Fix being tested: paths stored as (action, y, M) with moves rebuilt only for used paths, dedup by 64-bit hash of M, pass 2 in GPU-sized groups and 2 threads (old binary kept as gpuboundx.v1).
11-04 09:31: gpuboundx compact paths (UD 33 1/1000 sample again, x33s1000b): same result, 982 of 982 covered; peak 1.9 GB (was 3.3); class work ~20 s (GPU actions 4 s, CPU paths 12 s incl. pass 2).  gpubound --cpu-sample 20 on both samples' blocks: checks 2 and 4 pass, 5,191,199,977 tries each (identical), CPU = GPU on 50 blocks; check 3 FAILs only for the unsampled classes (7,844,800 - 7,836,952 = 7,848 = 982 x 8).  **Full UD 33 dry run started** 09:29 (gpubnd/x.sh x33 33, pid file gpubnd/x33.pid; 2 CPU threads + GPU; ~5.5 h expected; blocks ~1.3 GB in /var/tmp/orbitsolve/gpubnd/x33.txt).  gpubound now streams the boundary file in 32k-block chunks (UD 33 would have held ~3 GB); old source/binary in gpubnd/*.v1.  Queued (gpubnd/chain3.sh, waits on the pid): regression of the streaming gpubound on UD 34 (full PASS + 3 negatives, compare with the 09:12 counts), then gpubound --cpu-sample 1000 on the UD 33 blocks.
11-04 09:45: devtools/axindcheck.py (independent 3ax/4op check, pypy, only obl8ax/obl8op.tws): each line's start on the 8x8 puzzle, other centre orbits (CENTERS3, 4, 6, 8) kept, stage start (3ax: pair U and D home; 4op: also every side piece on its axis), coverage bitmap under the 16 stage symmetries (y and x2 via the puzzle's rotation moves with a per-orbit colour relabelling, times the L/R mirror by renaming; cross-checked against renamed lines).  First run stopped at its own consistency check (one relabelling for all orbits; the 3ax pair labels F and B alike): fixed to per orbit.  On the saved certificate: 4op 24,010,000 of 24,010,000 states, 0 twice, PASS (1.5 min); 3ax 165,636,900 of 165,636,900, 0 twice, PASS (6 min).  Negative tests on 4op copies (dropped line: 16 unmarked; duplicate: 16 twice; a line moving another orbit): all FAIL.  All four independent checks being rerun on the read-only certsafe copy, outputs to certruns/cert-2026-11-04/ind-*.chk.
11-04 09:44: all four independent checks PASS on the read-only certsafe copy (certruns/cert-2026-11-04/ind-*.chk; checker sources copied there): 1ud 112,911,876, 2ud 23,474,025, 3ax 165,636,900, 4op 24,010,000 states, each exactly once, 0 problems.  Methods: methods.md checkers list updated.
11-04 09:50: interim 2ud fold (r2ud-int; copy of r2ud-si2 + 13-move tail finds so far + 2 pulls): 4,058 spliced + 18,496 pulled; certcheck PASS; 2ud 18s 287,575 -> 272,730.  The 13-move tails were made from fold 1's stage and went stale (fold 2 + pulls replaced most 18s), so tails13 stopped early (STOP); trim13.sh keeps only the searched tails in tails13/g.txt (g-full.txt = all) so the loop's mktails won't skip unsearched ones; the loop then folds and makes fresh tails from the current stage.
11-04 09:50: UD 33 full run at 53,248 / 981,333 classes: **6 not covered** after pass 2 (one action, slack 5), 14,241 (27%) went to pass 2 (sample: 13%); CPU paths 1,093 s vs GPU 41 s, so pass 2 on the CPU is the bottleneck (~6 h total); blocks ~2.5 GB at this rate (135 MB so far).  UD 33 not yet shown feasible: the uncovered classes need two-action paths (boundx --deep), not yet on the GPU; next code.
11-04 10:25: boundcheck.md written (user): objects, the claim, the bijection lemma (positions with A state u <-> B states via L), checks 1-4, proof, conservative shortcuts, what it trusts (certcube.h; covered by the .tws checks, the frame check, the GPU re-implementation), negative tests, UD 34, axbcheck.  New boundcheck negative tests on UD 34 (bcert/ud34neg/neg.sh, 40 blocks): base 0 bad blocks; l0only: block 0 with 2,299,768 colourings not solved (= 2ud positions at 18); badmove: 1 line not solving its block's state, block rejected.  Linked from methods.md.
11-04 10:25: **Clean CPU vs GPU boundary-check benchmark** (user; gpubnd/bench.sh): the 2ud twsearch workers and the UD 33 gpuboundx paused with kill -STOP 10:10:40-10:24:23 (machine 93% idle), resumed with kill -CONT by an EXIT trap.  UD 34 (bcert/ud34 snap + ud34.txt, --keep ud, 16 threads):
- CPU boundcheck: PASS, 25,532,454,577 tries, **378.4 s** wall (5,648 s CPU), peak 1.29 GB.
- gpubound (streaming version; also its regression: same counts as boundcheck and gpubound.v1): PASS, **6.9 s** wall (40 s CPU), peak 0.45 GB; stage B 0.7 s, stage A 3.4 s, GPU check 4 1.3 s (9.2 G pairs/s).  55x on the whole check.
- Same check-4 loop, CPU vs GPU in one run (gpubound --cpu-sample 1: every block also the boundcheck way): CPU 423.6 s on 16 threads vs GPU 1.3 s (**~325x**), 5,294 of 5,294 blocks identical; total 430 s.
Note: the other session had STOPped tails13 at 09:49 and sent SIGTERM to its 15 workers while they were paused; a stopped process acts on SIGTERM only when continued, so they exited at the CONT (10:24:23; partial .p.log kept, failed.txt lists them, loop.sh went on to fold round 3).  Not caused by the pause, only delayed by it.  Pre-existing stopped processes (orbitbound31, 24 days; foldloop9.sh, 2 days) left alone.
11-04 11:02: devtools/gpu/gpuaxbcheck.mm = axbcheck with check 4 on the GPU (a 4op state = 4 masks of 8, per orbit F/B and L/R axis; a line = 4 x 8 targets, M = D_P o D_L^-1; rank by a 256-entry table; stage lines and blocks streamed; checks 1-3 on the CPU as axbcheck; --cpu-sample N re-runs every Nth block the axbcheck way).  **AX 42 (bnd/x42m.txt on snap-42): PASS, 383,406,031,310 positions tried = axbcheck exactly, 0 of 5,590,944 3ax states without a block; 120 s on 2 threads (GPU loop 52 s) vs axbcheck 19,046 s on 14 threads; peak 0.46 GB**; CPU = GPU on 71 sampled blocks.  Negative: drop line 1 of block 0 -> FAIL (2 left).  41 sample first stopped on a 32-bit guard (a wide-pass block's states x lines >= 2^32); tries now counted in 64 bits (low word + carries) in gpuaxbcheck and gpubound.  UD 33 gpuboundx paused (STOP) during these runs to keep to 2 CPU threads.  Running (gpubnd/ax41.sh): 41 sample, drop-block, gpubound UD 34 regression, merge x41m (x41a minus its 1,563 left + x41b), full 41 check.
11-04 11:15: **AX 41 checked (gpuaxbcheck PASS)** on bnd/x41m.txt (merge41.py: x41a minus its 1,563 left classes + the 1,563 x41b blocks = 1,326,357 blocks, the axbx class count; 788 MB) with snap-41: 2,712,178,633,745 positions tried, 0 not solved, 0 bad blocks, frame errors 0; 21,177,590 3ax states with d3 > 19, 0 without a block; 557 s on 2 threads + GPU (GPU loop 396 s, 3.0 G pairs/s), peak 0.69 GB; CPU = GPU on 266 sampled blocks (every 5000th).  Before it, on the same binary: 41 sample (s41.txt) 65,173,930,310 tries and 20,729,966 missing = axbcheck exactly; AX 42 drop block 0 -> FAIL (16 3ax states without a block); gpubound UD 34 regression PASS (25,532,454,577).  **So P <= 75 = UD 34 + AX 41 checked (dry run)**, by the GPU checkers (gpuaxbcheck matched axbcheck exactly on all of 42 and the 41 sample; a full CPU axbcheck of 41 has not been run).  Scoreboard: scoreboard.md.
11-04 11:24: devtools/gpu/gpuaxbx.mm = axbx on the GPU (action kernel on 3ax states as two 16-bit masks, d3 lookup; check kernel on 4op masks; the --deep two-action pass = the action kernel run on every a1(u); CPU builds paths with axbx's simplify, dedups by M; --only/--left as axbx; lines/tables as gpuboundx, 3ax source lines in two passes).  Validation, AX 41 --sample 1000 --prop 8 --deep 2, 2 threads each: axbx 1,326 of 1,327 covered, 1,184,552,120 bad positions, 3 left (46 s); **gpuaxbx: identical** (same counts, same uncovered class; 208 vs 215 alternatives per class: dedup by M), 86 s (mostly table loads), peak 2.0 GB.  gpuaxbcheck on gpuaxbx's blocks: every line valid, exactly that class with its 3 positions not solved, CPU = GPU on 27 blocks (check 3 FAILs for the unsampled classes, as expected).  **AX 40 started** 11:23 (gpubnd/run40g.sh: cert40 stages = ax-m3 + 4op-m1; broad --prop 8 --deep 2, then --only passes 10/4/4, 10/5/5, 11/6/6 as run40.sh; merge (mergeN.py) + gpuaxbcheck), outputs bnd/g40*.  UD 33 gpuboundx paused (STOP) for it (2-thread rule); it resumes when run40g.sh exits.
11-04 11:28: user: up to 8 CPU threads is fine.  AX 40 restarted with --threads 6 (run40g.sh no longer pauses UD 33; UD 33 resumed 11:27 with its 2 threads).  The first AX 40 start (2 threads, 11:23-11:27) was killed after 4 min (185k classes, 491 not covered so far); killing its gpuaxbx first let the script start pass b on the partial .left before the script died: that orphan was killed too.  Lesson: stop a chain script before its child.
11-04 12:03: AX 40 broad pass (gpuaxbx --prop 8 --deep 2, 6 threads, 11:27-11:58) **crashed** near the end: "left states overflow (8,422,496)" in the check of a late batch (the longest 3ax lines, 22-23, have up to 13.5M bad states each; 256 classes with many states left > the 8M buffer) -> exit(1) after 2,676,992 of 2,707,627 classes (blocks complete up to there; 7,145 classes left among them).  run40g.sh then started pass b on that partial .left; stopped (script first, then the child).  Fix (gpuaxbx): runCheck splits the classes in halves until the left states fit (no state changed by an overflowing run; nBad and the listed states updated only on success).  Resumed without redoing the 30 min: rest40.py lists the 30,635 classes with no block (2,676,992 + 30,635 = 2,707,627) -> run40g2.sh: broad pass on them, g40a-all = g40a + g40a2, then the --only passes, merge, gpuaxbcheck.  Broad pass so far: 2.68M classes in 30 min on 6 threads + GPU (CPU estimate was 10-17 h on 15), 7,145 left (0.27%).  Same overflow exit exists in gpuboundx (leftCap 4M); to fix there too.
11-04 12:46: **AX 40 covered and checked (dry run): P <= 74 = UD 34 + AX 40.**  gpuaxbx on bnd/cert40/stages (3ax ax-m3, 4op 4op-m1; diff -rq: identical to the obl23pair-axis-2026-11-04 certificate's 3ax/4op, as bcert/ud34/snap is to its 1ud/2ud): broad --prop 8 --deep 2 over 2,707,627 classes (2,676,992 in run40g.sh 31 min until the overflow crash + 30,635 in run40g2.sh 8 min, split-on-overflow fix) leaves 7,626; b --prop 10 --slack 4 --deep 4 (411 s) leaves 3 (253 positions); c --prop 10 --slack 5 --deep 5 (67 s) covers them.  Merge (mergeN.py): 2,700,001 + 7,623 + 3 = 2,707,627 blocks (bnd/g40m.txt).  **gpuaxbcheck PASS**: 4,027,742,501,772 (block, 4op state) pairs, 10,316,275,681,776 positions tried, 0 not solved, 0 bad blocks, frame errors 0; 43,257,446 3ax states with d3 > 18, 0 without a block; 1,788 s on 6 threads + GPU (GPU loop 1,519 s), peak 1.2 GB; CPU = GPU on 542 sampled blocks.  CPU estimate had been 10-17 h broad pass on 15 threads + ~5 days axbcheck.  Not yet: a CPU axbcheck of 40 (gpuaxbcheck = axbcheck exactly on 42 and the 41 sample); evidence copy.
11-04 12:49: AX 39 started (gpubnd/runXg.sh 39 = run40g2 generalised: broad --prop 8 --deep 2 on all 3ax lines > 17 (~5.07M classes), wider --only passes, merge, gpuaxbcheck; 6 threads + GPU; UD 33 keeps its 2).  gpuaxbx leftCap raised 8M -> 16M entries (one class of 23 at X = 39 has ~22M bad states).
11-04 14:40: **UD 33 covered and checked (dry run): P <= 73 = UD 33 + AX 40** (all four stages = the obl23pair-axis-2026-11-04 certificate).  gpuboundx (one action: pass 1 slack 2, pass 2 slack 5) over all 981,333 1ud classes of 16-17 (09:29-13:19, 2 threads + GPU, paused 11:23-11:27): 981,302 covered, 31 not (37 colourings, 1 each mostly); 2,301,510,019,192 bad colourings.  gpubound --left on its blocks: exactly those 31 blocks / 37 colourings not solved, 0 A states without a block (CPU = GPU on 982 blocks).  The new gpuboundx (--only --left --deep, split-on-overflow) on the 31: --slack 5 --deep 1 --deepprop 6 (16,404 actions of <= 6 moves for the two-action pass) covers all 31 (214 s).  Merge (mergeN.py): 981,302 + 31 = 981,333 blocks (gpubnd/x33m.txt).  **gpubound PASS** (--keep ud): 5,235,321,438,007 colourings tried, 0 not solved, 0 bad blocks, frame errors 0; 7,844,800 A states with dA > 15, 0 without a block; 2,709 s (2 threads + GPU shared with AX 39), CPU = GPU on 982 sampled blocks.  Not yet: a CPU boundcheck of 33 (gpubound = boundcheck exactly on UD 34); evidence copies.

- 2026-11-04 16:17 All 17 twsearch processes (2ud owner /ud2si + 16 workers) kill -STOP at the user's request: memory pressure, the shm table is in the compressor (41.8 GB stored in 17.6 GB, swap 8.5 of 9.2 GB used).  loop.sh left as is (waits on its workers).
11-04 16:14: evidence ~/orbitsolve-certsafe/obl23pair-P73-evidence-2026-11-04 (610 MB, read-only): UD 33 blocks (981,333) and AX 40 blocks (2,707,627) zstd -10 (round-trip cmp OK), search and check logs, GPU sources + certcube.h, merge/run scripts, README, SHA256SUMS.  gpuboundx pass 2 now raises the slack in steps 3..SL on the classes still left, only new actions each step (--nosteps: old behaviour); being tested on the UD 33 1/1000 sample (gpubnd/steptest.sh).
- 2026-11-04 16:18 Paused until the user sorts out GPU memory use.  Resume: kill -CONT the twsi4 twsearch pids (owner 93082 first).  The loop round r2ud-si3 is still in its tail search.
- 2026-11-04 Round 3 (loop.sh, fold r2ud-si3 from r2ud-si2 + tails/tails13): certcheck PASS; 2ud lengths 15:428,261 16:683,116 17:1,068,373 **18:264,376** (was 287,575).  Tails r3k12: 55,513 searched, 2,854 shortened (done).  r3k13: 26 of 62 chunks done when stopped.
- Killed all twsearch (workers kill -9, owner kill -INT so it removes /ud2si) and loop.sh at the user's request, to give the machine to the GPU agent; ~41 GB free afterwards.  Partial r3k13 chunks' claims released (.tmp outputs deleted), so a rerun of search on r3k13 does only the 36 missing chunks.
- opt2ud/fold4.sh: loop.sh's fold step only (no twsearch), PREV r2ud-si3, TDIRS + r3k12 + r3k13 -> /var/tmp/orbitsolve/r2ud-si4.  Log opt2ud/fold4.log.
- 2026-11-04 16:22 User: the GPU boundary work (other session) has P down to 73 (dry run; details in its own notes).
11-04 16:25: memory: the machine was compressing heavily (compressor 58 GB stored in 36 GB, swap 9.3 of 10.2 GB): the 2ud twsearch workers' shared 39 GB table mostly non-resident.  Mine: gpuaxbx 3.0 GB (1.2 GB tables malloc, 0.8 GB Metal buffers), gpubound 0.25 GB; gpuaxbx grew ~26 MB/min: Metal command buffers / encoders are autoreleased and the batch loop had no @autoreleasepool, so every finished command buffer (and buffers it references) lived to the end.  Fixed in gpuaxbx and gpuboundx (pool per batch; gpubound / gpuaxbcheck already had one per chunk); both give byte-identical blocks on the AX 41 / UD 33 samples.  User stopped and killed the twsearch workers (free 87%) and gave this session the machine: up to ~40 GB and all cores, watch swapping.  gpubnd/memwatch.sh logs free %, swap, compressor and the GPU tools' footprints every 5 min (memwatch.log).
11-04 16:25: gpuboundx stepped pass-2 slack tested on the UD 33 1/1000 sample: same coverage (982 of 982, same 2,292,124,944 bad colourings), CPU paths 12 s -> 1 s, pass-2 GPU actions 4 s -> 0; gpubound on its blocks: checks 2 and 4 pass, CPU = GPU on 50.  **UD 32 started** (gpubnd/runUD.sh 32 10: a: one action with stepped slack, b/c: --only --deep 1 --deepprop 6 / 8, merge, gpubound; 10 threads + GPU, next to AX 39's 6).
- 2026-11-04 16:26 Fold 4 (r2ud-si4; splices from r3k12 + 26 r3k13 chunks, pulls): certcheck PASS; 2ud lengths 15:464,138 16:686,832 17:1,048,127 **18:235,010** (was 264,376).  Frozen copy for the GPU agent: /var/tmp/orbitsolve/bcert/snap-2ud-r4 (stage aUbU..aDbD + certcheck.out; pair with 1ud from bcert/ud34/snap).  UD table work stopped here; machine to the GPU agent.
11-04 16:31: overnight (user: keep going all night; on each finished bound notify, update the markdown, start the next): gpubnd/axloop.sh 38 (after AX 39: AX 38, 37, ... with runXg.sh, 6 threads, while covered + gpuaxbcheck PASS) and gpubnd/udloop.sh 31 (after UD 32: UD 31, 30, ... with runUD.sh, 10 threads, while covered + gpubound PASS); after each PASS the merged blocks are zstd'ed (round-trip cmp) and the pass files deleted.  Deleted g40a.txt, g40a2.txt, g40a-all.txt (merged g40m.txt kept; compressed copy in the P73 evidence).  Disk 65 GB free.
- 2026-11-04 17:00 Small certificate (user request): devtools/smallcert.py (pypy; .tws simulation as the independent checkers): drops every line whose class is one stage move from a class with a shorter line.  On obl23pair-axis-2026-11-04 -> ~/orbitsolve-certsafe/obl23pair-axis-2026-11-04-small (205 MB, was 1.5 GB): 1ud 1,635,317 of 14,119,851 kept (11.6%), 2ud 98,843 of 2,936,935 (3.4%), 3ax 1,892,481 of 10,366,284 (18.3%), 4op 295,715 of 1,508,387 (19.6%); pass 1 coverage complete on all four.  Log certruns/small-2026-11-04/run.log.
- 2026-11-04 17:05 devtools/obsolve.cpp (user request: "proof is in the pudding" oblique solver): reads a (small) certificate, rebuilds all four dense stage tables in memory by BFS over the stage moves (same move counts as smallcert: 30 UD, 18 AX), then solves every oblique pair of an N^3 xcube (gullicube's brob, linked unmodified): 1ud+2ud per pair, then 3ax+4op per pair; plain mode plays table lines, --bound chooses the first stage's solution as boundx/axbx do (L0, symmetric copies, action + line within slack, two actions) for UD 33 / AX 40.  Checks: layout self-test on N, every stage goal re-read from the cube, phase checks, final all-obliques solved, chosen alternatives keep the other obliques.  Tests on the full certificate: N=8 plain (3 pairs, worst 68), N=10 --bound (6 pairs, worst 72); table rebuild ~6.5 min (8 threads).  Running N=40 --bound on the small certificate (171 pairs): certruns/small-2026-11-04/obsolve40.out.
- 2026-11-04 17:20 obsolve 40 --bound --seed 40 on the small certificate: 171 pairs solved, all checks pass; worst 72, mean 64.81 (means 1ud 13.56, 2ud 15.90, 3ax 17.13, 4op 18.22); 4 pairs needed a 3ax alternative (action + line), none a UD one; 0 over 33/40.  Time 261 s = table rebuild 232 s (8 threads; 1ud 103, 3ax 87, 2ud 24, 4op 18) + actions 28 s (1ud 9.45M at prop 10, 3ax 384k) + ~1 s for all 171 pairs (~6 ms per pair).
11-04 17:42: **UD 32 stopped: out of reach with these stage tables.**  gpuboundx (stepped slack, 10 threads) over the 1ud lines > 14 in file order: the first 1,482,199 classes (the 15s of w5-w7) took 31 min with 58 not covered; then w8, where the 974,441 16s are (mixed with 15s; at X = 32 a 16 faces the 13.8M 2ud states with d >= 17): 5,161 classes in 40 min, 3,411 not covered (~2/3 after one action), CPU paths 424 -> 2,559 s.  Extrapolated: ~600k classes needing the two-action pass.  The way to UD 32 is the tables (fewer 1ud 16s, lower 2ud 17s/18s), not the boundary search.  Killed runUD.sh 32 and udloop.sh (u32a.left: 3,459 classes so far).  Instead AX 38 started in parallel with AX 39's last passes (gpubnd/runXg2.sh 38 10 = runXg.sh with a thread count); axloop.sh replaced by axloop2.sh 37 (waits for 39 and 38, tidies each PASS, then AX 37, 36, ... on 14 threads while each PASSes).  AX 38: 3ax lines > 16, ~7.26M classes.
11-04 18:02: AX 39 broad pass done (5.4 h, 6 threads + GPU): 4,984,033 of 5,071,318 covered, 87,285 not (2,656,588,092 positions left; 16,597 classes with 1000+ left); 13,177,119,856,700 bad positions; pass b (--prop 10 --slack 4 --deep 4) on the 87,285 running.

### 2026-11-04: obsolve N=1000, bound 73 (`obsolve 1000 --seed 1000 --threads 8 --bound --ud 33 --ax 40`)
- 124,251 pairs solved; all checks passed (stage goals, U/D home after phase 1, all obliques solved at the end); 0 over 73.
- Worst 73 (194 pairs), mean 65.04. Totals: 48:2 49:4 50:6 51:17 52:29 53:90 54:154 55:302 56:622 57:1087 58:1785 59:2779 60:4394 61:6362 62:8620 63:11057 64:13363 65:14915 66:15204 67:14252 68:11616 69:8407 70:5265 71:2707 72:1018 73:194.
- Boundary alternatives needed: 2,927 pairs (2.36%): 1ud 863 (0.69%), 3ax 2,064 (1.66%); all of kind action + line (no symmetric-copy or two-action choices needed).
- Time 4143 s: 247 s table rebuild + 31 s actions + ~3865 s solving (~31 ms/pair, ~5x the N=40 estimate; cause not yet profiled, likely the O(N) slice moves). Peak RSS 5.9 GB.
11-04 18:42: AX 39 pass b (--prop 10 --slack 4 --deep 4) too slow: 8,448 of 87,285 classes in 30 min (1 not covered, 844 to the deep pass at ~2 s of GPU each: a1 x a2 over all 384,238 actions = 1.5e11 per class; GPU shared with AX 38's broad pass) -> ~5 h.  gpuaxbx --deepprop C2: the two-action pass uses only actions of <= C2 moves (prefix of the sorted actions; default C = unchanged).  New pass list (gpubnd/runXgP.sh X THREADS [WAITPID]): b --prop 10 --slack 4 --deep 4 --deepprop 8 (40,902 actions: ~100x less per deep class), c the same with all actions, d --slack 5 --deep 5, e --prop 11 --slack 6 --deep 6.  Killed the AX 39 chain + its pass b, the AX 38 chain script (its broad-pass gpuaxbx 48608 continues) and axloop2; restarted AX 39 from pass b on g39a.left (runXgP.sh 39 6 with a dead WAITPID), AX 38 = runXgP.sh 38 10 48608 (waits for the broad pass), axloop3.sh 37 (waits for both, then AX 37, ... on 14 threads).
11-04 19:05: experiment (user asked whether to resume 2ud twsearch to lower 2ud levels): 2,000 1ud 16-classes (every 487th 16 of w8) at X = 32, gpuboundx one-action passes, 4 threads: 2ud as is: 906 not covered (511 with 1-9 colourings left, 11 with 1000+); --cap-b 17 (every 2ud 18 treated as 17; blocks not valid): 870 not covered, i.e. lowering 2ud 18s barely helps (an 18 -> 17 is still bad for a 16 at X = 32); --cap-b 16 is trivial (16 + 16 = 32, no class needs a block).  The obstacle for UD 32 is the 8.4M 2ud states at 17 (or the 976k 1ud 16s: as 15s they behave like UD 33, all covered).  Neither is reachable by a night of twsearch tails, so the 2ud workers stay off (they also need the 40 GB table, which caused the memory pressure).  UD 32 by boundary search alone: ~440k classes (45% of 976k) to a two-action pass, est. 15+ h, uncertain; AX 38 is the better use tonight.
11-04 20:00: AX 39 pass c (--prop 10 --slack 4 --deep 4, all actions) covered 21 of 134 -> 113 left (pass d --slack 5 --deep 5 running).  Unattended overnight driver gpubnd/axnight.sh (user: keep going, set up to run overnight without internet) replaces axloop3.sh: caffeinate; for AX 39 then 38: wait for runXgP.sh, extra passes on whatever is left (--prop 11/12, slack/deep 7-8, --deepprop 9-10), merge all passes, gpuaxbcheck; if both PASS, AX 37, 36, ... on 14 threads; each result appended to journal.md and the 'Overnight log' section of scoreboard.md; PASSed blocks zstd'ed; stops below 15 GB free disk.  Output gpubnd/axnight.out.
11-04 20:20: **AX 39 covered**: broad 4,984,033 of 5,071,318; b (--deepprop 8) 87,151 of 87,285; c 21 of 134; d (--slack 5 --deep 5) 112 of 113; e (--prop 11 --slack 6 --deep 6) the last 1 (56 s).  Merge + gpuaxbcheck running (runXgP.sh 39); axnight.sh logs the result (P <= 72 on PASS) to journal.md and scoreboard.md.  gpuaxbx now prints a line per two-action class that takes >= 5 s (else at most one a minute), ending covered / NOT covered (user: progress every few minutes, not per item).
11-04 23:18 **AX 39 PASS** (gpuaxbcheck): blocks: 0 bad 41739412487793 positions tried, 0 not solved; frame errors 0; 3ax states with d3 > 17: 81041362, 0 without a block; CPU sample: 1015 blocks, 0 differ from the GPU  => P <= 72 with UD 33 (axnight.sh)
11-05 06:50: **P <= 72 checked (dry run) = UD 33 + AX 39**: AX 39 gpuaxbcheck PASS 11-04 23:18 (41,739,412,487,793 positions tried, 0 not solved; 81,041,362 3ax states with d3 > 17, 0 without a block; CPU = GPU on 1,015 blocks; 10,660 s); blocks bnd/g39m.txt.zst (1.68 GB); evidence ~/orbitsolve-certsafe/obl23pair-P72-evidence-2026-11-05 (blocks by APFS clone).  AX 38: broad pass still running (6.74M of 7.26M classes after 13 h, 611,969 not covered = 9%; its plain block file is 24 GB; disk 35 GB free).  So: gpuaxbx writes --out FILE.zst through zstd, gpuaxbcheck reads .zst (zstd -dc per pass over the file), mergeN.py reads and writes .zst; all tested (AX 41 sample: identical blocks; check of the .zst sample = 65,173,930,310 tries as before).  runXgP.sh 38 and axnight.sh (only waiting) replaced by gpubnd/ax38.sh: waits for the broad pass, passes b..h with .zst outputs, merge to g38m.txt.zst, gpuaxbcheck, notes; AX 37 not attempted (~9.5M classes, > 1 day, ~60 GB plain).  Deleted g40m.txt, x33m.txt, x33.txt, u32a.txt (compressed copies in the P73 evidence; u32a abandoned).
- 2026-11-05 17:36 New solver from the ~/fast session (user request): see fastsolve.md.  2ud tails without twsearch: all 62 r3k13 chunks (61,049 lines) in one run, 27 min table build + 19 min search; 4,134 shortened (107 to 11), all independently re-checked; it finds every twsearch find on the 26 done chunks plus 21 more; 36 new chunks: 2,307.  Results /var/tmp/orbitsolve/opt2ud/r3k13-fast (not folded; r3k13/out untouched).

### 2026-11-05: fastsolve r3k13 tails folded (fold 5)
- fastsolve (solver session, ~/fast/proto; see fastsolve.md) searched all 62 r3k13 chunks (61,049 lines) in ~27 min build + 1,125 s search, with no shm owner. It shortened 4,134 tails (107 to 11, 4,027 to 12), and all of them were re-verified.
- On the 26 chunks twsearch had finished: twsearch 1,806, fastsolve 1,827. fastsolve found every twsearch hit, plus 21 that twsearch reported as "No solution found in 12". This is probably twsearch pruning its fill on the lossy table. It means only missed shortenings; certificates are unaffected.
- Tail dir view /var/tmp/orbitsolve/opt2ud/r3k13f (chunks -> r3k13/chunks, out -> r3k13-fast logs; Solving counts match the chunk lines). fold5.sh = fold4.sh with r3k13f added and PREV r2ud-si4; output in /var/tmp/orbitsolve/r2ud-si5.
- 11-05: user: the GPU agent will likely finish P 71 tonight; resume 2ud tail work (with fastsolve) after that.
- Fold 5 (11-05 17:39-17:43): splice 3,367 classes improved, pull +21,246; certcheck PASS (max 18). Lengths 15:464,722 16:693,544 17:1,057,924 18:217,884 (fold 4: 18:235,010, so 17,126 fewer 18s).
- 11-05: twsearch c73aa20 (unpack fix) reviewed again. The r3k13 twsearch runs used twsi4, which has the fix: the owner /ud2si started 11-03 19:37 and read the file with the fixed unpack. So that bug does not explain fastsolve's 21 extra finds; fastsolve.md's guess (fill pruning on the lossy table) is still open. Runs before 11-03 12:54 that loaded a table from file (twsi2: opt3ax shm, hq23, 4op shm, early 1ud) can only have missed shortenings, and their 'No solution' results are not trustworthy.
- 11-05: user: the expected benefit of redoing the pre-fix runs (or chasing the 21) is small; not pursued.
- 11-05 17:55: **twsearch fill bug found (the cause of the 21 missed r3k13 solutions; upstream code, not ours).** In makeworkchunks (workchunks.cpp), fills (forpruning=1) on a tws with rotations (rotgroup > 1) drop a chunk whose state equals an earlier chunk's up to rotation (slowmodm2), but the two chunks can be in different canonical-sequence states. The canonical order is not rotation-equivariant: per axis it is F 2F 3F 3B 2B B, L 2L 3L 3R 2R R; y keeps it, y' and y2 reverse it. So part of the dropped chunk's subtree is never enumerated, and those states stay unfilled. They read too high, and the search prunes wrongly.
  - Test: /var/tmp/orbitsolve/twdedup. twsi4 vs. the same build without the fill dedup (nodedup/; condition `nodeduplicatesolve || forpruning` -> `nodeduplicatesolve`), both -M 2000 --startprunedepth 6 on 2ud-si.tws. The table files differ. New slots per depth, as-is vs. no dedup: d4 113,322 vs 113,475; d5 2,966,636 vs 3,000,960 (1.1% missing); d6 78,458,162 vs 79,518,313 (1.3% missing). Without dedup the fill enumerates ~4.4x more (d6 500.7M vs 114.9M; 26 s vs 5.5 s).
  - Affects every twsearch table we built on a tws with rotations (1ud-si, 2ud-si: y; obl8 3ax/4op: x y z). The effect is only missed shortenings; certificates are unaffected.
- 11-05 18:05: fill fix tested (user's suggestion): drop the canonical state machine in makeworkchunks for fills (`mask = forpruning ? 0 : canonmask[st]`), and start each chunk's fill DFS from canonical state 0 (fillstart: `filltable(pd, pt, togo, sp, 0)`). Build /var/tmp/orbitsolve/twdedup/nomask. Depth-6 fill, -M 2000, 2ud-si: new slots per depth equal the no-dedup fill (d4 113,475, d5 3,000,960, d6 79,518,313). The file is identical to the no-dedup file apart from 4 header bytes (66-69, a fill counter). Cost at d6: 205.3M enumerated in 10.6 s, vs 114.9M / 5.5 s as-is and 500.7M / 26 s with no dedup, so ~1.9x the buggy fill.
11-05 20:33 AX 38: broad pass did not finish (          2496459072  peak memory footprint) (ax38.sh)
11-05 20:40: **AX 38 broad pass stopped** at 7,236,352 of 7,263,089 classes (736,160 not covered so far; 3.04M to the deep pass; 19.9 h of run time, 4 h of it paused by kill -STOP for the user): "left states overflow for one class: raise leftCap" (gpuaxbx exit 1; the split-on-overflow halved a batch down to one class whose left states exceeded leftCap = 2^24).  Fix: gpuaxbx grows leftCap (and bLeftOut) to 1.25x the count for a single class and reruns it (a class has at most the 24,010,000 4op states, so <= ~240 MB); old binary kept as gpuaxbx.old.  g38a.txt / .left end cleanly (blank line / newline).  Resume: gpubnd/rest38.py lists the classes with no block in g38a.txt and not in g38a.left -> bnd/g38a-rest.only; gpubnd/ax38b.sh = broad pass on the rest (g38a2), appended to g38a.txt / .left, then ax38.sh's passes b..h, merge, gpuaxbcheck.
11-05 22:21 AX 38 broad pass on the rest: classes: 13749 covered, 12988 not (0 duplicate lines skipped) (ax38b.sh)
11-05 22:21 AX 38 broad pass (all):   749148 classes left (ax38b.sh)
11-06 07:05: gpuaxbx leftCap growth verified: the AX 38 rest pass hit one class with 16,898,571 left states (leftCap -> 21,123,213) and finished (13,749 covered, 12,988 not); broad pass total 749,148 of 7,263,089 classes left (10.3%).  Pass b (--deepprop 8) since 22:21: 363,264 of 749,148 classes after 8.6 h, 3,824 not covered (1.05%), 153,684 to the deep pass; ETA ~16:00.  Disk 51 GB free (APFS snapshots released).

### 2026-11-06: certsolve, the whole cube from the certificates
- devtools/certsolve.cpp (obsolve's oblique code + certrest.h) solves an entire N^3 (N >= 6) in the stated order: oblique pairs (1ud, 2ud for every pair, then 3ax, 4op; --bound for the 73 boundary choice), mids (odd N: none..UD, UD..LFRB), diagonals (none..D, D..U, UD..LFRB), corners (even N: optimal, full 88M-state table, max 11) or corners+midges (odd N: two-phase, face turns only, held to <= 20), then each wing orbit (4 + 4 + 5 + 11 wings).
- Certificates read: oblique small cert (rebuilt in memory), cert/diag/centres-first, cert/mid/centres-first, cert/wing/centres-kept (4.6 GB loaded in ~9 s, 16 threads). D/M lookups by colour class under the stage symmetries (lookup self-test at load); wings by position rank (coverage checked at load).
- Layout of each orbit into brobdicube's xcube found by move-equivariance on a reference cube, then checked on the real N by random sequences. Checks: every stage start and goal re-read from the cube; all centres after the centre stages; at the end every centre, wing, corner, midge and face centre.
- Runs (certruns/certsolve): N=7 and N=8 (obliques started solved, testing): 369 and 450-486 moves, bounds 590 and 869, 10-14 s. N=9 --bound (73), 2 cubes: 739 and 756 moves, bound 974; pairs worst 69; 177 s, 14.4 GB peak. N=10 plain: 993 moves, bound 1315; 135 s. All checks passed, nothing over a bound.
- The two-phase search returns its first solution within 20, so corners+midges came out at exactly 20 every time. That is fine for the bound, but a search for shorter solutions would cost little.

### 2026-11-06: concise certificates (target: everything <= 30 MB uncompressed, no text compression)
- Sizes before: obliques small cert 206 MB, mid 353 MB, diag 187 MB, wing 4.6 GB.
- **Wings: 142 KB.** cert/wing/concise = MANIFEST + actions.txt: the 5,332 algorithms wingbound used (wingbound8 = wingbound.cpp + ACTIONS=FILE). certsolve rebuilds the four stages by Dijkstra (worst 32 + 32 + 40 + 58 = 162, as before) in ~45 s on 16 threads, and the 8x8x8 solve is unchanged.
- Persistence (user: optional, default on): certsolve keeps rebuilt tables in --cache DIR (default ~/.cache/certsolve; our runs use /var/tmp/orbitsolve/certcache), named by a fingerprint of the certificate data they come from; --nopersist turns it off. Implemented for the oblique stages and wing stages. Second N=8 run: 3 s, same solve.
- Mid/diag: devtools/colconcise (relaxed small cert). Actions are the face turns plus the stage's lines of <= --act moves under all 48 symmetries and inverted. Dijkstra from the goal and the kept lines; lines are kept greedily, shortest first, until every state is <= the stage worst. First run: mid UD..LFRB, --act 6, 2,228 actions.
- Concise mid/diag results (colconcise --act 6, relaxed: every state <= the stage worst). Mid none..UD: 3,553 lines, 196 KB (was 147 MB; 12,348 actions, 975 s). Mid UD..LFRB: 373 lines, 20 KB (was 206 MB; 2,228 actions, 622 s). Diag none..D: 259 lines, 6 KB. Diag D..U: 211 lines, 9 KB. Diag UD..LFRB: 163 lines, 4 KB (was 187 MB; 3,830 actions, 656 s). Outputs in /var/tmp/orbitsolve/concise/{mid,diag}; logs in certruns/concise.
- Oblique 2ud (certsolve --makeconcise, actions = stage moves + lines <= 6 moves, 510 actions):
  - strict (every state <= its certificate length): 84,570 lines, 4.0 MB (was 98,843). Too little.
  - relaxed (<= 18): 12,749 lines, 723 KB; reload verified. The rebuilt distances shift up (18s: 7.9M vs 2.3M states), which matters for the 73 boundary pass.
  - The first strict run wrote no goal line, so its reload self-check failed; fixed.
- 11-06: user: the GPU agent has P 72, working on 71.
- 11-06 decision (user): the repo's certificates stay human-readable text, with git's own compression handling size. Obliques: the exact small certificate (215 MB raw, ~40 MB gzip estimate; largest file 31 MB). Mid/diag/wing: the concise forms (~450 KB raw, ~75 KB gzip). GitHub: a warning above 50 MB per file, rejected above 100 MB; repo < 1 GB recommended.
- 11-06: upperbound5.md: new section 'Published bounds for comparison' (4x4x4 53 SSTM, 5x5x5 130 OBTM (<= 260 SSTM), none found from 6x6x6 on; ours 217, 265, 503/495, 590/582, ...).
- 11-06: ~/old/drupal (Domain of the Cube dump to 2013-08) checked: 4x4x4 history only (nodes 54, 62, 73, 93); Jaap's 2005 4x4x4 centres <= 22 SSTM (node 30). Added to upperbound5.md.
- 11-06: added the nxnxn paper's centres bound 36n^2 - 144n + 144 (12 three-cycles x 12 moves per orbit) and the r/Cubers 8chfuu thread to upperbound5.md's comparison section.
- 11-06: devtools/rcube_worst.py N: the worst case of RCube's centre solve (row-by-row/1D model of our nxnxn paper: 15 two-face swaps x 16 substages; each active (row, substage) costs 5 + 2 per orbit served). The phases are independent (each phase's firings depend only on its colour's places). Per phase: firing sets enumerated (R 10,626 patterns, ..., W 70); at most 4 firings per orbit per phase (20 in all). Hard-to-reach substages: 10 per phase (12 in W), constant G_p by a subset DP. Results: n=4: 140, 5: 275, 6: 546, 7: 814, 8: 1218, 9: 1604, 10: 2136 (exact, search); 11-56: upper bound; n >= 58 even exact: Y(40Y + 1105), Y = n/2 - 1 (n=64: 72,695; 100: 150,185; 1000: 10,511,435). Asymptotically 10 n^2 = (5/6) x 2 per centre cubie. Check: the paper's simulated RCube1D averages (NxNxN_solver/summarized/maketable6.txt) are below these worst cases at every n.

## 2026-11-06 15:20 certsolve: concise mid/diag tables built 8x too slowly (fixed)

Paused the concise 9x9x9 test at the user's request, resumed it, then found why ColStage::build (certrest.h) was much slower than colconcise
on the same stage (diag UD..LFRB: 87 min against 655 s): it relaxed every action from every state, while colconcise skips an action
that would take a state past the stage worst. Most states sit near the worst, so most work was wasted. Fix: `if (nv > maxV) continue`
(maxV = longest kept line = stage worst under the relaxed rule; states left unreached are still reported as an error), the
level loop ends at maxV, and a progress line per level goes to stderr. Cached tables stay valid (same values). Backup:
/var/tmp/orbitsolve/certrest.h.bak-precap. Restarted the test (certruns/certsolve/n9-concise.out/.err).
Result (15:39): the concise 9x9x9 test passed. Both mid tables were built in 1105 s (8 and 9 min each; before the fix the first had not finished after
more than an hour). Rebuilt worsts equal the stage worsts: diag 10 + 16 + 18, mid 18 + 21; wing 32 + 32 + 40 + 58. With --solvedobl,
seed 9: cube 1 = 95 + 108 + 20 + 378 = 601 moves, cube 2 = 96 + 113 + 20 + 345 = 574; bound 995; over a bound 0. All five
concise mid/diag tables are now cached in /var/tmp/orbitsolve/certcache.

## 2026-11-06 late: validation of the publishable certificate set; repo staging

- certsolve with the small oblique certificate (obl23pair-axis-2026-11-04-small) and the concise mid/diag/wing certificates,
  plain mode (no --bound), all tables from /var/tmp/orbitsolve/certcache: 10x10x10, 50 cubes (seed 1010): most 1,075 (bound 1,315),
  pair totals 53..74; 11x11x11, 50 cubes (seed 1111): most 1,206 (bound 1,480); 16x16x16, 10 cubes (seed 1616): most 2,500
  (bound 3,133). 0 over a bound, 0 errors. Logs certruns/certsolve/val-n{10,11,16}.
- MANIFESTs written for cert/mid/concise and cert/diag/concise (stages, positions, worst, kept/all lines, the rebuild rule, file list).
- Repo staged in ~/orbitsolve/repo (no git yet): certificate/{oblique (the 11-04 small form, 206 MB: the P 72 boundary evidence was
  made with these stage files, so not the fold-5 2ud), mid, diagonal, wing, actions (gullicube's pair3/p8s10/p9/eg8/c12x pools for
  --bound)}, solver/ (certsolve + certrest.h, certcache.h, certcube.h, brob sources, Makefile). devtools/mkrepo.sh REPO refreshes
  solver/ and actions/ from devtools, rewriting certsolve's default paths to the repo's certificate/ (new certRoot(argv[0]) in
  certsolve.cpp: the parent of the program's directory). Builds cleanly with make. Test from an empty cache:
  certruns/certsolve/repo-n10 (10x10x10 --bound, 5 cubes, seed 2024).
- Repo, continued: devtools/mkrepo.sh now also copies 17 CPU tools (checkers, boundary search/check, axbound/opbound,
  certextend/improve/merge, colconcise, cornerbfs), the 4 Metal tools, the four .tws puzzles and udindcheck/axindcheck/smallcert/
  rcube_worst.py (puzzle paths rewritten to the script's own tws/), and writes tools/Makefile (make; make gpu). All 21 build
  cleanly in the repo (certrest.h: header functions now static inline; colconcise: two unused functions removed). README.md drafted
  (result polynomials and table, phases, 80 and 72, certificate, solver, tools, comparison, references). The boundary blocks
  (UD 33 243 MB, AX 39 1.68 GB zstd) are not in the repo; open question for the user: GitHub release assets (2 GB limit per file)?
- Repo test from an empty cache (repo/solver/certsolve, default paths only): 10x10x10 --bound, 5 cubes (seed 2024): tables 610 s in
  all (obliques 66 s, actions 22 s, diagonals 489 s, wings 33 s), RSS ~5.2 GB, cache 1.7 GB; most 1,032, bound 1,273 (= P 73: the
  --bound default was UD 33 + AX 40). Then --ax 39, 20 cubes (seed 3939): worst pair 71, most 1,059, bound 1,267, 0 over (22 s).
  certsolve's --ax default is now 39 (P 72); header comment renamed to certsolve; devtools and repo rebuilt.
- Repo test 11x11x11 --bound (P 72 default), 20 cubes (seed 1111), mid tables built from scratch (1165 s with the cached
  diagonals): worst pair 70, mean 64.84, most 1,186, bound 1,432, 0 over. Log certruns/certsolve/repo-n11.

## 2026-11-07 certsolve command line as gullicube / brobdicube

At the user's request (and: with -o every line of moves starts with a space; one output line per set solved at once):
certsolve N [options], N the first non-option argument; -r n, -R (50 N), -i (moves on stdin), -f (Kociemba facelets on
stdin), -F (print the start facelets), -M n (cubes; was --cubes), -t n (default all cores; was --threads), -o, -d (orbit
and pair-phase detail; previously always printed), --verify (gullicube's src/pair/cube.h sticker simulator; solved = every
face one colour), --seed; -D dir the certificate directory (default ../certificate beside the program as named, else
./certificate; replaces --cert/--certd/--certm/--certw and the mkrepo path rewriting). Usage on stderr with no arguments;
unknown options/arguments exit 2. Every move played on the cube is recorded; -o prints one line per pair phase
(1ud+2ud, 3ax+4op), per mid/diagonal/wing orbit and for the corners, in conventional names (gullicube's
conventionalMoves). Odd N whose face centres are turned (middle-slice scramble moves, or facelets): the cube is rebuilt
from its facelets with each colour named by its centre's face. ~/orbitsolve/certificate -> repo/certificate (devtools
default). mkrepo.sh now takes brob, the simulator and the pools from ~/gullicube (the public, portable copy).
Tests (all SOLVED by --verify): 7 -r 200 (centres turned) -o -d; 8 -i; 9 -F then -f; 10 random positions -M 3; repo
build from the repo root, 7 -R -o --verify. Backup: /var/tmp/orbitsolve/certsolve.cpp.bak-precli.
- Pools (user: not from gullicube; find them in the current dir or by the exe path): --bound's five pools (pair3, p8s10, p9,
  eg8, c12x.cls) are now looked for beside certsolve as named on the command line, else in the current directory (error
  naming the pool if neither). Copies live in devtools/ and repo/solver/ (mkrepo copies devtools' own); repo/certificate/
  actions moved out to /var/tmp/orbitsolve/repo-certificate-actions-old. Tested: repo root, solver/, a binary without pools
  (error), and the current-directory fallback. brob and the --verify simulator still come from ~/gullicube in mkrepo.
- Own copies (user): brob (xcube, facelets, util, cornersolver, wingsolver + mv.h, threecube.h, xorbit.h) in devtools/brob and
  the sticker simulator in devtools/sim/cube.h, copied once from ~/gullicube; mkrepo.sh copies only from devtools now (no
  gullicube path). devtools certsolve is built from them: c++ ... -Ibrob -Isim -o certsolve certsolve.cpp brob/*.cpp (the five).
  Comments and README name the copies (provenance only). Tests: devtools 9 -R --verify SOLVED; repo 10 --bound --verify SOLVED.
- certsolve --bound now prints "pairs searched (table lines over the budget): 1ud + 2ud a, 3ax + 4op b, of n". 10x10x10 timings
  with a warm cache: plain 2.3 s; --bound 22 s (about 20 s of it building the actions from the five pools); 0 + 1 of 6 searched.
  Boundary blocks (dry-run evidence): AX 39 10.3 GiB plain / 1.56 GiB zstd; UD 33 1.31 GiB / 232 MiB (zstd -l).

## 2026-11-07 boundary budgets below the proof; timing

- Budgets tighter than proved (--bound --ud U --ax X; a pair not brought within budget is played as found and counted over):
  20x20x20, 5 cubes (180 pairs), seed 69: P 69 as 32+37 (46 s; searched UD 9, AX 42), 33+36 (32 s; 2, 69), 31+38 (77 s; 28, 24);
  P 67 as 31+36 (82 s; 28, 77): all pairs within budget, 0 over. P 65 (30+35) stopped after 9 min (superseded by the timing plan).
- certsolve --timing: wall, instructions and cycles (proc_pid_rusage) for table loading, action building, each phase (all pairs),
  per pair split into searched / not searched (mean ms, M instr, M cycles; cycle median, 90%, 99%, max), the rest, the total.
  The search is single-threaded and pairs go one at a time, so process counters measure it. The action build is single-threaded
  (19.6 s, 224 G instr on 10x10x10).
- Timing series (user): /var/tmp/orbitsolve/timing100.sh -> certruns/timing/{plain, base, udNN, axNN}.out/.err: 100x100x100 seed 100,
  plain; --bound 35/45 (no searches); UD 34..30 with AX 45; AX 44..36 with UD 35; 30 min cap per run, a capped run ends that half.
  The GPU agent may be running: compare cycles and instructions, not only wall.
11-07 08:42 AX 38 pass b (--prop 10 --slack 4 --deep 4 --deepprop 8): classes: 742118 covered, 7030 not (0 duplicate lines skipped) (ax38b.sh)
11-07 08:50: AX 38 pass b done 08:42: 742,118 covered, 7,030 not (by positions left: 1-9 5,076, 10-99 1,249, 100-999 504, 1000+ 201); 6.84e12 bad positions, 4,889,803 left; 123,657 s incl. the user's pauses; g38b.txt.zst 3.7 GB.  Pass c (--prop 10 --slack 4 --deep 4, all actions) on the 7,030 running.
- Timing series result (100x100x100, seed 100, 1,176 pairs; load average 2-3.6 throughout; per-run fixed costs: tables 1.9 s,
  --bound action build 19.4 s / 222 G instr / 71 G cycles single-threaded). Every run that finished had 0 pairs over budget.
  Plain (P 80): phase 1 16 ms, phase 2 12 ms in all (0.014 / 0.011 ms per pair, ~0.05 M cycles); whole run 1.98 s.
  UD (AX 45): 34: 0 searched; 33: 12 searched, 2.81 s each (10.2 G cycles), phase 33.8 s = 28.7 ms/pair; 32: 57, 2.21 s
  (8.1 G), 126 s = 107 ms/pair; 31: 206, 2.04 s (7.4 G), 420 s = 357 ms/pair; 30: hit the 30-min cap (exit 142), no result.
  AX (UD 35): 44, 43: 0 searched; 42: 1, 353 ms (0.82 G cycles), 0.32 ms/pair; 41: 6, 181 ms (0.51 G), 0.94 ms/pair; 40: 26,
  145 ms (0.47 G), 3.2 ms/pair; 39: 69, 134 ms (0.45 G), 7.9 ms/pair; 38: 158, 107 ms (0.36 G), 14.3 ms/pair; 37: 323,
  96 ms (0.33 G), 26.4 ms/pair; 36: 506, 87 ms (0.30 G), 37.4 ms/pair. Searched-pair tails: UD max 14-16 G cycles, AX max ~1 G.
  Not a proof, but on this sample every pair fit UD 31 and AX 36, i.e. 67.
- AX 35 (100x100x100, UD 35): hit the 30-min cap (exit 142). Diagnostic -d run, 10 min: 396 pairs done, 249 needed an
  alternative (all action + line), all within 35 (totals 32:75 33:78 34:75 35:68 at the top); ~1.5 s per pair, searched ones
  ~2.3 s (36: 87 ms). Rerunning AX 35 then 34 with a 90-min cap (certruns/timing/ax35, ax34).

### 2026-11-07 certsolve timing: AX 35 and 34

100x100x100 seed 100, --ud 35, 90-min cap. AX 35 finished: 691 of 1,176 pairs searched, phase 2 2,032 s, 7,257 G cycles; per searched pair median 245.5 M cycles, 90% 423 M, 99% 700 M, max 3,602 G (one pair ~1,000 s, half the phase). Worst pair 69, mean 62.06, 0 over. AX 34: exit 142 (90-min cap) inside phase 2, no result. Timing section added at the bottom of upperbound5.md (certruns/timing/*.out).
11-07 12:59: gpuaxbx --order random|long|file [--seed N] (user: random order for a predictable finish, longest first for a final run so a failure shows early); default random; the seed is printed.  Order does not change the result: AX 41 sample (--sample 1000) in all three orders: 1,327 blocks identical as a set, same .left (sha1).  Installed for AX 38 pass d onward (pass c, running, keeps file order); previous binary gpuaxbx.old2.  12:59: gpuaxbx (pass c, 5,632 of 7,030 classes) paused with kill -STOP for the user's car trip (no power).
11-07 21:07 AX 38 pass c (--prop 10 --slack 4 --deep 4): classes: 2007 covered, 5023 not (0 duplicate lines skipped) (ax38b.sh)
11-08 07:50: AX 38 pass c (all actions) 11-07 08:42 .. 21:07 (incl. a 6 h pause): 2,007 of 7,030 covered, 5,023 left (1-9 positions left: 3,508; 10-99: 973; 100-999: 373; 1000+: 169).  Pass d (--prop 10 --slack 5 --deep 5, order random, seed 5001255172353527665) since 21:07: 4,352 of 5,023 classes, 142 not covered (3.3%); ETA ~09:30, ~165 left for pass e.  Laptop on battery (89%).
11-08 08:51: AX 38 pass d at 4,864 of 5,023 classes, 159 not covered; gpuaxbx (pid 27162) paused with kill -STOP: laptop on battery at 28% (26 min left).
11-08 09:10: on AC again; AX 38 pass d resumed (gpuaxbx 27162).
11-08 09:20 AX 38 pass d (--prop 10 --slack 5 --deep 5): classes: 4859 covered, 164 not (0 duplicate lines skipped) (ax38b.sh)
11-08 09:25: AX 38 pass d (--slack 5 --deep 5) done 09:20: 4,859 of 5,023 covered, 164 left (1-9 positions left: 128, 10-99: 34, 1000+: 2; 4,631 positions left in all).  Pass e (--prop 11 --slack 6 --deep 6, 957,034 actions of cost <= 11) on the 164 running.
11-08 13:11 AX 38 pass e (--prop 11 --slack 6 --deep 6): classes: 162 covered, 2 not (0 duplicate lines skipped) (ax38b.sh)
11-08 13:12 AX 38 pass f (--prop 11 --slack 7 --deep 7 --deepprop 9): classes: 2 covered, 0 not (0 duplicate lines skipped) (ax38b.sh)
11-08 13:12 AX 38 covered (all classes); merge (ax38b.sh)
11-08 16:50: **AX 38 covered** (all 7,263,089 classes): broad 6,513,941 + b 742,118 + c 2,007 + d 4,859 + e (--prop 11 --slack 6 --deep 6, 3.9 h) 162 + f (--prop 11 --slack 7 --deep 7 --deepprop 9, 43 s) 2.  Pass e two-action time per class grows with line length: 17 moves avg 45 s, 19 84 s, 22 100 s (max 305 s).  Merged 13:17: g38m.txt.zst (7.9 GB, 7,263,089 blocks).  gpuaxbcheck (16 threads) since 13:17: 6,225,920 of 7,263,089 blocks after 12,114 s.

### 2026-11-08 twsearch: fill fix and -si fix committed locally (not pushed)

See twsearch.md "Upstream fixes ready".  One of the 21 missed r3k13 lines (depth-6 -M 2000 table, -s): fixed build found the 12-move solution in 283 s; upstream did not finish in 300 s (inconclusive).  A stray probe loop of mine ran a gearcubeextreme depth-9 fill for 5.5 h on 4 threads before the user asked; stopped.
11-08 23:52 **AX 38 PASS** (gpuaxbcheck): blocks: 0 bad 147437136202996 positions tried, 0 not solved; frame errors 0; 3ax states with d3 > 16: 116080766, 0 without a block; CPU sample: 1453 blocks, 0 differ from the GPU  => P <= 71 with UD 33 (ax38b.sh)
11-08 23:52: **AX 38 PASS** (gpuaxbcheck, 38,092 s, 16 threads, 5.2 GB peak): check 2: 0 bad blocks, 665,099,923 lines, 32,559,228,706,061 (block, 4op state) pairs; check 4 (GPU): 147,437,136,202,996 positions tried, 0 not solved; frame errors 0; 3ax: 10,366,284 lines, 165,636,900 states; 116,080,766 3ax states with d3 > 16, 0 without a block; CPU sample: 1,453 blocks, 0 differ from the GPU.  **=> P <= 71 checked (dry run) = UD 33 + AX 38.**
11-08 23:58: **Bug: ax38b.sh deleted the merged AX 38 blocks.**  Its after-PASS tidy `rm -f $B/g38a.txt $B/g38[b-z].txt.zst` meant to delete the pass files but [b-z] also matches g38m.txt.zst (the merged 7.9 GB blocks): all AX 38 blocks are gone from bnd/.  Fixed the glob to [b-h] in ax38.sh / ax38b.sh (the axloop/axnight tidies kept m: they delete plain g<X>[a-z].txt after compressing m).  Recovery: local APFS snapshots of 16:03..23:03 (after the merge at 13:17) should hold g38m.txt.zst; mounting one needs root (mount_apfs: Operation not permitted) -> asked the user to mount com.apple.TimeMachine.2026-11-08-230332.local.  Evidence dir ~/orbitsolve-certsafe/obl23pair-P71-evidence-2026-11-08 has the logs, merge output, scripts and sources but not yet the blocks.  The snapshots expire ~24 h after they were taken and may be purged early under disk pressure (34 GB free): no large writes until recovered.
11-09: upperbound5.md promoted to **P 71 checked** (dry run, UD 33 + AX 38): Constants, Polynomials, comparison table (n=6..10: 494, 581, 842, 968, 1261), Status, Certificates, Open.  bnd/g38m.txt.zst is back (7.9 GB, restored from snapshot); not yet in the P71 evidence dir; repo README still says 72.
11-09: repo/README.md updated to P 71 (formulas (71 n^2 + 398 n - 992)/8 even, (71 n^2 + 412 n - 1715)/8 odd; table; AX 38 in the boundary section; RCube ratio at n = 1000 0.84 (rcube_worst 10,511,435 vs ours 8,843,777); certsolve defaults left at --ax 39).  Backup /var/tmp/orbitsolve/README.md.bak-pre71.
11-09 08:30: g38m.txt.zst restored by the user from the 11-08 23:03 local snapshot (Time Machine browser) into bnd/: 7,870,578,150 bytes and mtime 11-08 13:17 as before, zstd -t OK, 7,263,089 blocks.  Evidence ~/orbitsolve-certsafe/obl23pair-P71-evidence-2026-11-08 complete: ax38-blocks.txt.zst (APFS clone; sha256 d59c856b...), check / merge / pass logs, ax38.sh, ax38b.sh, rest38.py, mergeN.py, gpuaxbx.mm, gpuaxbcheck.mm, certcube.h, README.txt, SHA256SUMS.
11-09 09:00: wrap-up (user: the other agent pulls the sources into a distributable repo; large data files not kept): devtools/gpu/BOUNDARY-GPU.md documents gpuboundx, gpubound, gpuaxbx, gpuaxbcheck (claim, options, multi-pass recipe with the AX 38 numbers, checks, speed, pitfalls); devtools/README.md rows for all four + build line; mergeN.py copied to devtools/gpu; gpuboundx usage now lists --nosteps.  All four build with no warnings.
11-09 10:05: **Pair order experiment** (user's idea: solve the easy oblique pairs first; other pairs' solves move an unsolved pair's pieces, so a hard pair may become easy).  New devtools/certorder.cpp = certsolve.cpp + --order fixed|random|greedy|maxfirst (plain sum only; certsolve unchanged; built against repo/solver's brob/sim).  greedy: in each phase solve next the pair with the least phase cost now (1ud + 2ud, or 3ax + 4op, by table lines), re-costing every unsolved pair after each pair; the predicted cost is checked against what is played.  Same cubes in every order (-F facelets md5 equal); whole-cube greedy solves at N = 12 pass --verify.  First run (gpubnd-free machine but the repo session's certsolve on ~7 cores, load ~20-54), obliques only, seed 1109, mean / worst moves per pair:
  N=12 (10 pairs, 200 cubes): fixed 65.30 / 77, random 65.23 / 73, greedy 62.24 / 75, maxfirst 67.51 / 76
  N=20 (36 pairs, 40 cubes): fixed 65.26 / 74, random 65.25 / 76, greedy 60.20 / 72, maxfirst 68.90 / 76
  N=40 (171 pairs, 6 cubes): fixed 65.23 / 74, random 65.13 / 74, greedy 57.64 / 71, maxfirst 70.26 / 76
  N=100 (1176 pairs, 1 cube): fixed 65.29 / 75, random 65.22 / 74, greedy 54.70 / 67 (-16%), maxfirst 71.62 / 77
Costing time (1 thread): N=40 0.75 s, N=100 5.8 s (n^2 pair costs: 692,076 per phase at N=100); on 8 threads N=100 2.5 s (identical choices).  Random order = fixed: the gain is the choice, not the reshuffle.
11-09 10:20: certorder, more cubes (seed 2209, obliques, mean moves per pair fixed -> greedy): N=12 (1000 cubes) 65.23 -> 62.29; N=20 (200) 65.15 -> 60.12; N=40 (30) 65.21 -> 57.61; N=100 (5) 65.18 -> 54.74 (worst 75 -> 69).  Whole cube (seed 3309): N=20 3,840 -> 3,621; N=40 14,131 -> 12,876; N=100 84,330 -> 71,982 (-14.6%).  Costing all pairs is O(pairs^2) (~4 us per phase cost; N=100 2.4 s on 8 threads vs ~0.04 s for the oblique solve): --cand K samples K candidates per step: N=100 K=4 62.37, 16 59.53, 64 57.21, 256 55.50, all 54.74.  greedy1 (first stage's distance only, ~0.4 us per cost): K=16 63.57, 64 62.80, all 61.58 -- the second stage matters.  Write-up: devtools/CERTORDER.md.
11-09: **repo made a git repo** (~/orbitsolve/repo, branch main, commit 41a4b43, not pushed; .git 61 MB).  MIT LICENSE (Copyright 2026 Tomas Rokicki: name to confirm); no binaries (top-level Makefile builds bin/, .gitignore bin/ work/); pools moved to pools/ (certsolve looks in ../pools beside its directory first); new certsolve --expand DIR (writes the four oblique stages in full from the small form, 25 s cached; certcheck --keep ud and axcheck PASS on the result, same class counts, worsts 17/18/23/22); tools/mergeblocks.py (mergeN.py cleaned); boundary-demo.sh WORKDIR [--ud X]... [--ax X]... [-t T] (default UD 34, AX 42, AX 41 on the GPU).  Internal references (devtools/, ~/orbitsolve, certruns, armory, /var/tmp, methods.md, orbitbound) removed from sources and MANIFESTs; gpuaxbx.mm/gpuboundx.mm refreshed from devtools/gpu.  Demo run (09:31-09:46, 14.5 min): UD 34 5,272 classes, 1 pass, gpubound PASS 7 s; AX 42 245,171 classes (pass 1 leaves 95, pass 2 0), PASS 33 s; AX 41 1,073,807 classes (1,158 -> 1 -> 1 -> 0 over passes 1-4), PASS 242 s.  Fresh clone: make all gpu clean (22 binaries, tree clean), certsolve 7 -R --verify SOLVED (474 moves; 1023 s with the mid tables built), demo --ud 34 --ax 42 PASS.  Log certruns/repodemo-2026-11-09.log.  devtools/mkrepo.sh retired (backup /var/tmp/orbitsolve/mkrepo.sh.bak-20261109); pre-git repo backup /var/tmp/orbitsolve/repo-bak-20261109 (APFS clone).
11-09 10:25: certorder with the P <= 71 choice (--bound --ud 33 --ax 38; --order now allowed with --bound, costed by the plain table lines), seed 2209, obliques, 8 threads; searched pairs (1ud+2ud + 3ax+4op), phase 1 + 2 time, mean, worst, fixed -> greedy: N=12 (200 cubes) 8+269, 43.4 s, 64.26, 71 -> 5+98, 22.2 s, 61.98, 71; N=20 (50) 12+237, 53.8 s, 64.19 -> 6+48, 22.0 s, 59.94; N=40 (10) 10+221, 46.4 s, 64.27 -> 1+16, 4.3 s, 57.49, 68; N=100 (2) 20+325, 76.4 s, 64.17, 71 -> 0+7, 1.1 s (+1.5 s costing), 54.69, 67.  0 over a bound in all.  Action build ~18.6 s either way.  In devtools/CERTORDER.md.
11-09/10 09:15: **Stage-move-only actions** (user: the boundary actions never included single horizontal slices; try actions made only of stage moves, 1, 2, 3, ... of them, no pools).  Single-move actions everywhere (certsolve buildActs, boundx, axbx, gpuboundx, gpuaxbx) were the 18 outer turns only; the 12 1ud stage slices (U/D axis, depths 2, 3) appeared only inside pool algorithms (shortest: 4-move commutators).  New --stage-acts L in gpuboundx (30 1ud stage moves) and gpuaxbx (18 outer turns): every distinct effect of <= L stage moves, breadth first; no pools unless --base.
  UD 32 sample (the 2,000 1ud 16-classes s16.only, X = 32, one-action passes, 8 threads): pools + outer turns (979,282 actions, 11-04): 906 not covered (387 s); --stage-acts 1 (30): 2,000 not; 2 (579): 722; 3 (9,435): 3; 4 (148,429): **0 not covered** (27 s incl. ~15 s of table loading).  gpubound --keep ud on the L = 4 blocks: 0 bad blocks, 89,639,221,502 colourings tried, 0 not solved, CPU = GPU on 40 blocks (FAIL only for the classes outside the sample, as expected).
  AX 38 sample (--sample 2000: 3,632 classes): pools --prop 8 --deep 2 (40,902 actions): 366 not covered (91 s); --stage-acts 6 (all 4,095 face rotations: outer turns commute on centres): 2,123-2,390 not (slack 2-6).  AX needs the pools (slices would move other orbits' pieces off their faces).
  Bug found on the way: gpubound's buf() read 16 bytes from a null pointer when every block failed check 2 (my first check ran without --keep ud, so all 2,000 blocks failed on side effects) -> segfault; fixed (empty input -> zeroed buffer) in gpubound, gpuboundx, gpuaxbx, gpuaxbcheck (builds: *.new / gpubound.fix / gpuboundx.sa).
  Full UD 32 started 09:14: gpubnd/ud32s.sh (pass a --stage-acts 4 on all 4,087,739 classes with lines > 14, 12 threads; pass b --stage-acts 5 --deep 2 on what is left; merge; gpubound --keep ud).  With AX 38: P <= 70 if it passes.
11-10 09:31 UD 34 (stage moves) pass a (--stage-acts 4): classes: 5294 covered, 0 not (0 duplicate lines skipped) (udstage.sh 34)
11-10 09:31 UD 34 (stage moves) covered; merge and check (udstage.sh 34)
11-10 09:31 **UD 34 PASS** (gpubound --keep ud, stage-move blocks): blocks: 0 bad 25990741879 colourings tried, 0 not solved; frame errors 0; A states with dA > 16: 42244, 0 without a block; CPU sample: 2 blocks, 0 differ from the GPU  (udstage.sh 34)
11-10 09:33 UD 34 (stage moves) pass a (--stage-acts 4): classes: 5294 covered, 0 not (0 duplicate lines skipped) (udstage.sh 34)
11-10 09:33 UD 34 (stage moves) covered; merge and check (udstage.sh 34)
11-10 09:33 **UD 34 PASS** (gpubound --keep ud, stage-move blocks): blocks: 0 bad 25990741879 colourings tried, 0 not solved; frame errors 0; A states with dA > 16: 42244, 0 without a block; CPU sample: 2 blocks, 0 differ from the GPU  (udstage.sh 34)
11-10 09:35: udstage.sh (= ud32s.sh for any X) first run wrote UD 34 to gpubnd/.txt etc. ($Pa read as $Pa, not ${P}a); its check (PASS, kept as s34m-firstrun.chk) was on the right file, but UD 33 then overwrote it: killed, fixed to ${P}, reran.  **UD 34 with stage-move actions only: PASS** (pass a --stage-acts 4: 5,294 of 5,294 covered in ~1 min; gpubound --keep ud: 25,990,741,879 colourings tried, 0 not solved; 42,244 A states with dA > 16, 0 without a block).  UD 33 running (udstage.sh 33); UD 32 (ud32s.sh) paused with kill -STOP at 322,560 of 4,087,739 classes (6 not covered) until 33 is done.
11-10 10:00: **gpuboundx CPU path building sped up** (user: the CPU did more than the GPU; no malloc in anything performance-critical).  Profile of the UD 33 run (sample, 5 s): the per-path lambda (replaying each candidate path's ~20 moves on 48 facelets to get its map, after building a fresh move vector and the table line) ~14k samples, malloc/free ~6k, idle threads ~10k.  Changes: (1) each A line's facelet map precomputed once (14,119,851 lines x 48 = 678 MB, 2.1 s on 8 threads); a path's map = composition D[k] = fac_s[L[fac_s^-1[a2[a[k]]]]] (self-tested against the replay on 2,000 random states at startup); (2) its simplified length by simplify's own rule on a stack array (no vectors); (3) the per-class unordered_set of map hashes -> FlatSet (open addressing, reused across batches); (4) stable_sort -> stable counting sort by length into a per-thread scratch; (5) blocks written from a reused char buffer, used paths rebuilt on a stack array.  UD 32 sample (2,000 1ud 16s, --stage-acts 4): blocks and .left byte-identical to the old build; CPU paths 7 s -> 1 s; malloc ~6,000 -> ~300 samples (the rest: rare symmetric-copy paths, setup).  Now GPU-bound (GPU check 10-11 s of the sample, shared with the UD 33 run).  Build: devtools/gpu/gpuboundx.new (source gpuboundx.mm).
11-10 10:05: killed the paused UD 32 run (ud32s.sh, then its gpuboundx.sa at 322,560 of 4,087,739 classes, 6 not covered; partial blocks deleted, its log kept as gpubnd/s32a-old-binary.err); it restarts from scratch with gpuboundx.new after UD 33's check.  Its partial output was 257 MB for 322k classes, so all of UD 32 is ~3-4 GB of blocks, not 30-40 GB.
11-10 10:15 UD 33 (stage moves) pass a (--stage-acts 4): classes: 981333 covered, 0 not (0 duplicate lines skipped) (udstage.sh 33)
11-10 10:15 UD 33 (stage moves) covered; merge and check (udstage.sh 33)
11-10 10:24 **UD 33 PASS** (gpubound --keep ud, stage-move blocks): blocks: 0 bad 5257715382342 colourings tried, 0 not solved; frame errors 0; A states with dA > 15: 7844800, 0 without a block; CPU sample: 197 blocks, 0 differ from the GPU  (udstage.sh 33)
11-10 10:30: **UD 33 with stage-move actions only: PASS** (udstage.sh 33, old gpuboundx.sa build): pass a --stage-acts 4 covered all 981,333 classes (2,528 s, 6,188.6 alternatives per class; with pools 31 classes needed --deep); gpubound --keep ud (546 s): 0 bad blocks, 5,257,715,382,342 colourings tried, 0 not solved; 7,844,800 A states with dA > 15, 0 without a block; CPU = GPU on 197 blocks; blocks gpubnd/s33m.txt (1.03 GB).  Both validation levels pass.  UD 32 restarted 10:30 with gpuboundx.new (udstage.sh 32).
11-10 10:50: UD 31 sample (the 2,000 1ud 16s, X = 31, gpuboundx.new): --stage-acts 4: 1,886 covered, 114 not (98 with 1-9 colourings left, 14 10-99, 2 100-999; 136 s).  --stage-acts 5 --deep 2 (2,295,345 actions, deep pass over all of them) was killed by the system after 1,210 s: memory (memwatch 10:26: 36% free, swap 20.8 GB, compressor 37 GB); the deep pass's a1 x a2 records over 2.3M actions blow up -- a setting error (the deep pass needs --deepprop), not a bug.  UD 32 (3 GB) ran on through it.
11-10 11:10: UD 31 sample leftovers (the 114 classes --stage-acts 4 left at X = 31): --stage-acts 5 alone (2,295,345 actions, one-action passes): 113 covered, 1 not (100-999 colourings left); 73 s, 3.6 GB peak.  --stage-acts 4 --deep 2 --deepprop 4: no result after 2,206 s, **109.9 GB peak footprint** (swap).  Cause (gpuboundx deep pass): runActs appends every (a1, a2) record of a class into one vector and buildPaths makes a Path (64 B) for each, so memory ~ (a1 kept) x (a2 kept) -- 1e8-1e9 with stage actions.  To fix: the deep pass in bounded chunks of a1, re-checking the left states after each chunk and stopping when covered.  Meanwhile UD 32's driver (udstage.sh 32, whose pass b was --stage-acts 5 --deep 2) was stopped before pass a ended (its pass-a gpuboundx 42829 runs on) and replaced by gpubnd/ud32c.sh: waits for pass a, then --stage-acts 5, then 6, one-action passes only, merge, gpubound --keep ud.  UD 32 pass a at 11:05: 1,822,208 of 4,087,739 classes, 7 not covered (slowed by the 110 GB run's swapping).
11-10 11:45: gpuboundx deep pass now in chunks of a1 states (records <= recCap/4 per chunk; after each chunk the left states try its paths, unused paths dropped, stop when covered; dedup within a chunk and against the earlier passes only), source gpuboundx.mm, build gpuboundx.deep (not used by the running jobs).  Test on the 114 UD 31 leftovers (--stage-acts 4 --deep 2 --deepprop 4): RSS plateaus at ~9.0 GB by 105 s -- before the deep pass starts (the one-action passes on these hard classes); no runaway.  But the deep pass itself is impractical with stage actions (148k x 148k per class; no class finished in 8 min) and pointless (two 4-move stage actions = an 8-move stage sequence, which --stage-acts 5/6 reaches directly with dedup): stopped.  With stage actions use one-action passes with larger L, not --deep.
  UD 31 on the 5,294 1ud 17-classes (X = 31): --stage-acts 4: 4,153 covered, 1,141 not (1,003 with 1-9 colourings left, 138 10-99); 684 s, 4.3 GB.  L5 on the 1,141 running.
11-10 12:20: UD 31 on the 5,294 1ud 17-classes: --stage-acts 5 on the 1,141 left by L4: all covered (291 s, 7.0 GB).  Merged (4,153 + 1,141 blocks), gpubound --keep ud at X = 31: 0 bad blocks, 1,945,819,197,286 colourings tried, 0 not solved, CPU = GPU on 53 blocks (FAIL only for the 63.6M A states of classes outside the test, as expected).  So at X = 31 the hardest classes (17s) are covered by stage actions of <= 5 moves; the 16s sample needed L5 for 113 of 114 and has 1 left.  UD 32 pass a: 2,406,400 of 4,087,739, 7 not covered (12:15).
11-10 12:45: paused for the user's presentation: UD 32 pass a gpuboundx (pid 42829) kill -STOP at 2,823,680 of 4,087,739 classes (7 not covered); ud32c.sh only polls it.  Resume: kill -CONT 42829.
11-10 16:36: resumed UD 32 (gpuboundx 42829, time 42828, ud32c.sh 47717) and memwatch after the user's presentation.
11-10 16:40: queued gpubnd/ud31s.sh (waits for ud32c.sh 47717): UD 31 pass a --stage-acts 4 on all ~7.96M classes (1ud lines > 13), then --stage-acts 5, 6 on what is left, merge, gpubound --keep ud; with AX 38, P <= 69 on a PASS.  (L6 = ~30M actions: memory to watch.)
11-10 16:58 UD 32 (stage moves) pass a (--stage-acts 4): classes: 4087732 covered, 7 not (0 duplicate lines skipped) (ud32c.sh)
11-10 16:59 UD 32 (stage moves) pass b (--stage-acts 5): classes: 4 covered, 3 not (0 duplicate lines skipped); peak 3703846848 bytes (ud32c.sh)
11-10 17:01 UD 32 (stage moves) pass c (--stage-acts 6): classes: 0 covered, 3 not (0 duplicate lines skipped); peak 13051082688 bytes (ud32c.sh)
11-10 17:01 UD 32 (stage moves):        3 classes not covered (/var/tmp/orbitsolve/gpubnd/s32c.left) (ud32c.sh)
11-10 17:10: UD 32 stage-move passes left 3 classes (all 1ud 15s): L5 covered 4 of 7, L6 (34,988,067 actions, 13.0 GB) 0 of 3, the same 1,730 colourings left after L5 and L6.  Pass h on the 3 with --stage-acts 4 plus the pools (--prop 10, 9,599,869 actions; gpuboundx.deep build): all 3 covered (35 s, 5.0 GB).  So the pools give paths stage-move sequences cannot (the table lines themselves use non-stage slices).  UD 32 covered; gpubnd/ud32fin.sh merges a, b, c, h and runs gpubound --keep ud.  UD 31 (ud31s.sh) pass a started 17:02.
11-10 17:15: gpuboundx --used-acts FILE (build gpuboundx.ua; blocks byte-identical on the s16 sample): per action, how many used paths start with it.  UD 32 sample (2,000 16s, --stage-acts 4): 10,096 of 148,429 actions used (top: U, U2, U', L, 2U, D2 ~1,850-1,950 each).  Stopped UD 31 (ud31s.sh) after 11 min: 50,688 of 7,958,829 classes, 27 not covered, but slow (CPU paths 572 s of 660: ~77 classes/s, i.e. ~29 h at that rate); its partial files in gpubnd/old31/.  gpubnd/uduse.sh X: passes a --stage-acts 4, b --stage-acts 5, h --stage-acts 4 + pools on the rest, each with --used-acts u<X><pass>.acts; merge; gpubound --keep ud.  Running uduse.sh 32, then uduse.sh 31.
11-10 17:45: gpuboundx: (1) --only compares move bytes (no text formatting of 14M lines; a short string, no allocation); (2) batches pipelined: the batch loop split into prep (classes, action kernel on its own buffer set, CPU paths) and process (check, pass 2, deep, blocks); prep of batch n+1 runs on a thread while batch n is processed; per-batch data in a Batch struct (two, alternating).  Output byte-identical to the previous build on the UD 32 16s sample (X = 32), the 3,738 and 38,512-class UD 31 14s samples (X = 31).  Timing on 38,512 14s at X = 31 (machine shared with the UD 32 rerun and its check): 119 s old vs 121 s pipelined -- no gain: the run is GPU-bound (GPU check 53-62 s of ~80 s of work).  The check is near-inherent work (each bad B state x paths until one works, ~2.3 tries per pair; a random dB lookup each).  UD 31 estimate: ~6e13 (class, state) pairs, ~8-10 h of GPU alone.  Installed as gpuboundx.ua (previous: gpuboundx.ua-prev) for uduse.sh's later passes and UD 31.
11-10 20:05 **UD 32 PASS** (gpubound --keep ud; blocks: stage-move passes L4/L5/L6 + pools for the last 3 classes): blocks: 0 bad 63869344227297 colourings tried, 0 not solved; frame errors 0; A states with dA > 14: 32685262, 0 without a block; CPU sample: 818 blocks, 0 differ from the GPU  => P <= 70 with AX 38 (ud32fin.sh)
11-10 20:15: **P <= 70 checked (dry run)** = UD 32 + AX 38.  UD 32 gpubound --keep ud PASS (s32m.chk, 10,877 s, 2.65 GB RSS,
  shared GPU with the used-acts rerun): 4,087,739 blocks, 310,874,063 lines, 0 bad; check 4: 63,869,344,227,297 colourings
  tried, 0 not solved (1.88 G pairs/s); 32,685,262 1ud states with dA > 14, 0 without a block; CPU = GPU on 818 blocks.
  Blocks: stage moves only for all but 3 classes (those 3, all 15-move 1ud lines, need a pool sequence).  Evidence
  ~/orbitsolve-certsafe/obl23pair-P70-evidence-2026-11-10 (logs, scripts, sources, small passes; the 18 GB merged file
  is only hashed for now: disk at 14 GB free).
11-10 20:15: disk: 14 GB free on the data volume (hourly TM local snapshots pin deletions ~24 h).  Stopped the uduse.sh
  chain scripts (not its gpuboundx): its merge (+18 GB) would fill the disk, and its check repeats s32m's.  UD 31 not
  started: it needs gpuboundx/gpubound .zst I/O first (UD 31 blocks plain est. tens of GB).
11-10 20:25: gpuboundx / gpubound: .zst block files (as gpuaxbx / gpuaxbcheck): gpuboundx --out FILE.zst writes through
  zstd -10 -T2; gpubound reads FILE.zst through zstd -dc.  Test (UD 33, --stage-acts 3 --sample 4000): zstd -dc of the .zst
  output = the plain output byte for byte (246,905 -> 53,897 bytes).  Builds gpuboundx.zst, gpubound.zst.
  uduse2.sh X = uduse.sh with .zst blocks and a fourth pass i (--stage-acts 5 + pools).  u32rest.sh: waits for the UD 32
  used-actions pass a, compares u32a.txt with s32a.txt (sha256 7ed39921...), runs passes b, h for their used actions, then
  uduse2.sh 31.
11-10 20:40: P70 evidence ud32-blocks.txt.zst (2,831,826,980 bytes): zstd -dc sha256 = s32m.txt's 59b67d90...; SHA256SUMS written; gpubnd/s32m.txt deleted (disk).
11-10 20:16 UD 32 used-actions pass a: classes: 4087732 covered, 7 not (0 duplicate lines skipped); used actions: 136360 of 148429 (/var/tmp/orbitsolve/gpubnd/u32a.acts); u32a.txt identical to s32a.txt (sha256 7ed399211343808325e8eab502337b09b833b14943482c724a06a59ddda89891; gpuboundx.ua vs gpuboundx.new) (u32rest.sh)
11-10 20:50: UD 32 used actions (pass a rerun, gpuboundx.ua): blocks byte-identical to s32a.txt (sha256 7ed39921...), so the
  pipelined / allocation-free build changes nothing.  Used: 136,360 of the 148,429 L<=4 stage actions (first-fit: each bad
  state takes the cheapest path that solves it), 306,786,638 uses: length 1 all 30 (78.4M uses), length 2 all 549 (212.2M),
  length 3 all 8,856 (14.7M), length 4 126,925 of 139,994 (1.6M); 10,106 used once.  Concentrated: top 100 actions 41.6% of
  uses, top 1,000 95.4%, top 10,000 99.5%, top 100,000 99.97%.  First-fit overstates the set needed; a minimal set needs
  a restricted rerun (gpuboundx --acts-file, not written yet: grow the list until no class is left).
11-10 20:17 UD 32 used-actions pass b: classes: 4 covered, 3 not (0 duplicate lines skipped); used actions: 4802 of 2295345 (/var/tmp/orbitsolve/gpubnd/u32b.acts) (u32rest.sh)
11-10 20:17 UD 32 used-actions pass h: classes: 3 covered, 0 not (0 duplicate lines skipped); used actions: 272 of 9599869 (/var/tmp/orbitsolve/gpubnd/u32h.acts) (u32rest.sh)
11-10 21:00: UD 32 used actions, passes b / h (u32rest.sh): b (L5 on the 7) 4 covered, 4,802 of 2,295,345 actions used;
  h (L4 + pools on the 3) 3 covered, 272 of 9,599,869 used.  UD 31 (uduse2.sh 31, .zst blocks) pass a started 20:17.
11-10 21:00: gpuboundx --acts FILE [--acts-top N] (build gpuboundx.acts): the actions are the listed sequences (a
  --used-acts file, most used first) instead of the outer turns / stage actions.  UD 32 samples, top N of u32a.acts:
    --sample 20000 (205 classes): all 136,360: 205 covered (same as --stage-acts 4; blocks differ only in which of equal-
      cost paths is taken: the action order changed); top 300: 70 not covered; 1,000: 13; 3,000: 1 (1 colouring); 10,000
      and 30,000: 0.
    --sample 2000 (2,044 classes): top 3,000: 14 not covered (28 colourings); top 10,000: 0.
  So ~10,000 of the 148,429 L<=4 stage actions (not closed under symmetry) look enough for the L4 part of UD 32; a full
  run with --acts-top 10000 (plus pass b/h's few for the last 7 classes) would confirm.  Queued after UD 31.
11-10: certorder --order lazy (greedy on stale costs, one list per cost; user: lists, not an STL heap) and a cost-change histogram for greedy.  Every pair moves every step (face turns), and costs change by 0 only 22-29% of steps, +-1 35%, +-2 19-21%, +-3+ 17-23%, symmetric, nearly the same for pairs disjoint from the solved pair as for those sharing a slice index.  Lazy: N=40 (30 cubes) 64.02 / 74, N=100 (5) 63.87 / 73 (greedy 57.61 / 54.74; fixed 65.2), ~2.5 re-costs per step; worse than --cand 4.  Stale costs are worthless; the gain needs many fresh costs per step.  devtools/CERTORDER.md "Stale costs".  Backup /var/tmp/orbitsolve/certorder.cpp.bak-prelazy.
11-11 00:30: arena (idle, 40 threads), N=1000 whole cube, seed 1000, certorder (devtools copy built in ~/orbitsolve-cand on arena from repo HEAD), plain (80): fixed 65.20/76, 8,178,377 moves, 54 s wall (oblique solve ~45 s, ~0.17 ms per pair, 1 thread); greedy --cand 64 56.68/67, 7,119,909, 2:12; 256 54.46/67, 6,844,397, 4:40; 1024 52.66/70, 6,620,119, 6:06; 4096 51.19/67, 6,437,320, 12:25; 16384 50.08/66, 6,300,167, 22:07.  Costing on 40 threads only ~3M costs/s (~13 us per cost per thread; threads are spawned every step): a persistent pool + precomputed line permutations would help.  fixed --bound (72) running since 22:48, single-threaded searches.  Greedy K=64 plain with -d: phase 1 over 33: 0 (max 33); phase 2 over 39: 6, all among the last 8 pairs of the phase (max 42).  So with greedy, --bound searches only the last few pairs per phase.
11-11 00:50: UD 31 pass a (--stage-acts 4, file order) at 00:17: 5,117,440 of 7,958,829 classes in 4.0 h, 7,462 not
  covered, but the rate fell from ~800 classes/s (the 14s and early 15s) to ~19/s from ~5.05M on (1.4% -> 8% not covered;
  GPU check time ~80% of it); left-state overflows (leftCap 2^22) in ~40% of the batches, each rerunning the batch's whole
  check in halves (169 splits).  gpuboundx.grow: on overflow the left buffer grows to the exact count (the kernel counts
  every left state; up to 2^27) and the batch reruns once; --start N skips the first N classes (resume).  Test on 512
  uncovered UD 31 classes: blocks and .left byte-identical to gpuboundx.zst, 131 s vs 159 s (GPU shared).
  Stopped pass a at 00:24 (SIGTERM; the zstd stream closed cleanly, zstd -t OK), kept the blocks through the last class
  whose .left line was complete (trunc31.py: 5,121,551 blocks, 7,861 left, every left line a kept block's first line;
  --start 5121550 --first 1 gives that class: aligned).  u31res.sh: gpuboundx.grow --start 5121551 (00:47), join, then
  uduse2.sh 31 b (passes b, h, i, merge, gpubound.zst check), then u32top.sh.  Used actions of UD 31 pass a: second part only.
11-11 02:10: N=1000 --bound (72), seed 1000, arena: fixed 64.77/72, 8,125,156 moves, 6,479 s (searched 846 1ud/2ud at 5.4 s + 6,568 3ax/4op at 0.27 s, single-threaded); greedy --cand 64 56.67/66, 7,119,562, 188 s (searched 1 + 7); 256 54.46/66, 6,844,329, 324 s (0 + 6); 1024 52.66/67, 6,620,157, 414 s (1 + 6); 4096 51.19/65, 6,437,018, 792 s (0 + 7); 16384 50.08/66, 6,300,241, 1,380 s (0 + 9).  Greedy at 72 = greedy at 80 + ~20-60 s (action build ~19 s + a handful of searches); every greedy worst <= 67 under the 72 bound.  greedyall-plain running since 01:28.
11-11 03:40: N=1000 full greedy (every unsolved pair costed every step; 7.72e9 costs per phase, ~3.6M costs/s on 40 threads): plain 49.29/67, 6,202,536 moves, 1:05:48; --bound 49.29/67, 6,202,518, 1:06:49 (searched 0 + 7).  vs fixed 8,178,377 (-24.2%); K=16384 6,300,167 (within 1.6% of full greedy).  Series done; logs on arena ~/orbitsolve-cand/runs.
11-11 09:30: UD 31 pass a, second part (gpuboundx.grow, since 00:47): 1,008,128 of 2,837,278 classes at 08:45, 77,148 not
  covered (7.7%), ~42 classes/s (was ~19/s with splitting; 5 buffer growths, 0 splits).  ETA of pass a ~21:00 if the rate
  holds; ~215k classes to pass b.  Sample of 200 of its uncovered classes (every 300th of u31a1.left), --threads 4, GPU
  shared: b (--stage-acts 5) 183 covered (94 s, ~40 s of it setup); h (L4 + pools) 14 of the 17; i (L5 + pools) 1 of 3;
  the last 2: --slack 7 covers 1, --deep 2 --deepprop 6 (L4 + pools) both.  Added to uduse2.sh: pass j (h + --deep 2
  --deepprop 6) and k (h + --slack 7 --deep 3 --deepprop 8).
11-11 10:00: u32top.sh (UD 32 with the top 10,000 of u32a.acts, gpuboundx.grow) started now, sharing the GPU with UD 31 pass a, instead of after UD 31; u32top.sh exits at once if t32.err already has its result.
11-11: certsolve --expand of the repo certificate (P 80): 2ud 18s 286,263 (certified table 287,575), 1ud 17s 5,272 (5,294); 3ax 549 and 4op 14,965 unchanged.  Not an error: the concise form's rebuild gives every state at most its certificate length (actions + kept lines, propagated), so it can find shorter lines than the certified table had.  Expansion in /var/tmp/orbitsolve/optfs/full.
11-11 09:50: fastsolve ported to x86 for arena (devtools/fastsolve-x86: SIMDe, AES-NI aesMix, perf_event_open counters, FTSHM tables in /dev/shm, fill progress).  2ud-si test (2000 r3k13 lines, 34 GB, old binary on 4K pages) running: layers 1-7 match the laptop exactly; depth 8 at 135.7 s; depth 9 running.  User remounted /dev/shm 1400G huge=within_size, THP always, swap off, perf_event_paranoid 1.  Priority (user: arena CPUs ~3x slower per thread): deep tails of the stage tops (1ud 17s 5,272; 3ax 23s 549 after a 144-position layout; 4op 22s), not a 2ud sweep.
11-11 10:05: x86 fastsolve validated: 2000 r3k13 2ud tails (34 GB, E=7, D=9) on arena = laptop line by line (125 shortened: 2 to 11, 123 to 12; 1,875 none).  Arena (old binary, 4K pages): build 62 min (depth 9 1.09e11 leaves), search 0.026 s/tail (laptop 0.018).  probebench 200 GB, 40 threads, interleave: anon 4K 158 M probes/s, anon THP 476, /dev/shm huge 475 (ShmemHugePages populated).  Rerun with solve2 + FTSHM=/dev/shm/2ud-34 (huge pages) started 10:05.
11-11 10:15: fastsolve fill false sharing (perf annotate on arena: leaves[t]++ on a packed vector<long long>(nth) and its &4095 test = 87% of the fill lambda, ~40% of the build).  Fix (user: per-worker structs, alignas(64), no thread_local): FillStat per worker, Q / Solver / FullSolver alignas(64) (devtools/fastsolve-x86, arena solve3).  2ud 34 GB: depth 8 146 s -> 87 s (cyc/leaf 4,158 -> 1,943, instr/leaf 2,247 same); depth 9 26 -> 71 M leaves/s.  Huge pages confirmed: ShmemPmdMapped 33,202,176 of 33,203,120 kB of the table mapping.

### 2026-11-11: arena 2ud 34 GB rerun with huge pages + false-sharing fix (solve3)
Table built in 1618.9 s (first run, 4 KB pages, old binary: 3724.1 s; laptop 27 min). Depth 8 87.0 s (was 135.7 s), depth 9 about 71 M leaves/s.
Search on the 2000 tails: 46.30 s, 0.023 s/tail wall (was 0.026), avg 1.159M nodes, 3048 instr/node. Results identical line by line to the first arena run (and so to the laptop): 125 shortened.
Table saved in /dev/shm/2ud-34 (+ .ex, .meta); 33.2 GB in shmem huge pages. Load-path test started (runs/t2000L.*).
Load-path test: mapped /dev/shm/2ud-34 in 1.6 s; search 46.32 s, same 125 results.
11-11 12:07 UD 32 with the top 10,000 of u32a.acts only: classes: 4087099 covered, 640 not (0 duplicate lines skipped); bad colourings 17710036609176, left 5262; used actions: 10000 of 10000 (/var/tmp/orbitsolve/gpubnd/t32.acts) (u32top.sh)

### 2026-11-11: arena 1ud test build (34 GB, solve3), 1ud 17s tails
Tails of the 5,272 1ud 17-move lines (expanded stage none..aUbU, NF==17), generating form, deduplicated:
/var/tmp/orbitsolve/optfs/1ud/t13.txt 4,627, t14 4,681, t15 4,845 (copied to arena ~/fastsolve-x86/1ud/).
1ud-si: 64 symmetries; exact layers 1..7: 2, 28, 414, 7,664, 159,803, 3,537,574, 79,361,641.
Found by the user: ~5 min single-threaded after layer 7 = the serial exact-set insertion (83M states).
Fixed in devtools/fastsolve-x86 (exInsertPar, CAS; per-layer parFor with per-worker queues; "exact set" line
now timed and flushed); arena binary solve4. Matters for the big table (E=8: ~20x more states).
1ud t13 (4,627 tails, D=9 34 GB table, solve3): 8 shortened, 0.014 s/tail wall. t14 running (runs/1ud34-t14.*, 32 threads).

### 2026-11-11: x86 fill speed (devtools/fastsolve-x86, arena solve5)
Arena vs laptop per leaf: 2,247 vs 1,038 instr (2ud depth 8/9). -T on 1ud: move 211 vs 55 instr, canonical hash 1,166 vs 574.
Leaf canonicalization (1ud, sampled): 6.5 feature candidates, 1.1-1.4 left after the partial stage, ~1.1 distinct images.
Changes (x86 only; ARM path unchanged):
- native PSHUFB gather plans per move/symmetry (only the source chunks each output chunk reads), native sigma relabel;
- PMOVMSKB masks; eqSt via PTEST, movemask16/N native (SIMDe vmaxvq/vaddv are scalar loops on x86);
- partial canon stage: one gathered chunk as a raw 128-bit key (PCX env, default 1; 3 best on 1ud), no AES
  (user OK'd x86-specific hashes);
- exact-set insertion parallel.
1 GB -T fill, depth 7: 928 -> 741 cyc/leaf (-20%). Remaining cost is structural: x86 has no 64-byte TBL, so a
16-byte output chunk needs 2-3 PSHUFB (orbits of 24 straddle chunks). Hot functions checked in objdump: native code.

### 2026-11-11 ~13:50: four stages at once on arena (user's plan)
Two per socket, 300 GB table each in /dev/shm, 20 threads, numactl --cpunodebind/--membind: node 0 = 1ud + 3ax,
node 1 = 2ud + 4op.  Binary solve6 (x86 native kernels + 144-position support: KMAX 144, 8-bit positions without
orientation, keys up to 7 words, goFull<9>).  LAYERGB=40 MAXEST=1e12.  Inputs (generating tails, deduplicated;
laptop copies /var/tmp/orbitsolve/optfs/<stage>/):
1ud 17s: t14+t15 (9,526); 2ud 18s (286,263 lines): t13+t14 (42,146 + 47,501); 3ax 23s (549): t13..t16 (1,493);
4op 22s (14,965): t13..t15 (32,804).  Logs arena ~/fastsolve-x86/runs/<stage>-300.out/.err.
Killed before: 1ud 800 GB build (E stayed 7: the layer-8 BFS needs ~68 GB of candidates, 54 moves).
Corrected estimate: 1ud grows ~22x per depth; depth 10 from E=8 is ~2.4e12 leaves (~7.5 h on 40 threads).
obl8ax on the laptop (4 GB, D=7): 128 symmetries, layers 8, 73, 1057, 21294, 528578; 32 tails searched, no bad solutions.
11-11 14:10: UD 32 with only the top 10,000 of u32a.acts (u32top.sh, gpuboundx.grow, 11,166 s, GPU shared with UD 31):
  4,087,099 covered, 640 not (635 with 1-9 colourings left; 5,262 colourings in all); all 10,000 used.  The 640 with all
  148,429 L<=4 actions: 633 covered (7 not: the known 7), 18,605 actions used, 9,157 of them not in the top 10,000.  Adding
  the most used of those 9,157 helps little (+100: 7 of 640 covered, +300: 37, +1000: 143): the rare states need rare
  actions, and first-fit (cheapest path first) does not prefer the base set.  Next: a priority mode (paths from the base
  list tried before any extra action's) so that only the extras actually needed are used.
(The 13:50 launch above was stopped on the user's advice: short test runs first.)

### 2026-11-11 afternoon: test runs before the long arena runs
Shallow test (300 GB tables in /dev/shm, LAYERGB=4 MAXEST=1e9, 20 threads, two stages per socket):
- verified per process: own /dev/shm file, ShmemPmdMapped = whole 300 GB (huge pages), numastat all on its node.
- prefault 300 GB: ~108 s per process (kernel zeroing huge pages; once per table, reloads map in seconds).
- 1ud 60 t13: D=7, 0 shortened, 4.0 s/tail, 81M nodes avg, 2108 instr/node.  2ud 60 t13: D=8, 2 shortened,
  0.10 s/tail, 2.5M nodes.  3ax 20 t13: D=7, 0 shortened, 13.8 s/tail, 266M nodes, 2478 instr/node.
- BUG found: obl8op has 384 symmetries; Canon capped the list at 255 (uint8 ids), a non-group, so symmetric
  states could canonicalize differently (wrong table misses = bounds too high) and canon cost 128 full
  evaluations/state.  Fixed: uint16 symmetry ids, no cap (error over 1024), plus a startup self-check that canon
  is invariant under every symmetry (all four stages pass; obl8op now 1.32 evaluations/state, layer 1 = 4 states).
- Table fingerprint now includes FS_TABVER (x86v3 / arm1) so a /dev/shm table from a build with other hashing
  is never loaded.  Binary solve7.
Cross-check (exact distances, SLACK=0): per stage 20 tails of the worst lines, lengths 9-12 (5 each), on arena
(solve7, runs/<stage>-xc.*) and on the laptop ARM build (scratchpad xc/<stage>.lap.*); distances must agree.
Production runs: default SLACK (search to length-1, improvements only).
Cross-check results (exact distances, SLACK=0; arena solve7 x86 vs laptop ARM build, independent hashing):
- worst-line tails (5 each of lengths 9-12): 1ud, 2ud, 3ax 20/20 identical; 4op 14/20 compared so far, identical.
  Only one shorter: a 2ud 11-move tail at 10 (both).  All others optimal as given.
- random 11-move sequences (seed 20261111), arena distances: 1ud 5..11, 2ud 5..10, 3ax 4..11, 4op 7..11 (laptop
  comparison running).
- arena rates (shallow D=7/8 tables, 20 threads, two jobs per socket): search ~1.0M nodes/s/thread (1ud t13
  81M nodes avg, 2108 instr/node), laptop ~3.1M/thread; fill depth E+1: 1ud 1356, 3ax 1761, 2ud 1664, 4op 2562
  cyc/leaf (~24, 27, 27, 13 M leaves/s per job).
~14:20 production started (arena ~/fastsolve-x86/prod.sh): 300 GB, E=7, D=9 (MAXEST 1e12), default SLACK.
Inputs (shortest first): 1ud t14+t15+t16 (14,544), 2ud t13+t14 (89,647), 3ax t13..t16 (1,493), 4op t13..t15
(32,804).  Logs runs/<stage>-prod.*.  Earlier 1ud t13 at D=9 (34 GB, solve3): 8 of 4,627 shortened (runs/1ud34-t13.out).
Laptop vs arena: all 80 worst-line tails identical distances (4op complete too); random-set comparison running.
Fold tools (/var/tmp/orbitsolve/optfs): fold.py [--pairs F] LINES INPUT OUT... (shortest replacement per line over
all searched tails), vline.py TWS PAIRS (independent tws simulation: inverse(old) from Solved, then new must
return to Solved and be shorter).  1ud t13 (D=9 run): 9 of 5,272 1ud 17s -> 16, vline PASS
(optfs/1ud/t13-improved.txt, t13-pairs.txt).  Not yet folded into the certificate.
Random 11-move set, laptop ARM vs arena x86: 80/80 identical exact distances (all four stages).  Cross-check complete:
160 exact distances (4..12) identical, no bad solutions.
11-11 17:28 UD 31 pass a: 7828923 of 7,958,829 classes covered,   129906 not (second part: classes: 2715233 covered, 122045 not (0 duplicate lines skipped), 61188.44 s) (u31res.sh)
11-11 18:30: UD 31 pass a done 17:28: 7,828,923 of 7,958,829 classes covered, 129,906 not (second part 2,715,233 / 122,045,
  61,188 s).  Pass b (--stage-acts 5 on the 129,906) ran 50 min without finishing its first batch: one CPU thread at 100%,
  GPU idle (sample: pass 2's thread pool).  Cause: pass 2 checked the left states against the new paths on the CPU (meant
  for few states); here every class is hard (tens of thousands of left states x up to ~1M new paths per class, groups of
  recCap / 2.3M actions = 7 classes).  gpuboundx.p2: pass 2's check on the GPU (kernel check2: one thread per (class, left
  state), the class's new paths in the same order; the per-path test shared with check as solves()); path building and
  trimming stay on the CPU threads.  Byte-identical blocks and .left on the 200-class UD 31 pass b sample (76 s vs 94 s;
  CPU paths 18 s vs 39 s) and the UD 32 --sample 20000.  Pass b restarted with it (uduse2.sh 31 b, now on gpuboundx.p2).

### 2026-11-11 18:25 arena 3ax production check
3ax-prod (solve7, obl8ax, 300 GB) not stuck: table capped at D=8 by MAXEST (codes <=7 0.038%, 8 1.11%, rest unset; ~30x growth per depth).
Results so far: 373 13-move tails searched to 12, none shorter; 372 14-move tails searched to 13, 1 shorter (13).
Since 17:18 on tail 746 = first 15-move tail (search to 14), ~30x the depth-13 cost with a D=8 table -> roughly an hour per tail, ~2 weeks for t15.
All four solve7 runs have 20 threads on 40 logical CPUs (load 80), so each thread gets ~50% CPU.
18:35 killed 3ax-prod (user OK).  Restarted as prod2-3ax.sh: rebuild with MAXEST=1e13 so depth 9 fills (D=9), input
3ax/prod2.txt = prod.txt lines 746-1493 (15- and 16-move tails); logs runs/3ax-prod2.out/.err.  Depth 8 had 2.75e10
leaves in 1574 s (63 per state per level) -> depth 9 about 1.7e12 leaves, roughly 27 h at the current shared CPU,
faster once 1ud/2ud/4op finish.  1ud/2ud/4op left running.
11-11 18:50: pass b on gpuboundx.p2: first batch 129 s (the old build had not finished it in 50 min); 1,024 classes in 668 s, 847 not covered (the first ~7,861 classes are pass a's part-1 leftovers, the hardest; the 200-class sample from part 2 left 8.5%).
19:00 3ax-prod2: rebuilt to depth 8 in 1020 s (2.75e10 leaves, same as before); depth 9 fill started (code estimate 1.11e12, likely ~1.7e12) -> about 18 h at this rate.
22:33 2ud-prod finished: 89,647 tails (t13+t14) in 24,620 s (0.275 s/tail), 4,182 shortened.  fold.py on 2ud18-solving
(286,263): 5,597 lines improved (18->17: 5,535, ->16: 61, ->15: 1); vline PASS 5597/0 (optfs/2ud/prod-improved.txt,
prod-pairs.txt, prod.out).  Not folded into the certificate yet.
22:40 2ud continued: prod2-2ud.sh, reloaded /dev/shm/2ud-300 (D=9) in 1.6 s; input 2ud/prod2.txt = t15 60,529 + t16
136,279 (deduplicated first k moves of 2ud18-gen.txt; same method reproduces t13 exactly).  Logs runs/2ud-prod2.*.
Status 22:33: 1ud 6,468 of 14,544 (151 shortened); 4op 10,883 of 32,804 (94); 3ax depth 9 fill 38%, ~6 h left.
23:00 1ud results so far (t13 + 6,468 of prod t14..): fold.py 175 of 5,272 1ud 17s -> 16, vline PASS 175/0
(optfs/1ud/prod-improved.txt; inputs all.txt = t13+prod.txt, outs t13.out prod.out).
Folds started (user): /var/tmp/orbitsolve/fsfold/fsfold.sh STAGE NAME = copy expanded P 80 stage (optfs/full),
certmerge fold.py lines, pool of all fastsolve finds (pairlogcheck --emit 7 10), certextend, up to 3 --proptop 16
pulls, certcheck --keep ud.  dropend.py (old scratchpad) is gone, so no trailing-move drop.  Running 1ud f1 then
2ud f1 (logs fsfold/<st>-f1.log).
23:13 fsfold f1 results (both certcheck PASS; logs /var/tmp/orbitsolve/fsfold/<st>-f1.log, stages fsfold/<st>-f1/stages):
- 1ud: merge 175, certextend 5,786, pulls 3,314 + 11.  17s 5,272 -> 4,507; 16s 966,907 -> 960,247.  (6 min)
- 2ud: merge 5,597, pool 414 actions from 4,182 finds, certextend 107,760, pulls 89,150 + 224.
  18s 286,263 -> 208,982; 17s 1,049,362 -> 1,023,583; 16s 671,980 -> 751,071.  (2 min)
The pool (shortened tails as actions) does most of the work: it lowers ~14x more lines than the direct folds.
Not in the repo certificate.
23:25 2ud-prod2 (t15 in original order): 306 tails in ~60 min (~11 s/tail; node 1 shared with 4op, 40 threads on 20 CPUs),
53 shortened (17%).  Full t15 would take ~a week, so killed and replaced (prod3-2ud.sh): 15-move tails of the 208,982
lines still 18 after fold f1 (no new 13/14 tails exist: remaining 18s are all original lines), minus the 306 done,
ordered by how many 18s share the tail (5,474 tails cover >=10 lines each): 2ud/prod3.txt 45,482; runs/2ud-prod3.*.
prod2 output kept: optfs/2ud/prod2.out (inputs = first 306 of t15; check the last block is complete before folding).
11-12 00:20 overnight (user: keep arena generating results all night):
- 2ud prod3 (most-shared 15-tails first): 0 of 100 shortened, ~18 s/tail; prod2's first 306 (original order) hit 12%
  on still-18 lines in every sharing bucket.  Replaced by prod4-2ud.sh: same 45,382 tails shuffled (seed 20261112).
- 1ud replaced by prod2-1ud.sh: only the not-yet-searched 15/16-move tails of the 4,507 still-17 lines (6,687:
  15s first); prod.out had 6,823 complete blocks (optfs/1ud/prod-done.txt = those inputs).
- 4op results so far (11,890 tails, 103 shortened): fold.py 129 of 14,965 22s improved (->21: 120, ->20: 9), vline
  PASS.  Fold 4op-f1 (laptop): load = improved lines + all suffixes passing linecheck (252 of 1,699; lfilter.py =
  linecheck that prints good lines), opbound --rounds 0 --prop 10 --base opt4op/merge/4op-m1.tab (the P 80 4op).
00:50 **Bug: rotations as moves in 3ax/4op.**  obl8ax.tws and obl8op.tws contain Move x, y, z.  fastsolve drops a
rotation as a free symmetry only if it fixes the solved colouring exactly (true for y in 1ud-si/2ud-si: fine, no
rotations in their output, certcheck PASS).  In 3ax/4op none qualifies, so x/y/z (9 moves) are ordinary cost-1
moves: tables built over 63 moves, and solutions may use rotations, which certificates do not allow (linecheck:
"move another set").  4op prod so far: 103 shortened, 62 use y2 (invalid), 41 valid.  The laptop/arena
cross-check could not catch it (both builds share the semantics).  Fix: obl8ax-nr.tws / obl8op-nr.tws
(fsfold/, Move x/y/z blocks removed, 18 moves); laptop test on obl8op-nr running (fsfold/nrtest.*).
Fold 4op-f1 rerun with only rotation-free lines (50 improved lines; 93 valid suffixes = good2.txt).
01:05 fold 4op-f1 (rotation-free lines only: 50 improved + suffixes, good2.txt 93): opbound --rounds 0 --prop 10 on
4op-m1: **4op 22s 14,965 -> 14,822**, 21s 113,539 -> 113,492; replay 0 bad; axcheck PASS (22) on a copy
(fsfold/4op-f1/cert, from optfs/full stages/4op + axcertupdate.sh).  Table fsfold/4op-f1/4op-f1.tab.
obl8op-nr.tws: still 384 symmetries.
01:20 laptop test obl8op-nr (8 GB, D=8): the 10 tails with valid finds all found again, the 10 rotation-only finds now
"No solution" (spurious), 10 negatives unchanged, no rotations in output.  Arena: killed 3ax-prod2 (D=9 fill was
~55%, rotation moves) and 4op-prod (12,060 tails done; 62 rotation finds).  prodnr.sh 3ax 4op: rebuild both
300 GB tables on obl8ax-nr/obl8op-nr to D=9 (MAXEST 1e13).  Inputs 3ax/prod2.txt (748, unchanged); 4op/prod2.txt
31,960 = rotation-find tails + unsearched t14..t16 tails of 22s not improved by valid finds (13: 58, 14: 9,639,
15: 11,045, 16: 11,218).  Earlier 3ax/4op negatives stay valid (searched with a superset of moves).  Logs
runs/<st>-nr.*.  (The folded 4op l22 lines are new representatives, so tails must come from the original lines.)
01:30 3ax-nr/4op-nr builds running: exact layer 7 3ax 333,364,478 (was 438,169,327 with rotations), 4op 121,885,513 (was ~165M exact set); depth 8 started on both.
11-12 01:35 UD 31 (uduse2) pass b (--stage-acts 5): classes: 113064 covered, 16842 not (0 duplicate lines skipped); used actions: 1305810 of 2295345 (/var/tmp/orbitsolve/gpubnd/u31b.acts); 26031.49 s (uduse2.sh 31)
11-12 02:04 UD 31 (uduse2) pass h (--stage-acts 4 --prop 10 + pools): classes: 13046 covered, 3796 not (0 duplicate lines skipped); used actions: 272254 of 9599869 (/var/tmp/orbitsolve/gpubnd/u31h.acts); 1794.33 s (uduse2.sh 31)
02:15 fold f2 (all results to 02:00; both certcheck PASS; fsfold/<st>-f2):
- 1ud: 242 lines improved directly (vline PASS), pool 245 finds -> 17s 5,272 -> **4,268** (f1 4,507); 16s 958,932.
- 2ud: 5,846 direct (vline PASS), 4,272 finds -> 18s 286,263 -> **199,787** (f1 208,982); 17s 1,021,950.
Arena 02:00: 1ud-prod2 1,019 tails, 51 shortened (~7 s/tail); 2ud-prod4 576, 35 (~12.5 s/tail); 4op-nr depth 9 25%
(~5 h left), 3ax-nr depth 9 11% (~11.6 h left).
11-12 03:28 UD 31 (uduse2) pass i (--stage-acts 5 --prop 10 + pools): classes: 976 covered, 2820 not (0 duplicate lines skipped); used actions: 261117 of 11746201 (/var/tmp/orbitsolve/gpubnd/u31i.acts); 5007.54 s (uduse2.sh 31)
11-12 03:44 UD 31 (uduse2) pass j failed: time: signal: Invalid argument (uduse2.sh 31)
04:10 fold f3 (results to 04:00; vline PASS; certcheck PASS): 1ud 362 direct, 363 finds -> 17s **3,897**; 2ud 5,933
direct, 4,309 finds -> 18s **195,560**.  Arena 04:00: 1ud-prod2 2,105 tails / 169 found; 2ud-prod4 1,165 / 72;
4op-nr depth 9 56% (~2.8 h left); 3ax-nr depth 9 27% (~9 h left).
06:15 fold f4 (vline PASS; certcheck PASS): 1ud 395 direct (one 17 -> 15), 395 finds -> 17s **3,834**; 2ud 6,012
direct, 4,336 finds -> 18s **191,393**.  Arena 06:08: 1ud-prod2 2,402 / 201 (into the 16-move tails); 2ud-prod4
1,768 / 99; 4op-nr depth 9 90%; 3ax-nr 45% (~6.7 h left).
06:50 4op-nr table built in 23,548 s (depth 8 6.57e9 leaves 792 s, depth 9 2.20e11 leaves 23,485 s, 2,216 cyc/leaf); slots <=7 0.011%, 8 0.317%, 9 9.58%.  Search: first 207 tails (the 58 rotation-find tails first), 2 found, no rotations in output.
11-12 08:45: UD 31 passes after b: b (L5, 26,031 s) 113,064 covered, 16,842 not; h (L4 + pools, 1,794 s) 13,046 / 3,796;
  i (L5 + pools, 5,008 s) 976 / 2,820.  Pass j (L4 + pools --deep 2 --deepprop 6) killed by the OS at 03:44 (time: command
  terminated abnormally; peak memory footprint 93.7 GB) in its second batch; its first batch (256 classes) left only 2.
  Cause: memory grows with --batch: every class keeps all its pass-1 paths (with the pools, many per class), the GPU paths
  buffer holds the whole batch's (x2 capacity), and the pipeline holds two batches; peaks b 29.3 GB, i 20.2 GB, j 93.7 GB.
  Fix for now: passes j, k with --batch 32 (a class's result does not depend on its batch); restarted from j (uduse2.sh 31 j;
  the crashed j's files in gpubnd/crash).  To do in the code: drop unused pass-1 paths after the check (keep their hashes
  for the deep pass's dedup).
07:05 upperbound5.md: new Working tables section (1ud f4, 2ud f4, 3ax ax-m3, 4op f1), status line and Open item updated.
09:00 fold f5 / 4op f2 (results to 08:48; vline PASS; certcheck / axcheck PASS): 1ud 428 direct, 433 finds ->
17s **3,769**; 2ud 6,127 direct, 4,390 finds -> 18s **180,823**; 4op 56 rotation-free lines (prod-norot.out =
prod.out with the 64 rotation finds as misses; all.txt = first 12,060 of prod.txt + 4op-p2) -> 22s **14,789**.
fsfold4op.sh NAME scripts the 4op fold; ubtable.py 1UD 2UD 4OP rewrites the upperbound5.md Working tables.
Arena 08:48: 1ud-prod2 2,470 / 239 (16-move tails ~2.3 min each, ~half shortened); 2ud-prod4 2,563 / 153;
4op-nr 4,761 / 6 (t13 + t14, ~1.5 s/tail); 3ax-nr depth 9 69% (~3.7 h left).
11-12 10:15: gpuboundx memory (build gpuboundx.mem, installed as gpuboundx.p2 by rename for pass k; old copy
  gpubnd/gpuboundx.p2-old).  Measured on 64 of UD 31's pass-j classes (L4 + pools --deep 2 --deepprop 6, 4 threads):
  peak footprint 5.8 GB at --batch 1, 8.2 at 8, 17.7 at 32 (pass j's own peak), same run time at every batch size.
  Cause: buildPaths' P.resize(w) after dedup kept the capacity of every candidate path (millions with the pools; a few
  thousand kept), ~0.37 GB a class, for every class of the batch at once: --batch 256 -> the 94 GB kill.  Fix: shrink_to_fit
  when the capacity is over twice the size.  Now 9.1 GB at --batch 32 and at --batch 64.  Also: unused pass-1 paths dropped
  after the check (their hashes kept for the deep pass's dedup; "alternatives per class" unchanged), pass 2 run class-group
  by class-group through all its slack steps with each group's seen sets released after, the per-thread sort scratch
  released when over 4M paths (these three did not change the measured peak: the earlier guesses were wrong; kept as they
  bound memory in other cases).  Byte-identical blocks and .left: UD 31 pass-b sample (200), UD 32 --sample 20000, deep
  samples j16 and j64 (batch 8, 32, 64).
11-12 09:40 arena-week.md: plan for a 7-day all-four run (user: plan only, undecided).  1ud/2ud to D=10 (~34/47 h, needs code 0 = <=E+1 mode), 3ax/4op stay D=9 (D=10 9-16 days and over-full); chunked queue runners; armory watcher folds.
11-12 09:40 UD 31 (uduse2) pass j (--stage-acts 4 --prop 10 + pools): classes: 2770 covered, 50 not (0 duplicate lines skipped); used actions: 189947 of 9599869 (/var/tmp/orbitsolve/gpubnd/u31j.acts); 4323.02 s (uduse2.sh 31)
11-12 09:41 UD 31 (uduse2) pass k (--stage-acts 4 --prop 10 + pools): classes: 50 covered, 0 not (0 duplicate lines skipped); used actions: 16381 of 9599869 (/var/tmp/orbitsolve/gpubnd/u31k.acts); 41.36 s (uduse2.sh 31)
11-12 09:41 UD 31 (uduse2) covered; merge and check (uduse2.sh 31)

### 2026-11-12 week-run prep (user: proceed with arena-week.md)
10:00 fastsolve FTLEV (devtools/fastsolve-x86/src, backup /var/tmp/orbitsolve/fastsolve-src.bak-prelev3): FTLEV=3 =
three DFS levels from layer E, D = E+3, code 0 = <= E+1 (exact set separates <= E from E+1), codes 1/2 = E+2/E+3; Z
in .meta (absent = 0, so existing tables still load); " levels 3" in the meta fingerprint line (an older binary
never maps a 3-level table).  DFS rewritten as one recursive level walker (same cuts: within E, symmetric sibling).
Laptop test (lev3/run.sh, 1ud, E=5, SLACK=0, 40 xc sequences): FTLEV=2 identical to the old binary (leaves, new
slots, 8.995M nodes/position); FTLEV=3 D=8: same 40 distances, 1.156M nodes (7.8x fewer) with 10% of slots at 8;
FTSHM save + map: same; FTLEV=2 on a 3-level meta: "does not match; rebuilding".  run2.sh (2ud, 3ax-nr, 4op-nr) running.
Week files (laptop /var/tmp/orbitsolve/week): runner.sh (chunk queue runner), mkqueue.py.  fsfold/fsfoldax.sh 3ax|4op.
Armory: arm64 macOS 24.5 like the laptop (binaries copy over); no pypy; no ssh key for arena yet (needs the user).
10:45 FTLEV tests all PASS: 2ud old = FTLEV2 = FTLEV3 depths; 3ax-nr, 4op-nr FTLEV2 = FTLEV3; arena solve8 (x86)
FTLEV=3 1ud = laptop.  Nodes for one more level (E=5, D 7 -> 8): 1ud 7.8x (10% at 8), 2ud 7.5x (10.7%), 4op 4.3x
(21%), 3ax 2.0x (46%).  Scale model of arena (occ.sh, 1ud, tables sized so D-1 fills 3.1% / 4.9% like arena
1ud/2ud at 9): D+1 at 48% -> 1.93x, at 63% -> 1.45x.  So 2ud D=10 (47 h) gains nothing in a week; 1ud D=10 ~1.5x
net; whole-line 1ud searches still out of reach.  arena-week.md revised: 1ud D=10, others keep D=9 tables.
Runner dry run on arena (week/dry, solve8b mapping /dev/shm/2ud-300 read-only with FTNOBUILD=1): chunks OK,
STOP OK, missing table -> FAILED + failed/ with "no matching table" (nothing created); env read at runner start.
11:20 user: 1ud D=10 rebuild only as part of the week run (Friday night, maybe); the week run itself undecided.
11-12 12:20 UD 31 merge: 7,958,829 blocks (a 7,828,923 + b 113,064 + h 13,046 + i 976 + j 2,770 + k 50), u31m.txt.zst
  24.5 GB.  Check (gpubound.zst --keep ud --threads 12 --cpu-sample 5000) started 09:55; steady ~65K blocks / 70 s to
  5.05M blocks at 12:05 (7825 s), then one chunk on the GPU for 15+ min (main thread in waitUntilCompleted, GPU 99%,
  not hung).  Later blocks have longer lines (stage file is shortest first): more bad states, larger blocks.
  Disk 17 GB free.
12:20 fold f6 / 4op f3 (results to 12:06; vline PASS; certcheck / axcheck PASS): 1ud 471 direct -> 17s **3,677**;
2ud 6,319 direct -> 18s **172,695**; 4op 145 direct (93 finds; t15 tails now hitting) -> 22s **14,545**.
upperbound5.md Working tables updated (ubtable.py f6 f6 f3).  Arena 12:06: 1ud 2,561 / 289; 2ud 3,525 / 212;
4op 9,760 / 52; 3ax-nr depth 9 98.7%.
12:25 3ax-nr table built in 43,278 s (D=9; slots <=7 0.029%, 8 0.86%, 9 23.4%); searching 3ax/prod2.txt (748).
11-12 12:50 UD 31 check slowdown explained.  Blocks are in stage-file order (w0..w8, lines >= 14 moves); in 32768-block
  chunks: chunks 0-153 have mean |L0| ~14.1 and ~0-10 hard classes (pass-a .left) each, ~36 s per chunk; chunks 154-~210
  have mean |L0| 15.5 and ~2,000-3,000 hard classes each (big multi-pass blocks), and chunk 154 took 1351 s (37x);
  chunks ~211-242 mean 15.0, few hard.  Bad 2ud states per block: |L0| 14 -> 2.3M (d 18), 15 -> 10.7M, 16 -> 16.1M,
  17 -> 19.5M.  At this rate ~57 heavy chunks x 22 min + ~32 light: ~22-24 h left (finish ~Fri 11-13 midday).
  Idea: the checker tries each block's lines in file order (cheapest first, as gpuboundx's first-fit); any order is valid,
  so trying the most-used lines first (counted on a sample of states) should cut the positions tried a lot.
11-12 15:00 gpubound: --reorder S [--reorder-iters N] and --k1 K (build gpubound.k1, installed as gpubound.zst; old binary
  gpubnd/gpubound.zst-old, source gpubnd/gpubound.mm.bak-reorder).  Check 4 may try a block's lines in any order (a bad
  state needs some line with |P| + dB <= X).  --reorder: a sample kernel (firsthit) counts, on every 2^S-th bad state, the
  first line that solves it; lines sorted by count (stable), N times; the CPU sample uses the same order.  --k1 K: a thread
  per state tries K lines, unsolved states go to a list finished by one SIMD group each (32 lines per round, simd_min);
  tries counted exactly as one thread in order.  Tests:
  - counts identical with and without --k1 (u31k 3,706,503,895 tries; reordered 841,800,692; m3000 591.5G / 237.0G);
    CPU sample 0 differ throughout (all 50 u31k blocks with --cpu-sample 1);
  - --drop-line 3:1 on u31k: "2 not solved in 1 blocks" in both orders; --drop-block 7: same A-coverage count;
  - j300 (first 300 pass-j blocks): 4 not solved in 2 blocks in both orders, both in u31j.left (covered by pass k).
  Speed, 3000 blocks from the slow region (5,150,000+, mean 1055 lines), main check paused (SIGSTOP): plain 129-164 s,
  --reorder 6 --reorder-iters 3: 61 s, + --k1 8: 43 s, + --k1 16: 54-60 s (GPU timings noisy, +-20%).
  Divergence alone (--k1 without reorder) did not help; fewer tries per state is the gain.
11-12 15:05 UD 31 check run 1 (plain order) stopped at 5,177,344 / 7,958,829 blocks (13,762 s, no failure lines; logs
  gpubnd/u31m.chk.run1, .err.run1); ~27 h left at its rate.  Rerun from the start: u31chk2.sh (gpubound --keep ud
  --threads 12 --cpu-sample 5000 --reorder 6 --reorder-iters 3 --k1 8 -> u31m.chk / .chk.err); estimate ~12-14 h.
11-12 14:10 correction: the two entries above stamped 15:00 and 15:05 were written ~13:45 (wrong clock in my notes); u31chk2.sh started 13:47.  At 14:06: 557,056 blocks in 1,065 s, ~10% slower than run 1 in the easy region; ETA for 5.05M blocks ~16:10, finish ~02:00-04:00 Fri 11-13.
14:20 fold f7 / 4op f4 (results to 14:17; vline PASS; certcheck / axcheck PASS): 1ud 494 direct -> 17s **3,606**;
2ud 6,421 direct -> 18s **169,463**; 4op 146 direct -> 22s **14,541**.  upperbound5.md Working tables updated
(ubtable.py f7 f7 f4).  Arena 14:17: 1ud 2,611 / 6,687 (t16s next); 2ud 4,161 / 45,382; 4op 9,922 / 31,960;
3ax-nr 61 / 748, no finds yet (all "no solution in 14", ~14 min each).  Queues last > 1 day; no requeue.
11-12 16:25 UD 31 rerun (u31chk2.sh) ran at ~300 blocks/s from 0.6M to 2.7M blocks where run 1 did 500-800; not the
  code: with the main check paused (SIGSTOP, a few minutes each), 3000 blocks at 2.6M: old binary 10.65 s, new plain
  10.60 s, new --reorder 6 --reorder-iters 3 --k1 8 10.13 s; a full 32768-block chunk at 2.9M: plain 61.9 s, options
  55.7 s, + --blocks 128 51.6 s.  The live run is back to ~620 blocks/s (= the standalone rate) from 2.8M.  Stack samples
  of the slow phase: main thread in waitUntilCompleted (firsthit and check about equally).  Memory compressor was 3-8 GB
  in the afternoon (0.7 GB during run 1) and my sample extractions read the 24.5 GB file several times; cause not pinned.
  ETA unchanged: easy region done ~17:00, heavy ~57 chunks + ~32 lighter, finish ~02:00-04:00 Fri.
16:50 fold f8 / 4op f5 (results to 16:50; vline PASS; certcheck / axcheck PASS): 1ud 514 direct -> 17s **3,564**;
2ud 6,534 direct -> 18s **164,988**; 4op 146 direct (no new finds in 180 searches) -> 22s 14,541.  Arena 16:50: 1ud
2,671 / 6,687; 2ud 4,908 / 45,382; 4op 10,102 / 31,960; 3ax-nr 160 / 748, no finds (159 "no solution in 14").
11-12 18:50 UD 31 rerun: 5,570,560 / 7,958,829 blocks at 17,677 s; heavy chunks ~370-400 s each (run 1: 1150-1350 s), 3.3x faster; ~40 heavy + ~32 lighter chunks left, ETA ~00:30-01:30 Fri.  No failures so far.
18:55 fold f9 / 4op f6 / **3ax f1** (vline PASS; certcheck / axcheck PASS): 1ud 539 direct -> 17s **3,513**; 2ud 6,635
direct -> 18s **162,430**; 4op 146 direct -> 22s 14,541 (no finds in ~320 searches); 3ax first find (3ax/nr.out, a 15-move
tail -> 14; optfs/3ax/all.txt = prod2.txt) -> 23s **548** (fsfold/3ax-f1, axbound --prop 9 on ax-m3).  Arena 18:47: 1ud
2,712 / 6,687; 2ud 5,475 / 45,382; 4op 10,242 / 31,960; 3ax 221 / 748.
19:18 ubtable.py takes an optional 3ax fold name (ubtable.py f9 f9 f6 f1); Working tables now show 3ax-f1.
21:20 fold f10 / 4op f7 / 3ax f2 (new fsfold/refold.sh U1 U2 O4 A3 does fetch + fold.py + vline + all four folds +
ubtable; vline PASS x4; certcheck / axcheck PASS): 1ud 573 direct -> 17s **3,446**; 2ud 6,765 direct -> 18s
**158,771**; 4op 146 -> 14,541; 3ax 2 direct -> 23s **546**.  Arena 21:18: 1ud 2,784 / 6,687; 2ud 6,217 / 45,382; 4op
10,421 / 31,960; 3ax 319 / 748.
4op stall checked: the queue is 58 t13, 9,639 t14, 11,045 t15, 11,218 t16 (in order); hits 98 of 22,481 searched.  No
search bug: every searched tail whose one-shorter prefix was a hit was itself a hit (t14 5/5, t15 4/4); t15 with a
missed prefix 2 / 720 (0.3%), t14 52 / 10,880 (0.5%).  4op lines are locally near-optimal at these lengths.
21:54 user: kill 4op, give 2ud the CPU.  Killed 4op solve7 (pid 2006313) at 10,461 / 31,960 tails.  2ud (pid 2004618,
20 threads) now has node 1's 20 logical CPUs to itself (was sharing them with 4op's 20).  4op-nr table stays in
/dev/shm (for the week run).  4op stays at f7 (22s 14,541).
11-12 22:55 UD 31 rerun: 6,979,584 / 7,958,829 blocks at 32,598 s, ~240 s per chunk now (lighter tail); ~30 chunks left, ETA ~01:00 Fri.  No failures so far.
23:25 fold f11 / 3ax f3 (4op f7r = f7, no new data; vline PASS x4; certcheck / axcheck PASS): 1ud 594 direct -> 17s
**3,405**; 2ud 6,901 direct -> 18s **154,819**; 3ax 546; 4op 14,541.  Arena 22:58: 1ud 2,815; 2ud 6,846 (~7/min since
4op stopped, was ~4.9); 3ax 374 / 748.
11-13 01:01 **UD 31 PASS** (gpubound --keep ud --reorder 6 --reorder-iters 3 --k1 8; uduse2.sh blocks): blocks: 0 bad 220726760863456 colourings tried, 0 not solved; frame errors 0; A states with dA > 13: 63649344, 0 without a block; CPU sample: 1592 blocks, 0 differ from the GPU  (u31chk2.sh)
01:55 fold f12 / 3ax f4 (vline PASS x4; certcheck / axcheck PASS): 1ud 610 direct -> 17s **3,365**; 2ud 7,037 direct ->
18s **152,075**; 3ax 546; 4op 14,541.  Arena 01:53: 1ud 2,862; 2ud 8,120 (~7.3/min); 3ax 374 / 748.
02:06 3ax stopped (pid 2006312): 3ax/prod2.txt = 1 t14, 373 t15, 374 t16; all t14/t15 done (2 finds, 0.5%); the first
t16 (search to 15) ran 3.5 h without finishing (~33x a t14-search of ~14 min, so ~7-8 h each, 374 of them).  Same rule
as 4op (user 21:54): 1ud (pid 2004954) now has node 0's 20 logical CPUs alone.  3ax-nr table kept in /dev/shm;
restart on 3ax t16s possible any time.
04:10 fold f13 (3ax f5 / 4op f7t: no new data; vline PASS x4; certcheck / axcheck PASS): 1ud 636 direct -> 17s
**3,308**; 2ud 7,254 direct -> 18s **146,755**.  Arena 04:06 (load 40, two solvers): 1ud 2,932 (~32 t16/h since 3ax
stopped, was ~19); 2ud 9,085 (~7.3/min).
06:20 fold f14 (vline PASS x4; certcheck / axcheck PASS): 1ud 758 direct (+122) -> 17s **3,145**; 2ud 7,436 direct ->
18s **142,643**.  Arena 06:19: 1ud 3,075 (+143 in 2.2 h; t16s now ~65/h, ~85% shorten); 2ud 10,060 (~7.4/min).
11-13 01:01 **UD 31 PASS -> P <= 69 (UD 31 + AX 38), dry run.**  gpubound --keep ud --threads 12 --cpu-sample 5000
  --reorder 6 --reorder-iters 3 --k1 8, 40,447 s: 7,958,829 blocks, 2,762,179,240 lines, 0 bad; 58,099,290,489,000 (block,
  state) pairs, 220,726,760,863,456 colourings tried, 0 not solved; frame errors 0; 63,649,344 1ud states with dA > 13,
  0 without a block; CPU sample 1,592 blocks, 0 differ.  Times: GPU check 4 30,955 s, --reorder 3,501 s, check 2 + maps
  3,034 s; peak footprint 1.9 GB.  Heavy chunks ~380 s vs run 1's ~1,250 s (3.3x).
11-13 08:20 P69 evidence folder ~/orbitsolve-certsafe/obl23pair-P69-evidence-2026-11-13: ud31-blocks.txt.zst (moved from
  gpubnd/u31m.txt.zst, symlinked back), all pass logs/.left, merge and check logs (run 1 stopped too), scripts, sources.
  Scoreboard: P 69 row.
11-13 08:30 upperbound5.md brought to P 69 (constants, polynomials rows 70 and 69, comparison table n=6..10: 492, 579, 836, 962, 1249; status; certificates).  It had been left at P 71 when UD 32 passed (11-10): my miss.
11-13 08:50 Freed (user: boundary blocks need not be kept once checked and passed): UD 31 pass block files u31{a,a-part,b,h,
  i,j,k}.txt.zst, the merged ud31-blocks.txt.zst (P69 evidence), ud32-blocks.txt.zst (P70 evidence), t32.txt.zst (UD 32
  top-10K test), s33a/s33m.txt (UD 33 plain blocks): ~63 GB.  Kept .left/.acts/logs.  P69 evidence README + SHA256SUMS
  written (no blocks).  Free space returns as the hourly TM local snapshots roll off (~24 h).  Older evidence blocks
  (AX 38 7.9 GB, AX 39 1.7 GB, AX 40 0.4 GB, UD 33 0.2 GB) left for the user's call.
11-13 09:05 Freed more (user: "we don't need the evidence, as long as we have the code and results"): evidence blocks
  ax38 (P71), ax39 (P72), ax40 + ud33 (P73; P72/P73 dirs made writable for the rm and set read-only again); working
  copies bnd/g38m.txt.zst, g39m.txt.zst, x41a/m, x42a/m, x43/m/b, x41b, x44, g40b, s41 (.txt); gpubnd/sa test outputs.
  ~24 GB more (~87 GB today).  Kept: stage snapshots (bnd/cert40, snap-41/42/c30), .left/.only/.err, logs, sources.
  Evidence READMEs that mention block files are now stale on that point (blocks deleted after PASS).
08:25 upperbound5.md Open: arena item updated (4op, 3ax stopped; folds are on the P 80 stages, not the UD 31 pass stages).
08:30 correction: the UD 31 pass used the certified 1ud stage (its 5,294 17-classes), so the 'not the UD 31 stages' note was wrong; removed.
08:40 disk 13 GB free: deleted superseded fold dirs (fsfold 21 GB -> 2.3 GB; kept 1ud-f14, 2ud-f14, 3ax-f6, 4op-f7u + all logs).  refold.sh reuses an existing 4op/3ax fold dir (both stopped).
08:45 fold f15 (3ax f6 / 4op f7u reused; vline PASS x4; certcheck / axcheck PASS): 1ud -> 17s **2,998**; 2ud 7,642
direct -> 18s **139,685**.  Arena: 1ud 3,221; 2ud 11,031.  Superseded 1ud-f14/2ud-f14 deleted (disk 12 GB free).
08:50 user: armory -> arena ssh.  Made armory ~/.ssh/id_ed25519 (no passphrase, comment armory-to-arena); appended its .pub to arena ~/.ssh/authorized_keys (backup authorized_keys.bak-20261113); arena host key added to armory known_hosts (same key as the laptop's); tested: armory 'ssh 10.0.0.155' works.
11-13 09:40 ~/orbitsolve is now a git repo (branch main, commit 328c797, 1,445 files ~22 MB): .gitignore (repo/, dist/gullicube/, cert stage/boundary data, *.zst/*.pool/*.cls/logs, files over 2 MB, 187 binaries), .githooks/pre-commit (rejects > 5 MB and binaries; core.hooksPath), .gitattributes (journal.md merge=union).  For syncing with armory (no ingress; reachable only via Remote Control): hub option pending the user's choice.
09:00 user: repo (~/orbitsolve/repo) remote 'obliques' = https://github.com/rokicki/obliques.git (public); pushed main (41a4b43, the 11-09 commit, P 80 certificate).  HTTPS via gh; pushes failed with LibreSSL 'bad record mac' until http.version HTTP/1.1 (now set in the repo's config).  User also made rokicki/orbitsolve for this directory.
09:25 armory: FileVault is On (laptop: Off), so after a power loss armory waits at the unlock screen; user keeps it (no decryption now; clean reinstall later).  A power loss on armory stops its Claude until the user is back.  Remote Control session listed as '--name armory'.
09:50 pypy on PATH, both machines: ~/pypy/current -> installed version (laptop pypybin 3.10, armory ~/pypy/pypy3.12-v8.0.1), PATH in ~/.zshenv (backups .zshenv.bak-20261113; armory .zshrc line 6 pointed at ~/pypy3-install (3.9) now at ~/pypy/current, backup .zshrc.bak-20261113).  fsfold.sh / refold.sh use $(command -v pypy3).
10:10 armory set up for folds (user: move everything needed, test a fold).  Scripts in git: arena/ (ar, aget, arwatch.sh,
refold.sh, fsfold.sh, fsfoldax.sh, fold.py, vline.py, inv.py, lfilter.py + twsval.py, ubtable.py, runner.sh, mkqueue.py;
README.md lists the data); devtools/tws/obl8ax-nr.tws, obl8op-nr.tws.  Data rsynced to the same paths (7.5 GB, 4 min):
optfs/{1ud,2ud,3ax,4op,full}, ax-m3.tab, 4op-m1.tab, fold/pools (symlinks resolved), dist/gullicube/data 5 .cls,
fsfold/{3ax-f6,4op-f7u}.  Binaries built on armory (Apple clang 16): certextend needed std::atomic_ref -> __atomic
builtins (committed; laptop rebuilt from the same source, old binary /var/tmp/orbitsolve/certextend.bin-pre-builtins).
Test on armory from the f15 inputs: 1ud identical to laptop f15 (17s 2,998; all lengths equal; 345 s, 5.9 GB peak);
2ud 18s 139,766 vs laptop 139,685 -- certextend is not run-to-run deterministic (threads): the laptop's own rerun of
the same fold gave 139,735; certcheck PASS (150 s, 2.6 GB); 4op identical to 4op-f7u (14,541; 143 s, 4.1 GB); 3ax
identical to 3ax-f6 (546; 317 s, 6.5 GB).  First 3ax/4op try kept nothing: lfilter.py exec'd /var/tmp/orbitsolve/op/
twsval.py (missing on armory) and fsfoldax.sh carried on; fixed (twsval.py in arena/, fsfoldax.sh stops when lfilter
fails or keeps no line).  armory ar / aget reach arena with its own key.
11:00 fold f16 (first refold from git arena/refold.sh; vline PASS x4; certcheck / axcheck PASS): 1ud 17s **2,891**; 2ud
18s **136,081**; 3ax 546; 4op 14,541.  Arena: 1ud 3,353; 2ud 11,957.
12:20 handover tooling (arena/, git): refold.sh now takes no arguments (per stage optfs/<st>/{outs,lines.txt,lastfold}:
fetches every open run, folds only stages whose prod-improved.txt changed, names folds f<N+1>, removes the previous
fold dir, ubtable, copies prod-improved.txt to arena/results/); mktails.py (next list: K-move tails of the LEN-move
lines of the latest fold, not queued before; adds the lines to lines.txt); alaunch.sh (start a run: solve8b +
FTNOBUILD=1, so it never builds; --build after a reboot), astop.sh (exact pids), aclose.sh (close a run, cut
all.txt to what it searched), astatus.sh; RUNBOOK.md for the armory Claude.  outs rows written for all runs (4op
all.txt cut to its 22,521 searched tails, 3ax to 374; backups all.txt.bak-20261113).  pid files on arena for the
running 1ud prod2 (2004954) and 2ud prod4 (2004618).
Refold with the new script: 1ud f17 17s **2,838**; 2ud f17 18s **133,281**; 4op/3ax no change.  Test run 4op t3
(alaunch/aclose; solve8b mapped 4op-300): 3 15-move tails of the fold f7u's 22-move lines (different representatives
from the original lines: 14,530 of 14,541 lines new to lines.txt) -> **3 of 3 shortened** (108-336 s each, sharing
node 1 with 2ud).  So 4op's "stall" was the original representatives' tails; the fold's own lines give fresh tails.
12:55 [armory] first refold on armory (handover test, run by the laptop Claude over ssh): 1ud f18 17s **2,832**; 2ud f18 18s **133,140**; 4op f8 22s **14,535** (the 3 t3 finds); 3ax f6 546; vline / certcheck / axcheck PASS.  From here armory runs arena and the refolds (arena/RUNBOOK.md); the laptop stopped its refold cycle.
11-13 14:05 [armory] fold: 1ud f19 17s **2,732**; 2ud f19 18s **131,515**; 4op f8 14,535, 3ax f6 546 (no change); vline PASS x4, certcheck PASS x2; arena 1ud prod2 3,530/6,687 (~34 h left), 2ud prod4 13,397/45,382 (~91 h left).  2-hourly cycle scheduled in the armory session (cron, :13 every other hour).
11-13 14:51 [armory] fold: 1ud f20 17s **2,699**; 2ud f20 18s **131,222**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 3,565/6,687 (~34 h left), 2ud prod4 13,738/45,382 (~89 h left).
11-13 [laptop] **Move cost measurement** (user's idea: a slice turn later undone by its inverse need not move the whole slice).  devtools/movemeasure.cpp (N A B STAGEDIR...): plays each line on an N^3 xcube with labelled slots, depth 2 = slice A, 3 = slice B (as certsolve's play()), and counts, per "generic" orbit record (rows[A][j] + rows[j][A], j not A or B), the raw slots that changed (facerots are lazy and global, free).  Every generic record of a slice ends identical (0 mismatches), so a line's effect on all generic records is one sparse permutation, computable in O(moves) independent of N.  N=40, A 5, B 11, 20,000 lines per stage of repo/certificate (small form, unweighted by use): slot writes per generic orbit pair per line, old (domove) -> changed: 1ud 99.7 -> 19.2 (5.2x, 21% of lines change nothing); 2ud 82.0 -> 25.1 (3.3x, 12%); 3ax 116.8 -> 1.6 (71x, 90%); 4op 94.3 -> 22.6 (4.2x, 26%); all four 393 -> 68.6 (5.7x).  Wings: face turns cycle every wing orbit (not lazy): per generic wing orbit per line old 10.8/60.9/38.9/102.9 slot writes -> 4.5/9.4/12.1/14.0 changed (213 -> 40, 5.3x); wings are ~1/3 of the current per-pair move work.
11-13 16:51 [armory] fold: 1ud f21 17s **2,557**; 2ud f21 18s **128,946**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 3,709/6,687 (~33 h left), 2ud prod4 14,611/45,382 (~86 h left).
11-13 18:51 [armory] fold: 1ud f22 17s **2,452**; 2ud f22 18s **127,916**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 3,826/6,687 (~32 h left), 2ud prod4 15,490/45,382 (~83 h left).
11-13 20:51 [armory] fold: 1ud f23 17s **2,368**; 2ud f23 18s **125,624**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 3,937/6,687 (~31 h left), 2ud prod4 16,363/45,382 (~79 h left).
11-13 22:51 [armory] fold: 1ud f24 17s **2,320**; 2ud f24 18s **123,863**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,012/6,687 (~31 h left), 2ud prod4 17,232/45,382 (~76 h left).
11-14 00:51 [armory] fold: 1ud f25 17s **2,278**; 2ud f25 18s **122,289**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,090/6,687 (~31 h left), 2ud prod4 18,110/45,382 (~73 h left).
11-14 02:51 [armory] fold: 1ud f26 17s **2,217**; 2ud f26 18s **120,350**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,168/6,687 (astatus ~31 h left; at the current ~39/h nearer 65 h), 2ud prod4 18,983/45,382 (~71 h left).
11-14 04:51 [armory] fold: 1ud f27 17s **2,156**; 2ud f27 18s **118,862**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,255/6,687 (~43/h, ~57 h left at that rate), 2ud prod4 19,857/45,382 (~68 h left).
11-14 06:51 [armory] fold: 1ud f28 17s **2,102**; 2ud f28 18s **118,174**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,339/6,687 (~42/h, ~56 h left at that rate), 2ud prod4 20,730/45,382 (~65 h left).
11-14 08:51 [armory] fold: 1ud f29 17s **2,055**; 2ud f29 18s **117,053**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,428/6,687 (~44/h, ~51 h left at that rate), 2ud prod4 21,610/45,382 (~62 h left).
11-14 10:51 [armory] fold: 1ud f30 17s **2,021**; 2ud f30 18s **116,349**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,499/6,687 (~36/h, ~60 h left at that rate), 2ud prod4 22,482/45,382 (~60 h left).
11-14 12:51 [armory] fold: 1ud f31 17s **1,963**; 2ud f31 18s **114,666**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,601/6,687 (~51/h, ~41 h left at that rate), 2ud prod4 23,370/45,382 (~57 h left).
11-14 14:51 [armory] fold: 1ud f32 17s **1,933**; 2ud f32 18s **113,495**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,674/6,687 (~37/h, ~55 h left at that rate), 2ud prod4 24,257/45,382 (~55 h left).
11-14 16:51 [armory] fold: 1ud f33 17s **1,890**; 2ud f33 18s **112,120**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,745/6,687 (~36/h, ~55 h left at that rate), 2ud prod4 25,136/45,382 (~52 h left).
11-14 18:51 [armory] fold: 1ud f34 17s **1,820**; 2ud f34 18s **111,332**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,837/6,687 (~46/h, ~40 h left at that rate), 2ud prod4 26,013/45,382 (~50 h left).
11-14 20:51 [armory] fold: 1ud f35 17s **1,779**; 2ud f35 18s **110,787**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,920/6,687 (~42/h, ~43 h left at that rate), 2ud prod4 26,886/45,382 (~47 h left).
11-14 22:51 [armory] fold: 1ud f36 17s **1,735**; 2ud f36 18s **109,517**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 4,999/6,687 (~40/h, ~43 h left at that rate), 2ud prod4 27,757/45,382 (~45 h left).
11-15 00:51 [armory] fold: 1ud f37 17s **1,685**; 2ud f37 18s **108,857**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,087/6,687 (~44/h, ~36 h left at that rate), 2ud prod4 28,642/45,382 (~43 h left).
11-15 02:51 [armory] fold: 1ud f38 17s **1,638**; 2ud f38 18s **108,229**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,171/6,687 (~42/h, ~36 h left at that rate), 2ud prod4 29,528/45,382 (~40 h left).
11-15 04:51 [armory] fold: 1ud f39 17s **1,593**; 2ud f39 18s **107,239**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,251/6,687 (~40/h, ~36 h left at that rate), 2ud prod4 30,412/45,382 (~38 h left).
11-15 06:51 [armory] fold: 1ud f40 17s **1,542**; 2ud f40 18s **106,976**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,339/6,687 (~44/h, ~31 h left at that rate), 2ud prod4 31,276/45,382 (~36 h left).
11-15 08:51 [armory] fold: 1ud f41 17s **1,506**; 2ud f41 18s **106,008**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,415/6,687 (~38/h, ~33 h left at that rate), 2ud prod4 32,146/45,382 (~33 h left).
11-15 10:51 [armory] fold: 1ud f42 17s **1,449**; 2ud f42 18s **105,313**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,524/6,687 (~55/h, ~21 h left at that rate), 2ud prod4 33,022/45,382 (~31 h left).
11-15 12:51 [armory] fold: 1ud f43 17s **1,395**; 2ud f43 18s **104,838**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,636/6,687 (~56/h, ~19 h left at that rate), 2ud prod4 33,894/45,382 (~29 h left).
11-15 14:51 [armory] fold: 1ud f44 17s **1,362**; 2ud f44 18s **104,587**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,710/6,687 (~37/h, ~26 h left at that rate), 2ud prod4 34,779/45,382 (~26 h left).
11-15 16:51 [armory] fold: 1ud f45 17s **1,325**; 2ud f45 18s **103,323**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,784/6,687 (~37/h, ~24 h left at that rate), 2ud prod4 35,655/45,382 (~24 h left).
11-15 18:51 [armory] fold: 1ud f46 17s **1,276**; 2ud f46 18s 103,338 (+15 vs f45 despite 140 more improved lines: certextend run-to-run variation, as seen 11-13 on the f15 rerun; PASS); 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,874/6,687 (~45/h, ~18 h left at that rate), 2ud prod4 36,528/45,382 (~22 h left).
11-15 20:51 [armory] fold: 1ud f47 17s **1,217**; 2ud f47 18s **102,834**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 5,969/6,687 (~48/h, ~15 h left at that rate), 2ud prod4 37,394/45,382 (~20 h left).
11-15 22:51 [armory] fold: 1ud f48 17s **1,168**; 2ud f48 18s **101,977**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,064/6,687 (~48/h, ~13 h left at that rate), 2ud prod4 38,266/45,382 (~18 h left).
11-16 00:51 [armory] fold: 1ud f49 17s **1,099**; 2ud f49 18s **101,769**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,185/6,687 (~60/h, ~8 h left at that rate), 2ud prod4 39,139/45,382 (~15 h left).
11-16 02:51 [armory] fold: 1ud f50 17s **1,034**; 2ud f50 18s **101,499**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,300/6,687 (~58/h, ~6.7 h left at that rate), 2ud prod4 40,004/45,382 (~13 h left).
11-16 04:51 [armory] fold: 1ud f51 17s **987** (under 1,000); 2ud f51 18s **101,181**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,390/6,687 (~45/h; ends ~09:30-11:30, then aclose + mktails 1ud 17 17 --max 20 timing list), 2ud prod4 40,872/45,382 (~11 h left).
11-16 06:51 [armory] fold: 1ud f52 17s **948**; 2ud f52 18s **100,703**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,480/6,687 (~45/h, ends ~11:00), 2ud prod4 41,747/45,382 (~8 h left).
11-16 08:51 [armory] fold: 1ud f53 17s **911**; 2ud f53 18s **100,477**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,565/6,687 (~43/h, ends ~11:30), 2ud prod4 42,626/45,382 (~6 h left; next: mktails 2ud 18 15).
11-16 10:51 [armory] fold: 1ud f54 17s **898**; 2ud f54 18s **100,275**; 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod2 6,641/6,687 (ends ~12:00; 11:47 relaunch check set), 2ud prod4 43,506/45,382 (~4-5 h left).
11-16 11:57 [armory] 1ud prod2 finished 11:29: 6,687 of 6,687 searched, 2,950 shortened (44%), 57.9 s/tail wall.  Fold f55 (vline PASS x4, certcheck PASS x2): 1ud 17s **885**; 2ud 18s **100,096**; 4op f8, 3ax f6.  aclose 1ud; **1ud prod5** launched (node 0, pid 2194863): mktails.py 1ud 17 17 --max 20 = 20 whole 17-move lines of f55 (885 left), timing test (runbook: if each < ~2 h, queue the rest; else report and go to 16s/15-tails).  2ud prod4 44,055/45,382 (~3 h left).
11-16 12:51 [armory] fold: 1ud f56 17s **884**; 2ud f56 18s **99,886** (under 100K); 4op f8, 3ax f6 no change; vline PASS x4, certcheck PASS x2; arena 1ud prod5 1/20 (first whole 17: 2,169 s, 17 -> 15), 2ud prod4 44,391/45,382 (~2.4 h left).  Note: solve8b runs 16 threads, so node 0 load ~16 of 20 during prod5.
11-16 14:52 [armory] **1ud prod5 timing test done** (11:52-14:27 wall, 2.6 h for 20 = ~7.7 min/line; solve8b searches positions in parallel): **20 of 20 whole 17-move lines shortened** (4 to 15, 16 to 16); per-position search 465-9,087 s (mean ~70 min, 4 over 2 h).  Fold f57 (vline PASS x4, certcheck PASS x2): 1ud 17s **861**; 2ud 18s **99,376**; 4op f8, 3ax f6.  Per runbook (each under ~2 h): aclose 1ud; **1ud prod6** = mktails.py 1ud 17 17 = all 861 remaining 17-move lines, launched node 0 (pid 2196307); at prod5's rate ~110 h (~11-21).  If all shorten, 1ud's worst drops to 16.  2ud prod4 45,329/45,382 (ends ~15:00).
11-16 15:20 [armory] **2ud prod4 finished** 14:58: 45,382 of 45,382 searched, 2,801 shortened (6.2%), 8.8 s/tail wall.  Fold f58 (vline PASS x4, certcheck PASS): 2ud 18s **99,274**; 1ud f57 861.  aclose 2ud; **2ud prod5** = mktails.py 2ud 18 15 = **86** 15-move tails of the 18s not yet queued (28,059 already queued; runbook guessed ~200), launched node 1; ~15 min.  Next: runbook 2ud (b), 16-move tails of the 18s, shuffled, ~2,000 per list, first one timed.
11-16 15:50 [armory] 2ud prod5 finished: 86 of 86, 4 shortened (4.7%), 9.1 s/tail wall.  Fold f59 (vline PASS x4, certcheck PASS): 2ud 18s 99,275 (+1 vs f58: certextend variation); 1ud f57 861.  aclose 2ud; **2ud prod6** = mktails.py 2ud 18 16 --shuffle 1116 --max 2000 = 2,000 shuffled 16-move tails of the 18s (first list of runbook 2ud (b)), launched node 1 (pid 2197884); timing it to size later lists to ~1 day.  1ud prod6 0/861.
11-16 16:43 [armory] no new results: 1ud prod6 0/861 at 1 h 51 m, 2ud prod6 0/2,000 at 53 m (both solve8b at 2000% CPU, .err empty, nothing printed past the table map yet); folds unchanged (1ud f57 861, 2ud f59 99,275; vline PASS x4).
11-16 18:52 [armory] fold: 1ud f58 17s **851**; 2ud f60 18s **99,243**; vline PASS x4, certcheck PASS x2.  1ud prod6 9/861 at 3.85 h, all 9 shortened (4 to 15); solve8b runs one position per thread (20), per-line search mean 4,292 s (max 12,501), so ~17 lines/h, ~50 h for the list (astatus' 365 h is early-run lag).  2ud prod6 21/2,000 at 2.9 h, 3 shortened (14%); the .out gives no time for unshortened tails, so the rate isn't known yet; astatus 272 h -- if it holds, the list is far over the runbook's ~1 day (expected minutes per tail).
11-16 20:51 [armory] fold: 1ud f59 17s **797**; 2ud f61 18s **98,994**; vline PASS x4, certcheck PASS x2.  1ud prod6 52/861, **52 of 52 shortened**; +43 in 2 h (~21/h), ~38 h left (ends ~11-18 11:00).  2ud prod6 52/2,000, 10 shortened (19%); +31 in 2 h (~15/h), ~125 h left (~11-22) -- ~5x the runbook's ~1-day list size; asked the user whether to let it run or cut it (runbook: size later lists to ~1 day = ~370 tails at this rate).
11-16 22:51 [armory] fold: 1ud f60 17s **790**; 2ud f62 18s **98,371**; vline PASS x4, certcheck PASS x2.  1ud prod6 59/861 (59 of 59 shortened; only +7 in 2 h after +43: perhaps a few long searches; not checked whether solve8b reports in list order).  2ud prod6 91/2,000, 25 shortened (27%); +39 in 2 h (~20/h), ~95 h left.  2ud list size: awaiting the user's answer (let run / cut to ~1 day); running unchanged.
11-17 00:51 [armory] fold: 1ud f61 17s **740**; 2ud f63 18s **98,247**; vline PASS x4, certcheck PASS x2.  1ud prod6 106/861, 106 of 106 shortened (+47 in 2 h).  2ud prod6 115/2,000, 29 shortened (25%; +24 in 2 h).  2ud list size still awaiting the user; running unchanged.
11-17 02:51 [armory] fold: 1ud f62 17s **689**; 2ud f64 18s 98,451 (**+204** vs f63's 98,247 with only 7 more improved lines; certcheck PASS, so a valid bound, but larger than the ~50-80 certextend run-to-run variation seen before; each fold rebuilds from the base stage, so it shouldn't carry forward; watching); vline PASS x4.  1ud prod6 153/861, 153 of 153 shortened (+47 in 2 h).  2ud prod6 137/2,000, 31 shortened (+22 in 2 h).
11-17 04:51 [armory] fold: 1ud f63 17s 689->676 (prod6 166/861, all shortened); 2ud f65 18s 98,451->98,052 (f64 +204 was fold noise, back below f63's 98,247; prod6 170/2000, 36 shortened); vline x4 + certcheck x2 PASS.
11-17 06:51 [armory] fold: 1ud f64 17s 676->659 (prod6 187/861, all shortened); 2ud f66 18s 98,052->97,977 (prod6 198/2000, 41 shortened); vline x4 + certcheck x2 PASS.
11-17 08:51 [armory] fold: 1ud f65 17s 659->605 (prod6 237/861, all shortened, +50 in 2 h); 2ud f67 18s 97,977->97,630 (prod6 226/2000, 49 shortened); vline x4 + certcheck x2 PASS.
11-17 10:51 [armory] fold: 1ud f66 17s 605->578 (prod6 265/861, all shortened); 2ud f68 18s 97,630->97,474 (prod6 252/2000, 54 shortened); vline x4 + certcheck x2 PASS.
11-17 12:51 [armory] fold: 1ud f67 17s 578->552 (prod6 291/861, all shortened); 2ud f69 18s 97,474->96,415 (-1,059 from 33 more improved lines, 8 of them by 2; part may be fold noise; prod6 285/2000, 65 shortened); vline x4 + certcheck x2 PASS.
11-17 14:51 [armory] fold: 1ud f68 17s 552->529 (prod6 315/861, all shortened); 2ud f70 18s 96,415->96,293 (f69 drop held; prod6 315/2000, 73 shortened); vline x4 + certcheck x2 PASS.
11-17 16:51 [armory] fold: 1ud f69 17s 529->505 (prod6 342/861, all shortened); 2ud f71 18s 96,293->96,314 (+21, fold noise: 2 more improved lines; prod6 339/2000, 75 shortened); vline x4 + certcheck x2 PASS.  User: let 2ud prod6 run its full 2,000.
11-17 18:51 [armory] fold: 1ud f70 17s 505->476 (prod6 365/861, all shortened); 2ud f72 18s 96,314->96,246 (prod6 369/2000, 80 shortened); vline x4 + certcheck x2 PASS.
11-17 20:51 [armory] fold: 1ud f71 17s 476->446 (prod6 394/861, all shortened); 2ud f73 18s 96,246->96,136 (prod6 399/2000, 86 shortened); vline x4 + certcheck x2 PASS.
11-17 22:51 [armory] fold: 1ud f72 17s 446->406 (prod6 433/861, all shortened); 2ud f74 18s 96,136->95,884 (prod6 427/2000, 90 shortened); vline x4 + certcheck x2 PASS.
11-18 00:51 [armory] fold: 1ud f73 17s 406->393 (prod6 447/861, all shortened; only 14 searched in 2 h); 2ud f75 18s 95,884->95,805 (prod6 451/2000, 94 shortened); vline x4 + certcheck x2 PASS.
11-18 02:51 [armory] fold: 1ud f74 17s 393->359 (prod6 481/861, all shortened; +34 in 2 h, pace back); 2ud f76 18s 95,805->95,423 (prod6 483/2000, 100 shortened); vline x4 + certcheck x2 PASS.
11-18 04:51 [armory] fold: 1ud f75 17s 359->314 (prod6 526/861, all shortened); 2ud f77 18s 95,423->95,158 (prod6 510/2000, 107 shortened); vline x4 + certcheck x2 PASS.
11-18 06:51 [armory] fold: 1ud f76 17s 314->288 (prod6 549/861, all shortened); 2ud f78 18s 95,158->94,780 (prod6 541/2000, 118 shortened); vline x4 + certcheck x2 PASS.
11-18 08:51 [armory] fold: 1ud f77 17s 288->265 (prod6 576/861, all shortened); 2ud f79 18s 94,780->94,700 (prod6 567/2000, 119 shortened); vline x4 + certcheck x2 PASS.
11-18 10:51 [armory] fold: 1ud f78 17s 265->249 (prod6 593/861, all shortened); 2ud f80 18s 94,700->93,908 (prod6 601/2000, 135 shortened, +16 in 2 h); vline x4 + certcheck x2 PASS.
11-18 12:51 [armory] fold: 1ud f79 17s 249->212 (prod6 634/861, all shortened); 2ud f81 18s 93,908->93,785 (prod6 623/2000, 140 shortened); vline x4 + certcheck x2 PASS.
11-18 14:51 [armory] fold: 1ud f80 17s 212->179 (prod6 672/861, all shortened); 2ud f82 18s 93,785->93,567 (prod6 649/2000, 145 shortened); vline x4 + certcheck x2 PASS.
11-18 16:51 [armory] fold: 1ud f81 17s 179->156 (prod6 699/861, all shortened); 2ud f83 18s 93,567->93,443 (prod6 682/2000, 148 shortened); vline x4 + certcheck x2 PASS.
11-18 18:51 [armory] fold: 1ud f82 17s 156->137 (prod6 718/861, all shortened); 2ud f84 18s 93,443->93,259 (prod6 709/2000, 154 shortened); vline x4 + certcheck x2 PASS.
