# ML task: the 3ax classes at 25 (97 classes)

**Goal.**  Find solving sequences of **at most 24 moves** for the scrambles in `3ax25-t5.scr`.  A
larger move pool cut 3ax level 25 from 477 classes to 97.  These 97 are the whole of level 25.
Getting all of them to 24 lowers the 3ax bound to 24.  This list replaces `3ax25.scr` and
`3ax25-t4.scr`.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8ax.tws`: the puzzle, unchanged.  The pair is solved to the 3ax goal: U and D each their own
  colour, F and B one shared colour, L and R another.  Every other set must end fully solved.
- `3ax25-t5.scr`: 97 `ScrambleAlg` blocks named `3ax25-t5-NNN-L25`.
- `3ax25-t5.sol`: the current 25-move solutions, `NAME<TAB>moves`.  Every scramble and solution has
  been replayed on obl8ax.tws.

**Output.**  Append to `~/mlcube/3ax25_live.txt` as before, one solving sequence per line, moves
only.

**Limits.**  Use the watchdog with an MPS cap.  This should take hours at most.

**Goal 2 (2026-10-29 15:10, next level).**  After the 25s: `3ax24-t6.scr` has the 702 3ax classes
at 24 (from the current table ax-t6; the 97 at 25 are unchanged).  Find sequences of **at most 23
moves**.  Files `3ax24-t6.scr` / `.sol` (names `3ax24-t6-NNN-L24`), same puzzle obl8ax.tws, every
scramble and solution replayed on it.  Write them to `~/mlcube/3ax24_live.txt`, one sequence per
line, moves only.

**Update (2026-10-29 19:45).**  Superseded by `3ax-t9-ML.md` (67 classes left at 25, 525 at 24).
