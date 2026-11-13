# Upper bounds for solving the centres

Current results only, brief.  How it works: methods.md.  twsearch notes: twsearch.md.  Everything as it
happened (runs, approaches tried and dropped, old versions of these files): journal.md.

Metric: single-layer turn metric (every turn of one layer, quarter or half, counts 1).  Centres first, one
orbit (or chiral pair of oblique orbits) at a time, then corners (and midges on odd n), then wings.  An
algorithm for one orbit keeps the other centre orbits as the order requires; edges and corners may be
scrambled.  Same-coloured centre pieces are interchangeable.

## Constants

| constant | value | status |
|---|---|---|
| P, oblique chiral pair | **80** = 1ud 17 + 2ud 18 + 3ax 23 + 4op 22 (plain sum) | certified: ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01 (copy on armory ~/certsafe) |
| P with boundary passes | **69** = UD 31 + AX 38 | checked (dry run): UD 31 gpubound PASS (2.21e14 colourings, 0 not solved, 11-13 01:01), AX 38 gpuaxbcheck PASS (1.47e14 positions, 0 not solved, 11-08 23:52); evidence ~/orbitsolve-certsafe/obl23pair-P69-evidence-2026-11-13 (UD 31) and -P71-evidence-2026-11-08 (AX 38); boundary certificates at home |
| D, diagonal | 44 = 10 + 16 + 18 | certified (cert/diag/centres-first) |
| M, mid (odd n) | 39 = 18 + 21 | certified (cert/mid/centres-first) |
| W, wing orbit keeping centres | 162 = 32 + 32 + 40 + 58 | certified (cert/wing/centres-kept); 149 + 4 by 3-cycles is a sketch (journal.md) |
| corners (even n) / corners + midges (odd n) | 11 / 20 | proved (full BFS / God's number) |

With k = (n-2)/2 (even) or (n-3)/2 (odd): k(k-1)/2 oblique pairs, k diagonal orbits, k mid orbits (odd n),
k wing orbits.

## Polynomials

| P | status | whole cube, even n | whole cube, odd n | centres only, even n | centres only, odd n | n = 6, 7, 8 (whole) |
|---|---|---|---|---|---|---|
| **80** | certified | (80 n^2 + 344 n - 920) / 8 | (80 n^2 + 340 n - 1580) / 8 | (80 n^2 - 304 n + 288) / 8 | (80 n^2 - 308 n + 204) / 8 | 503, 590, 869 |
| 77 | checked (UD 35 + AX 42) | (77 n^2 + 362 n - 944) / 8 | (77 n^2 + 364 n - 1625) / 8 | (77 n^2 - 286 n + 264) / 8 | (77 n^2 - 284 n + 159) / 8 | 500, 587, 860 |
| 76 | checked (34 + 42) | (76 n^2 + 368 n - 952) / 8 | (76 n^2 + 372 n - 1640) / 8 | (76 n^2 - 280 n + 256) / 8 | (76 n^2 - 276 n + 144) / 8 | 499, 586, 857 |
| 75 | checked (34 + 41) | (75 n^2 + 374 n - 960) / 8 | (75 n^2 + 380 n - 1655) / 8 | (75 n^2 - 274 n + 248) / 8 | (75 n^2 - 268 n + 129) / 8 | 498, 585, 854 |
| 74 | checked (34 + 40) | (74 n^2 + 380 n - 968) / 8 | (74 n^2 + 388 n - 1670) / 8 | (74 n^2 - 268 n + 240) / 8 | (74 n^2 - 260 n + 114) / 8 | 497, 584, 851 |
| 73 | checked (33 + 40) | (73 n^2 + 386 n - 976) / 8 | (73 n^2 + 396 n - 1685) / 8 | (73 n^2 - 262 n + 232) / 8 | (73 n^2 - 252 n + 99) / 8 | 496, 583, 848 |
| 72 | checked (33 + 39) | (72 n^2 + 392 n - 984) / 8 | (72 n^2 + 404 n - 1700) / 8 | (72 n^2 - 256 n + 224) / 8 | (72 n^2 - 244 n + 84) / 8 | 495, 582, 845 |
| 71 | checked (33 + 38), dry run | (71 n^2 + 398 n - 992) / 8 | (71 n^2 + 412 n - 1715) / 8 | (71 n^2 - 250 n + 216) / 8 | (71 n^2 - 236 n + 69) / 8 | 494, 581, 842 |
| 70 | checked (32 + 38), dry run | (70 n^2 + 404 n - 1000) / 8 | (70 n^2 + 420 n - 1730) / 8 | (70 n^2 - 244 n + 208) / 8 | (70 n^2 - 228 n + 54) / 8 | 493, 580, 839 |
| **69** | **checked (31 + 38)**, dry run; certificate at home | (69 n^2 + 410 n - 1008) / 8 | (69 n^2 + 428 n - 1745) / 8 | (69 n^2 - 238 n + 200) / 8 | (69 n^2 - 220 n + 39) / 8 | 492, 579, 836 |

P = (1ud + 2ud) + (3ax + 4op): 35 + 45 plain = 80; with boundary passes UD 34..31 and AX 42..38 (scoreboard.md), 31 + 38 = 69.
Whole cube: P (n-2)(n-4)/8 + (n-2)/2 (D + W) + 11 (even), P (n-3)(n-5)/8 + (n-3)/2 (D + M + W) + 20 (odd);
centres only: drop W and the corner term.

## Published bounds for comparison (2026-11-06)

Our metric is the single-slice turn metric (SSTM: any one layer, quarter or half turn, counts 1). OBTM (outer block
turn metric) counts a turn of an outer block of k layers as 1, so k SSTM moves can be 1 OBTM move. An OBTM bound B for
the 5x5x5 (blocks of at most 2 layers) gives SSTM <= 2B.

| n | best published upper bound | in SSTM | ours, P 80 (certified) | ours, P 69 (GPU boundary pass) |
|---|---|---|---|---|
| 4 | 53 SSTM, 55 OBTM, 53 BTM (Shuang Chen, March 2015) [1][2] | 53 | 217 | 217 |
| 5 | 130 OBTM (xyzzy, Speedsolving forum, 2018-05-20; community computation, not independently checked) [3] | <= 260 (from 130 OBTM) | 265 | 265 |
| 6 | none found (lower bound 72 SSTM [4]) | - | 503 | 492 |
| 7 | none found (lower bound 95 SSTM [4]) | - | 590 | 579 |
| 8 | none found | - | 869 | 836 |
| 9 | none found | - | 995 | 962 |
| 10 | none found | - | 1315 | 1249 |
| N | Theta(N^2 / log N) (Demaine et al. 2011; constants not given) [5] | - | ~P N^2 / 8 | ~8.6 N^2 |

- For 4x4x4 and 5x5x5 the published reduction methods (phase searches over whole-cube subgroups) are far better
  than ours, which solves one orbit at a time. From 6x6x6 on, we found no published explicit upper bound in any
  metric, only lower bounds [4] and the asymptotic order [5].
- 4x4x4 history [2]: Norskog 79 slice turns (2006), 68 then 67 block turns (2007); Chen 57 OBTM (2013); Rokicki 57
  OBTM confirmed, 56 SSTM, 53 BTM (2015); Chen 55 OBTM, 53 SSTM (2015). A forum post claims 54 OBTM; not
  confirmed elsewhere.
- A 2026 social-media post (Math Cafe) quotes 92n^2 - 307n + 113 (odd n) and 92n^2 - 339n + 103 (even n) as upper
  bounds, without a source or proof; we could not trace it. Ours grows as about P n^2 / 8 (10 n^2 at P 80, about 8.6 n^2 at 69).

- Centres only, explicit O(n^2): our nxnxn paper (~/arXiv_nxnxn_paper/main.tex, line ~935) bounds one centre orbit by
  12 three-cycles of 12 moves = 144, so all centres take at most 144 (n/2 - 1)^2 = 36 n^2 - 144 n + 144 moves.
  Our centres-only polynomial is (80 n^2 - 304 n + 288) / 8 = 10 n^2 - 38 n + 36 at P 80 (about 8.6 n^2 at P 69),
  about 3.6 times smaller in the leading term. The paper's own asymptotic algorithms reach
  30.5 n^2 / lg n + o(n^2 / lg n) moves for the whole cube, which wins only for very large n.
- Reddit: r/Cubers, "I found a general formula for an upper bound of ..." (2018),
  https://old.reddit.com/r/Cubers/comments/8chfuu/ (found through a Hacker News comment; reddit blocks automated
  reading, so its formula and the 36 n^2 + O(n) comment are not checked here).
- Local archive: ~/old/drupal is a dump of the Domain of the Cube forum (159 nodes, to August 2013). It confirms the
  early 4x4x4 history: node 54 (Norskog, April 2006, 4x4x4 squares set), node 62 (Norskog, 79 STM, five stages),
  node 73 (Norskog, February 2007, 68 block turns), node 93 (Norskog, July/August 2007, 77 single-slice turns, then
  82 twist turns and 67 block turns). It has no bounds for 5x5x5 or larger. Node 30 (Jaap Scherphuis, 2005): the
  4x4x4 centres in at most 22 single-slice moves (two opposite colours onto any opposite pair in 9, then the
  other four in 13 with 63,063,000 positions; any final orientation). That is the same state space as our
  diagonal UD..LFRB stage (63,063,000) at 13 against our 18. Ours must keep every other centre orbit on every N
  and fix the frame; a 4x4x4-only solve has neither constraint.

References (fetched 2026-11-06; Speedsolving pages block automated reading, so [1][3][4] are as quoted by search
summaries and mirrors):
1. Speedsolving wiki, God's Algorithm, table of God's numbers: https://www.speedsolving.com/wiki/index.php?title=God%27s_Algorithm
2. W. Randelshofer, Virtual Cubes, Revenge Cube history of upper bounds, citing the Domain of the Cube forum
   (cubezzz.dyndns.org nodes 54, 73, 93, 525, 541): https://www.randelshofer.ch/rubik/virtual_cubes/revenge/instructions/4x_instructions_big.html
3. Speedsolving forum, "5x5x5 OBTM upper bound" (143 -> 141 -> 135 -> 134 -> 130 OBTM, 2017-2018): https://www.speedsolving.com/threads/5x5x5-obtm-upper-bound.61270/
4. Lower bounds: Speedsolving wiki [1]; T. Rokicki's counting lower bounds for NxNxN (2011, Domain of the Cube forum).
5. E. D. Demaine, M. L. Demaine, S. Eisenstat, A. Lubiw, A. Winslow, "Algorithms for Solving Rubik's Cubes", ESA 2011,
   arXiv:1106.5736: https://arxiv.org/abs/1106.5736
6. r/Cubers, "I found a general formula for an upper bound of ..." (2018), with a comment giving 36 n^2 + O(n):
   https://old.reddit.com/r/Cubers/comments/8chfuu/i_found_a_general_formula_for_an_upper_bound_of/
7. Our nxnxn paper (~/arXiv_nxnxn_paper): centres in 36 n^2 - 144 n + 144; whole cube 30.5 n^2 / lg n + o(n^2 / lg n).

## Stage by stage (what the polynomial adds up)

Solving order: every oblique pair, then the mids (odd n), then the diagonals, then corners (and midges on odd
n), then the wing orbits.  Each stage keeps what earlier stages solved (as methods.md states for each); edges
and corners may be scrambled until their own stages.  With k = (n-2)/2 (even) or (n-3)/2 (odd):

| component | how many | stages (worst line per stage) | per orbit | boundary merges | status |
|---|---|---|---|---|---|
| oblique chiral pair, P | k(k-1)/2 | 1ud 17 (both orbits' U pieces; other obliques keep U and D), 2ud 18 (D pieces), 3ax 23 (F/B vs L/R pieces onto their axis), 4op 22 (F vs B, L vs R) | **80** plain | 1ud + 2ud: 35 -> 33 (checked); 3ax + 4op: 45 -> 38 (checked) | 80 certified |
| mid (T-centre) orbit, M (odd n) | k | UD 18 (U and D pieces), LFRB 21 (the side ring) | **39** | none | certified |
| diagonal (X-centre) orbit, D | k | D 10, U 16 (D kept), LFRB 18 (U, D kept) | **44** | none | certified |
| corners (even n) | 1 | one stage, face turns only | **11** | none | exact (full BFS) |
| corners + midges (odd n) | 1 | one 3x3x3 solve (always a legal 3x3x3 position: earlier stages turn only faces and non-middle slices, apart from mid algorithms that restore the middle centre) | **20** | none | God's number |
| wing orbit, W | k | 4 wings 32, 4 wings 32, 5 wings 40, last 11 wings 58 (keep every centre, corners, midges, other wing orbits; parity in the last stage) | **162** | none | certified (149 + 4 once by 3-cycles: sketch) |

Even n: P k(k-1)/2 + (D + W) k + 11 with k = (n-2)/2, which is the polynomial above; odd n: P k(k-1)/2 +
(D + M + W) k + 20 with k = (n-3)/2.  Centres only: P k(k-1)/2 + D k (+ M k on odd n).

## Status (2026-11-13)

- **P <= 69 checked** (dry run, 11-13 01:01): UD 31 + AX 38.  UD 31: gpuboundx covered all 7,958,829 1ud classes with a line over 13 in passes a (--stage-acts 4: 129,906 left), b (--stage-acts 5: 16,842), h (L4 + pools pair3/p8s10/p9/c12x/eg8, --prop 10: 3,796), i (L5 + pools: 2,820), j (+ --deep 2: 50), k (+ --slack 7 --deep 3: 0); gpubound --keep ud PASS: 220,726,760,863,456 colourings tried, 0 not solved; 63,649,344 1ud states with dA > 13, 0 without a block; CPU = GPU on 1,592 blocks (11.2 h, with --reorder: lines tried most-used first).  Evidence ~/orbitsolve-certsafe/obl23pair-P69-evidence-2026-11-13 (blocks 24.5 GB .zst, logs, scripts, sources).
- P <= 70 checked (dry run, 11-10 20:05): UD 32 (4,087,739 blocks, nearly all from stage moves only; gpubound PASS, 6.39e13 colourings, 0 not solved) + AX 38.  Evidence ~/orbitsolve-certsafe/obl23pair-P70-evidence-2026-11-10.
- P <= 71 checked (dry run): UD 33 + AX 38 (gpuaxbx covered all 7,263,089 classes in passes broad..f; gpuaxbcheck PASS 11-08 23:52: 147,437,136,202,996 positions tried, 0 not solved; 116,080,766 3ax states with d3 > 16, 0 without a block; CPU = GPU on 1,453 blocks; 10.6 h).  Same four stages as P 72.  Evidence ~/orbitsolve-certsafe/obl23pair-P71-evidence-2026-11-08 (logs, scripts, sources); the blocks (bnd/g38m.txt.zst, 7.9 GB) were deleted by a script glob bug and restored from an APFS snapshot, not yet copied into the evidence dir.
- P <= 72 checked (dry run): UD 33 (gpuboundx covered 981,333 classes; gpubound PASS, 5.24e12 colourings, 0 not solved) + AX 39 (gpuaxbx covered all 5,071,318 classes; gpuaxbcheck PASS, 4.17e13 positions, 0 not solved, 11-04 23:18).  All four stages = the obl23pair-axis-2026-11-04 certificate.  Evidence ~/orbitsolve-certsafe/obl23pair-P72-evidence-2026-11-05.  Boundary certificates at home (AX 39 blocks 10.3 GiB plain, UD 33 1.31 GiB).
- Tops of the certified stages: 1ud 5,294 at 17, 2ud 287,575 at 18, 3ax 549 at 23, 4op 14,965 at 22.  Later folds not yet in a certificate: arena fastsolve folds 2026-11-13 (1ud 2,891 at 17, 2ud 136,081 at 18, 3ax 546 at 23, 4op 14,541 at 22; see Working tables); earlier 1ud r1ud-si4, 2ud r2ud-si5 (217,884 at 18).
- Whole-cube solver from the certificates: certsolve (repo staged in ~/orbitsolve/repo; timing at the bottom of this file).
- **Table size for a solver:** about 28 MB (2.25M stored lines; the rest are one stage move from a shorter class).

## Current tables (classes by length; P <= 80 plain)

P = 1ud + 2ud + 3ax + 4op, each stage from the previous one's goal, classes up to the stage symmetries.  1ud and 2ud: /var/tmp/orbitsolve/bcert/ud34/snap (r1ud-si4, r2ud-si2 + pulls; certcheck PASS, 2026-11-04 08:17); 3ax: /var/tmp/orbitsolve/opt3ax/merge/ax-m3.tab (merge m3: hq23 -si + tails), 4op: /var/tmp/orbitsolve/op/4op-m1.tab (4op merge m1: all-moves -si, 2026-11-03).

| length | 1ud | 2ud | 3ax | 4op |
|---|---|---|---|---|
| 0 | 1 | 1 | 1 | 1 |
| 3 | 12 |  |  |  |
| 4 | 32 |  | 4 | 4 |
| 5 | 256 | 16 | 22 | 6 |
| 6 | 1,231 | 44 | 58 | 8 |
| 7 | 5,619 | 212 | 101 | 37 |
| 8 | 23,338 | 326 | 506 | 125 |
| 9 | 99,080 | 1,280 | 2,022 | 268 |
| 10 | 381,391 | 11,168 | 8,088 | 636 |
| 11 | 1,101,171 | 22,916 | 24,726 | 1,611 |
| 12 | 1,813,481 | 48,777 | 77,071 | 4,469 |
| 13 | 2,735,410 | 140,043 | 225,254 | 12,142 |
| 14 | 3,871,090 | 267,978 | 444,068 | 30,650 |
| 15 | 3,106,406 | 427,438 | 829,220 | 54,619 |
| 16 | 976,039 | 673,843 | 1,492,054 | 86,332 |
| 17 | 5,294 | 1,055,318 | 2,191,771 | 165,453 |
| 18 |  | 287,575 | 2,363,691 | 305,543 |
| 19 |  |  | 1,633,820 | 406,304 |
| 20 |  |  | 828,636 | 311,675 |
| 21 |  |  | 208,902 | 113,539 |
| 22 |  |  | 35,720 | 14,965 |
| 23 |  |  | 549 |  |
| **classes** | **14,119,851** | **2,936,935** | **10,366,284** | **1,508,387** |
| **longest** | **17** | **18** | **23** | **22** |

## Working tables (2026-11-13 10:44; arena fastsolve tail folds, not yet in a certificate)

Base: the expanded P 80 certificate (certsolve --expand: 1ud 17s 5,272, 2ud 18s 286,263; optfs/full).  Arena fastsolve
(300 GB tables, D=9) searched the tails of the longest lines for shorter replacements; each fold merges the improved
lines (vline-checked), turns every shortened tail into an action for certextend (plus --proptop 16 pulls), and checks
the result (1ud/2ud certcheck --keep ud PASS; 4op opbound replay + axcheck PASS).  Stages: /var/tmp/orbitsolve/fsfold/
1ud-f16, 2ud-f16, 4op-f7u, 3ax-f6.
Tops: 1ud 17s 5,272 -> **2,891**; 2ud 18s 286,263 -> **136,081**; 4op 22s 14,965 -> **14,541**; 3ax 23s 549 -> **546**.
Longest per stage unchanged, so P stays 80 plain.

| length | 1ud (f16) | 2ud (f16) | 3ax (f6) | 4op (f7u) |
|---|---|---|---|---|
| 0 | 1 | 1 | 1 | 1 |
| 3 | 12 |  |  |  |
| 4 | 32 |  | 4 | 4 |
| 5 | 256 | 16 | 22 | 6 |
| 6 | 1,231 | 44 | 58 | 8 |
| 7 | 5,619 | 212 | 101 | 37 |
| 8 | 23,338 | 326 | 506 | 125 |
| 9 | 99,131 | 1,280 | 2,022 | 268 |
| 10 | 382,173 | 11,168 | 8,088 | 636 |
| 11 | 1,100,792 | 22,916 | 24,726 | 1,611 |
| 12 | 1,836,542 | 48,784 | 77,071 | 4,469 |
| 13 | 2,793,353 | 140,628 | 225,254 | 12,143 |
| 14 | 4,002,008 | 275,607 | 444,068 | 30,653 |
| 15 | 2,919,562 | 464,464 | 829,220 | 54,620 |
| 16 | 952,910 | 819,728 | 1,492,054 | 86,327 |
| 17 | 2,891 | 1,015,680 | 2,191,771 | 165,467 |
| 18 |  | 136,081 | 2,363,691 | 305,609 |
| 19 |  |  | 1,633,820 | 406,381 |
| 20 |  |  | 828,637 | 311,943 |
| 21 |  |  | 208,912 | 113,538 |
| 22 |  |  | 35,712 | 14,541 |
| 23 |  |  | 546 |  |
| **classes** | **14,119,851** | **2,936,935** | **10,366,284** | **1,508,387** |
| **longest** | **17** | **18** | **23** | **22** |

## Certificates

Each directory holds MANIFEST and stages/ (plus boundaries/ where approved); checkers in methods.md.

| result | where |
|---|---|
| **P 80, all four stages lowered 2026-11-04** (tops: 1ud 5,294 at 17, 2ud 287,575 at 18, 3ax 549 at 23, 4op 14,965 at 22) | ~/orbitsolve-certsafe/obl23pair-axis-2026-11-04 (certcheck --keep ud and axcheck PASS; independent .tws checks udindcheck.py / axindcheck.py PASS on all four stages; read-only; checks and SHA256SUMS in certruns/cert-2026-11-04) |
| **P 80** (1ud 17 + 2ud 18 + 3ax 23 + 4op 22) | ~/orbitsolve-certsafe/obl23pair-axis-P80-2026-11-01 |
| **P 69 evidence** (UD 31 blocks, search/merge/check logs, scripts, sources; AX 38 as P 71) | ~/orbitsolve-certsafe/obl23pair-P69-evidence-2026-11-13 |
| P 70 evidence (UD 32 blocks, logs, scripts, sources) | ~/orbitsolve-certsafe/obl23pair-P70-evidence-2026-11-10 |
| P 71 evidence (AX 38 search and check logs, scripts, sources; blocks to be added) | ~/orbitsolve-certsafe/obl23pair-P71-evidence-2026-11-08 |
| P 72 evidence (UD 33 + AX 39 blocks, check logs) | ~/orbitsolve-certsafe/obl23pair-P72-evidence-2026-11-05 |
| P 73 evidence (UD 33 + AX 40) | ~/orbitsolve-certsafe/obl23pair-P73-evidence-2026-11-04 |
| P 77 evidence (42 boundary blocks, check log) | ~/orbitsolve-certsafe/obl23pair-axis-P77-evidence-2026-11-02 |
| UD 34 evidence (1ud/2ud boundary blocks, boundcheck PASS) | ~/orbitsolve-certsafe/obl23pair-UD34-evidence-2026-11-04 |
| 3ax/4op working certificate | ~/orbitsolve-certsafe/obl23pair-axis-2026-10-29 (3ax c45, 4op c32) |
| P 80 with the rings (superseded) | ~/orbitsolve-certsafe/obl23pair-relaxed-P80-2026-10-27 |
| D 44, M 39, W 162 | cert/diag/centres-first, cert/mid/centres-first, cert/wing/centres-kept |

## Open

- Arena fastsolve campaign (2026-11-11..): 1ud (t16s) and 2ud (t15s) tail searches running, one per socket; 4op stopped 11-12 21:54 (t14/t15 hit rate ~0.4%), 3ax stopped 11-13 02:06 (t14/t15 done; t16 searches ~7-8 h each); refold every ~2 h (journal.md).  Folds start from the expanded P 80 stages (optfs/full).  Working tables above; certificate update when the user asks.
- At home: arena-plan.md (deep in-memory tables on arena, 1.5 TB; move sets and campaign order).
- AX 38 (P 71): copy the restored blocks into the evidence dir.
- 1ud: resume the -si run (full lines from chunk caaagh, tails2 15s) and fold; 2ud: running.  Neither the r1ud-si3 stage nor 2ud-t is in a certificate yet.
- Write ax-m3 / 4op-m1 into the certificate and axcheck; axbcheck 41 (and 40) at home; boundary certificates at home.
- Later: UD 33 (needs fewer 2ud 18s; GPU boundary, TODO.md L); a dense coset solver for 3ax+4op (idea, user's call; journal.md).
- twsearch: the table-load fix is upstream (c73aa20); shared-memory options on branch shm-tables (CI green, no PR yet); still to upstream: the -si bug, --prefixslack, the full-disk write guard (twsearch.md).

## Solver timing (certsolve, 2026-11-07)

100x100x100, seed 100: 1,176 oblique pairs; laptop (M3 Max), single-threaded search, load 2–3.6 from the GPU agent, so cycles and instructions (proc_pid_rusage) count more than wall time. One cube and a heavy-tailed distribution, so the means are rough. Bounds below the certified ones are empirical: the solver searched until each pair fitted, it is not a proof.

Fixed costs: oblique tables 1.6–1.9 s; boundary actions 19.4 s (222 G instr, 71 G cycles, single-threaded, only with --bound); other tables 0.6 s.

Plain (table lines only, P 80, no search): phase 1 16 ms, phase 2 12 ms (0.014 / 0.011 ms, ~0.05 M cycles per pair); whole solve 1.98 s.

**1ud + 2ud budget** (3ax + 4op at 45, never searched):

| UD | pairs searched | per searched pair | cycles mean / max | phase |
|---|---|---|---|---|
| 34 | 0 | – | – | 0.04 s |
| 33 | 12 | 2.81 s | 10.2 G / 15.9 G | 33.8 s |
| 32 | 57 | 2.21 s | 8.1 G / 13.2 G | 126 s |
| 31 | 206 | 2.04 s | 7.4 G / 14.3 G | 420 s |
| 30 | – | – | – | over 30 min (stopped) |

**3ax + 4op budget** (1ud + 2ud at 35, never searched):

| AX | pairs searched | per searched pair | cycles median / mean / max | phase |
|---|---|---|---|---|
| 44, 43 | 0 | – | – | 0.01 s |
| 42 | 1 | 353 ms | 0.82 G | 0.38 s |
| 41 | 6 | 181 ms | – / 0.51 / 0.80 G | 1.1 s |
| 40 | 26 | 145 ms | – / 0.47 / 0.81 G | 3.8 s |
| 39 | 69 | 134 ms | – / 0.45 / 1.02 G | 9.3 s |
| 38 | 158 | 107 ms | – / 0.36 / 0.95 G | 16.9 s |
| 37 | 323 | 96 ms | – / 0.33 / 0.96 G | 31.0 s |
| 36 | 506 | 87 ms | – / 0.30 / 0.99 G | 44.0 s |
| 35 | 691 | 2.94 s | 0.25 / 10.5 / 3,602 G | 2,032 s |
| 34 | – | – | – | over 90 min (stopped) |

At AX 35 all pairs fitted (worst 69, mean 62.06), but one pair took about 1,000 s, half the phase; the other 690 had median 0.25 G cycles (~70 ms), 99% 0.70 G. Down to 36 one action plus a table line nearly always suffices; at 35 more pairs need the second (deep) action, and the tail grows sharply. Every finished run: 0 over a bound.

N=20 (180 pairs) with the budgets below the proved P 72: P 69 as 32+37, 33+36 and 31+38, and P 67 as 31+36 (82 s), all fitted; P 65 (30+35) stopped after 9 min.
