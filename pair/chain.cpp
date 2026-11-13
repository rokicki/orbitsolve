// Colour-at-a-time reduction chain for the pair (brobdicube-style stages).
//
// Stage: bring the 4+4 pieces of colour k onto face k in both orbits, using
// only algorithms that map every already-solved face onto itself (so solved
// faces stay solved whatever else happens).  A stage state is just the 48-bit
// mask of positions holding colour k.  Each stage is solved (near-)optimally
// with up to 3 algorithms: a one-algorithm preimage table, a full scan for 2,
// and a top-K scan for 3.
//
//   chain [-n N] [-K K] [-o ORDER|adaptive] pool...
#include "pool.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <map>
#include <mutex>

static uint64_t faceMask(int f) { return (15ULL << (4 * f)) | (15ULL << (24 + 4 * f)); }

struct Stage {
  std::vector<uint32_t> algs;              // pool indices allowed in this stage
  std::unordered_map<uint64_t, int> one;   // state solved by one alg -> min cost
  std::unordered_map<uint64_t, uint32_t> oneAlg;
};

struct Ctx {
  const Pool *pool;
  std::vector<std::array<uint8_t, 48>> dst;  // dst[a][s] = where the piece at s goes
};

static uint64_t img(const Ctx &C, uint32_t a, uint64_t x) {
  uint64_t r = 0;
  while (x) {
    int s = __builtin_ctzll(x);
    x &= x - 1;
    r |= 1ULL << C.dst[a][s];
  }
  return r;
}

static Stage buildStage(const Ctx &C, uint64_t solved, int k) {
  const Pool &P = *C.pool;
  Stage st;
  uint64_t T = faceMask(k);
  for (uint32_t a = 0; a < P.size(); a++) {
    bool ok = true;
    for (int d = 0; d < 48 && ok; d++)
      if (solved >> d & 1) {
        int s = P.src[a][d];
        if (!(solved >> s & 1) || target(s) != target(d)) ok = false;
      }
    if (!ok) continue;
    st.algs.push_back(a);
    // the state b solves: preimage of T
    uint64_t pre = 0;
    for (int d = 0; d < 48; d++)
      if (T >> d & 1) pre |= 1ULL << P.src[a][d];
    auto it = st.one.find(pre);
    if (it == st.one.end() || P.cost[a] < it->second) { st.one[pre] = P.cost[a]; st.oneAlg[pre] = a; }
  }
  return st;
}

// best solution for mask x within a stage: returns cost and alg list
static int solveStage(const Ctx &C, const Stage &st, uint64_t x, uint64_t T, int K, std::vector<uint32_t> &out) {
  const Pool &P = *C.pool;
  out.clear();
  if (x == T) return 0;
  int best = 1 << 20;
  auto it = st.one.find(x);
  if (it != st.one.end()) { best = it->second; out = {st.oneAlg.at(x)}; }
  struct Kid { int key; uint32_t a; uint64_t y; };
  std::vector<Kid> kids;
  for (auto a : st.algs) {
    int c = P.cost[a];
    if (c + 4 >= best) break;  // stage algs are cost-sorted (pool is)
    uint64_t y = img(C, a, x);
    auto jt = st.one.find(y);
    if (jt != st.one.end() && c + jt->second < best) { best = c + jt->second; out = {a, st.oneAlg.at(y)}; }
    int wrong = __builtin_popcountll(y & ~T);
    kids.push_back({c + 3 * wrong, a, y});
  }
  if (K > 0) {
    std::partial_sort(kids.begin(), kids.begin() + std::min<size_t>(K, kids.size()), kids.end(),
                      [](const Kid &p, const Kid &q) { return p.key < q.key; });
    for (int i = 0; i < K && i < (int)kids.size(); i++) {
      int c0 = P.cost[kids[i].a];
      for (auto a : st.algs) {
        int c = c0 + P.cost[a];
        if (c + 4 >= best) break;
        uint64_t y = img(C, a, kids[i].y);
        auto jt = st.one.find(y);
        if (jt != st.one.end() && c + jt->second < best) {
          best = c + jt->second;
          out = {kids[i].a, a, st.oneAlg.at(y)};
        }
      }
    }
  }
  return best;
}

int main(int argc, char **argv) {
  int N = 32, K = 0, threads = 16;
  std::string order = "adaptive";
  std::vector<const char *> files;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-n") N = atoi(argv[++i]);
    else if (a == "-K") K = atoi(argv[++i]);
    else if (a == "-o") order = argv[++i];
    else if (a == "-t") threads = atoi(argv[++i]);
    else files.push_back(argv[i]);
  }
  Pool pool;
  for (auto f : files) pool.load(f, 99, false);
  pool.sortByCost();
  Ctx C;
  C.pool = &pool;
  C.dst.resize(pool.size());
  for (size_t a = 0; a < pool.size(); a++)
    for (int d = 0; d < 48; d++) C.dst[a][pool.src[a][d]] = d;
  // stages are built on demand and cached by (solved faces, colour)
  std::map<std::pair<int, int>, Stage> cache;
  std::mutex mu;
  auto getStage = [&](int solvedFaces, int k) -> const Stage & {
    std::lock_guard<std::mutex> g(mu);
    auto key = std::make_pair(solvedFaces, k);
    auto it = cache.find(key);
    if (it == cache.end()) {
      uint64_t solved = 0;
      for (int f = 0; f < 6; f++) if (solvedFaces >> f & 1) solved |= faceMask(f);
      it = cache.emplace(key, buildStage(C, solved, k)).first;
    }
    return it->second;
  };
  std::mt19937_64 rng(1);
  std::vector<State> inst;
  for (int i = 0; i < N; i++) inst.push_back(randomState(rng));
  std::vector<int> total(N);
  std::vector<std::string> log(N);
  std::atomic<int> nx{0};
  auto t0 = std::chrono::steady_clock::now();
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (int i; (i = nx++) < N;) {
        State s = inst[i];
        int solvedFaces = 0, sum = 0;
        std::string lg;
        for (int step = 0; step < 5; step++) {
          int bestK = -1, bestC = 1 << 20;
          std::vector<uint32_t> bestSeq, seq;
          for (int k = 0; k < 6; k++) {
            if (solvedFaces >> k & 1) continue;
            if (order != "adaptive" && k != face_of_char(order[step])) continue;
            uint64_t x = 0;
            for (int p = 0; p < 48; p++) if (s.col[p] == k) x |= 1ULL << p;
            int c = solveStage(C, getStage(solvedFaces, k), x, faceMask(k), K, seq);
            // adaptive: cheapest per piece still to place
            double score = (double)c / std::max(1, __builtin_popcountll(x & ~faceMask(k)));
            double bestScore = bestK < 0 ? 1e9 : (double)bestC / 1.0;
            (void)bestScore;
            if (bestK < 0 || c < bestC) { bestK = k; bestC = c; bestSeq = seq; }
            (void)score;
          }
          if (bestC >= (1 << 20)) { lg += " STUCK"; sum = 9999; break; }
          for (auto a : bestSeq) s = s.apply(pool, a);
          solvedFaces |= 1 << bestK;
          sum += bestC;
          lg += std::string(" ") + FACECH[bestK] + ":" + std::to_string(bestC);
        }
        if (sum < 9999 && s.correct() != 48) { fprintf(stderr, "chain failed\n"); exit(1); }
        total[i] = sum;
        log[i] = lg;
      }
    });
  for (auto &t : th) t.join();
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  double m = 0;
  for (int i = 0; i < N; i++) m += total[i];
  for (int i = 0; i < std::min(N, 8); i++) printf("%d%s\n", total[i], log[i].c_str());
  printf("order=%s K=%d  moves %.2f  %.1f ms/pair (incl. stage builds)\n", order.c_str(), K, m / N,
         1000 * el * threads / N);
}
