#!/usr/bin/env python3
"""Collect move counts and solve times: gullicube (several effort levels) and
brobdicube, over power-of-two cube sizes, into a CSV for the
moves-per-sticker vs microseconds-per-sticker plots.

    python3 collect.py [--out results.csv] [--sizes 4,8,16,...] [--cap 120]
                       [--threads 16] [--only gullicube|brobdicube]

- Stickers = 6 N^2.  Solve time excludes loading (gullicube reports its solve
  wall time; brobdicube's algorithm-load time is subtracted).
- Within each size the configurations run cheapest first; a configuration
  whose run exceeds --cap seconds is skipped at all larger sizes.
- Seeds: 8 at N <= 16, 4 at N = 32, 2 at N = 64, 1 beyond (the variance is
  high at small N, small at large N).
- Results are appended row by row, so an interrupted run keeps what it has;
  rows already in the output file are not repeated.

Standard library only; runs on macOS, Linux and Windows.
"""
import argparse, csv, math, os, re, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
FULL = HERE.parent / "dist" / "gullicube"  # the solver (source of truth)
BROB = HERE.parent / "brobfix"
EXE = ".exe" if os.name == "nt" else ""

FULL_CONFIGS = [  # (name, args), roughly cheapest first
    ("fast", ["--fast"]),
    ("fast+finish", ["--fast", "--finish"]),
    ("b1", ["-b", "1"]),
    ("b4", ["-b", "4"]),
    ("b16", ["-b", "16"]),
    ("b64", ["-b", "64"]),
    ("b256", ["-b", "256"]),
    ("b1024", ["-b", "1024"]),
]
FIELDS = ["solver", "config", "N", "seed", "moves", "solve_s", "cpu_s", "stickers",
          "moves_per_sticker", "us_per_sticker", "threads", "load_avg", "detail"]


def seeds_for(n):
    return 8 if n <= 16 else 4 if n <= 32 else 2 if n <= 64 else 1


def child_cpu():
    try:
        import resource
        r = resource.getrusage(resource.RUSAGE_CHILDREN)
        return r.ru_utime + r.ru_stime
    except Exception:  # Windows: no resource module
        return float("nan")


def load_avg():
    try:
        return os.getloadavg()[0]
    except Exception:
        return float("nan")


def run(cmd, cwd):
    c0, t0 = child_cpu(), time.perf_counter()
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return p.stdout + p.stderr, time.perf_counter() - t0, child_cpu() - c0, p.returncode


def run_gullicube(n, args, seed, threads):
    out, wall, cpu, rc = run([str(FULL / ("gullicube" + EXE)), "-t", str(threads), "--seed", str(seed)] + args + [str(n)], FULL)
    m = re.search(r"TOTAL\s+(\d+) moves\s+\(load ([\d.]+)s, corners [\d.]+s, wall ([\d.]+)s\)", out)
    if rc != 0 or not m or "NOT SOLVED" in out:
        return None, wall
    moves, solve = int(m.group(1)), float(m.group(3))
    detail = " ".join(l.strip() for l in out.splitlines() if l.startswith("  ") and ("moves" in l)).replace(",", ";")
    return (moves, solve, cpu, detail), wall


def run_brob(n, seed, threads, extra):
    p = max(1, math.ceil(n / (2 * threads)))  # chunking width that splits the cube across the threads
    out, wall, cpu, rc = run([str(BROB / ("brobdicube" + EXE)), "-D", str(BROB / "algs3"), "-t", str(threads), "-p", str(p)] + extra + [str(n)], BROB)
    m = re.search(r"^N \d+ moves (\d+)", out, re.M)
    lm = re.search(r"Loaded \d+ algs in ([\d.]+)", out)
    if rc != 0 or not m:
        return None, wall
    load = float(lm.group(1)) if lm else 0.0
    return (int(m.group(1)), wall - load, cpu, f"-p {p}; load {load:.2f}s"), wall


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--out", default=str(HERE / "results.csv"))
    ap.add_argument("--sizes", default="4,8,16,32,64,128,256,512,1024,2048")
    ap.add_argument("--cap", type=float, default=120.0, help="seconds per run before a configuration is dropped")
    ap.add_argument("--threads", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--only", choices=["gullicube", "brobdicube"])
    ap.add_argument("--configs", help="comma-separated gullicube configurations to run (default: all)")
    a = ap.parse_args()
    print("#", " ".join(sys.argv), flush=True)
    sizes = [int(x) for x in a.sizes.split(",")]
    outp = Path(a.out)
    done = set()
    if outp.exists():
        with outp.open() as f:
            for r in csv.DictReader(f):
                done.add((r["solver"], r["config"], int(r["N"]), int(r["seed"])))
    new = not outp.exists()
    f = outp.open("a", newline="")
    w = csv.DictWriter(f, fieldnames=FIELDS)
    if new:
        w.writeheader()
    dropped = set()
    configs = []
    if a.only != "gullicube":  # default planning (optimal: ~10 s at 4^3) and greedy planning (-g)
        configs += [("brobdicube", "greedy-plan", ["-g"]), ("brobdicube", "default", [])]
    if a.only != "brobdicube":
        configs += [("gullicube", name, args) for name, args in FULL_CONFIGS
                    if not a.configs or name in a.configs.split(",")]
    for n in sizes:
        stickers = 6 * n * n
        for solver, name, args in configs:
            if (solver, name) in dropped:
                continue
            for seed in range(1, seeds_for(n) + 1):
                if (solver, name, n, seed) in done:
                    continue
                la = load_avg()
                if solver == "brobdicube":
                    res, wall = run_brob(n, seed, a.threads, args)
                else:
                    res, wall = run_gullicube(n, args, seed, a.threads)
                if res is None:
                    print(f"N={n} {solver} {name} seed {seed}: FAILED", flush=True)
                    continue
                moves, solve, cpu, detail = res
                row = dict(solver=solver, config=name, N=n, seed=seed, moves=moves, solve_s=f"{solve:.3f}",
                           cpu_s=f"{cpu:.3f}", stickers=stickers, moves_per_sticker=f"{moves / stickers:.5f}",
                           us_per_sticker=f"{solve * 1e6 / stickers:.4f}", threads=a.threads, load_avg=f"{la:.1f}",
                           detail=detail)
                w.writerow(row)
                f.flush()
                print(f"N={n} {solver} {name} seed {seed}: {moves} moves ({moves / stickers:.3f}/sticker), "
                      f"{solve:.2f}s ({solve * 1e6 / stickers:.2f} us/sticker)", flush=True)
                if wall > a.cap:
                    dropped.add((solver, name))
                    print(f"  ({name} took {wall:.0f}s > {a.cap:.0f}s: dropped for larger sizes)", flush=True)
                    break
    f.close()


if __name__ == "__main__":
    main()
