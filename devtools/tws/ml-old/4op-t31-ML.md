# ML task: the 4op classes at 24, refreshed (1,952 classes)

**Goal.**  Find solving sequences of **at most 23 moves** for the scrambles in `4op24-t31.scr`.
Your 1,331 lines for the 24s and arena's 1,846 reduced-move-set optima, with what they pulled down,
cut level 24 from 12,291 to 1,952 classes.  These are the whole of level 24: getting all of them to
23 lowers the 4op bound to 23.  This list replaces `4op24-t26.scr`; every class on it is also on that
one.  The reduced move set (F R B L 2U2 3U2 2D2 3D2) has worked well here: arena's optima are mostly
21 and 22.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8op.tws` (the puzzle, unchanged);
`4op24-t31.scr`, 1,952 `ScrambleAlg` blocks named `4op24-t31-NNN-L24`, one per symmetry class;
`4op24-t31.sol`, the current 24-move solutions, `NAME<TAB>moves`.  Every scramble and solution has
been replayed on obl8op.tws.

**Output.**  Append to `~/mlcube/4op24_live.txt` as before, one solving sequence per line, moves
only.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass would take more than
a day.

**Update (2026-10-29 18:40).**  Superseded by `4op-t33-ML.md` (946 classes left at 24).
