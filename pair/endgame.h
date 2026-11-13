// Exact endgame: IDA* over algorithms whose support lies inside the current
// set of wrong positions (so solved positions are never touched).  Algorithms
// are found through an index keyed by exact support mask, so a node only
// enumerates subsets of its wrong set.  Optimal within that restricted pool.
#pragma once
#include "pool.h"
#include <unordered_map>

struct Endgame {
  Pool pool;  // small-support algorithms
  std::unordered_map<uint64_t, std::vector<uint32_t>> bySupport;
  int maxSup = 0;
  double bestRate = 1;  // max pieces moved per move, for the lower bound

  void build() {
    pool.sortByCost();
    for (size_t a = 0; a < pool.size(); a++) {
      int s = __builtin_popcountll(pool.support[a]);
      maxSup = std::max(maxSup, s);
      bestRate = std::max(bestRate, (double)s / pool.cost[a]);
      bySupport[pool.support[a]].push_back(a);
    }
    fprintf(stderr, "endgame pool %zu algs, %zu supports, max support %d, best rate %.2f\n", pool.size(),
            bySupport.size(), maxSup, bestRate);
  }
  int lb(int m) const { return (int)std::ceil(m / bestRate - 1e-9); }

  struct Ctx {
    std::vector<uint32_t> path, bestPath;
    long nodes = 0;
    long nodeLimit = 0;
  };

  // enumerate subsets of W with 3..maxSup bits, collecting candidate algs
  void subsets(uint64_t W, uint64_t cur, int from, int size, std::vector<uint32_t> &out,
               const std::array<uint8_t, 48> &col, int budget) const {
    if (size >= 3) {
      auto it = bySupport.find(cur);
      if (it != bySupport.end())
        for (auto a : it->second) {
          if (pool.cost[a] > budget) break;
          out.push_back(a);
        }
    }
    if (size == maxSup) return;
    for (int p = from; p < 48; p++)
      if (W >> p & 1) subsets(W, cur | 1ULL << p, p + 1, size + 1, out, col, budget);
  }

  bool extra = true;
  void subsetsWith(uint64_t W, uint64_t cur, int from, int size, std::vector<uint32_t> &out, int budget) const {
    if (size >= 3) {
      auto it = bySupport.find(cur);
      if (it != bySupport.end())
        for (auto a : it->second) {
          if (pool.cost[a] > budget) break;
          out.push_back(a);
        }
    }
    if (size == maxSup) return;
    for (int p = from; p < 48; p++)
      if (W >> p & 1) subsetsWith(W, cur | 1ULL << p, p + 1, size + 1, out, budget);
  }

  // depth-first with cost bound; returns true if a solution within bound found
  bool dfs(const State &s, int g, int bound, Ctx &cx) const {
    uint64_t W = 0;
    for (int p = 0; p < 48; p++)
      if (s.col[p] != target(p)) W |= 1ULL << p;
    int m = __builtin_popcountll(W);
    if (m == 0) { cx.bestPath = cx.path; return true; }
    if (g + lb(m) > bound) return false;
    if (cx.nodeLimit && ++cx.nodes > cx.nodeLimit) return false;
    std::vector<uint32_t> cand;
    subsets(W, 0, 0, 0, cand, s.col, bound - g);
    if (extra)  // also subsets using exactly one currently-correct position
      for (int e = 0; e < 48; e++)
        if (!(W >> e & 1)) subsetsWith(W, 1ULL << e, 0, 1, cand, bound - g);
    // keep those making net progress, best gain/cost first
    std::vector<std::pair<double, uint32_t>> ord;
    int c0 = 48 - m;
    for (auto a : cand) {
      int gain = correctAfter(pool.S[a], s.M) - c0;
      if (gain <= 0) continue;
      ord.push_back({-(double)gain / pool.cost[a], a});
    }
    std::sort(ord.begin(), ord.end());
    for (auto &o : ord) {
      uint32_t a = o.second;
      cx.path.push_back(a);
      bool ok = dfs(s.apply(pool, a), g + pool.cost[a], bound, cx);
      cx.path.pop_back();
      if (ok) return true;
    }
    return false;
  }

  // IDA*: smallest cost solution (or -1 if none within maxCost / node limit)
  int solve(const State &s, int maxCost, std::vector<uint32_t> *path = nullptr, long nodeLimit = 0) const {
    int m = 48 - s.correct();
    for (int bound = lb(m); bound <= maxCost; bound++) {
      Ctx cx;
      cx.nodeLimit = nodeLimit;
      if (dfs(s, 0, bound, cx)) {
        if (path) *path = cx.bestPath;
        return bound;  // costs are integers and bound increases by 1
      }
      if (nodeLimit && cx.nodes > nodeLimit) return -1;
    }
    return -1;
  }
};
