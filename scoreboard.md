# Scoreboard: oblique chiral pair P (boundary passes)

P = (1ud + 2ud) + (3ax + 4op).  Plain sum 16 + 18 + 23 + 22 = 79 (certified 11-24; was 80).  A boundary pass bounds a
pair of consecutive stages together.  "Covered" = the search tool found blocks for every class; "checked"
= an independent checker passed on those blocks.  All boundary results are dry runs (no boundary
certificates until home).  Details: journal.md; methods: methods.md ("Boundary passes").

## Best so far

| P | UD (1ud + 2ud) | AX (3ax + 4op) | status |
|---|---|---|---|
| 79 | 34 (plain) | 45 (plain) | certified 11-24 (obl23pair-axis-2026-11-24; 80 before) |
| 77 | 35 | 42 | checked (axbcheck PASS, 11-01) |
| 76 | 34 | 42 | checked (UD 34: boundcheck PASS 11-04, gpubound PASS) |
| 75 | 34 | 41 | checked (AX 41: gpuaxbcheck PASS 11-04 11:15, snap-41 stages) |
| 74 | 34 | 40 | checked (AX 40: gpuaxbx covered, gpuaxbcheck PASS 11-04 12:46) |
| 73 | 33 | 40 | checked (UD 33: gpuboundx covered, gpubound PASS 11-04 14:40); evidence ~/orbitsolve-certsafe/obl23pair-P73-evidence-2026-11-04 |
| **72** | **33** | **39** | **checked** (AX 39: gpuaxbx covered 11-04 20:19, gpuaxbcheck PASS 11-04 23:18); all four stages = the obl23pair-axis-2026-11-04 certificate; evidence ~/orbitsolve-certsafe/obl23pair-P72-evidence-2026-11-05 |
| **71** | 33 | 38 | **checked (dry run), 11-08 23:52**: AX 38 gpuaxbcheck PASS (147,437,136,202,996 positions tried, 0 not solved; 116,080,766 3ax states with d3 > 16, 0 without a block; CPU = GPU on 1,453 blocks); evidence ~/orbitsolve-certsafe/obl23pair-P71-evidence-2026-11-08 |
| **70** | 32 | 38 | **checked (dry run), 11-10 20:05**: UD 32 gpubound --keep ud PASS (63,869,344,227,297 colourings tried, 0 not solved; 32,685,262 1ud states with dA > 14, 0 without a block; CPU = GPU on 818 blocks); blocks from stage moves only (--stage-acts 4/5/6) except 3 classes (+ pools, prop 10); evidence ~/orbitsolve-certsafe/obl23pair-P70-evidence-2026-11-10 |
| **69** | 31 | 38 | **checked (dry run), 11-13 01:01**: UD 31 gpubound --keep ud PASS (220,726,760,863,456 colourings tried, 0 not solved; 63,649,344 1ud states with dA > 13, 0 without a block; CPU = GPU on 1,592 blocks); 7,958,829 blocks from passes a (--stage-acts 4), b (5), h/i (+ pools, prop 10), j/k (+ --deep); evidence ~/orbitsolve-certsafe/obl23pair-P69-evidence-2026-11-13 |

## Levels

| level | search | check | notes |
|---|---|---|---|
| UD 34 | boundx: 5,294 of 5,294 covered (304 s, 5 threads); gpuboundx: same (110 s) | boundcheck PASS (378 s on 16 threads, idle); gpubound PASS (6.9 s) | evidence ~/orbitsolve-certsafe/obl23pair-UD34-evidence-2026-11-04 |
| UD 33 | gpuboundx: 981,302 of 981,333 covered by one action (3.8 h, 2 threads); the 31 left (37 colourings) by --deep 1 --deepprop 6 (214 s) | **gpubound PASS**: 5.24e12 colourings, 0 not solved (2,709 s) | blocks gpubnd/x33m.txt |
| AX 42 | axbx: 350,629 covered | axbcheck PASS (19,046 s, 14 threads); gpuaxbcheck PASS, same counts (120 s, 2 threads) | evidence ~/orbitsolve-certsafe/obl23pair-axis-P77-evidence-2026-11-02 |
| AX 41 | axbx: 1,326,357 covered (broad 18,231 s + wider pass on 1,563) | axbcheck 1/50 sample (5,245 s, 4 threads); **gpuaxbcheck full PASS**: 2.71e12 positions, 0 not solved (557 s, 2 threads) | blocks bnd/x41m.txt |
| UD 32 | gpuboundx --stage-acts: 4,087,739 classes; a (L4, 6.6 h) leaves 7; b (L5) 3; c (L6) 3; h (L4 + pools prop 10) 0 | **gpubound PASS**: 6.39e13 colourings, 0 not solved (10,877 s, 11-10 20:05) | blocks gpubnd/s32m.txt (18 GB); first attempt with pools (11-06) stopped: ~2/3 of the 16s not covered by one action |
| AX 39 | gpuaxbx: all 5,071,318 covered (broad 5.4 h leaves 87,285; b 87,151; c 21; d 112; e 1) | **gpuaxbcheck PASS**: 4.17e13 positions, 0 not solved (3.0 h) | blocks bnd/g39m.txt.zst |
| AX 38 | gpuaxbx: all 7,263,089 covered (broad 11-04 17:39 .. 11-05 22:21 leaves 749,148; b 742,118 (34 h incl. pauses); c 2,007; d (slack 5) 4,859; e (slack 6) 162; f 2) | **gpuaxbcheck PASS**: 1.47e14 positions, 0 not solved (10.6 h, 11-08 23:52) | blocks bnd/g38m.txt.zst (deleted by an ax38b.sh glob bug, restored from the 11-08 23:03 snapshot); evidence ~/orbitsolve-certsafe/obl23pair-P71-evidence-2026-11-08 |
| AX 40 | gpuaxbx: 2,707,627 covered (broad 39 min, leaves 7,626; b 411 s leaves 3; c 67 s) | **gpuaxbcheck PASS**: 1.03e13 positions, 0 not solved (1,788 s, 6 threads) | blocks bnd/g40m.txt; CPU estimate was 10-17 h + ~5 days check |

## Tools: CPU vs GPU (UD 34 rows: idle machine, 16 threads, 11-04)

| check | wall | notes |
|---|---|---|
| boundcheck (CPU) | 378 s | reference |
| gpubound (GPU) | 6.9 s | same counts; check-4 loop 1.3 s vs 424 s on 16 CPU threads (~325x) |
| axbcheck (CPU), AX 42 | 19,046 s | 14 threads, 11-01 (not idle-machine timing) |
| gpuaxbcheck (GPU), AX 42 | 120 s | 2 threads + GPU, machine busy; same 383,406,031,310 tries |
| axbcheck (CPU), AX 41 1/50 sample | 5,245 s | 4 threads |
| gpuaxbcheck (GPU), AX 41 1/50 sample / full | 68 s / 557 s | 2 threads + GPU; sample: same 65,173,930,310 tries |
| axbx (CPU) / gpuaxbx (GPU), AX 41 1/1000 sample | 46 s / 86 s | 2 threads each; identical results (gpuaxbx time mostly table loading) |
| gpuaxbx, AX 40 full | ~47 min | 6 threads + GPU, all passes (CPU estimate 10-17 h broad on 15 threads) |
| gpuaxbcheck, AX 40 full | 1,788 s | 6 threads + GPU |

## Overnight log (automatic, axnight.sh)

- 11-04 23:18 **AX 39 PASS** (gpuaxbcheck): blocks: 0 bad 41739412487793 positions tried, 0 not solved; frame errors 0; 3ax states with d3 > 17: 81041362, 0 without a block; CPU sample: 1015 blocks, 0 differ from the GPU  => P <= 72 with UD 33
- 11-05 20:33 AX 38: broad pass stopped at 7,236,352 of 7,263,089 classes (one class overflowed the left-state buffer; fixed in gpuaxbx); the last 26,737 classes resumed 20:36 by ax38b.sh
- 11-05 22:21 AX 38 broad pass on the rest: classes: 13749 covered, 12988 not (0 duplicate lines skipped)
- 11-05 22:21 AX 38 broad pass (all):   749148 classes left
- 11-07 08:42 AX 38 pass b (--prop 10 --slack 4 --deep 4 --deepprop 8): classes: 742118 covered, 7030 not (0 duplicate lines skipped)
- 11-07 21:07 AX 38 pass c (--prop 10 --slack 4 --deep 4): classes: 2007 covered, 5023 not (0 duplicate lines skipped)
- 11-08 09:20 AX 38 pass d (--prop 10 --slack 5 --deep 5): classes: 4859 covered, 164 not (0 duplicate lines skipped)
- 11-08 13:11 AX 38 pass e (--prop 11 --slack 6 --deep 6): classes: 162 covered, 2 not (0 duplicate lines skipped)
- 11-08 13:12 AX 38 pass f (--prop 11 --slack 7 --deep 7 --deepprop 9): classes: 2 covered, 0 not (0 duplicate lines skipped)
- 11-08 13:12 AX 38 covered (all classes); merge
- 11-08 23:52 **AX 38 PASS** (gpuaxbcheck): blocks: 0 bad 147437136202996 positions tried, 0 not solved; frame errors 0; 3ax states with d3 > 16: 116080766, 0 without a block; CPU sample: 1453 blocks, 0 differ from the GPU  => P <= 71 with UD 33
- 11-10 09:31 UD 34 (stage moves) pass a (--stage-acts 4): classes: 5294 covered, 0 not (0 duplicate lines skipped)
- 11-10 09:31 UD 34 (stage moves) covered; merge and check
- 11-10 09:31 **UD 34 PASS** (gpubound --keep ud, stage-move blocks): blocks: 0 bad 25990741879 colourings tried, 0 not solved; frame errors 0; A states with dA > 16: 42244, 0 without a block; CPU sample: 2 blocks, 0 differ from the GPU 
- 11-10 09:33 UD 34 (stage moves) pass a (--stage-acts 4): classes: 5294 covered, 0 not (0 duplicate lines skipped)
- 11-10 09:33 UD 34 (stage moves) covered; merge and check
- 11-10 09:33 **UD 34 PASS** (gpubound --keep ud, stage-move blocks): blocks: 0 bad 25990741879 colourings tried, 0 not solved; frame errors 0; A states with dA > 16: 42244, 0 without a block; CPU sample: 2 blocks, 0 differ from the GPU 
- 11-10 10:15 UD 33 (stage moves) pass a (--stage-acts 4): classes: 981333 covered, 0 not (0 duplicate lines skipped)
- 11-10 10:15 UD 33 (stage moves) covered; merge and check
- 11-10 10:24 **UD 33 PASS** (gpubound --keep ud, stage-move blocks): blocks: 0 bad 5257715382342 colourings tried, 0 not solved; frame errors 0; A states with dA > 15: 7844800, 0 without a block; CPU sample: 197 blocks, 0 differ from the GPU 
- 11-10 16:58 UD 32 (stage moves) pass a (--stage-acts 4): classes: 4087732 covered, 7 not (0 duplicate lines skipped)
- 11-10 16:59 UD 32 (stage moves) pass b (--stage-acts 5): classes: 4 covered, 3 not (0 duplicate lines skipped); peak 3703846848 bytes
- 11-10 17:01 UD 32 (stage moves) pass c (--stage-acts 6): classes: 0 covered, 3 not (0 duplicate lines skipped); peak 13051082688 bytes
- 11-10 17:01 UD 32 (stage moves):        3 classes not covered (/var/tmp/orbitsolve/gpubnd/s32c.left)
- 11-10 20:05 **UD 32 PASS** (gpubound --keep ud; blocks: stage-move passes L4/L5/L6 + pools for the last 3 classes): blocks: 0 bad 63869344227297 colourings tried, 0 not solved; frame errors 0; A states with dA > 14: 32685262, 0 without a block; CPU sample: 818 blocks, 0 differ from the GPU  => P <= 70 with AX 38
- 11-10 20:16 UD 32 used-actions pass a: classes: 4087732 covered, 7 not (0 duplicate lines skipped); used actions: 136360 of 148429 (/var/tmp/orbitsolve/gpubnd/u32a.acts); u32a.txt identical to s32a.txt (sha256 7ed399211343808325e8eab502337b09b833b14943482c724a06a59ddda89891; gpuboundx.ua vs gpuboundx.new)
- 11-10 20:17 UD 32 used-actions pass b: classes: 4 covered, 3 not (0 duplicate lines skipped); used actions: 4802 of 2295345 (/var/tmp/orbitsolve/gpubnd/u32b.acts)
- 11-10 20:17 UD 32 used-actions pass h: classes: 3 covered, 0 not (0 duplicate lines skipped); used actions: 272 of 9599869 (/var/tmp/orbitsolve/gpubnd/u32h.acts)
- 11-11 12:07 UD 32 with the top 10,000 of u32a.acts only: classes: 4087099 covered, 640 not (0 duplicate lines skipped); bad colourings 17710036609176, left 5262; used actions: 10000 of 10000 (/var/tmp/orbitsolve/gpubnd/t32.acts)
- 11-11 17:28 UD 31 pass a: 7828923 of 7,958,829 classes covered,   129906 not (second part: classes: 2715233 covered, 122045 not (0 duplicate lines skipped), 61188.44 s)
- 11-12 01:35 UD 31 (uduse2) pass b (--stage-acts 5): classes: 113064 covered, 16842 not (0 duplicate lines skipped); used actions: 1305810 of 2295345 (/var/tmp/orbitsolve/gpubnd/u31b.acts); 26031.49 s
- 11-12 02:04 UD 31 (uduse2) pass h (--stage-acts 4 --prop 10 + pools): classes: 13046 covered, 3796 not (0 duplicate lines skipped); used actions: 272254 of 9599869 (/var/tmp/orbitsolve/gpubnd/u31h.acts); 1794.33 s
- 11-12 03:28 UD 31 (uduse2) pass i (--stage-acts 5 --prop 10 + pools): classes: 976 covered, 2820 not (0 duplicate lines skipped); used actions: 261117 of 11746201 (/var/tmp/orbitsolve/gpubnd/u31i.acts); 5007.54 s
- 11-12 03:44 UD 31 (uduse2) pass j failed: time: signal: Invalid argument
- 11-12 09:40 UD 31 (uduse2) pass j (--stage-acts 4 --prop 10 + pools): classes: 2770 covered, 50 not (0 duplicate lines skipped); used actions: 189947 of 9599869 (/var/tmp/orbitsolve/gpubnd/u31j.acts); 4323.02 s
- 11-12 09:41 UD 31 (uduse2) pass k (--stage-acts 4 --prop 10 + pools): classes: 50 covered, 0 not (0 duplicate lines skipped); used actions: 16381 of 9599869 (/var/tmp/orbitsolve/gpubnd/u31k.acts); 41.36 s
- 11-12 09:41 UD 31 (uduse2) covered; merge and check
- 11-13 01:01 **UD 31 PASS** (gpubound --keep ud --reorder 6 --reorder-iters 3 --k1 8; uduse2.sh blocks): blocks: 0 bad 220726760863456 colourings tried, 0 not solved; frame errors 0; A states with dA > 13: 63649344, 0 without a block; CPU sample: 1592 blocks, 0 differ from the GPU  (u31chk2.sh)
