#!/usr/bin/env python3
"""Build the frontier page (moves per sticker vs microseconds per sticker, one
log-log panel per cube size) from results.csv.

    python3 frontier.py [--csv results.csv] [--out frontier.html]

Standard library only.
"""
import argparse, csv, json, math, statistics, sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent

# dwalton76/rubiks-cube-NxNxN-solver README (09/28/2021 row): moves to reduce
# to 3x3x3, plus ~20 for the 3x3x3 (outer block turn metric; no timings).
DWALTON = {4: 51.6 + 20, 5: 78.9 + 20, 6: 135.7 + 20, 7: 193.8 + 20, 8: 306.4 + 20, 9: 368.6 + 20, 10: 523.4 + 20}

LABELS = {
    ("gullicube", "fast"): "fast",
    ("gullicube", "fast+finish"): "fast + finish table",
    ("gullicube", "b1"): "quality, beam 1",
    ("gullicube", "b4"): "quality, beam 4",
    ("gullicube", "b16"): "quality, beam 16",
    ("gullicube", "b64"): "quality, beam 64",
    ("gullicube", "b256"): "quality, beam 256",
    ("gullicube", "b1024"): "quality, beam 1024",
    ("brobdicube", "greedy-plan"): "brobdicube -g",
    ("brobdicube", "default"): "brobdicube",
}
ORDER = [k for k in LABELS]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=str(HERE / "results.csv"),
                    help="comma-separated CSV files; a later file's cells replace an earlier file's")
    ap.add_argument("--out", default=str(HERE / "frontier.html"))
    ap.add_argument("--sizes", help="comma-separated sizes to include (default: all)")
    a = ap.parse_args()
    cells = defaultdict(list)
    meta = {"threads": set(), "load": []}
    for fn in a.csv.split(","):
      fresh = defaultdict(list)
      with open(fn) as f:
        for r in csv.DictReader(f):
            if a.sizes and r["N"] not in a.sizes.split(","):
                continue
            if r["solver"] == "fullsolve":  # results collected before the rename (2026-09-27)
                r["solver"] = "gullicube"
            fresh[(r["solver"], r["config"], int(r["N"]))].append(r)
      cells.update(fresh)
      for rows in fresh.values():
        for r in rows:
          meta["threads"].add(r["threads"])
          try:
            meta["load"].append(float(r["load_avg"]))
          except ValueError:
            pass
    data = []
    for (solver, config, n), rows in cells.items():
        mps = [float(r["moves_per_sticker"]) for r in rows]
        ups = [float(r["us_per_sticker"]) for r in rows]
        moves = [int(r["moves"]) for r in rows]
        secs = [float(r["solve_s"]) for r in rows]
        k = len(rows)
        data.append(dict(
            solver=solver, config=config, label=LABELS.get((solver, config), f"{solver} {config}"),
            order=ORDER.index((solver, config)) if (solver, config) in ORDER else 99, N=n, n=k,
            mps=statistics.fmean(mps), mps_se=(statistics.stdev(mps) / math.sqrt(k)) if k > 1 else 0,
            ups=statistics.fmean(ups), moves=statistics.fmean(moves), secs=statistics.fmean(secs)))
    data.sort(key=lambda d: (d["N"], d["order"]))
    dw = [dict(N=n, mps=m / (6 * n * n), moves=m) for n, m in DWALTON.items()]
    load = meta["load"]
    info = dict(threads=",".join(sorted(meta["threads"])),
                load=(f"{min(load):.0f}–{max(load):.0f}" if load else "?"), rows=sum(len(v) for v in cells.values()))
    html = (HERE / "frontier_template.html").read_text()
    html = html.replace("/*DATA*/", "const DATA = " + json.dumps(data) + ";\nconst DWALTON = " + json.dumps(dw) +
                        ";\nconst INFO = " + json.dumps(info) + ";")
    Path(a.out).write_text(html)
    print(f"wrote {a.out}: {len(data)} cells from {info['rows']} runs")


if __name__ == "__main__":
    main()
