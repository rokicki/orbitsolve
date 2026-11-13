# ML task: the 4op classes at 25, refreshed (1,073 classes)

**Goal.**  Find solving sequences of **at most 24 moves** for the scrambles in `4op25-t13.scr`.
Your 692 lines for the 25s so far, together with a band pass, have cut level 25 from 2,479 to
1,073 classes.  This list replaces `4op25-t11.scr`: work on it in place of the rest of t11.  Lines
for t11 classes are still useful.  Getting all of these to 24 lowers the 4op bound to 24.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8op.tws` (the puzzle, unchanged);
`4op25-t13.scr`, 1,073 `ScrambleAlg` blocks named `4op25-t13-NNN-L25`, one per symmetry class;
`4op25-t13.sol`, the current 25-move solutions, `NAME<TAB>moves`.  Every scramble and solution has
been replayed on obl8op.tws.

**Output.**  Append to `~/mlcube/4op25_live.txt` as before, one solving sequence per line, moves
only.

**Limits.**  Use the watchdog with an MPS cap.  If a full pass would take more than a day, stop and
report the hit rate so far.

**Update (2026-10-29 10:45).**  Superseded by `4op-t20-ML.md` (340 classes left at 25).
