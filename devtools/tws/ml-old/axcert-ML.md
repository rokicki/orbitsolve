# Training data: the full axis-split certificate (both stages, every class)

Snapshot: `~/orbitsolve-certsafe/obl23pair-axis-snap-2026-10-30/` (frozen 2026-10-30 14:15; read-only
use please).  Both stages, every symmetry class (16 symmetries), one solving sequence per line:

- `stages/3ax/lNN.txt`: 3ax (puzzle `obl8ax.tws`), 10,366,284 classes, lengths 0-24 (24: 503).
- `stages/4op/lNN.txt`: 4op (puzzle `obl8op.tws`), 1,508,387 classes, lengths 0-23 (23: 269).

File `lNN.txt` holds the classes whose current line has NN moves.  Each line is a **solving
sequence** (moves only) from that class's representative position; the scramble is its inverse.
Every line is checked (devtools/axcheck: legal, solves a stage start, at most NN moves, one per
class, every stage start covered, no other set disturbed except the diagonals).  The lines are the
best we have, not proven optimal; the 4op 21s-23s include arena's optimal solves in the reduced move
set F R B L 2U2 3U2 2D2 3D2.  `MANIFEST` lists the files with line counts.

The live certificate (`~/orbitsolve-certsafe/obl23pair-axis-2026-10-29/`) keeps changing (4op is
rewritten hourly while arena runs); use the snapshot for training.
