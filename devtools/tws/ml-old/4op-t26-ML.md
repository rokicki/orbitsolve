# ML task: the last 11 4op classes at 25, then the 24s

**Goal 1.**  Find solving sequences of **at most 24 moves** for the 11 scrambles in
`4op25-t26.scr`.  Your 25 lines (1,630 in `4op25_live.txt`) have cut level 25 from 1,073 to 11.
These 11 are the whole of level 25: getting all of them to 24 lowers the 4op bound to 24.  This
list replaces `4op25-t23.scr` and every earlier 4op25 list.  They may be the hardest of the lot; if
one resists, report it rather than spending long on it.

**Goal 2 (next level).**  `4op24-t26.scr` has the 15,273 classes at 24.  Find sequences of **at
most 23 moves**, in file order.  This is a long list: report the hit rate after the first few
hundred.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8op.tws` (the puzzle, unchanged);
`4op25-t26.scr` / `.sol` (11 classes, names `4op25-t26-NNN-L25`); `4op24-t26.scr` / `.sol` (15,273
classes, names `4op24-t26-NNN-L24`).  The .sol files hold the current solutions, `NAME<TAB>moves`.
Every scramble and solution has been replayed on obl8op.tws.

**Output.**  The 25s: append to `~/mlcube/4op25_live.txt` as before.  The 24s: write to
`~/mlcube/4op24_live.txt`.  Write one solving sequence per line, moves only, each as soon as it is
found.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass over the 24s would
take more than a day.

**Update (2026-10-29 15:00).**  Goal 1 is done: no 4op class is at 25 any more (4op <= 24).  Work on goal 2, the 24s.

**Update (2026-10-29 18:10).**  Goal 2 is superseded by `4op-t31-ML.md` (1,952 classes left at 24).
