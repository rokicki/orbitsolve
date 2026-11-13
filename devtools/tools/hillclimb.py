#!/usr/bin/env python3
"""Hill-climb the seedbeam tunables on instruction counts.

    python3 hillclimb.py STATES [--evals K] [--start "P=160 S=16 ..."] [--baseline-only]

Run from dist/gullicube.  Each configuration runs seedbeam on one thread under
`/usr/bin/time -l` (macOS: instructions retired); load instructions (measured
once with -n 0) are subtracted.  The objective is the average move count minus
the plain beam's (no opening; levels 0-1 on the start's top 4000) move count at
the same instructions, interpolated in log(instructions) between plain-beam
widths: negative means better than the plain beam.  Neighbours change one
tunable one step; first improvement wins; stops when no neighbour improves.
"""
import math, os, re, subprocess, sys

SEEDBEAM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "seedbeam")
LADDER = {
    "P": [40, 80, 160, 320, 640, 1280, 2560],
    "S": list(range(12, 34, 2)),
    "N": [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024],
    "b": [8, 16, 32, 64, 128, 256, 512, 1024],
    "M": [0, 1000, 2000, 4000, 8000, 16000],
    "L": [0, 1, 2, 3],
    "H": [2, 2.5, 3, 3.5, 4],
}


def run(states, cfg, n=None):
    args = [SEEDBEAM, states, "-t", "1", "-F", "2"]
    for k, v in cfg.items():
        args += ["-" + k, str(v)]
    if n is not None:
        args += ["-n", str(n)]
    p = subprocess.run(["/usr/bin/time", "-l"] + args, capture_output=True, text=True)
    instr = re.search(r"(\d+)\s+instructions retired", p.stderr)
    cyc = re.search(r"(\d+)\s+cycles elapsed", p.stderr)
    m = re.search(r"RESULT solved (\d+)/(\d+) avg ([\d.]+) merged ([\d.]+)", p.stdout)
    if not instr or not m:
        sys.exit("run failed: %s\n%s\n%s" % (" ".join(args), p.stdout[-2000:], p.stderr[-2000:]))
    return {"instr": int(instr.group(1)), "cycles": int(cyc.group(1)) if cyc else 0,
            "avg": float(m.group(3)), "merged": float(m.group(4)),
            "ok": int(m.group(1)), "n": int(m.group(2)), "line": p.stdout.strip().splitlines()[-1]}


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__.strip())
    states, evals, start, baseOnly = a[0], 60, "P=160 S=16 N=64 b=64 M=0 L=1 H=3", False
    i = 1
    while i < len(a):
        if a[i] == "--evals":
            evals = int(a[i + 1]); i += 2
        elif a[i] == "--start":
            start = a[i + 1]; i += 2
        elif a[i] == "--baseline-only":
            baseOnly = True; i += 1
        else:
            sys.exit("unknown option " + a[i])
    print("# hillclimb.py " + " ".join(a), flush=True)
    load = run(states, {"P": 0}, n=0)
    L0, C0 = load["instr"], load["cycles"]
    print("load: %.1fG instructions, %.1fG cycles" % (L0 / 1e9, C0 / 1e9), flush=True)
    base = []
    for b in [8, 16, 32, 64, 128, 256, 512]:
        r = run(states, {"P": 0, "b": b, "M": 4000, "L": 2})
        base.append((math.log((r["instr"] - L0) / 1e9), r["avg"]))
        print("baseline b %4d: %.3f moves (merged %.3f), %.2fG instr, %.2fG cycles" %
              (b, r["avg"], r["merged"], (r["instr"] - L0) / 1e9, (r["cycles"] - C0) / 1e9), flush=True)
    if baseOnly:
        return

    def curve(li):  # plain-beam moves at log-instructions li (linear in log; extrapolated at the ends)
        for j in range(len(base) - 1):
            if li <= base[j + 1][0] or j == len(base) - 2:
                (x0, y0), (x1, y1) = base[j], base[j + 1]
                return y0 + (y1 - y0) * (li - x0) / (x1 - x0)

    cache = {}

    def evaluate(cfg):
        key = tuple(sorted(cfg.items()))
        if key not in cache:
            r = run(states, cfg)
            gi = (r["instr"] - L0) / 1e9
            obj = r["avg"] - curve(math.log(max(gi, 1e-3)))
            cache[key] = (obj, r, gi)
            print("  %-45s %.3f moves (merged %.3f) %7.2fG instr  vs plain %+.3f" %
                  (" ".join("%s=%s" % kv for kv in sorted(cfg.items())), r["avg"], r["merged"], gi, obj), flush=True)
        return cache[key][0]

    cfg = {}
    for kv in start.split():
        k, v = kv.split("=")
        cfg[k] = float(v) if k == "H" else int(v)
    cur = evaluate(cfg)
    improved = True
    while improved and len(cache) < evals:
        improved = False
        for k in LADDER:
            lad = LADDER[k]
            j = lad.index(cfg[k]) if cfg[k] in lad else min(range(len(lad)), key=lambda t: abs(lad[t] - cfg[k]))
            for jj in (j - 1, j + 1):
                if 0 <= jj < len(lad) and len(cache) < evals:
                    nc = dict(cfg)
                    nc[k] = lad[jj]
                    o = evaluate(nc)
                    if o < cur - 0.02:
                        cfg, cur, improved = nc, o, True
                        print("-> %s  %+.3f" % (" ".join("%s=%s" % kv for kv in sorted(cfg.items())), cur), flush=True)
                        break
            if improved:
                break
    print("best: %s  %+.3f vs plain" % (" ".join("%s=%s" % kv for kv in sorted(cfg.items())), cur))


if __name__ == "__main__":
    main()
