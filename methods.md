# Methods (brief reference)

Results: upperbound5.md.  Details and run logs: journal.md.

## The oblique chiral pair: four stages

A chiral pair is two oblique orbits a = (p, q) and b = (q, p).  Stages: 1ud (none .. aUbU), 2ud (aUbU ..
aDbD), then the axis split 3ax and 4op.  The same sequences work for every pair (p, q) (devtools/pairdepth).
P = 1ud + 2ud + 3ax + 4op as a plain sum, or less with a boundary pass between two stages.

### Order and side conditions (UD half)

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

(The rings 3f/4f in that list were replaced on 2026-11-01 by the axis split below.)

### Axis split: 3ax and 4op

| name | solves | positions | classes (16 symmetries) |
|---|---|---|---|
| **3ax** ("axis") | from the pair's U and D solved: every F/B piece onto F or B and every L/R piece onto L or R, both orbits | C(16,8)^2 = 165,636,900 | 10,366,284 |
| **4op** ("opposite") | from the 3ax goal: F vs B and L vs R, both orbits at once | 70^4 = 24,010,000 | 1,508,387 |

- **Symmetries:** the 8 rotations that keep the U-D axis, times the mirror (which swaps orbits a and b).
- **Side conditions:** both stages keep every other oblique orbit colour-solved; the diagonals may
  move (they are solved last).  So a 3ax line can be followed directly by a 4op line, and a 3ax/4op
  boundary pass is possible (alternative B; journal.md has the comparison with A, slice half twists
  in 3ax, which saved about 1 at the top).
- **Lower bound:** 4op >= 15 (it contains 4hb, exactly 15).
- **State encodings** (devtools/axbound.cpp): 3ax, per orbit 16 side slots (L F R B, 4 each; bit set =
  F/B piece), SOLVED 0xF0F0F0F0; 4op, per orbit 8 F/B slots (bit set = F piece) and 8 L/R slots (bit
  set = L piece), SOLVED 0x0F0F0F0F.
- In alternative A (phase 3 run on every pair first) the slice half-turns 2U2 3U2 2D2 3D2 are also stage moves in 3ax (axbound --slices).

## Lowering the stage tables

- **Fold** (devtools/axbound, opbound = the same source with OP4): distance table over every position, actions =
  outer turns (cost 1) and pool algorithms; propagation, bands, a merge pass (cancellations, commuting turns,
  final no-op outer turns dropped); --base TABLE --load LINES lowers an existing table; --save writes one line per
  class.  --onemove counts classes one stage move from a shorter class (and prefix-plus-lookup).
- **Sources of lines:** the ML agent (~/mlcube *_live.txt), twsearch -si passes on the certificate lines (all moves,
  then reduced sets; twsearch.md), shortenseqs, cheap scans (scan/cheapscan.py: merges and final no-op turns),
  suffixes of new lines at intermediate stage starts (op/suffixes.py).  Every new line is checked first
  (op/linecheck.py: solving, keeps the other orbits, stage start).
- Certificate lines are **solving** sequences; twsearch -s/-si read **generating** sequences: invert with twsearch -i.
- 1ud/2ud: certextend / rfeed.sh (certruns/relaxed), devtools/udonemove for one-move counts.

## Boundary passes

### Rings and UD (boundx / boundcheck)

(Full explanation of what boundcheck proves and why: boundcheck.md.)

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

GPU versions (devtools/gpu, Metal; UD-shaped stages only: one target group per orbit).  The colouring
loop tracks B's target pieces by position: L0 solves A's state, so a piece at B-state facelet p sits at
D_L0^-1(p) and a path P carries it to M(p), M = D_P o D_L0^-1 (40 bytes per path); one GPU thread per
(class, bad B state) moves the 8 pieces through each path's M, ranks them and reads dB.
gpubound (same arguments as boundcheck; checks 1-3 on the CPU, check 4 on the GPU; --cpu-sample N
re-runs every Nth block the boundcheck way and compares counts) gave the same counts as boundcheck on
UD 34, with check 4 taking 1.4 s instead of most of 872 s.  gpuboundx (boundx's arguments plus
--batch): the action search (every action on the class's A state, looked up in dA) and the colouring
loop on the GPU, path building on the CPU; no two-action (--deep) pass.  A third implementation: the
certificate check stays boundcheck until gpubound has matched it on the level in question.

### 3ax/4op (axbx / axbcheck)

devtools/axbx STAGE3AX STAGE4OP X: for every 3ax class with a + 22 > X, the 4op positions beyond X - a are
"bad"; alternatives (--prop, --slack, --deep) must carry each onto a 4op position within budget.  Broad pass
first, then wider passes on the leftover classes (--only).  --out writes blocks; devtools/axbcheck checks them
independently.  Scripts: /var/tmp/orbitsolve/bnd/run41.sh, run42.sh.  A class that cannot be covered rules
out that X.

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
- devtools/axcheck 3ax|4op STAGEDIR: the axis-split stages (start, coverage, side effects on 6..10).
- devtools/axbcheck: 3ax/4op boundary blocks.
- devtools/udindcheck.py 1ud|2ud STAGEDIR, devtools/axindcheck.py 3ax|4op STAGEDIR (pypy, 2026-11-04): independent
  second checks that use only the twsearch puzzles (devtools/tws/1ud-si, 2ud-si, obl8ax, obl8op .tws), nothing from
  certcube.h: legal moves, other orbits kept, stage start, and coverage by counting (every state exactly once under
  the stage symmetries).  8x8 (3ax/4op) or the 1ud/2ud stand-ins only, so the side effects on 6..10 stay certcheck's
  and axcheck's job.
