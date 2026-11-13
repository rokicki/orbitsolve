# ML task: the 3ax classes at 25 and 24, refreshed (67 and 525 classes)

**Goal 1.**  Find solving sequences of **at most 24 moves** for the 67 scrambles in `3ax25-t9.scr`.
Your 3ax lines and a new move-merging pass cut level 25 from 97 to 67 classes.  These 67 are the
whole of level 25: getting all of them to 24 lowers the 3ax bound to 24.  This list replaces
`3ax25-t5.scr`; every class on it is also on that one.

**Goal 2 (next level).**  `3ax24-t9.scr` has the 525 classes at 24 (replaces `3ax24-t6.scr`, 702).
Find sequences of **at most 23 moves**.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8ax.tws` (the puzzle, unchanged: the pair solved
to the 3ax goal, U and D each their own colour, F and B one colour, L and R another; every other
set fully solved); `3ax25-t9.scr` / `.sol` (names `3ax25-t9-NNN-L25`); `3ax24-t9.scr` / `.sol`
(names `3ax24-t9-NNN-L24`).  The .sol files hold the current solutions, `NAME<TAB>moves`.  Every
scramble and solution has been replayed on obl8ax.tws.  A reduced move set (F R B L 2U 3U 2D 3D)
may work well here, as F R B L 2U2 3U2 2D2 3D2 did for 4op.

**Output.**  The 25s: append to `~/mlcube/3ax25_live.txt` as before.  The 24s: write to
`~/mlcube/3ax24_live.txt`.  One solving sequence per line, moves only, each as soon as it is found.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass would take more than
a day.

**Update (2026-10-30 00:45).**  Superseded by `3ax-t11-ML.md` (35 classes left at 25, 547 at 24).
