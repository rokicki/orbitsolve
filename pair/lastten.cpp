// Hill-climb for the cheapest "last K": the set T of K positions that is
// cheapest to finish when everything outside T is already solved.
//
//   lastten [-k K] [-e SAMPLES] [-w W] [-i ITERS] [-s SEED] [-a init] pool...
//
// cost(T) = mean over SAMPLES scrambles of the full solve cost (beam, no
// cancellation), where a scramble permutes colours within T∩A and within T∩B so
// that every position of T is wrong.  Neighbours swap one position in T for one
// outside; the best of 16 (evaluated in parallel) replaces T if it is cheaper,
// and T's own estimate is refreshed with new samples every iteration.
#include "subset.h"
#include <atomic>
#include <thread>

static bool scramble(uint64_t T, std::mt19937_64 &rng, State &s) {
  for (int p = 0; p < 48; p++) s.col[p] = target(p);
  for (int o = 0; o < 2; o++) {
    std::vector<int> P;
    for (int p = o * 24; p < o * 24 + 24; p++) if (T >> p & 1) P.push_back(p);
    if (P.empty()) continue;
    std::vector<uint8_t> c;
    for (int p : P) c.push_back(target(p));
    bool ok = false;
    for (int tries = 0; tries < 2000 && !ok; tries++) {
      std::shuffle(c.begin(), c.end(), rng);
      ok = true;
      for (size_t k = 0; k < P.size(); k++) if (c[k] == target(P[k])) { ok = false; break; }
    }
    if (!ok) return false;
    for (size_t k = 0; k < P.size(); k++) s.col[P[k]] = c[k];
  }
  s.setMasks();
  return true;
}

int main(int argc, char **argv) {
  int K = 10, E = 8, W = 4, iters = 60, threads = 16;
  uint64_t seed = 1, init = 0;
  std::vector<const char *> files;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-k") K = atoi(argv[++i]);
    else if (a == "-e") E = atoi(argv[++i]);
    else if (a == "-w") W = atoi(argv[++i]);
    else if (a == "-i") iters = atoi(argv[++i]);
    else if (a == "-s") seed = atoll(argv[++i]);
    else if (a == "-a") init = strtoull(argv[++i], 0, 16);
    else files.push_back(argv[i]);
  }
  Pool pool;
  for (auto f : files) pool.load(f, 99, false);
  pool.sortByCost();
  std::mt19937_64 rng(seed);
  // cost of T with E samples; returns 1e9 if T admits no all-wrong scramble
  auto evalT = [&](uint64_t T, uint64_t s0) {
    std::mt19937_64 r(s0);
    double sum = 0;
    for (int e = 0; e < E; e++) {
      State st;
      if (!scramble(T, r, st)) return 1e9;
      sum += solveSubset(pool, pool.S, st, (1ULL << 48) - 1, W).first;
    }
    return sum / E;
  };
  auto randT = [&]() {
    std::vector<int> p(48);
    for (int i = 0; i < 48; i++) p[i] = i;
    std::shuffle(p.begin(), p.end(), rng);
    uint64_t T = 0;
    for (int k = 0; k < K; k++) T |= 1ULL << p[k];
    return T;
  };
  uint64_t cur = init;
  if (!cur) { State tmp; do { cur = randT(); std::mt19937_64 r(1); if (scramble(cur, r, tmp)) break; } while (true); }
  double curCost = evalT(cur, rng()), curN = 1;
  printf("init %012llx cost %.2f\n", (unsigned long long)cur, curCost);
  fflush(stdout);
  for (int it = 0; it < iters; it++) {
    std::vector<uint64_t> nb(threads);
    for (auto &n : nb) {
      std::vector<int> in, out;
      for (int p = 0; p < 48; p++) (cur >> p & 1 ? in : out).push_back(p);
      n = cur ^ (1ULL << in[rng() % in.size()]) ^ (1ULL << out[rng() % out.size()]);
    }
    std::vector<double> nc(threads + 1);
    uint64_t s0 = rng();
    std::vector<std::thread> th;
    for (int t = 0; t <= threads; t++)
      th.emplace_back([&, t] { nc[t] = evalT(t < threads ? nb[t] : cur, s0 + t); });
    for (auto &t : th) t.join();
    curCost = (curCost * curN + nc[threads]) / (curN + 1);
    curN++;
    int b = std::min_element(nc.begin(), nc.begin() + threads) - nc.begin();
    if (nc[b] < curCost) {
      // confirm on fresh samples (the minimum of 16 noisy estimates is biased low)
      std::vector<double> rc(threads);
      uint64_t s1 = rng();
      std::vector<std::thread> th2;
      for (int t = 0; t < threads; t++) th2.emplace_back([&, t] { rc[t] = evalT(nb[b], s1 + 1000 + t); });
      for (auto &t : th2) t.join();
      double conf = 0;
      for (auto x : rc) conf += x;
      conf /= threads;
      if (conf < curCost) {
        cur = nb[b];
        curCost = conf;
        curN = threads;
      }
    }
    int inA = __builtin_popcountll(cur & 0xFFFFFF);
    if (it == iters - 1) {
      std::vector<double> rc(threads);
      std::vector<std::thread> th2;
      uint64_t s1 = rng();
      for (int t = 0; t < threads; t++) th2.emplace_back([&, t] { rc[t] = evalT(cur, s1 + 5000 + t); });
      for (auto &t : th2) t.join();
      double f = 0;
      for (auto x : rc) f += x;
      printf("final %012llx cost %.2f over %d samples\n", (unsigned long long)cur, f / threads, threads * E);
    }
    printf("it %d  T %012llx  cost %.2f (n=%g)  inA %d  best-nb %.2f\n", it, (unsigned long long)cur, curCost, curN, inA, nc[b]);
    fflush(stdout);
  }
}
