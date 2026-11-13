#include <mutex>
// Training data for pair-state heuristics.
//   pairdata gen K seed            : print K random pair states (48 digits, colour per position)
//   pairdata solve W [-t T] [--path] [--seed s]
//       read states from stdin, solve each with the TableBeam at width W and
//       print "state cost"; with --path also print every intermediate state of
//       the solution with its remaining (suffix) cost, prefixed by "p ".
#include "solvers.h"
#include "poolcache.h"
#include "classpool.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

static std::string str(const State &s) {
  std::string r(48, '0');
  for (int p = 0; p < 48; p++) r[p] = '0' + s.col[p];
  return r;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fputs(
        "usage: pairdata gen K seed        print K random pair states (48 digits each)\n"
        "       pairdata solve W [options]  solve states from stdin with the pair beam at width W;\n"
        "                                   prints \"state cost\" per state, a summary on stderr\n"
        "  -t T              threads (default 4)\n"
        "  --seed s          beam table seed (default 1)\n"
        "  --pool file       pair pool, .cls or expanded (default ../dist/gullicube/data/p8s10.cls)\n"
        "  (env BEAMTIME=1: filter/score time and per-node counts by correct positions)\n"
        "  --path            also print every intermediate state with its remaining cost (\"p state cost\")\n"
        "  --rootrank        for solution steps 1..6: rank of the algorithm by score (12*gain - eff4) at the start\n"
        "  --grow g [cap]    the beam at depth d keeps W * g^d nodes, up to cap (default 65536)\n"
        "  --firstwidth K    the start keeps K children (default: the width)\n"
        "  --roottop M L     levels 1..L score only the M algorithms that scored best at the start\n"
        "  -q                no per-state output (summary only)\n"
        "  --algstats        summary of the solutions' algorithms: length by the correct count before it\n"
        "  score (default: 3 moves per misplaced piece):\n"
        "  --slope x         x moves per misplaced piece\n"
        "  --tab file        h(m) in moves for m = 0..48 misplaced\n"
        "  --tab2 file       H[a][b] in moves, 25 x 25, a/b misplaced in orbit A/B (overrides --tab)\n"
        "  --k f             scale --tab2 by f\n"
        "  --face e0..e4     extra moves for an (orbit, face) with k = 0..4 pieces correct\n"
        "  --finish          finish table: record children one algorithm from solved\n"
        "  --finishskip      ... and do not keep them in the beam\n"
        "  --finishlb m      ... children with <= m misplaced not in the table cost >= 2 algorithms\n"
        "  --seam            seam-aware costs (moves cancelling with the previous algorithm)\n"
        "  --setupcost q     quarter-moves per setup face turn (default 4 = a full move)\n"
        "  --finishpool f    add class file f's algorithms to the finish table (repeatable; implies\n"
        "                    --finish), e.g. --finishpool data/c12x.cls --finishpool data/p9.cls --finishpool data/eg8.cls\n",
        stderr);
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::string mode = argv[1];
  if (mode == "gen") {
    std::mt19937_64 rng(strtoull(argv[3], 0, 10));
    for (int k = 0, K = atoi(argv[2]); k < K; k++) puts(str(randomState(rng)).c_str());
    return 0;
  }
  int W = atoi(argv[2]), threads = 4;
  bool path = false, quiet = false;
  double slope = 0;
  const char *tabFile = nullptr, *tab2File = nullptr;
  double tabScale = 1, faceMoves[5] = {0, 0, 0, 0, 0};
  bool useFace = false, useFinish = false, finishSkip = false;
  int finishLB = 0, setupCost = 4;
  bool seam = false;
  std::vector<std::string> finishPools;
  uint64_t seed = 1;
  int rootTopM = 0, rootTopL = 0, firstW = 0, growCap = 1 << 16;
  double growF = 1;
  bool algStats = false, rootRank = false;
  const char *poolFile = "../dist/gullicube/data/p8s10.cls";
  for (int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-t") threads = atoi(argv[++i]);
    else if (a == "--path") path = true;
    else if (a == "--seed") seed = strtoull(argv[++i], 0, 10);
    else if (a == "--pool") poolFile = argv[++i];
    else if (a == "--slope") slope = atof(argv[++i]);
    else if (a == "--tab") tabFile = argv[++i];
    else if (a == "--tab2") tab2File = argv[++i];
    else if (a == "--k") tabScale = atof(argv[++i]);
    else if (a == "--face") {  // five values in moves: extra cost of a face with k = 0..4 correct
      for (int k = 0; k < 5; k++) faceMoves[k] = atof(argv[++i]);
      useFace = true;
    }
    else if (a == "-q") quiet = true;
    else if (a == "--finish") useFinish = true;
    else if (a == "--finishskip") finishSkip = true;
    else if (a == "--seam") seam = true;
    else if (a == "--setupcost") setupCost = atoi(argv[++i]);
    else if (a == "--finishpool") finishPools.push_back(argv[++i]);
    else if (a == "--finishlb") finishLB = atoi(argv[++i]);
    else if (a == "--algstats") algStats = true;
    else if (a == "--rootrank") rootRank = true;
    else if (a == "--grow" && i + 1 < argc) {
      growF = atof(argv[++i]);
      if (i + 1 < argc && isdigit(argv[i + 1][0])) growCap = atoi(argv[++i]);
    }
    else if (a == "--firstwidth" && i + 1 < argc) firstW = atoi(argv[++i]);
    else if (a == "--roottop" && i + 2 < argc) { rootTopM = atoi(argv[++i]); rootTopL = atoi(argv[++i]); }
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  Pool pool;
  if (std::string(poolFile).size() > 4 && std::string(poolFile).substr(std::string(poolFile).size() - 4) == ".cls") {
    ClassPool cp;
    if (!cp.load(poolFile)) { fprintf(stderr, "cannot read %s\n", poolFile); return 1; }
    poolFromClasses(pool, cp, pairModel());
  } else loadPoolCached(pool, poolFile);
  pool.sortByCost();
  ColourSolver cs;
  Weights wts;
  wts.setupFace = setupCost;
  cs.build(pool, nullptr, wts);
  TableBeam beam;
  beam.timing = getenv("BEAMTIME") != nullptr;
  beam.build(cs);
  beam.slotSeed = seed;
  beam.seamAware = seam;
  if (slope > 0) beam.mulOverride = (int)std::lround(4 * slope);
  if (tabFile) {  // h(m) in moves for m = 0..48 misplaced; score by correct count c is -4 h(48 - c)
    FILE *f = fopen(tabFile, "r");
    std::vector<double> h(49);
    for (auto &x : h) if (fscanf(f, "%lf", &x) != 1) { fprintf(stderr, "bad tab\n"); return 1; }
    fclose(f);
    beam.scoreTab.resize(49);
    for (int c = 0; c <= 48; c++) beam.scoreTab[c] = -(int)std::lround(4 * h[48 - c]);
  }
  if (tab2File) {  // H[a][b] in moves, a/b misplaced in orbit A/B (25 x 25)
    FILE *f = fopen(tab2File, "r");
    std::vector<double> h(625);
    for (auto &x : h) if (fscanf(f, "%lf", &x) != 1) { fprintf(stderr, "bad tab2\n"); return 1; }
    fclose(f);
    beam.scoreTab2.resize(625);
    for (int ca = 0; ca <= 24; ca++)
      for (int cb = 0; cb <= 24; cb++) beam.scoreTab2[ca * 25 + cb] = -(int)std::lround(4 * tabScale * h[(24 - ca) * 25 + 24 - cb]);
  }
  if (useFace) {
    double mn = *std::min_element(faceMoves, faceMoves + 5);  // shift to >= 0 (a constant: same ranking)
    for (int k = 0; k < 5; k++) beam.faceExtra.push_back((int)std::lround(4 * (faceMoves[k] - mn)));
  }
  // finish tables: the beam's own pool (id 0), chained to the --finishpool
  // symmetry-class files (ids 10, 11, ...; expanded into the table on load)
  FinishTable fin, finExt;
  std::vector<ClassPool> finClasses(finishPools.size());
  OrbitModel pm;
  if (!finishPools.empty()) useFinish = true;
  if (useFinish) {
    pm = pairModel();
    std::vector<std::pair<const ClassPool *, uint8_t>> list;
    for (size_t i = 0; i < finishPools.size(); i++) {
      if (!finClasses[i].load(finishPools[i].c_str())) { fprintf(stderr, "cannot read %s\n", finishPools[i].c_str()); return 1; }
      list.push_back({&finClasses[i], (uint8_t)(10 + i)});
    }
    buildFinishFromClassList(fin, finExt, pool, beam.Z, cs.eff4, list, pm);
    beam.finish = &fin;
    beam.finishSkip = finishSkip;
    beam.finishLBMis = finishLB;
  }
  beam.rootTop = rootTopM;
  beam.rootLevels = rootTopL;
  beam.firstWidth = firstW;
  beam.widthGrowth = growF;
  beam.widthCap = growCap;
  auto t0 = std::chrono::steady_clock::now();
  std::vector<State> st;
  for (std::string line; std::getline(std::cin, line);) {
    if (line.size() < 48 || line[0] == '#') continue;
    State s;
    for (int p = 0; p < 48; p++) s.col[p] = line[p] - '0';
    s.setMasks();
    st.push_back(s);
  }
  std::vector<std::string> out(st.size());
  // --algstats: [correct before / 8][length], beam algorithms and finish-table ones
  std::mutex statMu;
  long lenBeam[7][16] = {}, lenFin[7][16] = {}, gainSum[7] = {}, nStep[7] = {};
  // --rootrank: [step][rank bucket], and per step: disjoint from earlier steps, rejected at the start by keep-solved,
  // gain at the start, gain when applied
  const long rankB[] = {16, 64, 256, 1024, 4096, 16384, 65536, 1L << 40};
  long rk[6][8] = {}, rkDisj[6] = {}, rkRej[6] = {}, rkN[6] = {}, rkG0[6] = {}, rkG[6] = {};
  std::vector<uint64_t> Xface(pool.size());
  if (rootRank)
    for (size_t a = 0; a < pool.size(); a++)
      for (int d = 0; d < 48; d++) { int sp = pool.src[a][d]; if ((sp % 24) / 4 != (d % 24) / 4) Xface[a] |= 1ULL << sp; }
  std::atomic<int> nx{0};
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (int k; (k = nx++) < (int)st.size();) {
        bool ok;
        auto sol = beam.solve(st[k], ok, W);
        if (!ok) { out[k] = str(st[k]) + " -1\n"; continue; }
        std::vector<State> seq{st[k]};
        std::vector<int> c{0};
        bool extFinish = false;  // last algorithm from a --finishpool: checked by its hash, not replayed
        std::vector<uint16_t> merged;  // the whole solution with same-axis groups combined at the seams
        for (auto &x : sol) {
          if (x.pool == 0) {
            seq.push_back(seq.back().apply(pool, x.idx));
            c.push_back(c.back() + pool.cost[x.idx]);
            merge_into(merged, std::vector<uint16_t>(pool.gbegin(x.idx), pool.gend(x.idx)));
            continue;
          }
          uint64_t ks[4] = {beam.zob(seq.back()), 0, 0, 0};  // (full tables here: no rotation keys)
          FinishTable::Hit fh;
          if (!fin.find(ks, fh) || fh.pid != x.pool || fh.alg != x.idx) { c.push_back(-1 << 20); break; }
          c.push_back(c.back() + fh.cost);
          const ClassPool &cp = finClasses[x.pool - 10];
          merge_into(merged, to_groups(pm.image(cp.rep[x.idx / 96], x.idx % 96)));
          extFinish = true;
        }
        if (c.back() < 0 || (!extFinish && seq.back().correct() != 48)) { out[k] = str(st[k]) + " -1\n"; continue; }
        if (rootRank) {
          const State &s0 = st[k];
          const int c0 = s0.correct();
          uint64_t corr0 = 0;
          for (int q = 0; q < 48; q++) corr0 |= (uint64_t)(s0.col[q] == target(q)) << q;
          auto rootScore = [&](size_t a) {
            int c = 0;
            for (int col = 0; col < 6; col++) c += __builtin_popcountll(pool.S[a][col] & s0.M[col]);
            return 12 * (c - c0) - cs.eff4[a];
          };
          std::vector<int> sc(pool.size());
          for (size_t a = 0; a < pool.size(); a++) sc[a] = rootScore(a);
          uint64_t suppSoFar = 0;
          std::lock_guard<std::mutex> g(statMu);
          for (size_t i = 0; i < sol.size() && i < 6; i++) {
            if (sol[i].pool != 0) break;
            const uint32_t a = sol[i].idx;
            long r = 1;
            for (size_t b = 0; b < pool.size(); b++) r += sc[b] > sc[a];
            int bk = 0;
            while (r > rankB[bk]) bk++;
            rk[i][bk]++; rkN[i]++;
            uint64_t sa = 0;
            for (int d = 0; d < 48; d++) if (pool.src[a][d] != d) sa |= 1ULL << d;
            rkDisj[i] += (sa & suppSoFar) == 0;
            suppSoFar |= sa;
            rkRej[i] += (Xface[a] & corr0) != 0;
            rkG0[i] += (sc[a] + cs.eff4[a]) / 12;
            rkG[i] += seq[i + 1].correct() - seq[i].correct();
          }
        }
        if (algStats) {
          std::lock_guard<std::mutex> g(statMu);
          for (size_t i = 0; i < sol.size(); i++) {
            const int before = seq[i].correct(), len = c[i + 1] - c[i];
            const int after = i + 1 < seq.size() ? seq[i + 1].correct() : 48;
            (sol[i].pool == 0 ? lenBeam : lenFin)[before / 8][std::min(len, 15)]++;
            gainSum[before / 8] += after - before; nStep[before / 8]++;
          }
        }
        std::string o = str(st[k]) + " " + std::to_string(c.back()) + " " + std::to_string(groups_cost(merged)) + "\n";
        if (path)
          for (size_t i = 1; i + 1 < seq.size(); i++) o += "p " + str(seq[i]) + " " + std::to_string(c.back() - c[i]) + "\n";
        out[k] = o;
      }
    });
  for (auto &t : th) t.join();
  long sum = 0, msum = 0, nok = 0;
  for (auto &o : out) {
    if (!quiet) fputs(o.c_str(), stdout);
    char *e;
    long c = strtol(o.c_str() + 49, &e, 10);
    long mc = strtol(e, &e, 10);
    if (c >= 0) { sum += c; msum += mc; nok++; }
  }
  fprintf(stderr, "W %d: %ld/%zu solved, avg %.3f (merged %.3f), %.2fs\n", W, nok, st.size(), (double)sum / std::max(1L, nok),
          (double)msum / std::max(1L, nok), std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  if (beam.timing) {
    double tot = 0;
    for (int d = 0; d < 60; d++) tot += beam.lvNs[d];
    fprintf(stderr, "level    nodes  avg-correct  scored/node   time-share  cumulative\n");
    double cum = 0;
    for (int d = 0; d < 60; d++)
      if (beam.lvNodes[d]) {
        cum += beam.lvNs[d];
        fprintf(stderr, "%5d %8ld %12.1f %12.0f %11.1f%% %10.1f%%\n", d, (long)beam.lvNodes[d], (double)beam.lvCorrect[d] / beam.lvNodes[d],
                (double)beam.lvSurv[d] / beam.lvNodes[d], 100.0 * beam.lvNs[d] / tot, 100.0 * cum / tot);
      }
    fprintf(stderr, "filter %.2fs, score %.2fs, node probes %.2fs, seam bonus %.2fs (%zu heads), expand %.2fs, weakest rescans %ld, admit finish probes %ld in %.2fs; nodes from list %ld, restricted %ld, bitset %ld (rebuilt from scratch %ld) (thread time); pool %zu\n", beam.nsFilter * 1e-9, beam.nsScore * 1e-9, beam.nsNodePre * 1e-9, beam.nsSeam * 1e-9, beam.heads.size(), beam.nsExpand * 1e-9, (long)beam.nWeakScan, (long)beam.nAdmitFin, beam.nsAdmitFin * 1e-9, (long)beam.nFromList, (long)beam.nRestricted, (long)beam.nBitset, (long)beam.nRebuild, pool.size());
    fprintf(stderr, "correct    nodes  scored/node  admitted/node\n");
    for (int c = 0; c <= 48; c++)
      if (beam.histNodes[c])
        fprintf(stderr, "%7d %8ld %12.0f %14.1f\n", c, (long)beam.histNodes[c], (double)beam.histSurv[c] / beam.histNodes[c],
                (double)beam.histAdmit[c] / beam.histNodes[c]);
  }
  if (algStats) {
    fprintf(stderr, "algorithm lengths in the solutions, by correct positions before it (b = beam algorithm, f = finish table)\n");
    fprintf(stderr, "correct   steps  avg-gain  avg-len  lengths\n");
    for (int r = 0; r < 7; r++) {
      if (!nStep[r]) continue;
      long tl = 0;
      std::string h;
      for (int L = 0; L < 16; L++) {
        tl += L * (lenBeam[r][L] + lenFin[r][L]);
        if (lenBeam[r][L]) h += " " + std::to_string(L) + "b:" + std::to_string(lenBeam[r][L]);
        if (lenFin[r][L]) h += " " + std::to_string(L) + "f:" + std::to_string(lenFin[r][L]);
      }
      fprintf(stderr, "%2d-%-2d %9ld %8.2f %8.2f %s\n", 8 * r, std::min(8 * r + 7, 47), nStep[r], (double)gainSum[r] / nStep[r], (double)tl / nStep[r], h.c_str());
    }
  }
  if (rootRank) {
    fprintf(stderr, "solution step: rank at the start by score (share of steps), disjoint from earlier steps, keep-solved-rejected at the start, gain at start / when applied\n");
    fprintf(stderr, "step     n   <=16  <=64 <=256  <=1k  <=4k <=16k <=64k  more  disjoint  rej@start  gain0  gain\n");
    for (int i = 0; i < 6; i++) {
      if (!rkN[i]) continue;
      fprintf(stderr, "%4d %5ld", i + 1, rkN[i]);
      for (int b = 0; b < 8; b++) fprintf(stderr, " %5.1f", 100.0 * rk[i][b] / rkN[i]);
      fprintf(stderr, " %8.1f%% %9.1f%% %6.2f %5.2f\n", 100.0 * rkDisj[i] / rkN[i], 100.0 * rkRej[i] / rkN[i], (double)rkG0[i] / rkN[i], (double)rkG[i] / rkN[i]);
    }
  }
}
