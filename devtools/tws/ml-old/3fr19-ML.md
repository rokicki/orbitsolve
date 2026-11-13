# ML task: the 273 hardest 3fr classes (19 moves)

**Goal.**  Find a solving sequence of **at most 17 moves** for as many as possible of the 273
scrambles in `3fr19.scr`.  Each one currently has a 19-move solution in the certificate.  A 17 lowers
the 3fr bound only once all 273 are at 18 or less, so getting every class to 18 is worth more than
taking a few classes far below that.  18 helps, 17 helps more, and anything shorter is a bonus.

**Files** (all in ~/orbitsolve/devtools/tws):
- `obl8fr.tws`: the puzzle.  It is obl8aring.tws with a different Solved state.  The chiral pair
  is CENTERS2 (b) and CENTERS5 (a).  Each of those has U, F and D solved, and L, R and B share one
  colour, so those three faces' pieces are interchangeable.  Every other set must end fully solved.
  The moves are the same as in obl8aring: outer and inner face turns, metric htm, each turn costs 1.
- `3fr19.scr`: 273 `ScrambleAlg` blocks named `3fr19-NNN-nK`.  K counts the other 19s one move away,
  and the file is sorted by decreasing K.  When a hub (a class with a high K) drops to 17, its
  neighbours drop to 18 too, so work the file in order.
- `3fr19.sol`: the current 19-move solutions, `NAME<TAB>moves`.  Use them as seeds or for checking.
  Every scramble and solution here has been replayed on obl8fr.tws.

**Stage.**  3fr ("front") follows the U/D stages of the oblique chiral pair.  It solves the 8 front
pieces, F of both orbits, while keeping U and D of both orbits and every other centre orbit.  The
positions it reaches are C(16,4)^2 = 3,312,400; under 4 symmetries that is 829,206 classes.  For
comparison, the a ring (obl8aring) is 21 and also resisted search; 3fr is a different stage.

**Output.**  Append to `~/mlcube/3fr19_live.txt`, one solving sequence per line, in plain move
notation as in `3fr19.sol`.  Write only the moves: no names and no positions.  Write each line as
soon as it is found.  Any length up to 18 is useful.  Lines are replayed and checked on this side
before they are used, so an occasional wrong line does no harm.

**Limits.**  Use the watchdog with an MPS cap, as for the earlier obl8 runs.  If several days of
work would be needed, stop and report instead.
