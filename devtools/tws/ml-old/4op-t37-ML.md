# ML task: the 4op classes at 24, the last 10 classes

**Goal.**  Find solving sequences of **at most 23 moves** for the scrambles in `4op24-t37.scr`.
Your 2,854 lines for the 24s and arena's 2,952 reduced-move-set optima, a move-merging pass, with what they pulled down,
cut level 24 from 12,291 to 10 classes.  These are the whole of level 24: getting all of them to
23 lowers the 4op bound to 23.  This list replaces `4op24-t35.scr` and the earlier 24 lists; every class on it is also on those.
The reduced move set (F R B L 2U2 3U2 2D2 3D2) has worked well here: arena's optima are mostly
21 and 22.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8op.tws` (the puzzle, unchanged);
`4op24-t37.scr`, 10 `ScrambleAlg` blocks named `4op24-t37-NNN-L24`, one per symmetry class;
`4op24-t37.sol`, the current 24-move solutions, `NAME<TAB>moves`.  Every scramble and solution has
been replayed on obl8op.tws.

**Output.**  Append to `~/mlcube/4op24_live.txt` as before, one solving sequence per line, moves
only.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass would take more than
a day.

**Update (2026-10-29 22:15).**  Done: 4op has no 24s any more (<= 23).  See `4op-t39-ML.md`.
