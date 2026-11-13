# ML task: the deepest 4op classes (33 at 26, 5,808 at 25)

**Goal.**  Two lists, in this order:
1. `4op26-t9.scr`: 33 classes at 26.  Find solving sequences of **at most 25 moves**.  These are
   the only classes at 26, so getting all of them to 25 lowers the 4op bound to 25.
2. `4op25-t9.scr`: 5,808 classes at 25.  Find sequences of **at most 24 moves**.  This is the next
   level; work through it in file order after the 26s.

These replace `4op26.scr` and `4op26-t5/t6.*`.  The table was rebuilt (2026-10-28 23:45) with
every one of your 26 lines so far folded in, plus a larger move pool, and the 26s fell from 577 to 33.
Lines for classes from the earlier lists are still useful, so keep any run that is close to done.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8op.tws`: the puzzle, unchanged.  The chiral pair (CENTERS2 = b, CENTERS5 = a) is fully
  solved: every face has its own colour.  Every other set must also end fully solved.  The moves
  are outer and inner face turns, metric htm, and each turn costs 1.
- `4op26-t9.scr` and `4op25-t9.scr`: `ScrambleAlg` blocks, one per symmetry class (16 symmetries).
  They are named `4op26-t9-NNN-L26` and `4op25-t9-NNN-L25`.
- `4op26-t9.sol` and `4op25-t9.sol`: the current solutions, `NAME<TAB>moves`.  Use them as seeds or
  for checking.  Every scramble and solution has been replayed on obl8op.tws.

**Stage.**  Same as before: 4op is the last stage of the 3ax/4op split of the oblique chiral pair.
It swaps pieces between F and B and between L and R, in both orbits at once, and keeps every other
centre orbit.

**Output.**  Append the 26s to `~/mlcube/4op26_live.txt` as before.  Write the 25s to
`~/mlcube/4op25_live.txt`.  Write one solving sequence per line, in plain move notation as in the
.sol files: moves only, no names and no positions.  Write each line as soon as it is found.  Lines
are replayed and checked on this side before they are used.

**Limits.**  Use the watchdog with an MPS cap, as for the earlier obl8 runs.  The 26s should take
hours at most.  For the 25s, if a full pass would take more than a day, stop and report the hit
rate so far.
