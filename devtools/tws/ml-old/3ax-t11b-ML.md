# ML task: 3ax 25s and 24s, then the last 4op 23s (8, 545 and 273 classes)

**Goal 1.**  Find solving sequences of **at most 24 moves** for the 8 scrambles in `3ax25-t11b.scr`.
Your latest 3ax lines cut level 25 from 35 to these 8 classes, the whole of level 25: getting them to
24 lowers the 3ax bound to 24.  This list replaces `3ax25-t11.scr`; every class on it is also on that
one.

**Goal 2.**  `3ax24-t11b.scr` has the 545 classes at 24 (replaces `3ax24-t11.scr`).  Find sequences
of **at most 23 moves**.

**Goal 3 (4op).**  `4op23-t45.scr` has the 273 4op classes still at 23, the whole of level 23
(replaces `4op23-t39.scr`, 25,745 classes: arena's reduced-move-set optimal solves took the rest).
Every one of these is already optimal at 23 in the reduced move set F R B L 2U2 3U2 2D2 3D2, so a
shorter line must use other moves.  Find sequences of **at most 22 moves**.

**Files** (all in ~/orbitsolve/devtools/tws): `obl8ax.tws` (3ax puzzle, unchanged) for goals 1 and
2; `obl8op.tws` (4op puzzle, unchanged) for goal 3.  `3ax25-t11b.scr` / `.sol` (names
`3ax25-t11b-NNN-L25`), `3ax24-t11b.scr` / `.sol` (`3ax24-t11b-NNN-L24`), `4op23-t45.scr` / `.sol`
(`4op23-t45-NNN-L23`).  The .sol files hold the current solutions, `NAME<TAB>moves`.  Every scramble
and solution has been replayed on its .tws.

**Output.**  Append to `~/mlcube/3ax25_live.txt`, `~/mlcube/3ax24_live.txt` and
`~/mlcube/4op23_live.txt`.  One solving sequence per line, moves only, each as soon as it is found.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass would take more than
a day.

**Update (2026-10-30 12:30).**  Your 23-move line for one of the 8 is in: 7 classes remain at 25, listed in `3ax25-t15.scr` / `.sol` (names `3ax25-t15-NNN-L25`; same classes as the other 7 of `3ax25-t11b`).  No other 25 dropped with it (none is one move from it).  Goals 2 and 3 unchanged.

**Update (2026-10-30 13:10).**  Your lines finished level 25: all 7 are below 25 (the 2 the beam missed were pulled down by propagation from the others).  **3ax <= 24 is certified.**  Goal 1 is done.  Goal 2 is now `3ax24-t16.scr` / `.sol` (512 classes at 24, names `3ax24-t16-NNN-L24`; replaces `3ax24-t11b`): sequences of at most 23 moves.  Goal 3 (the 273 4op 23s) unchanged.

**Superseded 2026-10-31 08:00 by axis-c9-ML.md** (current open lists at fold c9).
