# ML task: the 4op classes at 26 moves (4,140 classes)

**Goal.**  Find solving sequences of **at most 25 moves** for the scrambles in `4op26.scr`.  4op is
now at 26 (your lines took every 27 down to 21-26, thank you).  This batch is much harder than the
last one: many of these classes may truly be 26 or close to it, so expect a lower hit rate.  The
bound drops only once every class is at 25 or less; report every class you get to 25 or below, and
tell us early if most classes resist (that tells us 4op may really be 26).  This list is from
the table with your 27s folded in (refreshed 2026-10-28 18:35; it replaces the earlier 5,695-class
list).  Lines for classes from the earlier list are still useful.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8op.tws`: the puzzle.  It is obl8fr.tws with the chiral pair (CENTERS2 = b, CENTERS5 = a)
  fully solved: every face has its own colour.  Every other set must also end fully solved.  The
  moves are the same as in obl8aring and obl8fr: outer and inner face turns, metric htm, each turn
  costs 1.
- `4op26.scr`: 4,140 `ScrambleAlg` blocks named `4op26-NNNN`, one per symmetry class (16
  symmetries), all with current length 26.
- `4op26.sol`: the current 26-move solutions, `NAME<TAB>moves`.  Use them as seeds or for checking.
  Every scramble and solution here has been replayed on obl8op.tws.

**Stage.**  4op ("opposite") is the last stage of a new 3/4 split for the oblique chiral pair.  At
its start the pair's U and D pieces are solved in both orbits, every F/B piece is on F or B, and every
L/R piece is on L or R.  4op swaps pieces between F and B and between L and R, in both orbits at once,
until all six faces are solved.  It keeps every other centre orbit.  There are 70^4 = 24,010,000
positions, about 1.5M classes.  For comparison, the b ring (the current last stage) is 29.  4op is at
least 15 (it contains the L/R-only stage, which is exactly 15).

**Output.**  Append to `~/mlcube/4op26_live.txt`, one solving sequence per line, in plain move
notation as in `4op26.sol`.  Write only the moves: no names and no positions.  Write each line as
soon as it is found.  Lines are replayed and checked on this side before they are used, so an
occasional wrong line does no harm.

**Limits.**  Use the watchdog with an MPS cap, as for the earlier obl8 runs.  This batch is large: work
through it in file order, write lines as you go, and if a full pass would take more than a day,
stop and report the hit rate so far.
