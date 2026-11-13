#!/usr/bin/env python3
"""Experiment drivers for the orbit solver testbed.

    ./experiments.py quality [POOL]   pool size x beam width -> solve% and moves
    ./experiments.py speed   [POOL]   pool size x beam width -> ms/orbit
    ./experiments.py filter  [POOL]   survival curve of the >=2-fix prefilter
    ./experiments.py compare          conjugated vs unconjugated pools

POOL defaults to pools/conj12.costalgs.  Every cell is time-boxed, so the
expensive ones report fewer orbits; the `orbits` column says how many actually
ran.  Solve percentages from cells with few orbits are noisy.
"""

import random
import sys
import collections

import numpy as np

from orbitbench import (TARGET, Pool, benchmark, beam_solve, greedy_solve,
                        load_pool, random_orbit, _popcount24, _wrong_mask)

DEFAULT_POOL = "pools/conj12.costalgs"
SIZES = [5000, 10000, 20000, 50000, None]      # None = whole pool
WIDTHS = [1, 4, 16, 64]


def _subsets(items, rng):
    for size in SIZES:
        yield (len(items) if size is None else size,
               items if size is None or size >= len(items)
               else rng.sample(items, size))


def quality(path, trials=50, budget=60.0):
    items = load_pool(path)
    print(f"pool {path}: {len(items)} distinct permutations")
    print(f"(up to {trials} orbits per cell, {budget:.0f}s budget; "
          f"n = orbits actually run)\n")
    rng = random.Random(1)
    print(f"{'pool':>7} " + " ".join(f"{'W=' + str(w):>20}" for w in WIDTHS))
    for size, subset in _subsets(items, rng):
        pool = Pool(subset)
        cells = []
        for width in WIDTHS:
            r = benchmark(pool, width, orbits=trials, budget_s=budget)
            cells.append(
                f"{r['solve_pct']:3.0f}% {r['moves_per_orbit']:5.1f}mv n={r['orbits']}"
                if r["solved"] else f"   0%      -  n={r['orbits']}")
        print(f"{size:>7} " + " ".join(f"{c:>20}" for c in cells), flush=True)


def speed(path, orbits=1000, budget=15.0):
    items = load_pool(path)
    print(f"pool {path}: {len(items)} distinct permutations\n")
    rng = random.Random(1)
    print(f"{'pool':>7} {'W':>3} {'solved':>8} {'moves':>7} {'ms/orbit':>10} {'orbits':>7}")
    for size, subset in _subsets(items, rng):
        pool = Pool(subset)
        for width in WIDTHS:
            r = benchmark(pool, width, orbits=orbits, budget_s=budget)
            print(f"{size:>7} {width:>3} {r['solve_pct']:>7.0f}% "
                  f"{r['moves_per_orbit']:>7.1f} {r['ms_per_orbit']:>10.2f} "
                  f"{r['orbits']:>7}", flush=True)


def filter_survival(path, samples=8):
    """How much of the pool survives 'support contains >=2 misplaced'?

    Walks a greedy trajectory from a random orbit down to solved, recording the
    surviving fraction at each misplaced count.  This is the ceiling on what the
    prefilter can save, and it shows the saving is all in the endgame.
    """
    pool = Pool(load_pool(path))
    rng = random.Random(3)
    buckets = collections.defaultdict(list)
    for _ in range(samples):
        state = random_orbit(rng)
        for _ in range(40):
            wrong = int((state != TARGET).sum())
            if wrong == 0:
                break
            overlap = _popcount24(pool.SUPPORT & _wrong_mask(state))
            rows = np.flatnonzero(overlap >= min(2, wrong))
            buckets[wrong].append(len(rows) / pool.n)
            # advance along the best immediate gain
            gain = -((state[pool.SRCS] != pool.TARGET_OF_MOVED).sum(1)
                     - (state[pool.MOVED] != pool.TARGET_OF_MOVED).sum(1))
            if gain.max() <= 0:
                break
            best = int(np.argmax(gain))
            nxt = state.copy()
            nxt[pool.MOVED[best]] = state[pool.SRCS[best]]
            state = nxt
    print(f"pool {path}: {pool.n} permutations, "
          f"{len(set(int(s) for s in pool.SUPPORT))} distinct support masks\n")
    print("survival of the '>=2 misplaced in support' prefilter:")
    for wrong in sorted(buckets, reverse=True):
        vals = buckets[wrong]
        print(f"  {wrong:2d} misplaced -> {100 * sum(vals) / len(vals):5.1f}% of pool")


def compare(width=64, trials=25, budget=60.0):
    """Conjugations matter more than raw algorithm count.

    Time-boxed: the large pools are seconds per orbit at W=64, so each entry
    reports how many orbits it actually managed within the budget.
    """
    print(f"beam width {width}, up to {trials} orbits per pool, "
          f"{budget:.0f}s budget each\n")
    for label, path in (("orientations only", "pools/noconj.costalgs"),
                        ("conjugated <=12", "pools/conj12.costalgs"),
                        ("conjugated <=14", "pools/conj14.costalgs")):
        try:
            items = load_pool(path)
        except OSError:
            print(f"{label:>20}: {path} missing (run makepools.sh)")
            continue
        pool = Pool(items)
        r = benchmark(pool, width, orbits=trials, budget_s=budget)
        print(f"{label:>20}: {pool.n:>7} perms  "
              f"{r['solve_pct']:5.0f}%  {r['moves_per_orbit']:5.1f} moves  "
              f"{r['ms_per_orbit']:8.1f} ms/orbit  n={r['orbits']}", flush=True)


def solve_rate(path, width=64, orbits=200, budget=600.0):
    """Solve rate at one configuration, with enough samples to be meaningful."""
    pool = Pool(load_pool(path))
    r = benchmark(pool, width, orbits=orbits, budget_s=budget)
    print(f"pool {path} ({pool.n} perms), W={width}")
    print(f"  {r['solved']}/{r['orbits']} solved ({r['solve_pct']:.1f}%), "
          f"{r['moves_per_orbit']:.1f} moves, {r['algs_per_orbit']:.2f} algs, "
          f"{r['ms_per_orbit']:.0f} ms/orbit")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "quality"
    rest = sys.argv[2:]
    if what == "quality":
        quality(rest[0] if rest else DEFAULT_POOL)
    elif what == "speed":
        speed(rest[0] if rest else DEFAULT_POOL)
    elif what == "filter":
        filter_survival(rest[0] if rest else DEFAULT_POOL)
    elif what == "compare":
        compare(int(rest[0]) if rest else 64)
    elif what == "solverate":
        path = rest[0] if rest and not rest[0].isdigit() else DEFAULT_POOL
        width = int(rest[-1]) if rest and rest[-1].isdigit() else 64
        solve_rate(path, width)
    else:
        print(__doc__)
        sys.exit(1)
