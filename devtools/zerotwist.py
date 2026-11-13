#!/usr/bin/env python3
"""Random move sequences of a given length, using outer face turns and depth-2 and depth-3
single-layer turns only, in which every layer's net twist is zero (mod 4).

Moves are twsearch-style: F (outer face), 2F (2nd layer from F), 3F (3rd layer from F), each
with a quarter turn, ' or 2.  The 18 layers (6 faces x 3 depths) are counted separately, so
2R and 2L are different layers.  Consecutive turns of the same layer are not allowed, so the
sequence length is exact in the single-layer metric.

usage: zerotwist.py LENGTH [--count N] [--seed S]
"""
import random
import sys

FACES = "ULFRBD"
SUFFIX = {1: "", 2: "2", 3: "'"}


def layer_counts(n, rng):
    """Split n turns over the 18 layers; a layer used at all gets at least 2 turns."""
    while True:
        counts = [0] * 18
        left = n
        while left > 0:
            layer = rng.randrange(18)
            add = 2 if counts[layer] == 0 else 1
            if add > left:
                break
            counts[layer] += add
            left -= add
        if left == 0:
            return counts


def layer_turns(c, rng):
    """c turn amounts (1..3) on one layer summing to 0 mod 4."""
    while True:
        a = [rng.randint(1, 3) for _ in range(c - 1)]
        last = (-sum(a)) % 4
        if last:
            return a + [last]


def sequence(n, rng):
    while True:
        moves = []
        for layer, c in enumerate(layer_counts(n, rng)):
            if c:
                moves += [(layer, t) for t in layer_turns(c, rng)]
        rng.shuffle(moves)
        if all(moves[i][0] != moves[i + 1][0] for i in range(len(moves) - 1)):
            return " ".join(("" if layer // 6 == 0 else str(layer // 6 + 1)) + FACES[layer % 6] + SUFFIX[t]
                            for layer, t in moves)


def main(argv):
    print("#", " ".join(argv))
    args, count, seed = [], 1, None
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--count" and i + 1 < len(argv):
            count = int(argv[i + 1]); i += 2
        elif a == "--seed" and i + 1 < len(argv):
            seed = int(argv[i + 1]); i += 2
        elif a.startswith("-"):
            sys.exit(f"unknown option {a}")
        else:
            args.append(a); i += 1
    if len(args) != 1:
        sys.exit(__doc__.strip().splitlines()[-1])
    n = int(args[0])
    if n < 2:
        sys.exit("LENGTH must be at least 2")
    if seed is None:
        seed = random.SystemRandom().randrange(2**32)
    print(f"# seed {seed}")
    rng = random.Random(seed)
    for _ in range(count):
        print(sequence(n, rng))


if __name__ == "__main__":
    if len(sys.argv) == 1:
        sys.exit(__doc__.strip().splitlines()[-1])
    main(sys.argv)
