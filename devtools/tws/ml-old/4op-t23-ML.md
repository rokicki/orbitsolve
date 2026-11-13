# ML task: the 4op classes at 25, refreshed (37 classes)

**Goal.**  Find solving sequences of **at most 24 moves** for the scrambles in `4op25-t23.scr`.
Your 25 lines so far (1,607 in `4op25_live.txt`) have cut level 25 from 1,073 to 37 classes.
These 37 are the whole of level 25: getting all of them to 24 lowers the 4op bound to 24.  This
list replaces `4op25-t21.scr` and the earlier t13, t15, t18 and t20 lists; every class on it is
also on those.  Lines for classes no longer listed are harmless but no longer needed.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8op.tws` (the puzzle, unchanged);
`4op25-t23.scr`, 37 `ScrambleAlg` blocks named `4op25-t23-NNN-L25`, one per symmetry class;
`4op25-t23.sol`, the current 25-move solutions, `NAME<TAB>moves`.  Every scramble and solution has
been replayed on obl8op.tws.

**Output.**  Append to `~/mlcube/4op25_live.txt` as before, one solving sequence per line, moves
only.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass would take more than
a day.

**Update (2026-10-29 14:45).**  Superseded by `4op-t26-ML.md` (11 classes left at 25, then the 24s).
