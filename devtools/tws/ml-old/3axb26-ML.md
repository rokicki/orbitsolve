# ML task: the 2 deepest 3ax classes (26 moves)

**Goal.**  Find solving sequences of **at most 25 moves** for the 2 scrambles in `3axb26.scr`.
They are the only 3ax classes at 26, so getting both to 25 lowers the 3ax bound by one.  Each has
a 26-move solution now, built by chaining shorter algorithms without any cancellation, so they
should come down quickly.  Shorter is better: the next level, 25, has 486 classes, and a follow-up
batch of those may come next.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8ax.tws`: the puzzle.  It is obl8fr.tws with the chiral pair (CENTERS2 = b, CENTERS5 = a)
  solved to the 3ax goal: U and D each their own colour, F and B one shared colour, L and R another
  shared colour.  So a solution must put every F/B piece of both orbits on F or B and every L/R piece
  on L or R, with U and D solved; which of F and B (or L and R) a piece ends on does not matter.
  Every other set must end fully solved.  The moves are the same as in obl8aring and obl8fr: outer
  and inner face turns, metric htm, each turn costs 1.
- `3axb26.scr`: 2 `ScrambleAlg` blocks named `3axb26-NNN-L26`, one per symmetry class (16
  symmetries).
- `3axb26.sol`: the current 26-move solutions, `NAME<TAB>moves`.  Use them as seeds or for checking.
  Both scrambles and solutions have been replayed on obl8ax.tws; each scramble leaves the pair's U and
  D solved and every other set solved.

**Stage.**  3ax ("axis") is the third stage of a new 3/4 split for the oblique chiral pair.  It starts
with the pair's U and D pieces solved in both orbits and sorts the remaining 32 pieces by axis.
There are C(16,8)^2 = 165,636,900 positions, about 10.4M classes.  The current bound is 26 (these
2 classes); the bulk of the table is at 17-19.

**Output.**  Append to `~/mlcube/3axb26_live.txt`, one solving sequence per line, in plain move
notation as in `3axb26.sol`.  Write only the moves: no names and no positions.  Write each line as
soon as it is found.  Lines are replayed and checked on this side before they are used, so an
occasional wrong line does no harm.

**Limits.**  Use the watchdog with an MPS cap, as for the earlier obl8 runs.  This should take
minutes; if it needs more than a few hours, stop and report what you have.
