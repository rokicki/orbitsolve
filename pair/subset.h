#pragma once
#include "pool.h"
#include <queue>
#include <unordered_set>
// cheapest found cost to make all of T correct; masks restricted to T
inline std::pair<int, int> solveSubset(const Pool &pool, const std::vector<std::array<uint64_t, 6>> &ST,
                                       const State &start, uint64_t T, int W) {
  auto corrT = [&](const State &s) {
    int c = 0;
    for (int p = 0; p < 48; p++)
      if ((T >> p & 1) && s.col[p] == target(p)) c++;
    return c;
  };
  int need = __builtin_popcountll(T);
  struct N { State s; int g, algs; };
  struct C { int mis, g, parent; uint32_t a; bool operator<(const C &o) const { return mis < o.mis; } };
  const int MAXG = 200;
  std::vector<N> nodes = {{start, 0, 0}};
  std::vector<std::priority_queue<C>> buckets(MAXG);
  std::unordered_set<uint64_t> seen;
  int best = MAXG, bestAlgs = 0;
  size_t cap = 4 * W;
  auto expand = [&](int ni) {
    const N nd = nodes[ni];
    for (size_t a = 0; a < pool.size(); a++) {
      int ng = nd.g + pool.cost[a];
      if (ng >= best) break;
      int c = correctAfter(ST[a], nd.s.M);
      int mis = need - c;
      if (mis == 0) { best = ng; bestAlgs = nd.algs + 1; continue; }
      auto &h = buckets[ng];
      if (h.size() >= cap && mis >= h.top().mis) continue;
      if (h.size() >= cap) h.pop();
      h.push({mis, ng, ni, (uint32_t)a});
    }
  };
  if (corrT(start) == need) return {0, 0};
  expand(0);
  for (int g = 1; g < best; g++) {
    auto &h = buckets[g];
    std::vector<C> cs;
    while (!h.empty()) { cs.push_back(h.top()); h.pop(); }
    std::sort(cs.begin(), cs.end(), [](const C &x, const C &y) { return x.mis < y.mis; });
    int taken = 0;
    for (auto &c : cs) {
      if (taken >= W) break;
      State ns = nodes[c.parent].s.apply(pool, c.a);
      if (!seen.insert(ns.hash()).second) continue;
      nodes.push_back({ns, c.g, nodes[c.parent].algs + 1});
      taken++;
      expand(nodes.size() - 1);
    }
  }
  return {best, bestAlgs};
}

