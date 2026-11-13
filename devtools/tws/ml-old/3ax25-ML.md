# ML task: the 485 deepest 3ax classes (25 moves)

**Goal.**  Find solving sequences of **at most 24 moves** for the 485 scrambles in `3ax25.scr`.
Your lines took both 26s down (to 24 and 25), so 3ax is now 25, and these are the classes at 25
(two more already shorten to 24 and are left out).
The bound drops once every class is at 24 or less, so covering all 485 matters more than taking a
few far down.  Each has a 25-move solution now, built by chaining shorter algorithms without any
cancellation.  The next level, 24, has 1,050 classes.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8ax.tws`: the puzzle.  It is obl8fr.tws with the chiral pair (CENTERS2 = b, CENTERS5 = a)
  solved to the 3ax goal: U and D each their own colour, F and B one shared colour, L and R another
  shared colour.  So a solution must put every F/B piece of both orbits on F or B and every L/R piece
  on L or R, with U and D solved; which of F and B (or L and R) a piece ends on does not matter.
  Every other set must end fully solved.  The moves are the same as in obl8aring and obl8fr: outer
  and inner face turns, metric htm, each turn costs 1.
- `3ax25.scr`: 485 `ScrambleAlg` blocks named `3ax25-NNN`, one per symmetry class (16
  symmetries).
- `3ax25.sol`: the current 25-move solutions, `NAME<TAB>moves`.  Use them as seeds or for checking.
  All scrambles and solutions have been replayed on obl8ax.tws; each scramble leaves the pair's U and
  D solved and every other set solved.

**Stage.**  3ax ("axis") is the third stage of a new 3/4 split for the oblique chiral pair.  It starts
with the pair's U and D pieces solved in both orbits and sorts the remaining 32 pieces by axis.
There are C(16,8)^2 = 165,636,900 positions, about 10.4M classes.  The current bound is 25 (these
485 classes); the bulk of the table is at 17-19.

**Output.**  Append to `~/mlcube/3ax25_live.txt`, one solving sequence per line, in plain move
notation as in `3ax25.sol`.  Write only the moves: no names and no positions.  Write each line as
soon as it is found.  Lines are replayed and checked on this side before they are used, so an
occasional wrong line does no harm.

**Limits.**  Use the watchdog with an MPS cap, as for the earlier obl8 runs.  If a full pass would
take more than a day, stop and report what you have.
