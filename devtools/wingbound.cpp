#include <set>
// Upper bound on the moves needed to solve one wing orbit (24 distinct
// pieces) with wing algorithms: sequences that change nothing but that wing
// orbit and the centres (wings are solved first, so centres may end up
// anything).
//
// Staged: the wings are placed in a given order, a chunk at a time.  While
// placing a chunk, only algorithms that keep every already placed wing in
// place are used.  The stage state is the positions of the chunk's wings among
// the free positions (n! / (n-k)! states).  Dijkstra from the goal (pools are
// closed under inversion) with each algorithm costing its move count gives the
// stage's exact worst case; the bound is the sum (no cancellation counted).
//
//   wingbound [--chunks K1,K2,...] [--order P0,P1,...] [--random R] [--seed S]
//             [--budget E] [--maxcost C] [--parity J] POOL.cls...      (run from dist/gullicube)
//     --chunks   chunk sizes, summing to 24 (the last wing comes free)
//     --order    wing positions in placing order (default 0..23)
//     --random R also try R random orders, keep the best
//     --parity J stage J (from 0) also makes the whole wing permutation even; later stages keep it even
//     --worst F  write every position at each stage's worst distance (if at most 1e6):
//                "DIST w0 .. w23", the wing at each position ('-' = not tracked)
//
// CERT=DIR writes a certificate (checked by wingcheck): for every state of every stage, a
// solving sequence (the table's path, algorithm moves joined, adjacent turns of one layer
// combined), in DIR/stages/<placed>..<target>/wNN.txt (NN = target wings not at home) plus
// DIR/MANIFEST lines.  Wings are named 'a' + k in certcube.h's wingFacelets order.
#include "classpool.h"
#include "certcube.h"
#include <deque>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int nthreads;
static double budget = 4e11;
static FILE *worstFile = nullptr;

struct Alg { std::array<uint8_t, 24> dst; int cost; uint32_t file = 0, ref = 0; };  // the piece at position s goes to dst[s]
static std::vector<Alg> algs;
static std::deque<ClassPool> pools;
static OrbitModel *gModel;
static int phi[24];  // model position -> certcube wing index
static std::vector<std::array<int, 24>> wsym;  // the 48 cube symmetries on model wing positions
static void certStage(const struct Stage &st, uint32_t placed, const std::vector<uint8_t> &dist, const std::vector<int> &caAlg,
                      const std::vector<int> &caCost, const std::vector<uint8_t> &dT);

struct Stage {
  std::vector<int> pos;      // free positions (P positions)
  std::vector<int> cidx;     // P position -> free index
  std::vector<int> pieces;   // wings placed by this stage (their home positions)
  int n, k;
  std::vector<uint64_t> radix;
  uint64_t total = 1;
};
static Stage makeStage(uint32_t placed, const std::vector<int> &pieces) {
  Stage st;
  st.cidx.assign(24, -1);
  for (int p = 0; p < 24; p++) if (!(placed >> p & 1)) { st.cidx[p] = st.pos.size(); st.pos.push_back(p); }
  st.n = st.pos.size();
  st.pieces = pieces;
  st.k = pieces.size();
  for (int i = 0; i < st.k; i++) { st.radix.push_back(st.n - i); st.total *= st.n - i; }
  return st;
}
// state: free index of each chunk wing
static inline uint64_t rankOf(const Stage &st, const uint8_t *x) {
  uint64_t r = 0;
  uint32_t used = 0;
  for (int i = 0; i < st.k; i++) {
    r = r * st.radix[i] + (x[i] - __builtin_popcount(used & ((1u << x[i]) - 1)));
    used |= 1u << x[i];
  }
  return r;
}
static inline void unrank(const Stage &st, uint64_t r, uint8_t *x) {
  uint64_t d[24];
  for (int i = st.k - 1; i >= 0; i--) { d[i] = r % st.radix[i]; r /= st.radix[i]; }
  uint32_t used = 0;
  for (int i = 0; i < st.k; i++) {
    int v = -1;
    for (uint64_t c = d[i] + 1; c; c--) { v++; while (used >> v & 1) v++; }
    x[i] = v;
    used |= 1u << v;
  }
}

// exact worst case of a stage (-1 if some state is unreachable)
// parMode: 0 ignore wing parity; 1 also make the whole wing permutation even (from either parity);
// 2 start even and stay even (worst over even states only)
static int solveStage(const Stage &st, uint32_t placed, std::string &info, int parMode = 0) {
  double t0 = now();
  // usable: placed wings stay put; reduced to the free positions, cheapest per action
  struct H { size_t operator()(const std::vector<uint8_t> &v) const { size_t h = 1469598103934665603ULL; for (auto c : v) h = (h ^ c) * 1099511628211ULL; return h; } };
  std::unordered_map<std::vector<uint8_t>, std::pair<int, int>, H> best;  // action -> (cost, alg)
  long usable = 0;
  for (size_t ai = 0; ai < algs.size(); ai++) {
    const Alg &a = algs[ai];
    bool ok = true;
    for (int p = 0; p < 24 && ok; p++) if (placed >> p & 1) ok = a.dst[p] == p;
    if (!ok) continue;
    usable++;
    std::vector<uint8_t> d(st.n);
    bool id = true;
    for (int i = 0; i < st.n; i++) { d[i] = st.cidx[a.dst[st.pos[i]]]; id &= d[i] == i; }
    if (parMode) { int p = 0; for (int i = 0; i < 24; i++) for (int j = i + 1; j < 24; j++) p ^= a.dst[i] > a.dst[j]; d.push_back(p); id &= p == 0; }
    if (id) continue;
    auto it = best.find(d);
    if (it == best.end()) best.emplace(std::move(d), std::make_pair(a.cost, (int)ai));
    else if (a.cost < it->second.first) it->second = {a.cost, (int)ai};
  }
  struct CA { std::vector<uint8_t> d; int cost, alg; };
  auto algPar = [](const Alg &a) { int p = 0; for (int i = 0; i < 24; i++) for (int j = i + 1; j < 24; j++) p ^= a.dst[i] > a.dst[j]; return p; };
  std::vector<CA> ca;
  for (auto &kv : best) ca.push_back({kv.first, kv.second.first, kv.second.second});
  std::sort(ca.begin(), ca.end(), [](const CA &a, const CA &b) { return a.cost < b.cost; });
  int cap = 255;
  while (ca.size() > 1 && (double)st.total * ca.size() > budget) {
    cap = ca.back().cost - 1;
    while (!ca.empty() && ca.back().cost > cap) ca.pop_back();
  }
  (void)algPar;
  std::vector<uint8_t> dT(ca.size() * 24), aPar(ca.size(), 0);
  for (size_t a = 0; a < ca.size(); a++) { for (int i = 0; i < st.n; i++) dT[a * 24 + i] = ca[a].d[i]; if (parMode) aPar[a] = ca[a].d[st.n]; }
  const int PB = parMode ? 2 : 1;  // states per rank (parity bit low)
  const uint64_t NS = st.total * PB;
  std::vector<uint8_t> dist(NS, 0xFF);
  uint8_t goal[24];
  for (int i = 0; i < st.k; i++) goal[i] = st.cidx[st.pieces[i]];
  dist[rankOf(st, goal) * PB] = 0;
  uint64_t reached = 0;
  int maxd = 0;
  for (int d = 0; d < 255; d++) {
    std::atomic<uint64_t> next{0}, found{0}, later{0};
    auto work = [&] {
      uint8_t x[24], y[24];
      uint64_t f = 0, l = 0;
      const uint64_t chunk = 1 << 14;
      for (uint64_t lo; (lo = next.fetch_add(chunk)) < NS;) {
        const uint64_t hi = std::min<uint64_t>(NS, lo + chunk);
        for (uint64_t r = lo; r < hi; r++) {
          const uint8_t dv = std::atomic_ref<uint8_t>(dist[r]).load(std::memory_order_relaxed);
          if (dv != d) { if (dv != 0xFF && dv > d) l++; continue; }
          f++;
          unrank(st, r / PB, x);
          const int bit = PB == 2 ? r & 1 : 0;
          for (size_t a = 0; a < ca.size(); a++) {
            const int nd = d + ca[a].cost;
            if (nd >= 255) break;
            const uint8_t *D = &dT[a * 24];
            for (int i = 0; i < st.k; i++) y[i] = D[x[i]];
            std::atomic_ref<uint8_t> ar(dist[rankOf(st, y) * PB + (PB == 2 ? bit ^ aPar[a] : 0)]);
            uint8_t cur = ar.load(std::memory_order_relaxed);
            while (nd < cur && !ar.compare_exchange_weak(cur, (uint8_t)nd, std::memory_order_relaxed)) {}
          }
        }
      }
      found += f, later += l;
    };
    std::vector<std::thread> th;
    for (int i = 0; i < nthreads; i++) th.emplace_back(work);
    for (auto &t : th) t.join();
    if (found) { reached += found; maxd = d; }
    if (!found && !later) {
      bool any = false;
      for (uint64_t r = 0; r < NS && !any; r++) any = dist[r] != 0xFF && dist[r] > d;
      if (!any) break;
    }
  }
  if (parMode) {
    // mode 1: every (rank, parity); mode 2: even states only.  On a stage that places every free wing
    // the parity is the arrangement's own, so only consistent pairs count.
    uint64_t cnt = 0, miss = 0;
    std::vector<uint64_t> h(256, 0);
    int wb[2] = {0, 0};
    uint64_t mb[2] = {0, 0};
    uint8_t x[24];
    for (uint64_t r = 0; r < NS; r++) {
      const int bit = r & 1;
      if (parMode == 2 && bit) continue;
      if (st.k == st.n) {
        unrank(st, r / 2, x);
        int par = 0;
        for (int i = 0; i < st.k; i++) for (int j = i + 1; j < st.k; j++) par ^= x[i] > x[j];
        if (par != bit) continue;
      }
      cnt++;
      if (dist[r] == 0xFF) miss++; else h[dist[r]]++, wb[bit] = std::max(wb[bit], (int)dist[r]);
      if (dist[r] == 0xFF) mb[bit]++;
    }
    int w = 0;
    for (int d = 0; d < 255; d++) if (h[d]) w = d;
    char pb[200];
    snprintf(pb, sizeof pb, "%s: %llu states, %llu unreachable, %zu actions, %.1fs", parMode == 1 ? "parity fixed here" : "even kept",
             (unsigned long long)cnt, (unsigned long long)miss, ca.size(), now() - t0);
    info = pb;
    info += "\n      distances:";
    for (int d = 0; d < 255; d++) if (h[d]) info += " " + std::to_string(d) + ":" + std::to_string(h[d]);
    info += "\n      even moves (parity kept): worst " + std::to_string(wb[0]) + ", unreachable " + std::to_string(mb[0]) +
            "; odd moves (parity swapped): worst " + std::to_string(wb[1]) + ", unreachable " + std::to_string(mb[1]);
    return miss ? -1 : w;
  }
  char buf[200];
  snprintf(buf, sizeof buf, "%llu states, %ld usable -> %zu actions%s, %.1fs", (unsigned long long)st.total, usable, ca.size(),
           cap < 255 ? (" (cost <= " + std::to_string(cap) + ")").c_str() : "", now() - t0);
  info = buf;
  if (reached == st.total) {
    std::vector<uint64_t> h(256, 0);
    for (uint64_t r = 0; r < st.total; r++) h[dist[r]]++;
    info += "\n      distances:";
    for (int d = 0; d < 255; d++) if (h[d]) info += " " + std::to_string(d) + ":" + std::to_string(h[d]);
    if (st.k == st.n) {  // every free wing is placed here: the state is a permutation, split by parity
      std::vector<std::vector<uint64_t>> hp(2, std::vector<uint64_t>(256, 0));
      std::atomic<uint64_t> nx{0};
      std::vector<std::vector<std::vector<uint64_t>>> hT(nthreads, hp);
      auto pw = [&](int t) {
        uint8_t x[24];
        for (uint64_t lo; (lo = nx.fetch_add(1 << 16)) < st.total;)
          for (uint64_t r = lo; r < std::min<uint64_t>(st.total, lo + (1 << 16)); r++) {
            unrank(st, r, x);
            int par = 0;
            for (int i = 0; i < st.k; i++) for (int j = i + 1; j < st.k; j++) par ^= x[i] > x[j];
            hT[t][par][dist[r]]++;
          }
      };
      std::vector<std::thread> th;
      for (int t = 0; t < nthreads; t++) th.emplace_back(pw, t);
      for (auto &t : th) t.join();
      for (auto &a : hT) for (int q = 0; q < 2; q++) for (int d = 0; d < 256; d++) hp[q][d] += a[q][d];
      for (int q = 0; q < 2; q++) {
        int w = 0;
        for (int d = 0; d < 255; d++) if (hp[q][d]) w = d;
        info += std::string("\n      ") + (q ? "odd" : "even") + " positions: worst " + std::to_string(w) + "; distances:";
        for (int d = 0; d < 255; d++) if (hp[q][d]) info += " " + std::to_string(d) + ":" + std::to_string(hp[q][d]);
      }
    }
  }
  if (getenv("CERT") && reached == st.total) {
    std::vector<int> caAlg, caCost;
    for (auto &a : ca) caAlg.push_back(a.alg), caCost.push_back(a.cost);
    certStage(st, placed, dist, caAlg, caCost, dT);
  }
  if (worstFile && reached == st.total) {
    uint64_t cnt = 0;
    for (uint64_t r = 0; r < st.total; r++) cnt += dist[r] == maxd;
    fprintf(worstFile, "# stage placing %d wings (%d placed): %llu positions at %d%s\n", st.k, 24 - st.n,
            (unsigned long long)cnt, maxd, cnt > 1000000 ? " (too many, not listed)" : "");
    if (cnt <= 1000000) {
      uint8_t x[24];
      for (uint64_t r = 0; r < st.total; r++) {
        if (dist[r] != maxd) continue;
        unrank(st, r, x);
        // wing at each position: placed wings at home, this stage's wings where they are, '-' untracked
        int at[24];
        for (int p = 0; p < 24; p++) at[p] = (placed >> p & 1) ? p : -1;
        for (int i = 0; i < st.k; i++) at[st.pos[x[i]]] = st.pieces[i];
        fprintf(worstFile, "%d", maxd);
        for (int p = 0; p < 24; p++) { if (at[p] < 0) fprintf(worstFile, " -"); else fprintf(worstFile, " %d", at[p]); }
        fprintf(worstFile, "\n");
      }
    }
    fflush(worstFile);
  }
  return reached == st.total ? maxd : -1;
}

static std::vector<int> parseList(const std::string &s) {
  std::vector<int> r;
  size_t p = 0;
  while (p < s.size()) {
    size_t q = s.find(',', p);
    if (q == std::string::npos) q = s.size();
    r.push_back(atoi(s.substr(p, q - p).c_str()));
    p = q + 1;
  }
  return r;
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::vector<int> chunks = {5, 4, 4, 11}, order;
  int nRandom = 0, maxCost = 99, parStage = -1;
  uint64_t seed = std::random_device{}();
  int ai = 1;
  for (; ai < argc && argv[ai][0] == '-' && argv[ai][1] == '-'; ai++) {
    std::string o = argv[ai];
    if (o == "--chunks" && ai + 1 < argc) chunks = parseList(argv[++ai]);
    else if (o == "--order" && ai + 1 < argc) order = parseList(argv[++ai]);
    else if (o == "--random" && ai + 1 < argc) nRandom = atoi(argv[++ai]);
    else if (o == "--seed" && ai + 1 < argc) seed = strtoull(argv[++ai], 0, 10);
    else if (o == "--budget" && ai + 1 < argc) budget = atof(argv[++ai]);
    else if (o == "--maxcost" && ai + 1 < argc) maxCost = atoi(argv[++ai]);
    else if (o == "--parity" && ai + 1 < argc) parStage = atoi(argv[++ai]);
    else if (o == "--worst" && ai + 1 < argc) {
      worstFile = fopen(argv[++ai], "w");
      if (!worstFile) { fprintf(stderr, "cannot write %s\n", argv[ai]); return 1; }
    }
    else { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
  }
  if (ai >= argc) {
    fprintf(stderr, "usage: wingbound [--chunks K1,K2,..] [--order P0,..,P23] [--random R] [--seed S] [--budget E] [--maxcost C] [--parity J] POOL.cls...\n");
    return 1;
  }
  int sum = 0;
  for (int c : chunks) sum += c;
  if (sum != 24 && sum != 23) { fprintf(stderr, "chunks must sum to 23 or 24\n"); return 1; }
  if (order.empty()) for (int p = 0; p < 24; p++) order.push_back(p);
  if (order.size() != 24) { fprintf(stderr, "--order needs 24 positions\n"); return 1; }
  printf("seed %llu\n", (unsigned long long)seed);
  nthreads = std::max(1u, std::thread::hardware_concurrency());
  OrbitModel m = wingModel();
  struct H { size_t operator()(const std::array<uint8_t, 24> &v) const { size_t h = 1469598103934665603ULL; for (auto c : v) h = (h ^ c) * 1099511628211ULL; return h; } };
  gModel = &m;
  struct Src { uint8_t cost; uint32_t file, ref; };
  std::unordered_map<std::array<uint8_t, 24>, Src, H> bestPerm;
  for (; ai < argc; ai++) {
    pools.emplace_back();
    ClassPool &cp = pools.back();
    if (!cp.load(argv[ai])) { fprintf(stderr, "cannot read %s\n", argv[ai]); return 1; }
    const uint32_t fi = pools.size() - 1;
    size_t before = bestPerm.size();
    cp.expand(m, [&](const uint8_t *src, int c, uint32_t ref) {
      if (c > maxCost) return;
      std::array<uint8_t, 24> d;
      for (int j = 0; j < 24; j++) d[src[j]] = j;
      auto it = bestPerm.find(d);
      if (it == bestPerm.end()) bestPerm.emplace(d, Src{(uint8_t)c, fi, ref});
      else if (c < it->second.cost) it->second = {(uint8_t)c, fi, ref};
    }, false);
    printf("%s: %zu new permutations (total %zu)\n", argv[ai], bestPerm.size() - before, bestPerm.size());
    fflush(stdout);
  }
  {  // cheapest cost of every 3-cycle over all pools
    std::map<int, int> h, h2; int n3 = 0, n22 = 0;
    for (auto &kv : bestPerm) {
      int mv = 0;
      for (int p = 0; p < 24; p++) mv += kv.first[p] != p;
      bool cyc = mv == 3;
      if (cyc) for (int p = 0; p < 24; p++) if (kv.first[p] != p) cyc &= kv.first[kv.first[p]] != p;
      if (cyc) h[kv.second.cost]++, n3++;
      bool dt = mv == 4;
      if (dt) for (int p = 0; p < 24; p++) if (kv.first[p] != p) dt &= kv.first[kv.first[p]] == p;
      if (dt) h2[kv.second.cost]++, n22++;
    }
    {  // every double swap as the cheapest product of two pool 3-cycles (or a direct pool algorithm)
      std::vector<std::pair<std::array<uint8_t, 24>, int>> c3;
      std::map<std::array<uint8_t, 24>, int> ds;
      for (auto &kv : bestPerm) {
        int mv = 0;
        for (int p = 0; p < 24; p++) mv += kv.first[p] != p;
        bool cyc = mv == 3;
        if (cyc) for (int p = 0; p < 24; p++) if (kv.first[p] != p) cyc &= kv.first[kv.first[p]] != p;
        if (cyc) c3.push_back({kv.first, kv.second.cost});
        bool dt = mv == 4;
        if (dt) for (int p = 0; p < 24; p++) if (kv.first[p] != p) dt &= kv.first[kv.first[p]] == p;
        if (dt) ds[kv.first] = kv.second.cost;
      }
      for (auto &x : c3) for (auto &y : c3) {
        std::array<uint8_t, 24> z;
        int mv = 0;
        for (int p = 0; p < 24; p++) z[p] = x.first[y.first[p]], mv += z[p] != p;
        if (mv != 4) continue;
        bool dt = true;
        for (int p = 0; p < 24; p++) if (z[p] != p) dt &= z[z[p]] == p;
        if (!dt) continue;
        auto it = ds.find(z);
        if (it == ds.end()) ds[z] = x.second + y.second; else it->second = std::min(it->second, x.second + y.second);
      }
      std::map<int, int> hd;
      for (auto &kv : ds) {
        hd[kv.second]++;
        if (kv.second >= 20) {
          printf("  dswap cost %d:", kv.second);
          for (int p = 0; p < 24; p++) if (kv.first[p] != p && p < kv.first[p]) printf(" (%d %d)", p, kv.first[p]);
          printf("\n");
        }
      }
      printf("double swaps (direct or two 3-cycles): %zu;", ds.size());
      for (auto &x : hd) printf(" %d:%d", x.first, x.second);
      printf("\n");
    }
    {
      std::map<int, int> h1;
      for (auto &kv : bestPerm) {
        int mv = 0;
        for (int p = 0; p < 24; p++) mv += kv.first[p] != p;
        if (mv == 2) h1[kv.second.cost]++;
      }
      printf("single swaps (of 276):");
      for (auto &x : h1) printf(" %d:%d", x.first, x.second);
      printf("\n");
    }
    printf("double swaps: %d of 31878;", n22);
    for (auto &x : h2) printf(" %d:%d", x.first, x.second);
    printf("\n");
    printf("3-cycles: %d;", n3);
    for (auto &x : h) printf(" %d:%d", x.first, x.second);
    printf("\n");
  }
  for (auto &kv : bestPerm) {
    bool id = true;
    for (int p = 0; p < 24 && id; p++) id = kv.first[p] == p;
    if (!id) algs.push_back({kv.first, kv.second.cost, kv.second.file, kv.second.ref});
  }
  // ACTIONS=FILE: every algorithm (one per distinct wing permutation, the cheapest) as certcube moves,
  // by cost then text (the concise wing certificate's actions)
  if (getenv("ACTIONS")) {
    std::vector<std::pair<int, std::string>> out;
    for (auto &al : algs) {
      const auto mv = m.image(pools[al.file].rep[al.ref / 96], al.ref % 96);
      std::string t;
      for (auto &x : mv) t += (t.empty() ? "" : " ") + move_str(x);
      std::vector<cc::Mv> chk;
      if (!cc::parseMoves(t, chk) || (int)chk.size() != al.cost) { fprintf(stderr, "ACTIONS: an algorithm's length is not its cost\n"); return 1; }
      out.push_back({al.cost, t});
    }
    std::sort(out.begin(), out.end());
    FILE *f = fopen(getenv("ACTIONS"), "w");
    if (!f) { fprintf(stderr, "ACTIONS: cannot write\n"); return 1; }
    for (auto &x : out) fprintf(f, "%s\n", x.second.c_str());
    fclose(f);
    printf("ACTIONS: %zu algorithms written\n", out.size());
  }
  // model positions -> certcube wing indices (6x6x6, depth 2), by matching every move's action
  {
    cc::Cube C6(6);
    const auto wf = cc::wingFacelets(C6, 2);
    std::vector<int> in(C6.pos.size(), -1);
    for (int k = 0; k < 24; k++) in[wf[k]] = k;
    std::vector<std::array<int, 24>> dM, dC;
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 2; d++) {
        const auto q = m.permOf({Move{(uint8_t)f, (uint8_t)d, 1}});
        std::array<int, 24> a, b;
        for (int k = 0; k < 24; k++) a[q[k]] = k;  // src form -> dst form
        std::vector<cc::Mv> cm;
        cc::parseMoves(move_str(Move{(uint8_t)f, (uint8_t)d, 1}), cm);
        for (int k = 0; k < 24; k++) b[k] = in[C6.perms[cm[0].face][cm[0].depth - 1][wf[k]]];
        dM.push_back(a), dC.push_back(b);
      }
    int found = 0;
    for (int c0 = 0; c0 < 24; c0++) {
      int ph[24];
      for (int k = 0; k < 24; k++) ph[k] = -1;
      ph[0] = c0;
      bool ok = true, grew = true;
      while (ok && grew) {
        grew = false;
        for (size_t mi = 0; mi < dM.size() && ok; mi++)
          for (int p = 0; p < 24 && ok; p++) {
            if (ph[p] < 0) continue;
            const int want = dC[mi][ph[p]];
            if (ph[dM[mi][p]] < 0) ph[dM[mi][p]] = want, grew = true;
            else ok = ph[dM[mi][p]] == want;
          }
      }
      for (int k = 0; k < 24 && ok; k++) ok = ph[k] >= 0;
      if (ok) { found++; memcpy(phi, ph, sizeof phi); }
    }
    if (found != 1) { fprintf(stderr, "cannot match the wing model to the facelet cube (%d matches)\n", found); return 1; }
  }
  // cube symmetries on wing positions: signed axis permutations acting on wing cubie coordinates
  {
    cc::Cube C6(6);
    const auto wf = cc::wingFacelets(C6, 2);
    std::array<int, 3> cub[24];
    for (int p = 0; p < 24; p++) {
      const int i = wf[phi[p]];
      for (int r = 0; r < 3; r++) cub[p][r] = C6.pos[i][r] - C6.nrm[i][r];
    }
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        std::array<int, 24> y;
        for (int p = 0; p < 24; p++) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = cub[p][pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          y[p] = -1;
          for (int x = 0; x < 24; x++) if (cub[x] == q) y[p] = x;
          if (y[p] < 0) { fprintf(stderr, "a symmetry leaves the wing orbit\n"); return 1; }
        }
        wsym.push_back(y);
      }
    {  // classes of wing pairs (single swaps) under the 48 symmetries
      std::set<std::pair<int, int>> done;
      int ncls = 0;
      for (int a = 0; a < 24; a++) for (int b = a + 1; b < 24; b++) {
        if (done.count({a, b})) continue;
        int sz = 0;
        for (auto &y : wsym) { auto q = std::minmax(y[a], y[b]); if (done.insert({q.first, q.second}).second) sz++; }
        printf("swap class %d: (%d %d) size %d, cubies (%d,%d,%d) (%d,%d,%d)\n", ncls++, a, b, sz,
               cub[a][0], cub[a][1], cub[a][2], cub[b][0], cub[b][1], cub[b][2]);
      }
    }
    // T_d (any axis permutation, an even number of sign changes): two orbits of 12, one wing per edge
    bool seen[24] = {};
    printf("T_d wing orbits:");
    for (int p = 0; p < 24; p++) {
      if (seen[p]) continue;
      std::vector<int> o;
      for (int pi = 0; pi < 6; pi++)
        for (int sg = 0; sg < 8; sg++)
          if (__builtin_popcount(sg) % 2 == 0) { const int q = wsym[pi * 8 + sg][p]; if (!seen[q]) seen[q] = true, o.push_back(q); }
      std::sort(o.begin(), o.end());
      printf(" {");
      for (size_t i = 0; i < o.size(); i++) printf("%s%d", i ? "," : "", o[i]);
      printf("}");
    }
    printf("\n");
  }
  std::map<int, long> byCost;
  for (auto &a : algs) byCost[a.cost]++;
  printf("%zu algorithms; by cost:", algs.size());
  for (auto &kv : byCost) printf(" %d:%ld", kv.first, kv.second);
  printf("\n");
  // wing partners: the two wings of an edge (the half-turn symmetry about the edge axis swaps them;
  // printed for choosing orders by hand)
  fflush(stdout);
  std::mt19937_64 rng(seed);
  int bestTotal = 1 << 30;
  std::vector<int> bestOrder;
  for (int trial = 0; trial <= nRandom; trial++) {
    std::vector<int> ord = order;
    if (trial > 0) std::shuffle(ord.begin(), ord.end(), rng);
    printf("order:");
    for (int p : ord) printf(" %d", p);
    printf("\n");
    uint32_t placed = 0;
    int total = 0;
    size_t at = 0;
    for (size_t si = 0; si < chunks.size(); si++) {
      const int c = chunks[si];
      std::vector<int> pieces(ord.begin() + at, ord.begin() + std::min<size_t>(24, at + c));
      at += c;
      if (at >= 23) pieces.assign(ord.begin() + (at - c), ord.end());  // the last chunk takes every remaining wing
      Stage st = makeStage(placed, pieces);
      std::string info;
      int M = solveStage(st, placed, info, parStage < 0 || (int)si < parStage ? 0 : (int)si == parStage ? 1 : 2);
      uint32_t pmask = 0;
      for (int p : pieces) pmask |= 1u << p;
      int nsym = 0;
      for (auto &y : wsym) {
        uint32_t a = 0, b = 0;
        for (int p = 0; p < 24; p++) { if (placed >> p & 1) a |= 1u << y[p]; if (pmask >> p & 1) b |= 1u << y[p]; }
        nsym += a == placed && b == pmask;
      }
      printf("  place %zu wings (%d placed): worst %d, %d cube symmetries keep the stage   (%s)\n", pieces.size(), __builtin_popcount(placed), M, nsym, info.c_str());
      fflush(stdout);
      if (M < 0) { total = 1 << 29; break; }
      total += M;
      for (int p : pieces) placed |= 1u << p;
      if (at >= 23) break;
    }
    printf("  total %d\n", total);
    if (total < bestTotal) bestTotal = total, bestOrder = ord;
  }
  printf("bound: %d moves; order:", bestTotal);
  for (int p : bestOrder) printf(" %d", p);
  printf("\n");
}

// ---- certificate output ----
static std::string wingSet(uint32_t mask) {
  std::string s;
  for (int k = 0; k < 24; k++)
    for (int p = 0; p < 24; p++)
      if ((mask >> p & 1) && phi[p] == k) s += (char)('a' + k);
  return s.empty() ? "none" : s;
}
static void certStage(const Stage &st, uint32_t placed, const std::vector<uint8_t> &dist, const std::vector<int> &caAlg,
                      const std::vector<int> &caCost, const std::vector<uint8_t> &dT) {
  const double t0 = now();
  const std::string root = getenv("CERT");
  uint32_t tmask = 0;
  for (int p : st.pieces) tmask |= 1u << p;
  const std::string sname = wingSet(placed) + ".." + wingSet(tmask), dir = root + "/stages/" + sname;
  if (system(("mkdir -p '" + dir + "'").c_str())) { fprintf(stderr, "CERT: cannot make the stage directory\n"); exit(1); }
  // each action's moves (certcube form)
  const size_t na = caAlg.size();
  std::vector<std::vector<cc::Mv>> amv(na);
  for (size_t a = 0; a < na; a++) {
    const Alg &al = algs[caAlg[a]];
    const auto mv = gModel->image(pools[al.file].rep[al.ref / 96], al.ref % 96);
    std::string t;
    for (auto &x : mv) t += (t.empty() ? "" : " ") + move_str(x);
    cc::parseMoves(t, amv[a]);
    if ((int)amv[a].size() != caCost[a]) { fprintf(stderr, "CERT: an algorithm's length is not its cost\n"); exit(1); }
  }
  const int wd = st.k >= 10 ? 2 : 1;
  std::vector<FILE *> seg(st.k + 1);
  for (int w = 0; w <= st.k; w++) {
    char fn[32];
    snprintf(fn, sizeof fn, "/w%0*d.txt", wd, w);
    seg[w] = fopen((dir + fn).c_str(), "w");
    if (!seg[w]) { fprintf(stderr, "CERT: cannot write a segment\n"); exit(1); }
  }
  std::vector<long> segN(st.k + 1, 0);
  std::vector<int> segMax(st.k + 1, 0);
  std::mutex mu;
  std::atomic<uint64_t> next{0};
  std::atomic<long> over{0}, stuck{0};
  std::vector<std::array<long, 128>> lenH(nthreads);
  for (auto &h : lenH) h.fill(0);
  std::atomic<int> tidN{0};
  uint8_t goal[24];
  for (int i = 0; i < st.k; i++) goal[i] = st.cidx[st.pieces[i]];
  auto work = [&] {
    const int tid = tidN++;
    std::vector<std::string> buf(st.k + 1);
    std::vector<long> n(st.k + 1, 0);
    std::vector<int> mx(st.k + 1, 0);
    auto flush = [&] {
      std::lock_guard<std::mutex> g(mu);
      for (int w = 0; w <= st.k; w++) {
        fwrite(buf[w].data(), 1, buf[w].size(), seg[w]);
        buf[w].clear();
        segN[w] += n[w], n[w] = 0;
        segMax[w] = std::max(segMax[w], mx[w]);
      }
    };
    uint8_t x[24], y[24];
    std::vector<cc::Mv> seq;
    size_t bytes = 0;
    for (uint64_t lo; (lo = next.fetch_add(1 << 14)) < st.total;) {
      const uint64_t hi = std::min<uint64_t>(st.total, lo + (1 << 14));
      for (uint64_t r = lo; r < hi; r++) {
        unrank(st, r, x);
        int wr = 0;
        for (int i = 0; i < st.k; i++) wr += x[i] != goal[i];
        seq.clear();
        uint64_t cur = r;
        bool ok = true;
        while (dist[cur]) {
          size_t a = 0;
          uint64_t nxt = 0;
          for (; a < na; a++) {
            const uint8_t *D = &dT[a * 24];
            for (int i = 0; i < st.k; i++) y[i] = D[x[i]];
            nxt = rankOf(st, y);
            if (dist[nxt] + caCost[a] == dist[cur]) break;
          }
          if (a == na) { ok = false; break; }
          for (auto &m : amv[a]) {  // append, combining with the previous turn of the same layer
            if (!seq.empty() && seq.back().face == m.face && seq.back().depth == m.depth) {
              const int t = (seq.back().amt + m.amt) & 3;
              if (t) seq.back().amt = t; else seq.pop_back();
            } else seq.push_back(m);
          }
          memcpy(x, y, st.k);
          cur = nxt;
        }
        if (!ok) { stuck++; continue; }
        if ((int)seq.size() > dist[r]) over++;
        std::string &o = buf[wr];
        for (size_t i = 0; i < seq.size(); i++) { if (i) o += ' '; o += cc::mvText(seq[i]); }
        o += '\n';
        bytes += seq.size() * 3;
        n[wr]++;
        mx[wr] = std::max<int>(mx[wr], seq.size());
        lenH[tid][std::min<size_t>(127, seq.size())]++;
      }
      if (bytes > (8 << 20)) { flush(); bytes = 0; }
    }
    flush();
  };
  std::vector<std::thread> th;
  for (int t = 0; t < nthreads; t++) th.emplace_back(work);
  for (auto &t : th) t.join();
  FILE *man = fopen((root + "/MANIFEST").c_str(), "a");
  int longest = 0;
  for (int w = 0; w <= st.k; w++) {
    fclose(seg[w]);
    if (man) fprintf(man, "stages/%s/w%0*d.txt %ld lines, longest %d\n", sname.c_str(), wd, w, segN[w], segMax[w]);
    longest = std::max(longest, segMax[w]);
  }
  if (man) fclose(man);
  std::array<long, 128> h{};
  for (auto &a : lenH) for (int i = 0; i < 128; i++) h[i] += a[i];
  printf("    CERT %s: %llu states, %ld stuck, %ld over the table, longest %d; lengths:", sname.c_str(), (unsigned long long)st.total, stuck.load(), over.load(), longest);
  for (int i = 0; i < 128; i++) if (h[i]) printf(" %d:%ld", i, h[i]);
  printf(" (%.0fs)\n", now() - t0);
  fflush(stdout);
}
