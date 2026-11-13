// Compare exact endgame (IDA*, support inside wrong set) with the beam on
// random "only K positions wrong" states.
#include "eg2.h"
#include "subset.h"
#include <atomic>
#include <chrono>
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
    for (int t = 0; t < 2000 && !ok; t++) {
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
  int K = atoi(argv[1]), N = atoi(argv[2]), depth = atoi(argv[3]);
  EG2 eg;
  eg.cand.load("eg8.txt", 99, false);
  eg.fin.load("eg8.txt", 99, false);
  eg.fin.load("p9.txt", 99, false);
  eg.fin.load("c12x.txt", 99, false);
  eg.build();
  if (argc > 4) eg.topK = atoi(argv[4]);
  Pool bp;
  bp.load("p8.txt", 99, false);
  bp.load("c12x.txt", 99, false);
  bp.sortByCost();
  std::mt19937_64 rng(3);
  std::vector<State> st;
  while ((int)st.size() < N) {
    std::vector<int> p(48);
    for (int i = 0; i < 48; i++) p[i] = i;
    std::shuffle(p.begin(), p.end(), rng);
    uint64_t T = 0;
    for (int k = 0; k < K; k++) T |= 1ULL << p[k];
    State s;
    if (scramble(T, rng, s)) st.push_back(s);
  }
  std::vector<int> ce(N), cb(N);
  std::vector<double> te(N);
  std::atomic<int> nx{0};
  std::vector<std::thread> th;
  for (int t = 0; t < 16; t++)
    th.emplace_back([&] {
      for (int i; (i = nx++) < N;) {
        cb[i] = solveSubset(bp, bp.S, st[i], (1ULL << 48) - 1, 4).first;
        auto t0 = std::chrono::steady_clock::now();
        auto sol = eg.solve(st[i], depth, cb[i] + 1);
        ce[i] = sol.cost <= cb[i] ? sol.cost : cb[i];
        te[i] = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      }
    });
  for (auto &t : th) t.join();
  double sb = 0, se = 0, tt = 0;
  int ne = 0;
  for (int i = 0; i < N; i++) {
    if (ce[i] >= 0) { se += ce[i]; sb += cb[i]; ne++; }
    tt += te[i];
    printf("%d %d %.3f\n", cb[i], ce[i], te[i]);
  }
  printf("K=%d on found: beam %.2f  exact %.2f (%d/%d found)  exact time %.3f s avg\n", K, sb / ne, se / ne, ne, N, tt / N);
}
