#!/usr/bin/env python3
"""Build the moves-per-sticker vs wall-clock page from perfdb data files.

    python3 perfchart.py OUT.html "LABEL|SUBTITLE|perfdb.txt" ["LABEL|SUBTITLE|perfdb.txt" ...]

Each data file (perfdb.py's space-separated output) becomes one chart, in the
order given, with a hover tooltip per run and a table of all runs.
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def load(fn):
    rows = []
    for line in open(fn):
        f = line.split()
        if f and f[0].isdigit():
            rows.append({"n": int(f[0]), "w": int(f[1]), "wall": float(f[2]), "solve": float(f[3]), "load": float(f[4]),
                         "cpu": float(f[7]), "instr": int(f[8]), "cycles": int(f[9]), "moves": int(f[10]), "rss": int(f[11])})
    rows.sort(key=lambda d: (d["n"], d["w"]))
    return rows


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__.strip())
    out = sys.argv[1]
    sets = []
    for spec in sys.argv[2:]:
        label, sub, fn = spec.split("|", 2)
        sets.append({"label": label, "sub": sub, "data": load(fn)})
    head = open(os.path.join(HERE, "perfchart_head.html")).read()
    sections = ""
    for i, st in enumerate(sets):
        sections += f"""
  <section class="card" aria-label="{st['label']}">
    <h2>{st['label']}</h2>
    <p class="sub">{st['sub']} · {len(st['data'])} runs</p>
    <div class="legend" id="legend{i}"></div>
    <div class="chartbox">
      <svg id="chart{i}" viewBox="0 0 1000 560" role="img" aria-label="{st['label']}: moves per sticker against wall-clock seconds, one line per cube size"></svg>
      <div class="tip" id="tip{i}" hidden></div>
    </div>
    <details>
      <summary>All {len(st['data'])} runs as a table</summary>
      <div class="tablebox"><table id="table{i}"></table></div>
    </details>
  </section>"""
    html = head.replace("</style>", "h2 { font-size: 1.1rem; margin: 0; }\n</style>") + f"""
<div class="wrap">
  <header>
    <h1>gullicube: moves per sticker against wall-clock time</h1>
    <p class="sub">Each line is one cube size; each point is one solve at beam width 1, 2, 4, … (wider = further right).
      Moves per sticker = solution moves / 6N². Wall-clock time includes loading (~1.8 s). M3 Max, 16 threads, seed 1.
      Hover or focus a point for its width, time, moves and peak memory. Time axis is logarithmic.</p>
  </header>{sections}
</div>
<script>
const SETS = {json.dumps([s['data'] for s in sets])};
""" + r"""
const SIZES = [4, 8, 16, 32, 64, 128, 256, 512, 1024];
const COLOR = { 4: "--b1", 8: "--b2", 16: "--b3", 32: "--b4", 64: "--b5", 128: "--o1", 256: "--o2", 512: "--o3", 1024: "--o4" };
const W = 1000, H = 560, M = { l: 64, r: 70, t: 14, b: 50 };
const xmin = 1.5, xmax = 800, ymin = 0.6, ymax = 1.25;
const X = t => M.l + (Math.log10(t) - Math.log10(xmin)) / (Math.log10(xmax) - Math.log10(xmin)) * (W - M.l - M.r);
const Y = v => M.t + (ymax - v) / (ymax - ymin) * (H - M.t - M.b);
const mps = d => d.moves / (6 * d.n * d.n);
const NS = "http://www.w3.org/2000/svg";
const css = v => `var(${v})`;
const fmt = (x, d = 0) => x.toLocaleString(undefined, { maximumFractionDigits: d, minimumFractionDigits: d });
function draw(i, DATA) {
  const svg = document.getElementById("chart" + i), tip = document.getElementById("tip" + i);
  const el = (tag, attrs) => { const e = document.createElementNS(NS, tag); for (const k in attrs) e.setAttribute(k, attrs[k]); svg.appendChild(e); return e; };
  for (let v = 0.6; v <= 1.2501; v += 0.05) {
    const y = Y(v);
    el("line", { x1: M.l, x2: W - M.r, y1: y, y2: y, stroke: css("--grid"), "stroke-width": 1 });
    el("text", { x: M.l - 8, y: y + 4, "text-anchor": "end", class: "tick" }).textContent = v.toFixed(2);
  }
  for (const t of [2, 5, 10, 20, 50, 100, 200, 500]) {
    const x = X(t);
    el("line", { x1: x, x2: x, y1: M.t, y2: H - M.b, stroke: css("--grid"), "stroke-width": 1 });
    el("text", { x, y: H - M.b + 18, "text-anchor": "middle", class: "tick" }).textContent = t + " s";
  }
  el("line", { x1: M.l, x2: W - M.r, y1: H - M.b, y2: H - M.b, stroke: css("--axis"), "stroke-width": 1 });
  el("text", { x: (M.l + W - M.r) / 2, y: H - 10, "text-anchor": "middle", class: "axislabel" }).textContent = "wall-clock time per solve (log scale)";
  el("text", { x: 14, y: (M.t + H - M.b) / 2, "text-anchor": "middle", class: "axislabel", transform: `rotate(-90 14 ${(M.t + H - M.b) / 2})` }).textContent = "moves per sticker";
  const showTip = (d, cx, cy) => {
    tip.innerHTML = `<b>${d.n}³, width ${fmt(d.w)}</b><br><span class="k">moves</span> ${fmt(d.moves)} (${mps(d).toFixed(3)} per sticker)` +
      `<br><span class="k">wall</span> ${fmt(d.wall, 2)} s <span class="k">· solve</span> ${fmt(d.solve, 2)} s<br><span class="k">CPU</span> ${fmt(d.cpu, 1)} s` +
      ` <span class="k">· peak memory</span> ${(d.rss / 1e9).toFixed(2)} GB`;
    tip.hidden = false;
    const r = svg.getBoundingClientRect(), s = r.width / W;
    let left = cx * s + 12;
    if (left + tip.offsetWidth > r.width) left = cx * s - tip.offsetWidth - 12;
    tip.style.left = left + "px"; tip.style.top = Math.max(0, cy * s - 10) + "px";
  };
  const labels = [];
  for (const n of SIZES) {
    const pts = DATA.filter(d => d.n === n).sort((a, b) => a.w - b.w);
    if (!pts.length) continue;
    const c = css(COLOR[n]);
    el("polyline", { points: pts.map(d => `${X(d.wall)},${Y(mps(d))}`).join(" "), fill: "none", stroke: c, "stroke-width": 2, "stroke-linejoin": "round" });
    for (const d of pts) {
      const cx = X(d.wall), cy = Y(mps(d));
      const p = el("circle", { cx, cy, r: 4, fill: c, stroke: css("--surface"), "stroke-width": 2, class: "pt", tabindex: 0,
        "aria-label": `${n} cubed, width ${d.w}: ${d.moves} moves, ${d.wall.toFixed(1)} seconds` });
      const hit = el("circle", { cx, cy, r: 11, fill: "transparent" });
      for (const t of [p, hit]) {
        t.addEventListener("mouseenter", () => showTip(d, cx, cy));
        t.addEventListener("mouseleave", () => { tip.hidden = true; });
      }
      p.addEventListener("focus", () => showTip(d, cx, cy));
      p.addEventListener("blur", () => { tip.hidden = true; });
    }
    const last = pts[pts.length - 1];
    labels.push({ n, x: X(last.wall), y: Y(mps(last)) });
  }
  labels.sort((a, b) => a.y - b.y);
  for (let k = 1; k < labels.length; k++)
    if (Math.abs(labels[k].x - labels[k - 1].x) < 60 && labels[k].y - labels[k - 1].y < 13) labels[k].y = labels[k - 1].y + 13;
  for (const l of labels) el("text", { x: l.x + 8, y: l.y + 4, class: "endlabel" }).textContent = l.n + "³";
  const lg = document.getElementById("legend" + i);
  for (const n of SIZES) {
    const s = document.createElement("span");
    s.innerHTML = `<i style="background:var(${COLOR[n]})"></i>${n}³`;
    lg.appendChild(s);
  }
  document.getElementById("table" + i).innerHTML =
    "<thead><tr><th>N</th><th>width</th><th>wall s</th><th>solve s</th><th>CPU s</th><th>instructions</th><th>cycles</th><th>moves</th><th>moves/sticker</th><th>peak memory</th></tr></thead><tbody>" +
    DATA.map(d => `<tr><td>${d.n}</td><td>${fmt(d.w)}</td><td>${d.wall.toFixed(2)}</td><td>${d.solve.toFixed(2)}</td><td>${d.cpu.toFixed(1)}</td>` +
      `<td>${(d.instr / 1e9).toFixed(1)} G</td><td>${(d.cycles / 1e9).toFixed(1)} G</td><td>${fmt(d.moves)}</td><td>${mps(d).toFixed(3)}</td><td>${(d.rss / 1e9).toFixed(2)} GB</td></tr>`).join("") + "</tbody>";
}
SETS.forEach((d, i) => draw(i, d));
</script>
"""
    open(out, "w").write(html)
    print(f"wrote {out}: " + ", ".join(f"{s['label']} {len(s['data'])} runs" for s in sets))


if __name__ == "__main__":
    main()
