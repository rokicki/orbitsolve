#!/usr/bin/env python3
"""Print COUNT random moves for an N x N x N cube on one line, space-separated
(brobdicube / gullicube -i notation: 3R2, U', 2F, ...).

    python3 scramble.py N COUNT [SEED]

Layer depths run 1 .. ceil(N/2) from each face (so odd cubes include the
middle layer); depth 1 is written without a prefix.  Without SEED the seed
comes from the system's entropy and is reported on stderr.
"""
import random, sys

if len(sys.argv) not in (3, 4) or not all(a.isdigit() for a in sys.argv[1:]):
    sys.exit(__doc__.strip())
n, count = int(sys.argv[1]), int(sys.argv[2])
if n < 2:
    sys.exit("N must be at least 2")
seed = int(sys.argv[3]) if len(sys.argv) == 4 else random.SystemRandom().randrange(1 << 48)
if len(sys.argv) == 3:
    print(f"seed {seed}", file=sys.stderr)
rng = random.Random(seed)
maxd = (n + 1) // 2
moves = []
for _ in range(count):
    d = rng.randint(1, maxd)
    moves.append((str(d) if d > 1 else "") + rng.choice("ULFRBD") + rng.choice(["", "2", "'"]))
print(" ".join(moves))
