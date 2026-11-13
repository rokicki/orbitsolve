// Testbed: an exact opening over disjoint algorithm sets, then a beam from
// its best sets.  For each pair state:
//   1. score every algorithm at the start (gain = correct after - before;
//      keep-solved: never carry a correct piece off its face);
//   2. opening: over the best P candidates by quarter-moves per fixed piece
//      (per support mask only the (cost, gain)-nondominated ones), branch and
//      bound over sets with pairwise disjoint supports (they commute, gains
//      add), keeping the N best sets with at least S pieces solved, ranked by
//      the beam's own score  4H * solved - quarter-moves  (node budget X);
//   3. beam of width b from those sets (or from the start if none), levels
//      0 .. L-1 scoring only the start's best M algorithms (M 0: all);
//      finish table F (0 none, 1 the pool's own, 2 + c12x, p9, eg8).
// Run from dist/gullicube (reads its .cls files).  Measure instructions and
// cycles outside (/usr/bin/time -l) with -t 1; the phase times printed are
// thread times.
//
//   seedbeam STATES [-P n] [-S n] [-N n] [-X n] [-b n] [-M n] [-L n] [-H x] [-F 0|1|2] [-n k] [-t T] [-v]
#include "solvers.h"
#include "classpool.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct Opt {
  int P = 640, S = 28, N = 64, b = 64, M = 4000, L = 1, F = 2, nmax = 1 << 30, threads = 1;
  long X = 10000000;
  double H = 3;
  bool verbose = false;
};

static void usage() {
  fprintf(stderr,
          "usage: seedbeam STATES [options]   (run from dist/gullicube)\n"
          "  opening (exact disjoint sets at the start):\n"
          "  -P n   prefix: candidates by quarter-moves per fixed piece (default 640; 0 = no opening)\n"
          "  -S n   seed at this many solved pieces, total (default 28)\n"
          "  -N n   seeds handed to the beam (default 64)\n"
          "  -X n   opening node budget (default 10000000)\n"
          "  beam:\n"
          "  -b n   beam width (default 64)\n"
          "  -M n   levels 0..L-1 score only the start's best M algorithms (default 4000; 0 = all)\n"
          "  -L n   (default 1)\n"
          "  -H x   moves per misplaced piece in the score (default 3)\n"
          "  -F f   finish table: 0 none, 1 the pool's own, 2 + c12x/p9/eg8 (default 2)\n"
          "  testbed:\n"
          "  -n k   first k states only;  -t T threads (default 1);  -v per-state lines\n");
}

struct Cand { uint64_t S, F; uint32_t alg; int e4, gain; float unit; };

// branch and bound over disjoint sets; keeps the N best qualifying sets
struct Opening {
  const std::vector<Cand> *C = nullptr;
  std::vector<std::vector<uint32_t>> lists;
  std::vector<uint32_t> chosen;
  int mul = 12, need0 = 0, N = 64;  // need0: gain needed to reach S
  long nodes = 0, X = 0;
  bool aborted = false;
  uint64_t wrong0 = 0;
  // kept sets: (f = quarter-moves - mul * gain; lower is better), as a max-heap on f
  std::vector<std::pair<int, std::vector<uint32_t>>> heap;
  int worst() const { return (int)heap.size() < N ? INT32_MAX : heap.front().first; }
  void offer(int f) {
    if ((int)heap.size() < N) {
      heap.push_back({f, chosen});
      std::push_heap(heap.begin(), heap.end(), [](auto &x, auto &y) { return x.first < y.first; });
    } else if (f < heap.front().first) {
      std::pop_heap(heap.begin(), heap.end(), [](auto &x, auto &y) { return x.first < y.first; });
      heap.back() = {f, chosen};
      std::push_heap(heap.begin(), heap.end(), [](auto &x, auto &y) { return x.first < y.first; });
    }
  }
  // sorted per-position least quarter-moves per piece over L, prefix sums P[0..k]
  int bound(const std::vector<uint32_t> &L, uint64_t avail, float *P) const {
    float u[48];
    for (int p = 0; p < 48; p++) u[p] = 1e9f;
    for (uint32_t i : L) {
      const Cand &c = (*C)[i];
      for (uint64_t f = c.F & avail; f; f &= f - 1) {
        const int p = __builtin_ctzll(f);
        if (c.unit < u[p]) u[p] = c.unit;
      }
    }
    float v[48];
    int k = 0;
    for (uint64_t f = avail; f; f &= f - 1) { const float x = u[__builtin_ctzll(f)]; if (x < 1e8f) v[k++] = x; }
    std::sort(v, v + k);
    P[0] = 0;
    for (int i = 0; i < k; i++) P[i + 1] = P[i] + v[i];
    return k;
  }
  // least f reachable from (g4, gain) given the bound: min over D >= need of g4 + P[D] - mul (gain + D)
  bool promising(int g4, int gain, const float *P, int k) const {
    const int need = std::max(0, need0 - gain);
    if (need > k) return false;
    // completions stop once they reach S, one algorithm (<= 10 pieces) past it at most
    float best = 1e30f;
    for (int d = need; d <= std::min(k, need + 10); d++) best = std::min(best, P[d] - mul * d);
    return g4 - mul * gain + (int)std::ceil(best - 1e-3f) < worst();
  }
  void dfs(int depth, uint64_t used, int g4, int gain) {
    if (++nodes > X) { aborted = true; return; }
    const std::vector<uint32_t> &L = lists[depth];
    float P[49];
    const int k = bound(L, wrong0 & ~used, P);
    if (!promising(g4, gain, P, k)) return;
    std::vector<uint32_t> &Mx = lists[depth + 1];
    for (size_t j = 0; j < L.size() && !aborted; j++) {
      const Cand &c = (*C)[L[j]];
      const int ng4 = g4 + c.e4, ng = gain + c.gain;
      chosen.push_back(c.alg);
      if (ng >= need0) {  // a seed: handed to the beam here, not extended
        offer(ng4 - mul * ng);
        chosen.pop_back();
        continue;
      }
      // the child's bound is at least this node's
      if (promising(ng4, ng, P, k)) {
        Mx.clear();
        for (size_t t = j + 1; t < L.size(); t++)
          if (!((*C)[L[t]].S & c.S)) Mx.push_back(L[t]);
        if (!Mx.empty()) dfs(depth + 1, used | c.S, ng4, ng);
      }
      chosen.pop_back();
    }
  }
};

int main(int argc, char **argv) {
  if (argc < 2) { usage(); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  Opt o;
  const char *statesFile = argv[1];
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() -> const char * {
      if (i + 1 >= argc) { fprintf(stderr, "%s needs a value\n", a.c_str()); exit(1); }
      return argv[++i];
    };
    if (a == "-P") o.P = atoi(next());
    else if (a == "-S") o.S = atoi(next());
    else if (a == "-N") o.N = atoi(next());
    else if (a == "-X") o.X = atol(next());
    else if (a == "-b") o.b = atoi(next());
    else if (a == "-M") o.M = atoi(next());
    else if (a == "-L") o.L = atoi(next());
    else if (a == "-H") o.H = atof(next());
    else if (a == "-F") o.F = atoi(next());
    else if (a == "-n") o.nmax = atoi(next());
    else if (a == "-t") o.threads = atoi(next());
    else if (a == "-v") o.verbose = true;
    else { fprintf(stderr, "unknown option %s\n", argv[i]); usage(); return 1; }
  }
  std::vector<State> st;
  {
    std::ifstream in(statesFile);
    if (!in) { fprintf(stderr, "cannot read %s\n", statesFile); return 1; }
    for (std::string line; std::getline(in, line) && (int)st.size() < o.nmax;) {
      if (line.size() < 48 || line[0] == '#') continue;
      State s;
      for (int p = 0; p < 48; p++) s.col[p] = line[p] - '0';
      s.setMasks();
      st.push_back(s);
    }
  }
  const double tLoad0 = now();
  Pool pool;
  {
    ClassPool cp;
    if (!cp.load("data/p8s10.cls")) { fprintf(stderr, "cannot read p8s10.cls (run from dist/gullicube)\n"); return 1; }
    poolFromClasses(pool, cp, pairModel());
  }
  pool.sortByCost();
  const size_t n = pool.size();
  ColourSolver cs;
  Weights wts;
  wts.setupFace = 3;
  cs.build(pool, nullptr, wts);
  TableBeam beam;
  beam.build(cs);
  beam.slotSeed = 1;
  beam.seamAware = true;
  const int mul = (int)std::lround(4 * o.H);
  beam.mulOverride = mul;
  FinishTable fin, finExt;
  std::vector<ClassPool> finClasses;
  OrbitModel pm = pairModel();
  if (o.F >= 1) {
    std::vector<std::pair<const ClassPool *, uint8_t>> list;
    if (o.F >= 2) {
      const char *fp[] = {"data/c12x.cls", "data/p9.cls", "data/eg8.cls"};
      finClasses.resize(3);
      for (int i = 0; i < 3; i++) {
        if (!finClasses[i].load(fp[i])) { fprintf(stderr, "cannot read %s\n", fp[i]); return 1; }
        list.push_back({&finClasses[i], (uint8_t)(10 + i)});
      }
    }
    buildFinishFromClassList(fin, finExt, pool, beam.Z, cs.eff4, list, pm);
    beam.finish = &fin;
  }
  // per algorithm: support, face-leave mask
  std::vector<uint64_t> supp(n), Xf(n);
  for (size_t a = 0; a < n; a++)
    for (int d = 0; d < 48; d++) {
      const int sp = pool.src[a][d];
      if (sp != d) supp[a] |= 1ULL << d;
      if ((sp % 24) / 4 != (d % 24) / 4) Xf[a] |= 1ULL << sp;
    }
  printf("load %.2fs; %zu states, P %d S %d N %d X %ld b %d M %d L %d H %g F %d\n", now() - tLoad0, st.size(), o.P, o.S, o.N, o.X, o.b,
         o.M, o.L, o.H, o.F);
  fflush(stdout);

  struct Res { int moves = -1, merged = -1, seeds = 0, seedSolved = 0, seedRank = -2; long nodes = 0; bool aborted = false; double tScore = 0, tOpen = 0, tBeam = 0; };
  std::vector<Res> res(st.size());
  std::atomic<int> nx{0};
  std::vector<std::thread> th;
  for (int t = 0; t < o.threads; t++)
    th.emplace_back([&] {
      std::vector<int> gain(n);
      for (int k; (k = nx++) < (int)st.size();) {
        Res &r = res[k];
        const State &s = st[k];
        const int c0 = s.correct();
        uint64_t corr = 0;
        for (int q = 0; q < 48; q++) corr |= (uint64_t)(s.col[q] == target(q)) << q;
        const uint64_t wrong = ~corr & ((1ULL << 48) - 1);
        // 1. score everything
        double t0 = now();
        for (size_t a = 0; a < n; a++) {
          int c = 0;
          for (int col = 0; col < 6; col++) c += __builtin_popcountll(pool.S[a][col] & s.M[col]);
          gain[a] = (Xf[a] & corr) ? INT32_MIN : c - c0;
        }
        std::vector<uint32_t> startList;
        if (o.M > 0) {
          std::vector<std::pair<int, uint32_t>> sc;
          sc.reserve(n);
          for (size_t a = 0; a < n; a++) if (gain[a] != INT32_MIN) sc.push_back({mul * gain[a] - cs.eff4[a], (uint32_t)a});
          const size_t m = std::min<size_t>(o.M, sc.size());
          std::nth_element(sc.begin(), sc.begin() + (m - 1), sc.end(), [](auto &x, auto &y) { return x.first > y.first; });
          for (size_t i = 0; i < m; i++) startList.push_back(sc[i].second);
          std::sort(startList.begin(), startList.end());
        }
        double t1 = now();
        r.tScore = t1 - t0;
        // 2. opening
        std::vector<std::vector<uint32_t>> seeds;
        if (o.P > 0) {
          std::map<uint64_t, std::vector<Cand>> byMask;
          for (size_t a = 0; a < n; a++) {
            if (gain[a] < 1) continue;
            uint64_t F = 0;
            for (int d = 0; d < 48; d++) {
              const int sp = pool.src[a][d];
              if (sp != d && s.col[sp] == target(d) && (wrong >> d & 1)) F |= 1ULL << d;
            }
            Cand c{supp[a], F, (uint32_t)a, cs.eff4[a], gain[a], (float)cs.eff4[a] / gain[a]};
            auto &v = byMask[c.S];
            bool dom = false;
            for (auto &x : v) if (x.e4 <= c.e4 && x.gain >= c.gain) { dom = true; break; }
            if (dom) continue;
            v.erase(std::remove_if(v.begin(), v.end(), [&](const Cand &x) { return c.e4 <= x.e4 && c.gain >= x.gain; }), v.end());
            v.push_back(c);
          }
          std::vector<Cand> C;
          for (auto &kv : byMask) for (auto &c : kv.second) C.push_back(c);
          std::sort(C.begin(), C.end(), [](const Cand &x, const Cand &y) { return x.unit < y.unit || (x.unit == y.unit && (x.gain > y.gain || (x.gain == y.gain && x.alg < y.alg))); });
          if ((int)C.size() > o.P) C.resize(o.P);
          Opening op;
          op.C = &C;
          op.mul = mul;
          op.need0 = o.S - c0;
          op.N = o.N;
          op.X = o.X;
          op.wrong0 = wrong;
          op.lists.assign(50, {});
          for (uint32_t i = 0; i < C.size(); i++) op.lists[0].push_back(i);
          if (op.need0 <= 0) op.need0 = 0;
          op.dfs(0, 0, 0, 0);
          r.nodes = op.nodes;
          r.aborted = op.aborted;
          std::sort(op.heap.begin(), op.heap.end(), [](auto &x, auto &y) { return x.first < y.first; });
          for (auto &h : op.heap) if (!h.second.empty()) seeds.push_back(h.second);
          r.seeds = seeds.size();
          if (!seeds.empty()) {
            int g = 0;
            for (auto a : seeds[0]) g += gain[a];
            r.seedSolved = c0 + g;
          }
        }
        double t2 = now();
        r.tOpen = t2 - t1;
        // 3. beam
        bool ok;
        auto sol = beam.solve(s, ok, o.b, seeds.empty() ? nullptr : &seeds, o.M > 0 ? &startList : nullptr, o.L);
        r.tBeam = now() - t2;
        if (!ok) continue;
        // which seed does the solution start with
        r.seedRank = -1;
        for (size_t i = 0; i < seeds.size(); i++) {
          bool pre = seeds[i].size() <= sol.size();
          for (size_t j = 0; pre && j < seeds[i].size(); j++) pre = sol[j].pool == 0 && sol[j].idx == seeds[i][j];
          if (pre) { r.seedRank = i; break; }
        }
        // replay: moves, merged moves
        State cur = s;
        int moves = 0;
        std::vector<uint16_t> merged;
        bool good = true;
        for (auto &x : sol) {
          if (x.pool == 0) {
            cur = cur.apply(pool, x.idx);
            moves += pool.cost[x.idx];
            merge_into(merged, std::vector<uint16_t>(pool.gbegin(x.idx), pool.gend(x.idx)));
            continue;
          }
          const FinishTable::Slot *fi = fin.find(beam.zob(cur));
          if (!fi || fi->pid != x.pool || fi->alg != x.idx) { good = false; break; }
          moves += fi->cost;
          if (x.pool >= 10) merge_into(merged, to_groups(pm.image(finClasses[x.pool - 10].rep[x.idx / 96], x.idx % 96)));
          else merge_into(merged, std::vector<uint16_t>(pool.gbegin(x.idx), pool.gend(x.idx)));
          cur.col = {};
          for (int q = 0; q < 48; q++) cur.col[q] = target(q);  // checked by hash
        }
        if (!good || cur.correct() != 48) continue;
        r.moves = moves;
        r.merged = groups_cost(merged);
      }
    });
  for (auto &t : th) t.join();
  long ok = 0, sm = 0, smg = 0, nodes = 0, aborted = 0, seeded = 0, ss = 0;
  double tS = 0, tO = 0, tB = 0;
  long rank0 = 0, rank1_3 = 0, rank4_15 = 0, rank16 = 0, rankNone = 0;
  for (size_t k = 0; k < st.size(); k++) {
    const Res &r = res[k];
    if (o.verbose) printf("  state %zu: moves %d merged %d seeds %d seed-solved %d rank %d nodes %ld%s\n", k, r.moves, r.merged, r.seeds, r.seedSolved, r.seedRank, r.nodes, r.aborted ? " (abort)" : "");
    tS += r.tScore; tO += r.tOpen; tB += r.tBeam;
    nodes += r.nodes; aborted += r.aborted;
    if (r.seeds) { seeded++; ss += r.seedSolved; }
    if (r.moves < 0) continue;
    ok++; sm += r.moves; smg += r.merged;
    if (r.seeds) {
      if (r.seedRank == 0) rank0++;
      else if (r.seedRank >= 1 && r.seedRank <= 3) rank1_3++;
      else if (r.seedRank >= 4 && r.seedRank <= 15) rank4_15++;
      else if (r.seedRank >= 16) rank16++;
      else rankNone++;
    }
  }
  const int ns = st.size();
  printf("RESULT solved %ld/%d avg %.3f merged %.3f | score %.3fs opening %.3fs beam %.3fs (per state ms: %.2f %.2f %.2f) | "
         "opening nodes/state %.0f aborted %ld seeded %ld seed-solved %.1f | winning seed rank 0:%ld 1-3:%ld 4-15:%ld 16+:%ld none:%ld\n",
         ok, ns, (double)sm / std::max(1L, ok), (double)smg / std::max(1L, ok), tS, tO, tB, 1e3 * tS / ns, 1e3 * tO / ns, 1e3 * tB / ns,
         (double)nodes / ns, aborted, seeded, seeded ? (double)ss / seeded : 0.0, rank0, rank1_3, rank4_15, rank16, rankNone);
}
