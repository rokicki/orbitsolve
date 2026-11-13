// Disjoint-support frontier.  For a pair state, algorithms with pairwise
// disjoint supports commute and their gains add, so the question "for each
// number of pieces we can solve by such a set, applied at the start, what is
// the least total move count" is a weighted set packing.  Solved exactly by
// branch and bound:
//   - candidates: algorithms with gain >= 1 at the start; per support mask only
//     the (length, gain)-nondominated ones; sorted by length / gain;
//   - each node carries its compatible candidates (disjoint from the chosen
//     supports, after the last chosen one): one AND per entry;
//   - bound: every chosen algorithm credits its gain to distinct initially-wrong
//     positions it fixes, so fixing D more pieces costs at least the sum of the D
//     smallest per-position values  min over remaining a fixing p of len(a)/gain(a).
//
//   disjoint POOL.cls [--nodes N] < states      (48-digit states, e.g. pairdata gen)
#include "solvers.h"
#include "classpool.h"
#include <string>
#include <iostream>
#include <map>
#include <chrono>

struct Cand { uint64_t S, F; uint8_t len, gain; float unit; };

struct Search {
  const std::vector<Cand> *C;
  int best[49];                 // least moves for exactly g gained
  std::vector<std::vector<uint32_t>> lists;  // per depth
  long nodes = 0, nodeLimit = 0;
  bool aborted = false;
  uint64_t wrong0 = 0;

  // sorted per-position unit costs over the positions in 'avail' fixable by L
  int bound(const std::vector<uint32_t> &L, uint64_t avail, float *P) {
    float u[48];
    for (int p = 0; p < 48; p++) u[p] = 1e9f;
    for (uint32_t i : L) {
      const Cand &c = (*C)[i];
      for (uint64_t f = c.F & avail; f; f &= f - 1) {
        int p = __builtin_ctzll(f);
        if (c.unit < u[p]) u[p] = c.unit;
      }
    }
    int k = 0;
    float v[48];
    for (uint64_t f = avail; f; f &= f - 1) { float x = u[__builtin_ctzll(f)]; if (x < 1e8f) v[k++] = x; }
    std::sort(v, v + k);
    P[0] = 0;
    for (int i = 0; i < k; i++) P[i + 1] = P[i] + v[i];
    return k;
  }
  // can a node with (cost, gain) and bound prefix P[0..k] still improve some best[]?
  bool useful(int cost, int gain, const float *P, int k) const {
    for (int d = 1; d <= k && gain + d <= 48; d++)
      if (cost + (int)std::ceil(P[d] - 1e-4f) < best[gain + d]) return true;
    return false;
  }
  void dfs(int depth, uint64_t used, int cost, int gain) {
    if (nodeLimit && ++nodes > nodeLimit) { aborted = true; return; }
    if (!nodeLimit) nodes++;
    if (aborted) return;
    const std::vector<uint32_t> &L = lists[depth];  // lists is sized once: references stay valid
    float P[49];
    const int k = bound(L, wrong0 & ~used, P);
    if (!useful(cost, gain, P, k)) return;
    std::vector<uint32_t> &M = lists[depth + 1];
    for (size_t j = 0; j < L.size() && !aborted; j++) {
      const Cand &c = (*C)[L[j]];
      const int nc = cost + c.len, ng = gain + c.gain;
      if (nc < best[ng]) best[ng] = nc;
      // the child's bound is at least this node's (fewer positions, fewer algorithms)
      if (!useful(nc, ng, P, k)) continue;
      M.clear();
      for (size_t t = j + 1; t < L.size(); t++)
        if (!((*C)[L[t]].S & c.S)) M.push_back(L[t]);
      if (!M.empty()) dfs(depth + 1, used | c.S, nc, ng);
    }
  }
};

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: disjoint POOL.cls [--nodes N] [--prefix K] < states\n"); return 1; }
  long nodeLimit = 0;
  size_t prefix0 = 20;
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--nodes" && i + 1 < argc) nodeLimit = atol(argv[++i]);
    else if (a == "--prefix" && i + 1 < argc) prefix0 = atol(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  ClassPool cp;
  if (!cp.load(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  Pool p;
  poolFromClasses(p, cp, pairModel());
  const size_t n = p.size();
  std::vector<uint64_t> supp(n);
  for (size_t a = 0; a < n; a++)
    for (int d = 0; d < 48; d++) if (p.src[a][d] != d) supp[a] |= 1ULL << d;
  double sumBest[49] = {};
  long cntBest[49] = {};
  int ns = 0;
  for (std::string line; std::getline(std::cin, line);) {
    if (line.size() < 48 || line[0] == '#') continue;
    State s;
    for (int q = 0; q < 48; q++) s.col[q] = line[q] - '0';
    const int c0 = s.correct();
    uint64_t wrong = 0;
    for (int q = 0; q < 48; q++) if (s.col[q] != target(q)) wrong |= 1ULL << q;
    auto t0 = std::chrono::steady_clock::now();
    // candidates, nondominated per support mask
    std::map<uint64_t, std::vector<Cand>> byMask;
    for (size_t a = 0; a < n; a++) {
      int g = 0;
      uint64_t F = 0;
      for (int d = 0; d < 48; d++) {
        const int sp = p.src[a][d];
        if (sp == d) continue;
        const bool after = s.col[sp] == target(d), before = s.col[d] == target(d);
        g += after - before;
        if (after && (wrong >> d & 1)) F |= 1ULL << d;
      }
      if (g < 1) continue;
      Cand c{supp[a], F, p.cost[a], (uint8_t)g, (float)p.cost[a] / g};
      auto &v = byMask[c.S];
      bool dom = false;
      for (auto &o : v) if (o.len <= c.len && o.gain >= c.gain) { dom = true; break; }
      if (dom) continue;
      v.erase(std::remove_if(v.begin(), v.end(), [&](const Cand &o) { return c.len <= o.len && c.gain >= o.gain; }), v.end());
      v.push_back(c);
    }
    std::vector<Cand> C;
    for (auto &kv : byMask) for (auto &c : kv.second) C.push_back(c);
    std::sort(C.begin(), C.end(), [](const Cand &x, const Cand &y) { return x.unit < y.unit || (x.unit == y.unit && x.gain > y.gain); });
    printf("state %d: %d correct, %zu candidates; per prefix, solved:least-moves\n", ns, c0, C.size());
    Search S;
    S.C = &C;
    S.wrong0 = wrong;
    S.nodeLimit = nodeLimit;
    for (auto &b : S.best) b = 1 << 20;
    S.best[0] = 0;
    S.lists.assign(50, {});
    // exact on growing prefixes of the (moves per piece)-sorted candidates; each
    // prefix's sets are valid, so they bound the next, larger search
    std::string stages;
    for (size_t K = std::min(prefix0, C.size());; K = std::min(K * 2, C.size())) {
      S.lists[0].clear();
      for (uint32_t i = 0; i < K; i++) S.lists[0].push_back(i);
      S.nodes = 0;
      S.aborted = false;
      auto ts = std::chrono::steady_clock::now();
      S.dfs(0, 0, 0, 0);
      char buf[128];
      snprintf(buf, sizeof buf, " %zu:%ldn/%.1fs%s", K, S.nodes, std::chrono::duration<double>(std::chrono::steady_clock::now() - ts).count(),
               S.aborted ? "(abort)" : "");
      stages += buf;
      // this stage's frontier: least moves for at least g more solved, as solved:moves
      printf("  prefix %6zu %10ld nodes %7.1fs%s:", K, S.nodes, std::chrono::duration<double>(std::chrono::steady_clock::now() - ts).count(),
             S.aborted ? " (abort)" : "");
      {
        int m = 1 << 20;
        std::vector<int> atl(49);
        for (int g = 48; g >= 0; g--) atl[g] = m = std::min(m, S.best[g]);
        for (int g = 1; g <= 48 - c0; g++) if (atl[g] < (1 << 20)) printf(" %d:%d", c0 + g, atl[g]);
      }
      printf("\n");
      fflush(stdout);
      if (K == C.size() || S.aborted) break;
    }
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    // at least g: suffix minimum
    int atl[49];
    atl[48] = S.best[48];
    for (int g = 47; g >= 0; g--) atl[g] = std::min(S.best[g], atl[g + 1]);
    printf("state %d: %d correct, %zu candidates%s, %.1fs\n  final                                    ", ns, c0, C.size(),
           S.aborted ? " NOT EXACT" : "", sec);
    for (int g = 1; g <= 48 - c0; g++) if (atl[g] < (1 << 20)) printf(" %d:%d", c0 + g, atl[g]);
    printf("\n");
    fflush(stdout);
    if (!S.aborted)
      for (int g = 1; g <= 48 - c0; g++) if (atl[g] < (1 << 20)) { sumBest[c0 + g] += atl[g]; cntBest[c0 + g]++; }
    ns++;
  }
  printf("average least moves by solved pieces (exact runs only; count of states reaching it):\n");
  for (int c = 0; c <= 48; c++) if (cntBest[c]) printf("  %2d: %6.2f (%ld)\n", c, sumBest[c] / cntBest[c], cntBest[c]);
}
