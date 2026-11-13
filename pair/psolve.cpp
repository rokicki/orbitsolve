// Beam search over products of pair algorithms.
//
//   psolve [-w W] [-l LAMBDA] [-n N] [-s SEED] [-t THREADS] [-m MAXLEN] [-x] pool...
//
// Ranks candidates by  g + LAMBDA * misplaced  and keeps the best W distinct
// states per level (level = number of algorithms applied).  g is the length of
// the merged move sequence: same-axis moves at the seam between consecutive
// algorithms combine or cancel (disable with -x to count raw lengths).
#include "pool.h"
#include "eg2.h"
#include <chrono>
#include <cmath>
#include <queue>
#include <thread>
#include <atomic>
#include <unordered_set>

struct Cand {
  float score;
  int g, corr;
  int parent;
  uint32_t alg;
  bool operator<(const Cand &o) const { return score < o.score; }
};

struct Node {
  State s;
  int g, corr, parent;
  uint32_t alg;
  std::vector<uint16_t> groups;
  uint64_t zob = 0;
};

struct Result {
  int moves = -1;
  int raw = 0;
  int algs = 0;
  std::vector<uint32_t> seq;
  int fin = -1;  // finishing algorithm from the finish pool, if any
  std::vector<uint32_t> egPath;  // endgame algorithms (EG2 cand pool)
  int egFin = -1;                // endgame finish (EG2 fin pool)
  std::string text;
};

// Every state that a single algorithm of the finish pool solves, keyed by a
// Zobrist hash of its colour arrangement.  Algorithm b solves t iff
// t[src_b[d]] == target(d) for all d.
struct Finish {
  Pool pool;
  uint64_t Z[48][6];
  std::vector<uint64_t> keys;
  std::vector<uint32_t> vals;
  uint64_t mask = 0;
  int maxMis = 0;
  uint64_t zob(const std::array<uint8_t, 48> &col) const {
    uint64_t h = 0;
    for (int d = 0; d < 48; d++) h ^= Z[d][col[d]];
    return h;
  }
  void build() {
    std::mt19937_64 r(12345);
    for (auto &z : Z) for (auto &x : z) x = r();
    size_t sz = 1;
    while (sz < 2 * pool.size()) sz <<= 1;
    keys.assign(sz, 0);
    vals.assign(sz, 0);
    mask = sz - 1;
    for (size_t b = 0; b < pool.size(); b++) {
      std::array<uint8_t, 48> t;
      int mis = 0;
      for (int d = 0; d < 48; d++) t[pool.src[b][d]] = target(d);
      for (int d = 0; d < 48; d++) mis += t[d] != target(d);
      maxMis = std::max(maxMis, mis);
      uint64_t k = zob(t);
      for (size_t i = k & mask;; i = (i + 1) & mask) {
        if (keys[i] == k) { if (pool.cost[b] < pool.cost[vals[i]]) vals[i] = b; break; }
        if (keys[i] == 0) { keys[i] = k; vals[i] = b; break; }
      }
    }
    size_t used = 0;
    for (auto k : keys) used += k != 0;
    fprintf(stderr, "finish table: %zu algorithms, %zu states, max misplaced %d\n", pool.size(), used, maxMis);
  }
  int find(uint64_t k) const {
    for (size_t i = k & mask;; i = (i + 1) & mask) {
      if (keys[i] == k) return vals[i];
      if (keys[i] == 0) return -1;
    }
  }
};

struct Params {
  int width = 16;
  double lambda = 1.0;
  int maxDepth = 40;
  double lbPerPiece = 1.0 / 3.0;  // admissible: 24 pieces in 8 moves is the best rate
  int slack = 4;
  const Finish *fin = nullptr;
  const EG2 *eg = nullptr;
  int egMaxMis = 16;   // beam nodes with at most this many wrong get an endgame try
  int egDeep = 0;      // how many of them (cheapest estimate first) get depth 2
  bool bucket = false;   // beam per exact move count instead of per algorithm count
  double mu = 0.0;       // imbalance penalty for the bucketed beam
  bool cancel = true;
};

// Savings from merging alg a onto a sequence whose last two groups are
// tail/prev.  Looks two groups deep, which is all the ranking needs; the
// exact cost is recomputed when a candidate is expanded.
struct SeamTable {
  int tailAxis = -1, prevAxis = -1;
  uint16_t tail = 0, prev = 0;
  int maxSave = 0;
  void init(const std::vector<uint16_t> &g) {
    tailAxis = prevAxis = -1;
    maxSave = 0;
    if (g.size() >= 1) { tail = g.back(); tailAxis = gaxis(tail); maxSave += 2 * gnz(gamt(tail)); }
    if (g.size() >= 2) { prev = g[g.size() - 2]; prevAxis = gaxis(prev); maxSave += 2 * gnz(gamt(prev)); }
  }
  inline int save(uint16_t head, uint16_t second) const {
    if (gaxis(head) != tailAxis) return 0;
    uint16_t s = gadd(gamt(tail), gamt(head));
    int sv = gnz(gamt(tail)) + gnz(gamt(head)) - gnz(s);
    if (s == 0 && second != 0xFFFF && gaxis(second) == prevAxis) {
      uint16_t s2 = gadd(gamt(prev), gamt(second));
      sv += gnz(gamt(prev)) + gnz(gamt(second)) - gnz(s2);
    }
    return sv;
  }
};

static int exactCost(const Pool &pool, const Node &nd, uint32_t a, std::vector<uint16_t> *out = nullptr) {
  std::vector<uint16_t> g = nd.groups;
  merge_into(g, std::vector<uint16_t>(pool.gbegin(a), pool.gend(a)));
  int c = groups_cost(g);
  if (out) *out = std::move(g);
  return c;
}

// Endgame re-optimisation over a whole beam tree: every node with few wrong
// positions is finished by enumeration + finish table (depth 1 for all, depth
// 2 for the egDeep most promising), keeping it if it beats `best`.
static bool endgame(const Pool &pool, const std::vector<Node> &nodes, const Params &P, int &best, Result &r) {
  if (!P.eg) return false;
  int egNode = -1;
  EG2::Sol egSol;
  std::vector<std::pair<double, int>> cand;
  for (size_t i = 0; i < nodes.size(); i++) {
    int m = 48 - nodes[i].corr;
    if (m <= P.egMaxMis && nodes[i].g + 8 < best) cand.push_back({nodes[i].g + 2.4 * m, (int)i});
  }
  std::sort(cand.begin(), cand.end());
  for (int pass = 0; pass < 2; pass++)
    for (size_t k = 0; k < cand.size(); k++) {
      if (pass == 1 && (int)k >= P.egDeep) break;
      int i = cand[k].second;
      if (nodes[i].g + 8 >= best) continue;
      auto sol = P.eg->solve(nodes[i].s, pass ? 2 : 1, best - nodes[i].g);
      if (nodes[i].g + sol.cost < best) {
        best = nodes[i].g + sol.cost;
        egNode = i;
        egSol = sol;
      }
    }
  if (egNode < 0) return false;
  for (int n = egNode; nodes[n].parent >= 0; n = nodes[n].parent) r.seq.push_back(nodes[n].alg);
  std::reverse(r.seq.begin(), r.seq.end());
  for (auto a : r.seq) r.raw += pool.cost[a];
  r.egPath = egSol.path;
  r.egFin = egSol.finAlg;
  for (auto a : r.egPath) r.raw += P.eg->cand.cost[a];
  if (r.egFin >= 0) r.raw += P.eg->fin.cost[r.egFin];
  r.algs = r.seq.size() + r.egPath.size() + (r.egFin >= 0);
  r.moves = r.raw;
  return true;
}

Result beam(const Pool &pool, const State &start, const Params &P) {
  std::vector<Node> nodes;
  nodes.push_back({start, 0, start.correct(), -1, 0, {}});
  std::vector<int> frontier = {0};
  int best = 1 << 30, bestNode = -1;
  uint32_t bestAlg = 0;
  size_t cap = (size_t)P.width * P.slack;
  std::unordered_set<uint64_t> seen = {start.hash()};
  for (int depth = 0; depth < P.maxDepth && !frontier.empty(); depth++) {
    std::priority_queue<Cand> heap;
    for (int ni : frontier) {
      const Node &nd = nodes[ni];
      SeamTable seam;
      if (P.cancel) seam.init(nd.groups);
      for (size_t a = 0; a < pool.size(); a++) {
        int ng = nd.g + pool.cost[a];
        if (ng - seam.maxSave >= best) break;  // pool sorted by cost
        if (heap.size() >= cap && ng - seam.maxSave >= heap.top().score) break;
        int c = correctAfter(pool.S[a], nd.s.M);
        if (P.cancel) ng -= seam.save(pool.head[a], pool.second[a]);
        if (c == 48) {
          if (P.cancel) ng = exactCost(pool, nd, a);
          if (ng < best) { best = ng; bestNode = ni; bestAlg = a; }
          continue;
        }
        int mis = 48 - c;
        if (ng + std::ceil(mis * P.lbPerPiece) >= best) continue;
        float sc = ng + P.lambda * mis;
        if (heap.size() < cap) heap.push({sc, ng, c, ni, (uint32_t)a});
        else if (sc < heap.top().score) {
          heap.pop();
          heap.push({sc, ng, c, ni, (uint32_t)a});
        }
      }
    }
    std::vector<Cand> cs;
    while (!heap.empty()) { cs.push_back(heap.top()); heap.pop(); }
    std::sort(cs.begin(), cs.end(), [](const Cand &x, const Cand &y) {
      if (x.score != y.score) return x.score < y.score;
      return x.g < y.g;
    });
    frontier.clear();
    for (auto &c : cs) {
      if (c.g + std::ceil((48 - c.corr) * P.lbPerPiece) >= best) continue;
      State ns = nodes[c.parent].s.apply(pool, c.alg);
      if (!seen.insert(ns.hash()).second) continue;
      Node nn{ns, c.g, c.corr, c.parent, c.alg, {}};
      if (P.cancel) nn.g = exactCost(pool, nodes[c.parent], c.alg, &nn.groups);
      nodes.push_back(std::move(nn));
      frontier.push_back(nodes.size() - 1);
      if ((int)frontier.size() >= P.width) break;
    }
  }
  Result r;
  if (endgame(pool, nodes, P, best, r)) return r;
  if (bestNode < 0) return r;
  r.moves = best;
  r.seq.push_back(bestAlg);
  for (int n = bestNode; nodes[n].parent >= 0; n = nodes[n].parent) r.seq.push_back(nodes[n].alg);
  std::reverse(r.seq.begin(), r.seq.end());
  r.algs = r.seq.size();
  std::vector<uint16_t> g;
  for (auto a : r.seq) {
    r.raw += pool.cost[a];
    merge_into(g, std::vector<uint16_t>(pool.gbegin(a), pool.gend(a)));
  }
  r.text = groups_str(g);
  if (P.cancel && groups_cost(g) != r.moves) { fprintf(stderr, "cost mismatch %d %d\n", groups_cost(g), r.moves); exit(1); }
  return r;
}

// Beam with one bucket per exact (merged) move count g.  States are processed
// in increasing g; each bucket keeps its best W by misplaced count (plus an
// optional imbalance penalty), so no cost/progress trade-off parameter is
// needed and the first solution reached is the cheapest this beam can find.
Result cbeam(const Pool &pool, const State &start, const Params &P) {
  const int MAXG = 160;
  std::vector<Node> nodes;
  nodes.push_back({start, 0, start.correct(), -1, 0, {}});
  std::vector<std::priority_queue<Cand>> buckets(MAXG);
  size_t cap = (size_t)P.width * P.slack;
  std::unordered_set<uint64_t> seen = {start.hash()};
  int best = 1 << 30, bestNode = -1;
  uint32_t bestAlg = 0;
  int bestFin = -1;  // -2: node itself finished by bestAlg... see below
  const uint64_t LO = 0xFFFFFF;
  const Finish *F = P.fin;
  auto finCost = [&](const Node &nd, int a, int b) {
    if (!P.cancel) return nd.g + (a >= 0 ? (int)pool.cost[a] : 0) + (int)F->pool.cost[b];
    std::vector<uint16_t> g = nd.groups;
    if (a >= 0) merge_into(g, std::vector<uint16_t>(pool.gbegin(a), pool.gend(a)));
    merge_into(g, std::vector<uint16_t>(F->pool.gbegin(b), F->pool.gend(b)));
    return groups_cost(g);
  };
  if (F) nodes[0].zob = F->zob(start.col);
  auto expand = [&](int ni) {
    const Node &nd = nodes[ni];
    SeamTable seam;
    if (P.cancel) seam.init(nd.groups);
    if (F && 48 - nd.corr <= F->maxMis) {
      int b = F->find(nd.zob);
      if (b >= 0) {
        int c = finCost(nd, -1, b);
        if (c < best) { best = c; bestNode = ni; bestAlg = UINT32_MAX; bestFin = b; }
      }
    }
    for (size_t a = 0; a < pool.size(); a++) {
      int ng = nd.g + pool.cost[a];
      if (ng - seam.maxSave >= best) break;
      int c = correctAfter(pool.S[a], nd.s.M);
      if (P.cancel) ng -= seam.save(pool.head[a], pool.second[a]);
      if (F && 48 - c <= F->maxMis && ng + 4 < best) {
        uint64_t h = nd.zob;
        const auto &sr = pool.src[a];
        for (int d = 0; d < 48; d++)
          if (sr[d] != d) h ^= F->Z[d][nd.s.col[d]] ^ F->Z[d][nd.s.col[sr[d]]];
        int b = F->find(h);
        if (b >= 0 && ng + F->pool.cost[b] - 6 < best) {
          int cc = finCost(nd, a, b);
          if (cc < best) { best = cc; bestNode = ni; bestAlg = a; bestFin = b; }
        }
      }
      if (c == 48) {
        if (P.cancel) ng = exactCost(pool, nd, a);
        if (ng < best) { best = ng; bestNode = ni; bestAlg = a; bestFin = -1; }
        continue;
      }
      int mis = 48 - c;
      if (ng + std::ceil(mis * P.lbPerPiece) >= best || ng >= MAXG) continue;
      auto &h = buckets[ng];
      if (h.size() >= cap && mis >= h.top().score) continue;
      float sc = mis;
      if (P.mu) {
        const auto &S = pool.S[a];
        const auto &M = nd.s.M;
        int ca = 0;
        for (int k = 0; k < 6; k++) ca += __builtin_popcountll(S[k] & M[k] & LO);
        int ma = 24 - ca, mb = mis - ma;
        sc += P.mu * std::abs(ma - mb);
        if (h.size() >= cap && sc >= h.top().score) continue;
      }
      if (h.size() >= cap) h.pop();
      h.push({sc, ng, c, ni, (uint32_t)a});
    }
  };
  expand(0);
  for (int g = 1; g < MAXG && g < best; g++) {
    auto &h = buckets[g];
    std::vector<Cand> cs;
    while (!h.empty()) { cs.push_back(h.top()); h.pop(); }
    std::sort(cs.begin(), cs.end(), [](const Cand &x, const Cand &y) { return x.score < y.score; });
    int taken = 0;
    for (auto &c : cs) {
      if (taken >= P.width) break;
      if (c.g + std::ceil((48 - c.corr) * P.lbPerPiece) >= best) continue;
      State ns = nodes[c.parent].s.apply(pool, c.alg);
      if (!seen.insert(ns.hash()).second) continue;
      Node nn{ns, c.g, c.corr, c.parent, c.alg, {}};
      if (F) nn.zob = F->zob(ns.col);
      if (P.cancel) {
        nn.g = exactCost(pool, nodes[c.parent], c.alg, &nn.groups);
        if (nn.g < g) nn.g = g;  // deeper cancellation than the seam estimate saw; keep order monotone
      }
      nodes.push_back(std::move(nn));
      taken++;
      expand(nodes.size() - 1);
    }
  }
  Result r;
  if (endgame(pool, nodes, P, best, r)) return r;
  if (bestNode < 0) return r;
  if (bestAlg != UINT32_MAX) r.seq.push_back(bestAlg);
  for (int n = bestNode; nodes[n].parent >= 0; n = nodes[n].parent) r.seq.push_back(nodes[n].alg);
  std::reverse(r.seq.begin(), r.seq.end());
  r.algs = r.seq.size();
  std::vector<uint16_t> gs;
  for (auto a : r.seq) {
    r.raw += pool.cost[a];
    merge_into(gs, std::vector<uint16_t>(pool.gbegin(a), pool.gend(a)));
  }
  if (bestFin >= 0) {
    r.fin = bestFin;
    r.algs++;
    r.raw += F->pool.cost[bestFin];
    merge_into(gs, std::vector<uint16_t>(F->pool.gbegin(bestFin), F->pool.gend(bestFin)));
  }
  r.text = groups_str(gs);
  r.moves = P.cancel ? groups_cost(gs) : r.raw;
  return r;
}

// Greedy cheap-first pre-phase: apply the best gain/cost algorithm of length
// <= maxc while its ratio is at least R.  Returns the algorithms applied.
static std::vector<uint32_t> prephase(const Pool &pool, State &s, int maxc, double R) {
  std::vector<uint32_t> seq;
  while (true) {
    int c0 = s.correct();
    double br = 0;
    size_t ba = 0;
    for (size_t a = 0; a < pool.size() && pool.cost[a] <= maxc; a++) {
      double q = (double)(correctAfter(pool.S[a], s.M) - c0) / pool.cost[a];
      if (q > br) { br = q; ba = a; }
    }
    if (br < R || c0 == 48) break;
    s = s.apply(pool, ba);
    seq.push_back(ba);
  }
  return seq;
}

int main(int argc, char **argv) {
  Params P;
  int preC = 0;
  double preR = 1;
  int N = 100, threads = 16, maxlen = 99;
  uint64_t seed = 1;
  bool verbose = false, trace = false;
  std::vector<const char *> files, finFiles;
  bool useEG = false;
  int egK = 20;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-w") P.width = atoi(argv[++i]);
    else if (a == "-l") P.lambda = atof(argv[++i]);
    else if (a == "-n") N = atoi(argv[++i]);
    else if (a == "-s") seed = atoll(argv[++i]);
    else if (a == "-t") threads = atoi(argv[++i]);
    else if (a == "-m") maxlen = atoi(argv[++i]);
    else if (a == "-k") P.slack = atoi(argv[++i]);
    else if (a == "-x") P.cancel = false;
    else if (a == "-b") P.bucket = true;
    else if (a == "-p") { preC = atoi(argv[++i]); preR = atof(argv[++i]); }
    else if (a == "-F") finFiles.push_back(argv[++i]);
    else if (a == "-E") useEG = true;
    else if (a == "-M") P.egMaxMis = atoi(argv[++i]);
    else if (a == "-D") P.egDeep = atoi(argv[++i]);
    else if (a == "-K") egK = atoi(argv[++i]);
    else if (a == "-u") P.mu = atof(argv[++i]);
    else if (a == "-v") verbose = true;
    else if (a == "-T") trace = true;
    else files.push_back(argv[i]);
  }
  Pool pool;
  for (auto f : files) pool.load(f, maxlen);
  pool.sortByCost();
  fprintf(stderr, "pool %zu algorithms\n", pool.size());
  static EG2 eg;
  if (useEG) {
    eg.cand.load("eg8.txt", 99, false);
    eg.fin.load("eg8.txt", 99, false);
    eg.fin.load("p9.txt", 99, false);
    eg.fin.load("c12x.txt", 99, false);
    eg.build();
    eg.topK = egK;
    P.eg = &eg;
  }
  static Finish fin;
  if (!finFiles.empty()) {
    for (auto f : finFiles) fin.pool.load(f, 99, false);
    fin.build();
    P.fin = &fin;
  }
  std::vector<State> inst;
  std::mt19937_64 rng(seed);
  for (int i = 0; i < N; i++) inst.push_back(randomState(rng));
  std::vector<Result> res(N);
  std::atomic<int> next{0};
  auto t0 = std::chrono::steady_clock::now();
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (int i; (i = next++) < N;) {
        State s = inst[i];
        std::vector<uint32_t> pre;
        if (preC) pre = prephase(pool, s, preC, preR);
        if (s.correct() == 48) { res[i] = Result(); res[i].moves = 0; }
        else res[i] = P.bucket ? cbeam(pool, s, P) : beam(pool, s, P);
        if (res[i].moves >= 0 && !pre.empty()) {
          for (auto a : pre) { res[i].moves += pool.cost[a]; res[i].raw += pool.cost[a]; }
          res[i].seq.insert(res[i].seq.begin(), pre.begin(), pre.end());
          res[i].algs += pre.size();
        }
      }
    });
  for (auto &t : th) t.join();
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  int solved = 0;
  double mv = 0, al = 0, mv2 = 0, raw = 0;
  for (int i = 0; i < N; i++) {
    if (res[i].moves < 0) continue;
    State s = inst[i];
    for (auto a : res[i].seq) s = s.apply(pool, a);
    if (res[i].fin >= 0) s = s.apply(fin.pool, res[i].fin);
    for (auto a : res[i].egPath) s = s.apply(eg.cand, a);
    if (res[i].egFin >= 0) s = s.apply(eg.fin, res[i].egFin);
    if (s.correct() != 48) { fprintf(stderr, "VERIFY FAILED %d\n", i); exit(1); }
    solved++;
    mv += res[i].moves;
    mv2 += res[i].moves * (double)res[i].moves;
    al += res[i].algs;
    raw += res[i].raw;
    if (trace) {
      State t = inst[i];
      auto mis = [&](int o) { int m = 0; for (int p = o * 24; p < o * 24 + 24; p++) m += t.col[p] != target(p); return m; };
      printf("T %d %d/%d", res[i].moves, mis(0), mis(1));
      for (auto a : res[i].seq) { t = t.apply(pool, a); printf("  %d:%d/%d", pool.cost[a], mis(0), mis(1)); }
      if (res[i].fin >= 0) { t = t.apply(fin.pool, res[i].fin); printf("  F%d:%d/%d", fin.pool.cost[res[i].fin], mis(0), mis(1)); }
      printf("\n");
    }
    if (verbose) { printf("V"); for (int p = 0; p < 48; p++) printf(" %d", inst[i].col[p]); printf(" | %s\n", res[i].text.c_str()); }
  }
  double mean = mv / solved;
  printf("W=%d l=%.2f pool=%zu%s solved %d/%d  moves %.2f (sd %.2f, /orbit %.2f, raw %.2f)  algs %.2f  %.1f ms/pair\n",
         P.width, P.lambda, pool.size(), P.cancel ? "" : " nocancel", solved, N, mean,
         std::sqrt(mv2 / solved - mean * mean), mean / 2, raw / solved, al / solved, 1000.0 * el * threads / N);
}
