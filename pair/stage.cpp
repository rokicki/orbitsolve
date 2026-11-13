// Ordered solving: positions are partitioned into groups solved in order.
//
//   stage -o ORDER [-g GAMMA] [-S] [-w W] [-n N] [-s SEED] [-t THREADS] pool...
//
// ORDER: groups separated by ',', each a '+'-joined list of items; an item is
// an orbit/face cell like AU or BD (4 positions) or a position number 0..47.
// Unlisted positions form a final group.
//
// Ranking (within a bucket of equal move count): weighted misplaced count,
// where a wrong position in group r weighs GAMMA^(G-1-r).  GAMMA=1 ignores the
// order; large GAMMA makes it lexicographic ("weak" ordering).
// -S enforces it ("strict"): every step must keep all earlier groups correct and
// fix at least one more position of the current group.  Candidates then come
// from an index of algorithms by (destination, source), so only algorithms that
// bring a right-coloured piece into a wrong slot of the current group are seen.
// No move cancellation: cost is the sum of algorithm lengths.
#include "pool.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <queue>
#include <thread>
#include <unordered_set>

struct Order {
  int ng = 0;
  int grp[48];
  std::vector<uint64_t> gmask;  // positions of each group
  double w[48];
};

static Order parseOrder(const std::string &spec, double gamma) {
  Order o;
  for (auto &g : o.grp) g = -1;
  std::string s = spec;
  size_t i = 0;
  while (i <= s.size()) {
    size_t j = s.find(',', i);
    if (j == std::string::npos) j = s.size();
    std::string grp = s.substr(i, j - i);
    uint64_t m = 0;
    size_t a = 0;
    while (a < grp.size()) {
      size_t b = grp.find('+', a);
      if (b == std::string::npos) b = grp.size();
      std::string it = grp.substr(a, b - a);
      if (!it.empty() && isdigit((unsigned char)it[0])) m |= 1ULL << atoi(it.c_str());
      else if (it.size() == 2) {
        int orb = it[0] == 'B';
        int f = face_of_char(it[1]);
        m |= 15ULL << (orb * 24 + f * 4);
      }
      a = b + 1;
    }
    if (m) {
      for (int p = 0; p < 48; p++)
        if ((m >> p & 1) && o.grp[p] < 0) o.grp[p] = o.ng;
      o.ng++;
    }
    i = j + 1;
  }
  uint64_t rest = 0;
  for (int p = 0; p < 48; p++)
    if (o.grp[p] < 0) { o.grp[p] = o.ng; rest |= 1ULL << p; }
  if (rest) o.ng++;
  o.gmask.assign(o.ng, 0);
  for (int p = 0; p < 48; p++) o.gmask[o.grp[p]] |= 1ULL << p;
  for (int p = 0; p < 48; p++) o.w[p] = std::pow(gamma, o.ng - 1 - o.grp[p]);
  return o;
}

// support lists and (dest, src) index
struct Aux {
  std::vector<uint32_t> soff;
  std::vector<uint8_t> sdat;
  std::vector<uint32_t> ioff;  // index[x*48+y] = algs with src[x]==y (x != y)
  std::vector<uint32_t> idat;
  void build(const Pool &pool) {
    soff.push_back(0);
    std::vector<uint32_t> cnt(48 * 48, 0);
    for (size_t a = 0; a < pool.size(); a++) {
      for (int d = 0; d < 48; d++)
        if (pool.src[a][d] != d) { sdat.push_back(d); cnt[d * 48 + pool.src[a][d]]++; }
      soff.push_back(sdat.size());
    }
    ioff.assign(48 * 48 + 1, 0);
    for (int k = 0; k < 48 * 48; k++) ioff[k + 1] = ioff[k] + cnt[k];
    idat.resize(ioff.back());
    std::vector<uint32_t> fill(ioff.begin(), ioff.end() - 1);
    for (size_t a = 0; a < pool.size(); a++)  // pool is cost-sorted, so each list is too
      for (uint32_t k = soff[a]; k < soff[a + 1]; k++) {
        int d = sdat[k];
        idat[fill[d * 48 + pool.src[a][d]]++] = a;
      }
  }
};

struct Node {
  State s;
  int g, parent;
  uint32_t alg;
};
struct Cand {
  double score;
  int g, parent;
  uint32_t alg;
  bool operator<(const Cand &o) const { return score < o.score; }
};
struct Params {
  int width = 8, slack = 4;
  bool strict = false;
};
struct Result {
  int moves = -1, algs = 0;
  std::vector<uint32_t> seq;
};

// per-group source masks: SG[a*ng+k][c] = { src_a[d] : d in group k, target(d)=c }
static std::vector<std::array<uint64_t, 6>> SG;

static Result solve(const Pool &pool, const Aux &aux, const Order &O, const State &start, const Params &P,
                    std::vector<uint32_t> &stamp, uint32_t &stampv) {
  const int MAXG = 300;
  std::vector<Node> nodes = {{start, 0, -1, 0}};
  std::vector<std::priority_queue<Cand>> buckets(MAXG);
  std::unordered_set<uint64_t> seen = {start.hash()};
  size_t cap = (size_t)P.width * P.slack;
  int best = MAXG, bestNode = -1;
  uint32_t bestAlg = 0;
  std::vector<uint32_t> cands;
  auto expand = [&](int ni) {
    const Node nd = nodes[ni];
    const auto &col = nd.s.col;
    uint64_t wrong = 0;
    double score0 = 0;
    for (int p = 0; p < 48; p++)
      if (col[p] != target(p)) { wrong |= 1ULL << p; score0 += O.w[p]; }
    int stg = 0;
    while (stg < O.ng && !(wrong & O.gmask[stg])) stg++;
    uint64_t prot = 0;
    for (int k = 0; k < stg; k++) prot |= O.gmask[k];
    uint64_t G = stg < O.ng ? O.gmask[stg] : 0;
    auto consider = [&](uint32_t a) {
      int ng = nd.g + pool.cost[a];
      if (ng >= best) return false;  // lists are cost-sorted: caller may stop
      double sc = score0;
      bool ok = true;
      int gainG = 0;
      for (uint32_t k = aux.soff[a]; k < aux.soff[a + 1]; k++) {
        int d = aux.sdat[k];
        bool wasW = wrong >> d & 1;
        bool isW = col[pool.src[a][d]] != target(d);
        if (wasW != isW) {
          sc += isW ? O.w[d] : -O.w[d];
          if (P.strict) {
            if (isW && (prot >> d & 1)) { ok = false; break; }
            if (G >> d & 1) gainG += isW ? -1 : 1;
          }
        }
      }
      if (!ok || (P.strict && gainG <= 0)) return true;
      if (sc < 1e-9) {
        if (ng < best) { best = ng; bestNode = ni; bestAlg = a; }
        return true;
      }
      auto &h = buckets[ng];
      if (h.size() >= cap && sc >= h.top().score) return true;
      if (h.size() >= cap) h.pop();
      h.push({sc, ng, ni, a});
      return true;
    };
    if (P.strict) {
      if (++stampv == 0) { std::fill(stamp.begin(), stamp.end(), 0); stampv = 1; }
      for (int x = 0; x < 48; x++) {
        if (!((G & wrong) >> x & 1)) continue;
        for (int y = (x / 24) * 24; y < (x / 24) * 24 + 24; y++) {
          if (y == x || col[y] != target(x)) continue;
          for (uint32_t k = aux.ioff[x * 48 + y]; k < aux.ioff[x * 48 + y + 1]; k++) {
            uint32_t a = aux.idat[k];
            if (nd.g + pool.cost[a] >= best) break;
            if (stamp[a] == stampv) continue;
            stamp[a] = stampv;
            consider(a);
          }
        }
      }
    } else {
      const auto &M = nd.s.M;
      int gsz[64];
      for (int k = 0; k < O.ng; k++) gsz[k] = __builtin_popcountll(O.gmask[k]);
      for (size_t a = 0; a < pool.size(); a++) {
        int ng = nd.g + pool.cost[a];
        if (ng >= best) break;
        double sc = 0;
        for (int k = 0; k < O.ng; k++) sc += O.w[__builtin_ctzll(O.gmask[k])] * (gsz[k] - correctAfter(SG[a * O.ng + k], M));
        if (sc < 1e-9) {
          if (ng < best) { best = ng; bestNode = ni; bestAlg = a; }
          continue;
        }
        auto &h = buckets[ng];
        if (h.size() >= cap && sc >= h.top().score) continue;
        if (h.size() >= cap) h.pop();
        h.push({sc, ng, ni, (uint32_t)a});
      }
    }
  };
  expand(0);
  for (int g = 1; g < best && g < MAXG; g++) {
    auto &h = buckets[g];
    std::vector<Cand> cs;
    while (!h.empty()) { cs.push_back(h.top()); h.pop(); }
    std::sort(cs.begin(), cs.end(), [](const Cand &x, const Cand &y) { return x.score < y.score; });
    int taken = 0;
    for (auto &c : cs) {
      if (taken >= P.width) break;
      State ns = nodes[c.parent].s.apply(pool, c.alg);
      if (!seen.insert(ns.hash()).second) continue;
      nodes.push_back({ns, c.g, c.parent, c.alg});
      taken++;
      expand(nodes.size() - 1);
    }
  }
  Result r;
  if (bestNode < 0) return r;
  r.moves = best;
  r.seq.push_back(bestAlg);
  for (int n = bestNode; nodes[n].parent >= 0; n = nodes[n].parent) r.seq.push_back(nodes[n].alg);
  std::reverse(r.seq.begin(), r.seq.end());
  r.algs = r.seq.size();
  return r;
}

int main(int argc, char **argv) {
  Params P;
  int N = 64, threads = 16;
  uint64_t seed = 1;
  double gamma = 1;
  std::string order;
  bool trace = false;
  std::vector<const char *> files;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-o") order = argv[++i];
    else if (a == "-g") gamma = atof(argv[++i]);
    else if (a == "-S") P.strict = true;
    else if (a == "-w") P.width = atoi(argv[++i]);
    else if (a == "-n") N = atoi(argv[++i]);
    else if (a == "-s") seed = atoll(argv[++i]);
    else if (a == "-t") threads = atoi(argv[++i]);
    else if (a == "-k") P.slack = atoi(argv[++i]);
    else if (a == "-T") trace = true;
    else files.push_back(argv[i]);
  }
  Pool pool;
  for (auto f : files) pool.load(f, 99, false);
  pool.sortByCost();
  Aux aux;
  aux.build(pool);
  Order O = parseOrder(order, gamma);
  if (!P.strict) {
    SG.assign(pool.size() * O.ng, {});
    for (size_t a = 0; a < pool.size(); a++)
      for (int d = 0; d < 48; d++) SG[a * O.ng + O.grp[d]][target(d)] |= 1ULL << pool.src[a][d];
  }
  fprintf(stderr, "pool %zu, index %zu entries, %d groups\n", pool.size(), aux.idat.size(), O.ng);
  std::mt19937_64 rng(seed);
  std::vector<State> inst;
  for (int i = 0; i < N; i++) inst.push_back(randomState(rng));
  std::vector<Result> res(N);
  std::atomic<int> next{0};
  auto t0 = std::chrono::steady_clock::now();
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      std::vector<uint32_t> stamp(pool.size(), 0);
      uint32_t sv = 0;
      for (int i; (i = next++) < N;) res[i] = solve(pool, aux, O, inst[i], P, stamp, sv);
    });
  for (auto &t : th) t.join();
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  int solved = 0;
  double mv = 0, mv2 = 0, al = 0;
  for (int i = 0; i < N; i++) {
    if (res[i].moves < 0) continue;
    State s = inst[i];
    for (auto a : res[i].seq) s = s.apply(pool, a);
    if (s.correct() != 48) { fprintf(stderr, "VERIFY FAILED\n"); exit(1); }
    solved++;
    mv += res[i].moves;
    mv2 += res[i].moves * (double)res[i].moves;
    al += res[i].algs;
    if (trace) {
      State t = inst[i];
      printf("T %d", res[i].moves);
      for (auto a : res[i].seq) {
        t = t.apply(pool, a);
        printf("  %d:%d", pool.cost[a], 48 - t.correct());
      }
      printf("\n");
    }
  }
  double m = mv / solved;
  printf("%-40s g=%-5g %s W=%d  solved %d/%d  moves %.2f (sd %.2f)  algs %.2f  %.1f ms/pair\n", order.c_str(), gamma,
         P.strict ? "strict" : "weak  ", P.width, solved, N, m, std::sqrt(mv2 / solved - m * m), al / solved,
         1000.0 * el * threads / N);
}
