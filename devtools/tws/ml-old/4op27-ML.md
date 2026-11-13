# ML task: the 339 deepest 4op classes (27 moves)

**Goal.**  Find solving sequences of **at most 26 moves** for the 339 scrambles in `4op27.scr`.
These are now the worst 4op classes (the 54 classes of 4op28 came down to 21-26 with your lines,
thank you).  Each has a 27-move solution now, built by chaining shorter algorithms without any
cancellation, so the true lengths are likely well below 27.  The bound drops only once every class
is at 26 or less, so covering all 339 matters more than taking a few far down; after that, shorter
is better (the next level, 26, has 5,734 classes).

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8op.tws`: the puzzle.  It is obl8fr.tws with the chiral pair (CENTERS2 = b, CENTERS5 = a)
  fully solved: every face has its own colour.  Every other set must also end fully solved.  The
  moves are the same as in obl8aring and obl8fr: outer and inner face turns, metric htm, each turn
  costs 1.
- `4op27.scr`: 339 `ScrambleAlg` blocks named `4op27-NNN-LK` (K = current length: 338 at 27, one at
  26), one per symmetry class (16 symmetries).
- `4op27.sol`: the current solutions, `NAME<TAB>moves`.  Use them as seeds or for checking.
  Every scramble and solution here has been replayed on obl8op.tws.

**Stage.**  4op ("opposite") is the last stage of a new 3/4 split for the oblique chiral pair.  At
its start the pair's U and D pieces are solved in both orbits, every F/B piece is on F or B, and every
L/R piece is on L or R.  4op swaps pieces between F and B and between L and R, in both orbits at once,
until all six faces are solved.  It keeps every other centre orbit.  There are 70^4 = 24,010,000
positions, about 1.5M classes.  For comparison, the b ring (the current last stage) is 29.  4op is at
least 15 (it contains the L/R-only stage, which is exactly 15).

**Output.**  Append to `~/mlcube/4op27_live.txt`, one solving sequence per line, in plain move
notation as in `4op27.sol`.  Write only the moves: no names and no positions.  Write each line as
soon as it is found.  Lines are replayed and checked on this side before they are used, so an
occasional wrong line does no harm.

**Limits.**  Use the watchdog with an MPS cap, as for the earlier obl8 runs.  If the classes need more
than a day of work in all, stop and report what you have.
