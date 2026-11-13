# ML task: 4op is at 23; the 3ax 25s come first

**Status.**  Every 4op class is now at most 23 moves (your 24 lines, arena's reduced-move-set optima
and a move-merging pass took level 24 from 12,291 to 0).  You can stop the 4op 24 lists.

**Priority now: 3ax** (`3ax-t9-ML.md`: the 67 classes at 25, goal <= 24), which holds the plain sum
at 48 (3ax 25 + 4op 23).

**Then, for 4op (next level).**  `4op23-t39.scr` has the 25,745 classes at 23.  Find sequences of
**at most 22 moves**, in file order; report the hit rate after the first few hundred.  Files in
~/orbitsolve/devtools/tws: `obl8op.tws` (unchanged); `4op23-t39.scr` / `.sol` (names
`4op23-t39-NNN-L23`), every scramble and solution replayed on obl8op.tws.  Output:
`~/mlcube/4op23_live.txt`, one sequence per line, moves only.  The reduced move set F R B L 2U2 3U2
2D2 3D2 has worked well for 4op.

**Limits.**  Use the watchdog with an MPS cap.  Stop and report if a full pass would take more than
a day.

**Superseded (2026-10-30 09:40)** by `3ax-t11b-ML.md`: the 4op 23s are now 273 (`4op23-t45.scr`).
