"""Bespoke per-orbit centre solving for large NxNxN cubes.

An orbit is 24 sticker positions holding 4 cubies of each of 6 colours.  Solved
means position i holds colour i//4 (Speffz order: U L F R B D, four positions
each).  A random orbit has ~20 of its 24 positions wrong.

An *algorithm* here is a move sequence that permutes stickers inside exactly one
orbit and is the identity on every other orbit ("orbit-isolating").  We only
ever need its permutation on the 24 positions and its move count, both of which
come from `algutil -c -d 1 -pc`.

The search then asks: given the orbit's state, which short product of pool
algorithms solves it?  This is the small-n alternative to brobdicube's master
sequences, which win only once there are enough orbits to amortise a shared
algorithm application across them.

Inner-loop trick
----------------
Unused algorithm slots are padded with MOVED[k] == SRCS[k].  Such a slot
contributes the same amount to the "wrong before" and "wrong after" counts, so
it cancels in the delta and no mask is needed in the hot loop.
"""

import random
import re
import time

import numpy as np

# Solved orbit: position i holds colour i//4.
TARGET = np.arange(24) // 4


def load_pool(*filenames):
    """Read `algutil -c -d 1 -pc` output into {permutation: move_count}.

    Line format is `COST (a b c)(d e) moves...`, where the parenthesised groups
    are the cycles of the permutation on the 24 orbit positions.  With `-d 1`
    the cost field is simply the move count.

    Both an algorithm and its inverse are kept (same cost), and duplicate
    permutations collapse to the cheapest representative.
    """
    pool = {}
    for filename in filenames:
        for line in open(filename):
            match = re.match(r"(\d+)\s+((?:\([0-9 ]+\))+)", line)
            if not match:
                continue
            moves = int(match.group(1))
            perm = list(range(24))
            for cycle in re.findall(r"\(([0-9 ]+)\)", match.group(2)):
                elems = [int(x) for x in cycle.split()]
                for k in range(len(elems)):
                    # perm[destination] = source
                    perm[elems[(k + 1) % len(elems)]] = elems[k]
            inverse = [0] * 24
            for i, p in enumerate(perm):
                inverse[p] = i
            for candidate in (perm, inverse):
                key = tuple(candidate)
                if key not in pool or moves < pool[key]:
                    pool[key] = moves
    return list(pool.items())


class Pool:
    """Pool of algorithms in the layout the search wants.

    MOVED[r] / SRCS[r] list the positions algorithm r disturbs and where their
    new contents come from; COST[r] is its move count; SUPPORT[r] is a 24-bit
    mask of the disturbed positions (used by the optional prefilter).
    """

    def __init__(self, items):
        width = max(sum(1 for i in range(24) if p[i] != i) for p, _ in items)
        n = len(items)
        self.MOVED = np.zeros((n, width), np.intp)
        self.SRCS = np.zeros((n, width), np.intp)
        self.COST = np.array([mv for _, mv in items], np.float32)
        self.SUPPORT = np.zeros(n, np.uint32)
        for r, (perm, _) in enumerate(items):
            disturbed = [i for i in range(24) if perm[i] != i]
            mask = 0
            for k, i in enumerate(disturbed):
                self.MOVED[r, k] = i
                self.SRCS[r, k] = perm[i]
                mask |= 1 << i
            # pad so the slot cancels out of the delta (see module docstring)
            for k in range(len(disturbed), width):
                self.MOVED[r, k] = 0
                self.SRCS[r, k] = 0
            self.SUPPORT[r] = mask
        self.TARGET_OF_MOVED = TARGET[self.MOVED]
        self.n = n
        self.width = width


_POPCOUNT = np.array([bin(i).count("1") for i in range(256)], np.uint8)


def _popcount24(x):
    return (_POPCOUNT[x & 255].astype(np.int16)
            + _POPCOUNT[(x >> 8) & 255]
            + _POPCOUNT[(x >> 16) & 255])


def _delta_wrong(pool, state, rows=None):
    """Change in the number of misplaced stickers, per algorithm."""
    moved = pool.MOVED if rows is None else pool.MOVED[rows]
    srcs = pool.SRCS if rows is None else pool.SRCS[rows]
    tgt = pool.TARGET_OF_MOVED if rows is None else pool.TARGET_OF_MOVED[rows]
    return (state[srcs] != tgt).sum(1) - (state[moved] != tgt).sum(1)


def _apply(pool, state, row):
    out = state.copy()
    out[pool.MOVED[row]] = state[pool.SRCS[row]]
    return out


def _wrong_mask(state):
    wrong = state != TARGET
    mask = 0
    for i in range(24):
        if wrong[i]:
            mask |= 1 << i
    return np.uint32(mask)


def beam_solve(pool, state, width=64, min_fix=0, max_depth=40):
    """Beam search for a cheap product of pool algorithms solving `state`.

    Ranked by (cost so far + misplaced count); misplaced count is a weak lower
    bound on the remaining cost, which is the main thing worth improving here.

    `min_fix` > 0 restricts candidates to algorithms whose support contains at
    least that many currently-misplaced positions.  It is a real pruning rule
    but it barely fires until the endgame (see README) and in numpy the fancy
    indexing costs more than it saves; it is here for a C++ port to use.

    Returns (total_moves, algorithms_used, solved).
    """
    frontier = [(0.0, state, 0)]
    for _ in range(max_depth):
        candidates = []
        for cost, cur, used in frontier:
            wrong = int((cur != TARGET).sum())
            if min_fix:
                overlap = _popcount24(pool.SUPPORT & _wrong_mask(cur))
                rows = np.flatnonzero(overlap >= min(min_fix, wrong))
                if len(rows) == 0:
                    continue
                new_wrong = wrong + _delta_wrong(pool, cur, rows)
                new_cost = cost + pool.COST[rows]
            else:
                rows = None
                new_wrong = wrong + _delta_wrong(pool, cur)
                new_cost = cost + pool.COST
            score = new_cost + new_wrong
            keep = min(width, len(score) - 1)
            for b in np.argpartition(score, keep)[:keep]:
                row = int(b) if rows is None else int(rows[b])
                candidates.append(
                    (float(new_cost[b]), int(new_wrong[b]), row, cost, cur, used))
        if not candidates:
            return 0.0, 0, False
        candidates.sort(key=lambda c: (c[0] + c[1], c[0]))
        nxt, seen = [], set()
        for new_cost, new_wrong, row, cost, cur, used in candidates:
            if new_wrong == 0:
                return new_cost, used + 1, True
            nextstate = _apply(pool, cur, row)
            key = nextstate.tobytes()
            if key in seen:
                continue
            seen.add(key)
            nxt.append((new_cost, nextstate, used + 1))
            if len(nxt) >= width:
                break
        if not nxt:
            return 0.0, 0, False
        frontier = nxt
    return 0.0, 0, False


def greedy_solve(pool, state, rng, max_algs=40):
    """Cheapest-gain-per-move hill climb, with a short plateau walk.

    Kept for reference: it never reaches a 100% solve rate at any pool size.
    """
    total, used, plateau = 0.0, 0, 0
    while True:
        wrong = int((state != TARGET).sum())
        if wrong == 0:
            return total, used, True
        if used > max_algs:
            return total, used, False
        gain = -_delta_wrong(pool, state)
        improving = gain > 0
        if improving.any():
            row = int(np.argmax(np.where(improving, gain / pool.COST, -1.0)))
            plateau = 0
        else:
            flat = np.flatnonzero(gain == 0)
            if len(flat) == 0 or plateau > 6:
                return total, used, False
            row = int(flat[rng.randrange(len(flat))])
            plateau += 1
        state = _apply(pool, state, row)
        total += pool.COST[row]
        used += 1


def random_orbit(rng):
    scrambled = list(TARGET)
    rng.shuffle(scrambled)
    return np.array(scrambled)


def benchmark(pool, width, min_fix=0, orbits=1000, budget_s=15.0, seed=11,
              solver=beam_solve):
    """Solve up to `orbits` random orbits, stopping early at `budget_s`."""
    rng = random.Random(seed)
    n = solved = algs = 0
    moves = 0.0
    start = time.perf_counter()
    while n < orbits and time.perf_counter() - start < budget_s:
        if solver is greedy_solve:
            cost, used, ok = greedy_solve(pool, random_orbit(rng), rng)
        else:
            cost, used, ok = beam_solve(pool, random_orbit(rng), width, min_fix)
        n += 1
        if ok:
            solved += 1
            moves += cost
            algs += used
    elapsed = time.perf_counter() - start
    return {
        "orbits": n,
        "solved": solved,
        "solve_pct": 100.0 * solved / n if n else 0.0,
        "ms_per_orbit": 1000.0 * elapsed / n if n else 0.0,
        "moves_per_orbit": moves / solved if solved else 0.0,
        "algs_per_orbit": algs / solved if solved else 0.0,
    }
