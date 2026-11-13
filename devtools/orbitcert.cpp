// Certificate data for a staged orbit bound (see orbitbound.cpp): for given
// stages, finds a small set of algorithm classes that still reaches every
// stage state within the stage's worst case, and writes JSON for a write-up:
// per stage the classes (representative moves; all 48 symmetries x inverse of
// each are usable), the worst case, the distance histogram and a hardest
// state with its solution; plus complete example solves of random scrambles.
//
// Class selection: Dijkstra from the goal keeps, per state, the cheapest
// distance and among equal ones the parent class of best priority; the classes
// used by that tree reach every state at the same distance.  Priorities are
// then re-ranked by usage and the pass repeated, which concentrates the tree
// on few classes.
//
//   orbitcert [--budget E] [--stagebudget I:E] [--passes K] [--examples K] [--seed S] diag|mid OUT.json STAGES POOL.cls...
//     STAGES: comma-separated S/T group lists, e.g. -/D,D/U,UD/LFRB
#include "classpool.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static uint64_t binom[65][6];
static int nthreads;

struct Act { std::vector<uint8_t> d; int cost; int cls; uint32_t ref; int pool; };  // d: free index -> free index

struct Stage {
  int S, T, n;
  std::vector<int> pos, cidx, tg;
  std::vector<uint64_t> orb, radix;
  uint64_t total = 1;
};
static Stage makeStage(int S, int T) {
  Stage st;
  st.S = S, st.T = T;
  st.cidx.assign(24, -1);
  for (int p = 0; p < 24; p++)
    if (!(S >> (p / 4) & 1)) { st.cidx[p] = st.pos.size(); st.pos.push_back(p); }
  st.n = st.pos.size();
  for (int g = 0; g < 6; g++)
    if (T >> g & 1) { st.tg.push_back(g); st.orb.push_back((1ULL << st.n) - 1); }
  int used = 0;
  for (size_t c = 0; c < st.tg.size(); c++) {
    st.radix.push_back(binom[st.n - 4 * used++][4]);
    st.total *= st.radix.back();
  }
  return st;
}
static inline uint64_t rankOf(const Stage &st, const uint64_t *m) {
  uint64_t r = 0, taken = 0;
  for (size_t c = 0; c < st.tg.size(); c++) {
    uint64_t avail = st.orb[c] & ~taken, sub = 0;
    int k = 0;
    for (uint64_t x = m[c]; x; x &= x - 1) sub += binom[__builtin_popcountll(avail & ((x & -x) - 1))][++k];
    r = r * st.radix[c] + sub;
    taken |= m[c];
  }
  return r;
}
static inline void unrank(const Stage &st, uint64_t r, uint64_t *m) {
  const int nc = st.tg.size();
  uint64_t sub[8], taken = 0;
  for (int c = nc - 1; c >= 0; c--) { sub[c] = r % st.radix[c]; r /= st.radix[c]; }
  for (int c = 0; c < nc; c++) {
    uint64_t avail = st.orb[c] & ~taken;
    int ai[64], na = 0;
    for (uint64_t x = avail; x; x &= x - 1) ai[na++] = __builtin_ctzll(x);
    uint64_t s = sub[c], mm = 0;
    int hi = na;
    for (int k = 4; k >= 1; k--) {
      int v = k - 1;
      while (v + 1 < hi && binom[v + 1][k] <= s) v++;
      s -= binom[v][k];
      mm |= 1ULL << ai[v];
      hi = v;
    }
    m[c] = mm;
    taken |= mm;
  }
}
static inline uint64_t applyAct(const Stage &st, const uint8_t *D, const uint64_t *m, uint64_t *t) {
  for (size_t c = 0; c < st.tg.size(); c++) {
    uint64_t x = 0;
    for (uint64_t y = m[c]; y; y &= y - 1) x |= 1ULL << D[__builtin_ctzll(y)];
    t[c] = x;
  }
  return rankOf(st, t);
}

// Dijkstra; key = dist << 24 | priority of the parent class (lower wins)
static void dijkstra(const Stage &st, const std::vector<Act> &acts, const std::vector<uint32_t> &prio,
                     std::vector<uint32_t> &key, std::vector<int> &parentCls) {
  key.assign(st.total, 0xFFFFFFFFu);
  uint64_t goal[8];
  for (size_t c = 0; c < st.tg.size(); c++) {
    goal[c] = 0;
    for (int k = 0; k < 4; k++) goal[c] |= 1ULL << st.cidx[4 * st.tg[c] + k];
  }
  key[rankOf(st, goal)] = 0;
  std::vector<uint8_t> dT(acts.size() * 64);
  for (size_t a = 0; a < acts.size(); a++) for (int i = 0; i < st.n; i++) dT[a * 64 + i] = acts[a].d[i];
  for (int d = 0; d < 255; d++) {
    std::atomic<uint64_t> next{0}, found{0}, later{0};
    auto work = [&] {
      uint64_t m[8], t[8], f = 0, l = 0;
      const uint64_t chunk = 1 << 14;
      for (uint64_t lo; (lo = next.fetch_add(chunk)) < st.total;) {
        const uint64_t hi = std::min<uint64_t>(st.total, lo + chunk);
        for (uint64_t r = lo; r < hi; r++) {
          uint32_t kv = std::atomic_ref<uint32_t>(key[r]).load(std::memory_order_relaxed);
          if ((kv >> 24) != (uint32_t)d) { if (kv != 0xFFFFFFFFu && (kv >> 24) > (uint32_t)d) l++; continue; }
          f++;
          unrank(st, r, m);
          for (size_t a = 0; a < acts.size(); a++) {
            const uint32_t nd = d + acts[a].cost;
            const uint64_t tr = applyAct(st, &dT[a * 64], m, t);
            const uint32_t nk = nd << 24 | prio[acts[a].cls];
            std::atomic_ref<uint32_t> ar(key[tr]);
            uint32_t cur = ar.load(std::memory_order_relaxed);
            while (nk < cur && !ar.compare_exchange_weak(cur, nk, std::memory_order_relaxed)) {}
          }
        }
      }
      found += f, later += l;
    };
    std::vector<std::thread> th;
    for (int i = 0; i < nthreads; i++) th.emplace_back(work);
    for (auto &x : th) x.join();
    if (!found && !later) {
      bool any = false;
      for (uint64_t r = 0; r < st.total && !any; r++) any = key[r] != 0xFFFFFFFFu && (key[r] >> 24) > (uint32_t)d;
      if (!any) break;
    }
  }
  parentCls.clear();
}

static std::string gname(int mask) {
  std::string s;
  for (int g = 0; g < 6; g++) if (mask >> g & 1) s += "ULFRBD"[g];
  return s.empty() ? "-" : s;
}
static int parseMask(const std::string &s) {
  int m = 0;
  for (char c : s) if (c != '-') m |= 1 << std::string("ULFRBD").find(c);
  return m;
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  double budget = 4e11;
  std::map<int, double> stageBudget;  // stage index -> edge budget
  int passes = 3, nExamples = 8;
  uint64_t seed = std::random_device{}();
  int ai = 1;
  for (; ai < argc && argv[ai][0] == '-' && argv[ai][1] == '-'; ai++) {
    std::string o = argv[ai];
    if (o == "--budget" && ai + 1 < argc) budget = atof(argv[++ai]);
    else if (o == "--stagebudget" && ai + 1 < argc) { std::string v = argv[++ai]; stageBudget[atoi(v.c_str())] = atof(v.c_str() + v.find(':') + 1); }
    else if (o == "--passes" && ai + 1 < argc) passes = atoi(argv[++ai]);
    else if (o == "--examples" && ai + 1 < argc) nExamples = atoi(argv[++ai]);
    else if (o == "--seed" && ai + 1 < argc) seed = strtoull(argv[++ai], 0, 10);
    else { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
  }
  if (argc - ai < 4) { fprintf(stderr, "usage: orbitcert [--budget E] [--passes K] [--examples K] [--seed S] diag|mid OUT.json STAGES POOL.cls...\n"); return 1; }
  printf("seed %llu\n", (unsigned long long)seed);
  for (int i = 0; i <= 64; i++) for (int k = 0; k <= 5; k++) binom[i][k] = k == 0 ? 1 : i == 0 ? 0 : binom[i - 1][k - 1] + binom[i - 1][k];
  nthreads = std::max(1u, std::thread::hardware_concurrency());
  const std::string kind = argv[ai++];
  OrbitModel m = kind == "mid" ? midModel() : diagModel();
  const char *outFn = argv[ai++];
  std::vector<std::pair<int, int>> stages;
  {
    std::string s = argv[ai++];
    size_t p = 0;
    while (p <= s.size()) {
      size_t q = s.find(',', p);
      if (q == std::string::npos) q = s.size();
      std::string tok = s.substr(p, q - p);
      size_t sl = tok.find('/');
      stages.push_back({parseMask(tok.substr(0, sl)), parseMask(tok.substr(sl + 1))});
      p = q + 1;
    }
  }
  // pools; per permutation the cheapest (cost, pool, ref)
  std::vector<ClassPool> pools;
  std::vector<int> clsBase;
  struct H { size_t operator()(const std::array<uint8_t, 24> &v) const { size_t h = 1469598103934665603ULL; for (auto c : v) h = (h ^ c) * 1099511628211ULL; return h; } };
  struct E { uint8_t cost; uint8_t pool; uint32_t ref; };
  std::unordered_map<std::array<uint8_t, 24>, E, H> best;
  int nCls = 0;
  for (; ai < argc; ai++) {
    pools.emplace_back();
    if (!pools.back().load(argv[ai])) { fprintf(stderr, "cannot read %s\n", argv[ai]); return 1; }
    clsBase.push_back(nCls);
    nCls += pools.back().size();
    const int pi = pools.size() - 1;
    pools.back().expand(m, [&](const uint8_t *src, int c, uint32_t ref) {
      std::array<uint8_t, 24> d;
      for (int j = 0; j < 24; j++) d[src[j]] = j;
      auto it = best.find(d);
      if (it == best.end() || c < it->second.cost) best[d] = {(uint8_t)c, (uint8_t)pi, ref};
    }, false);
    printf("%s: %zu classes, %zu permutations so far\n", argv[ai], pools.back().size(), best.size());
    fflush(stdout);
  }
  auto clsOf = [&](int pool, uint32_t ref) { return clsBase[pool] + (int)(ref / 96); };
  auto moveText = [&](int pool, uint32_t ref) {
    std::string t = pools[pool].text(m, ref);
    if (kind == "diag") for (size_t i = 0; i + 1 < t.size(); i++) if (t[i] == '3' && (i == 0 || t[i - 1] == ' ')) t[i] = '2';
    return t;
  };
  auto clsText = [&](int c) {
    int p = 0;
    while (p + 1 < (int)clsBase.size() && clsBase[p + 1] <= c) p++;
    return moveText(p, (uint32_t)(c - clsBase[p]) * 96);
  };
  auto clsCost = [&](int c) {
    int p = 0;
    while (p + 1 < (int)clsBase.size() && clsBase[p + 1] <= c) p++;
    return (int)pools[p].cost[c - clsBase[p]];
  };

  FILE *out = fopen(outFn, "w");
  fprintf(out, "{\"kind\":\"%s\",\"stages\":[\n", kind.c_str());
  std::vector<Stage> sts;
  std::vector<std::vector<Act>> stActs;
  std::vector<std::vector<uint8_t>> stDist;
  int total = 0;
  for (size_t si = 0; si < stages.size(); si++) {
    double t0 = now();
    Stage st = makeStage(stages[si].first, stages[si].second);
    // usable actions, cheapest per action on the free positions
    std::map<std::vector<uint8_t>, Act> red;
    for (auto &kv : best) {
      bool ok = true;
      for (int p = 0; p < 24 && ok; p++) if (st.S >> (p / 4) & 1) ok = kv.first[p] / 4 == p / 4;
      if (!ok) continue;
      std::vector<uint8_t> d(st.n);
      bool id = true;
      for (int i = 0; i < st.n; i++) { d[i] = st.cidx[kv.first[st.pos[i]]]; id &= d[i] == i; }
      if (id) continue;
      Act a{d, kv.second.cost, clsOf(kv.second.pool, kv.second.ref), kv.second.ref, kv.second.pool};
      auto it = red.find(d);
      if (it == red.end()) red.emplace(d, a);
      else if (a.cost < it->second.cost || (a.cost == it->second.cost && a.cls < it->second.cls)) it->second = a;
    }
    std::vector<Act> acts;
    for (auto &kv : red) acts.push_back(kv.second);
    std::sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.cost < b.cost; });
    const double bud = stageBudget.count(si) ? stageBudget[si] : budget;
    while ((double)st.total * acts.size() > bud) {
      int c = acts.back().cost;
      while (acts.back().cost == c) acts.pop_back();
    }
    printf("stage %s/%s: %llu states, %zu actions (cost <= %d)\n", gname(st.S).c_str(), gname(st.T).c_str(),
           (unsigned long long)st.total, acts.size(), acts.back().cost);
    fflush(stdout);
    // priorities: initially cost then class index
    std::vector<uint32_t> prio(nCls, (1u << 24) - 1), key;
    {
      std::vector<int> cl;
      for (auto &a : acts) cl.push_back(a.cls);
      std::sort(cl.begin(), cl.end());
      cl.erase(std::unique(cl.begin(), cl.end()), cl.end());
      std::stable_sort(cl.begin(), cl.end(), [&](int a, int b) { return clsCost(a) < clsCost(b); });
      for (size_t i = 0; i < cl.size(); i++) prio[cl[i]] = i;
    }
    std::vector<int> pc;
    std::vector<int> used;
    int M = 0;
    for (int pass = 0; pass < passes; pass++) {
      dijkstra(st, acts, prio, key, pc);
      std::map<uint32_t, long> cnt;  // prio -> states whose tree parent is that class
      M = 0;
      long reached = 0;
      for (uint64_t r = 0; r < st.total; r++)
        if (key[r] != 0xFFFFFFFFu) { reached++; M = std::max<int>(M, key[r] >> 24); if (key[r] >> 24) cnt[key[r] & 0xFFFFFF]++; }
      if ((uint64_t)reached != st.total) { fprintf(stderr, "stage not solvable: %ld of %llu states\n", reached, (unsigned long long)st.total); return 1; }
      std::vector<int> byP(1 << 24, -1);
      for (int c = 0; c < nCls; c++) if (prio[c] < (1u << 24) - 1) byP[prio[c]] = c;
      std::vector<std::pair<long, int>> u;
      for (auto &kv : cnt) u.push_back({-kv.second, byP[kv.first]});
      std::sort(u.begin(), u.end());
      used.clear();
      for (auto &x : u) used.push_back(x.second);
      printf("  pass %d: worst %d, tree uses %zu classes (%.0fs)\n", pass, M, used.size(), now() - t0);
      fflush(stdout);
      // new priorities: by usage; unused classes after
      std::vector<uint32_t> np(nCls, (1u << 24) - 1);
      uint32_t k = 0;
      for (int c : used) np[c] = k++;
      for (int c = 0; c < nCls; c++) if (prio[c] < (1u << 24) - 1 && np[c] == (1u << 24) - 1) np[c] = k++;
      prio = np;
    }
    // final: only the used classes; distances must stay the same
    std::set<int> keep(used.begin(), used.end());
    std::vector<Act> sub;
    for (auto &a : acts) if (keep.count(a.cls)) sub.push_back(a);
    // every image of a kept class that is usable in this stage (also the ones a cheaper class covered)
    {
      std::set<std::vector<uint8_t>> have;
      for (auto &a : sub) have.insert(a.d);
      for (int c : used) {
        int p = 0;
        while (p + 1 < (int)clsBase.size() && clsBase[p + 1] <= c) p++;
        const int r = c - clsBase[p];
        auto base = m.permOf(pools[p].rep[r]);
        uint8_t cur[48];
        for (int img = 0; img < 96; img++) {
          m.imagePerm(base.data(), img, cur);
          std::array<uint8_t, 24> d;
          for (int j = 0; j < 24; j++) d[cur[j]] = j;
          bool ok = true;
          for (int q = 0; q < 24 && ok; q++) if (st.S >> (q / 4) & 1) ok = d[q] / 4 == q / 4;
          if (!ok) continue;
          std::vector<uint8_t> dd(st.n);
          bool id = true;
          for (int i = 0; i < st.n; i++) { dd[i] = st.cidx[d[st.pos[i]]]; id &= dd[i] == i; }
          if (id || have.count(dd)) continue;
          have.insert(dd);
          sub.push_back({dd, pools[p].cost[r], c, (uint32_t)(r * 96 + img), p});
        }
      }
    }
    std::sort(sub.begin(), sub.end(), [](const Act &a, const Act &b) { return a.cost < b.cost; });
    std::vector<uint32_t> prio2(nCls, 0);
    dijkstra(st, sub, prio2, key, pc);
    std::vector<uint8_t> dist(st.total);
    std::map<int, long> hist;
    int M2 = 0;
    for (uint64_t r = 0; r < st.total; r++) {
      if (key[r] == 0xFFFFFFFFu) { fprintf(stderr, "subset misses a state\n"); return 1; }
      dist[r] = key[r] >> 24;
      hist[dist[r]]++;
      M2 = std::max<int>(M2, dist[r]);
    }
    printf("  final: %zu classes, %zu actions, worst %d (%.0fs)\n", used.size(), sub.size(), M2, now() - t0);
    fflush(stdout);
    if (M2 != M) { fprintf(stderr, "worst changed (%d -> %d)\n", M, M2); return 1; }
    total += M2;
    // a hardest state and its solution (greedy descent)
    uint64_t worst = 0;
    for (uint64_t r = 0; r < st.total; r++) if (dist[r] == M2) { worst = r; break; }
    fprintf(out, "%s{\"solved\":\"%s\",\"solve\":\"%s\",\"states\":%llu,\"worst\":%d,\"hist\":{", si ? ",\n" : "",
            gname(st.S).c_str(), gname(st.T).c_str(), (unsigned long long)st.total, M2);
    bool first = true;
    for (auto &kv : hist) { fprintf(out, "%s\"%d\":%ld", first ? "" : ",", kv.first, kv.second); first = false; }
    fprintf(out, "},\"classes\":[");
    // classes sorted by cost then text
    std::vector<int> uc(used.begin(), used.end());
    std::sort(uc.begin(), uc.end(), [&](int a, int b) { return std::make_pair(clsCost(a), clsText(a)) < std::make_pair(clsCost(b), clsText(b)); });
    for (size_t i = 0; i < uc.size(); i++) fprintf(out, "%s[%d,\"%s\"]", i ? "," : "", clsCost(uc[i]), clsText(uc[i]).c_str());
    fprintf(out, "]");
    // worst state: colours of the free positions (T colours; others '.')
    {
      uint64_t mm[8];
      unrank(st, worst, mm);
      std::string col(24, '.');
      for (int p = 0; p < 24; p++) if (st.S >> (p / 4) & 1) col[p] = "ULFRBD"[p / 4];
      for (size_t c = 0; c < st.tg.size(); c++)
        for (uint64_t x = mm[c]; x; x &= x - 1) col[st.pos[__builtin_ctzll(x)]] = "ULFRBD"[st.tg[c]];
      fprintf(out, ",\"worstState\":\"%s\",\"worstSolution\":[", col.c_str());
      uint64_t r = worst, t[8];
      bool f1 = true;
      while (dist[r]) {
        unrank(st, r, mm);
        bool stepped = false;
        for (auto &a : sub) {
          if (a.cost > dist[r]) break;
          uint64_t tr = applyAct(st, a.d.data(), mm, t);
          if (dist[tr] + a.cost == dist[r]) {
            fprintf(out, "%s\"%s\"", f1 ? "" : ",", moveText(a.pool, a.ref).c_str());
            f1 = false;
            r = tr;
            stepped = true;
            break;
          }
        }
        if (!stepped) { fprintf(stderr, "descent failed\n"); return 1; }
      }
      fprintf(out, "]}");
    }
    fflush(out);
    sts.push_back(st);
    stActs.push_back(sub);
    stDist.push_back(std::move(dist));
  }
  fprintf(out, "\n],\"bound\":%d,\"examples\":[\n", total);
  printf("bound %d\n", total);
  // examples: scramble = 40 random 3-cycle images (first pool), then solve stage by stage
  std::mt19937_64 rng(seed);
  for (int e = 0; e < nExamples; e++) {
    std::vector<uint8_t> col(24);  // colour (group) at each position
    for (int p = 0; p < 24; p++) col[p] = p / 4;
    std::string scr;
    for (int k = 0; k < 40; k++) {
      uint32_t ref = rng() % (pools[0].size() * 96);
      auto base = m.permOf(pools[0].rep[ref / 96]);
      uint8_t cur[48];
      m.imagePerm(base.data(), ref % 96, cur);
      std::vector<uint8_t> nc(24);
      for (int j = 0; j < 24; j++) nc[j] = col[cur[j]];
      col = nc;
      scr += (scr.empty() ? "" : " ") + moveText(0, ref);
    }
    fprintf(out, "%s{\"scramble\":\"%s\",\"stages\":[", e ? ",\n" : "", scr.c_str());
    for (size_t si = 0; si < sts.size(); si++) {
      const Stage &st = sts[si];
      // state from the colours
      for (int p = 0; p < 24; p++) if ((st.S >> (p / 4) & 1) && col[p] != p / 4) { fprintf(stderr, "stage precondition fails\n"); return 1; }
      uint64_t mm[8], t[8];
      auto stateRank = [&] {
        for (size_t c = 0; c < st.tg.size(); c++) {
          mm[c] = 0;
          for (int i = 0; i < st.n; i++) if (col[st.pos[i]] == st.tg[c]) mm[c] |= 1ULL << i;
        }
        return rankOf(st, mm);
      };
      uint64_t r = stateRank();
      fprintf(out, "%s[", si ? "," : "");
      bool f1 = true;
      while (stDist[si][r]) {
        bool stepped = false;
        for (auto &a : stActs[si]) {
          if (a.cost > stDist[si][r]) break;
          uint64_t tr = applyAct(st, a.d.data(), mm, t);
          if (stDist[si][tr] + a.cost == stDist[si][r]) {
            fprintf(out, "%s\"%s\"", f1 ? "" : ",", moveText(a.pool, a.ref).c_str());
            f1 = false;
            // apply to the colours: piece at free position i goes to a.d[i]
            std::vector<uint8_t> nc = col;
            for (int i = 0; i < st.n; i++) nc[st.pos[a.d[i]]] = col[st.pos[i]];
            col = nc;
            r = stateRank();
            stepped = true;
            break;
          }
        }
        if (!stepped) { fprintf(stderr, "example descent failed\n"); return 1; }
      }
      fprintf(out, "]");
    }
    fprintf(out, "]}");
    for (int p = 0; p < 24; p++) if (col[p] != p / 4) { fprintf(stderr, "example not solved\n"); return 1; }
  }
  fprintf(out, "\n]}\n");
  fclose(out);
}
