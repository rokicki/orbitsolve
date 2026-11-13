// Compare the GPU beam (stage 1) with the CPU TableBeam at the same width on
// pair states: move counts, identical solutions, and time.  Run from
// dist/gullicube.  Settings as the solver: p8s10, seam-aware (setup cost 3),
// finish tables c12x/p9/eg8, 3 moves per misplaced piece.
//   gpucmp STATES W [-n k] [-c candMult] [-g] (-g: GPU only)
#include "gpubeam.h"
#include "classpool.h"
#include <fstream>
#include <string>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: gpucmp STATES W [-n k] [-c candMult] [-g] [-s seenScale] [-L (2-ply lookahead ranking)] [-p POOL.cls] [-m maxLen]\n"); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  const int W = atoi(argv[2]);
  int nmax = 1 << 30, candMult = 16;
  bool gpuOnly = false;
  int seenScale = 0, lookahead = 0, maxLen = 99;
  const char *poolFile = "data/p8s10.cls";
  for (int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-n" && i + 1 < argc) nmax = atoi(argv[++i]);
    else if (a == "-c" && i + 1 < argc) candMult = atoi(argv[++i]);
    else if (a == "-g") gpuOnly = true;
    else if (a == "-s" && i + 1 < argc) seenScale = atoi(argv[++i]);
    else if (a == "-L") lookahead = 1;
    else if (a == "-p" && i + 1 < argc) poolFile = argv[++i];
    else if (a == "-m" && i + 1 < argc) maxLen = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  std::vector<State> st;
  {
    std::ifstream in(argv[1]);
    if (!in) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    for (std::string line; std::getline(in, line) && (int)st.size() < nmax;) {
      if (line.size() < 48 || line[0] == '#') continue;
      State s;
      for (int p = 0; p < 48; p++) s.col[p] = line[p] - '0';
      s.setMasks();
      st.push_back(s);
    }
  }
  Pool pool;
  {
    ClassPool cp;
    if (!cp.load(poolFile)) { fprintf(stderr, "cannot read %s (run from dist/gullicube)\n", poolFile); return 1; }
    const OrbitModel m = pairModel();
    cp.expand(m, [&](const uint8_t *p, int c, uint32_t ref) {  // poolFromClasses, keeping lengths <= maxLen
      if (c > maxLen) return;
      std::array<uint8_t, 48> s;
      for (int k = 0; k < 48; k++) s[k] = k < m.P ? p[k] : k;
      pool.addEntry(c, s, m.image(cp.rep[ref / 96], ref % 96), cp.dflag[ref / 96]);
    });
  }
  pool.sortByCost();
  ColourSolver cs;
  Weights wts;
  wts.setupFace = 3;
  cs.build(pool, nullptr, wts);
  TableBeam beam;
  beam.build(cs);
  beam.slotSeed = 1;
  beam.seamAware = true;
  FinishTable fin, finExt;
  std::vector<ClassPool> finClasses(3);
  OrbitModel pm = pairModel();
  {
    std::vector<std::pair<const ClassPool *, uint8_t>> list;
    const char *fp[] = {"data/c12x.cls", "data/p9.cls", "data/eg8.cls"};
    for (int i = 0; i < 3; i++) {
      if (!finClasses[i].load(fp[i])) { fprintf(stderr, "cannot read %s\n", fp[i]); return 1; }
      list.push_back({&finClasses[i], (uint8_t)(10 + i)});
    }
    buildFinishFromClassList(fin, finExt, pool, beam.Z, cs.eff4, list, pm);
    beam.finish = &fin;
  }
  GpuBeam gb;
  gb.candMult = candMult;
  gb.seenScale = seenScale;
  gb.lookahead = lookahead;
  if (!gb.init(beam)) { fprintf(stderr, "no Metal device\n"); return 1; }
  auto movesOf = [&](const State &s0, const std::vector<AlgRef> &sol) {
    State cur = s0;
    int moves = 0;
    for (auto &x : sol) {
      if (x.pool == 0) { cur = cur.apply(pool, x.idx); moves += pool.cost[x.idx]; continue; }
      const FinishTable::Slot *fi = fin.find(beam.zob(cur));
      if (!fi || fi->pid != x.pool || fi->alg != x.idx) return -1;
      moves += fi->cost;
      for (int q = 0; q < 48; q++) cur.col[q] = target(q);
    }
    return cur.correct() == 48 ? moves : -1;
  };
  long sumC = 0, sumG = 0, same = 0, okC = 0, okG = 0;
  double tC = 0, tG = 0;
  for (size_t k = 0; k < st.size(); k++) {
    bool ok;
    int mc = -1;
    std::vector<AlgRef> sc;
    if (!gpuOnly) {
      double t0 = now();
      sc = beam.solve(st[k], ok, W);
      tC += now() - t0;
      mc = ok ? movesOf(st[k], sc) : -1;
      if (mc >= 0) { okC++; sumC += mc; }
    }
    double t1 = now();
    auto sg = gb.solve(st[k], ok, W);
    tG += now() - t1;
    const int mg = ok ? movesOf(st[k], sg) : -1;
    if (mg >= 0) { okG++; sumG += mg; }
    bool eq = sc.size() == sg.size();
    for (size_t i = 0; eq && i < sc.size(); i++) eq = sc[i].pool == sg[i].pool && sc[i].idx == sg[i].idx;
    same += eq;
  }
  const int n = st.size();
  printf("W %d, %d states, candMult %d%s, pool %s (<= %d moves, %zu algorithms)\n", W, n, candMult, lookahead ? ", lookahead" : "", poolFile, maxLen, pool.size());
  if (!gpuOnly) printf("  CPU: solved %ld avg %.3f, %.3fs (%.1f ms/state, 1 thread)\n", okC, (double)sumC / std::max(1L, okC), tC, 1e3 * tC / n);
  printf("  GPU: solved %ld avg %.3f, %.3fs (%.1f ms/state): GPU %.3fs, host %.3fs; candidates/level %.0f; short levels %ld of %ld; lookahead GPU %.3fs\n", okG,
         (double)sumG / std::max(1L, okG), tG, 1e3 * tG / n, gb.gpuSec, gb.hostSec, (double)gb.candidates / std::max(1L, gb.levels),
         gb.shortLevels, gb.levels, gb.gpuNextSec);
  if (!gpuOnly) printf("  identical solutions: %ld of %d\n", same, n);
}
