// Two-algorithm combinations at the start: for each state (stdin, 48 digits),
// a from the start's top K1, b from its top K2 (by 12*gain - 4*length, keep-solved),
// exact score of "a then b".  How good is the best support-disjoint pair
// (score = sum, no rescoring) against the best pair overall?
//   pairtop POOL.cls K1 K2 < states
#include "solvers.h"
#include "classpool.h"
#include <string>
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 4) { fprintf(stderr, "usage: pairtop POOL.cls K1 K2 < states\n"); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  ClassPool cp;
  if (!cp.load(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  Pool p;
  poolFromClasses(p, cp, pairModel());
  const size_t n = p.size(), K1 = atoi(argv[2]), K2 = atoi(argv[3]);
  std::vector<uint64_t> X(n), S(n);
  for (size_t a = 0; a < n; a++)
    for (int d = 0; d < 48; d++) {
      int sp = p.src[a][d];
      if ((sp % 24) / 4 != (d % 24) / 4) X[a] |= 1ULL << sp;
      if (sp != d) S[a] |= 1ULL << d;
    }
  auto correctAfter = [&](const State &s, size_t a) {
    int c = 0;
    for (int col = 0; col < 6; col++) c += __builtin_popcountll(p.S[a][col] & s.M[col]);
    return c;
  };
  long ns = 0, bestIsDisj = 0, rankSum = 0;
  double lossSum = 0, disjFracTop100 = 0, singleSum = 0, bestSum = 0;
  for (std::string line; std::getline(std::cin, line);) {
    if (line.size() < 48 || line[0] == '#') continue;
    State s;
    for (int q = 0; q < 48; q++) s.col[q] = line[q] - '0';
    s.setMasks();
    const int c0 = s.correct();
    uint64_t corr = 0;
    for (int q = 0; q < 48; q++) corr |= (uint64_t)(s.col[q] == target(q)) << q;
    std::vector<std::pair<int, uint32_t>> sc;
    for (size_t a = 0; a < n; a++)
      if (!(X[a] & corr)) sc.push_back({12 * (correctAfter(s, a) - c0) - 4 * p.cost[a], (uint32_t)a});
    const size_t k2 = std::min(K2, sc.size()), k1 = std::min(K1, k2);
    std::partial_sort(sc.begin(), sc.begin() + k2, sc.end(), [](auto &x, auto &y) { return x.first > y.first; });
    int best = INT32_MIN, bestD = INT32_MIN;
    bool bestDisj = false;
    std::vector<std::pair<int, bool>> all;
    for (size_t i = 0; i < k1; i++) {
      const uint32_t a = sc[i].second;
      State t = s.apply(p, a);
      const int c1 = t.correct();
      uint64_t corr1 = 0;
      for (int q = 0; q < 48; q++) corr1 |= (uint64_t)(t.col[q] == target(q)) << q;
      for (size_t j = 0; j < k2; j++) {
        if (j == i) continue;
        const uint32_t b = sc[j].second;
        const bool disj = !(S[a] & S[b]);
        int v;
        if (disj) v = sc[i].first + sc[j].first;  // additive, no rescoring
        else {
          if (X[b] & corr1) continue;
          v = sc[i].first + 12 * (correctAfter(t, b) - c1) - 4 * p.cost[b];
        }
        all.push_back({v, disj});
        if (v > best || (v == best && disj)) { best = v; bestDisj = disj; }
        if (disj) bestD = std::max(bestD, v);
      }
    }
    std::sort(all.begin(), all.end(), [](auto &x, auto &y) { return x.first > y.first; });
    int nd = 0;
    for (size_t i = 0; i < std::min<size_t>(100, all.size()); i++) nd += all[i].second;
    long r = 1;
    for (auto &x : all) r += x.first > bestD;
    ns++;
    bestIsDisj += bestDisj;
    lossSum += (best - bestD) / 12.0;
    rankSum += r;
    disjFracTop100 += nd / 100.0;
    singleSum += sc[0].first / 12.0;
    bestSum += best / 12.0;
  }
  printf("%ld states, a in top %zu, b in top %zu: best single %.2f, best pair %.2f (pieces-equivalent, 12 per piece, 4 per move)\n",
         ns, K1, K2, singleSum / ns, bestSum / ns);
  printf("  best pair is support-disjoint: %.1f%%; best disjoint pair loses %.2f (rank of best disjoint pair: avg %.1f)\n",
         100.0 * bestIsDisj / ns, lossSum / ns, (double)rankSum / ns);
  printf("  disjoint share of the top 100 pairs: %.1f%%\n", 100.0 * disjFracTop100 / ns);
}
