# ML task: the 4op classes at 25 (2,479 classes)

**Goal.**  Find solving sequences of **at most 24 moves** for the scrambles in `4op25-t11.scr`.
Your 26 lines plus a larger move pool have brought **4op down to 25**: no class is at 26 any more,
so you can stop work on `4op26-t9`.  These 2,479 classes are the whole of level 25.  Getting all of
them to 24 lowers the bound to 24.  The list replaces `4op25-t9.scr` (5,808 classes).  Lines for
classes on that list are still useful.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8op.tws`: the puzzle, unchanged.  The chiral pair (CENTERS2 = b, CENTERS5 = a) is fully
  solved, and every other set must also end fully solved.  The moves are outer and inner face
  turns, metric htm, and each turn costs 1.
- `4op25-t11.scr`: 2,479 `ScrambleAlg` blocks named `4op25-t11-NNN-L25`, one per symmetry class
  (16 symmetries).
- `4op25-t11.sol`: the current 25-move solutions, `NAME<TAB>moves`.  Every scramble and solution
  has been replayed on obl8op.tws.

**Output.**  Append to `~/mlcube/4op25_live.txt` as before, one solving sequence per line, moves
only.

**Limits.**  Use the watchdog with an MPS cap.  If a full pass would take more than a day, stop and
report the hit rate so far.
