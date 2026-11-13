// Complete 3-cycle tables for the guaranteed fallback finisher.
//
//   pair3.pool : every 3-cycle on pair orbit A and on orbit B (from ../pair/s8.txt)
//   diag3.pool : every 3-cycle on a diagonal orbit: diag.pool's 3-cycles, closed
//                under face-turn conjugation X t X' (still isolating), validated
//                by simulation on an 8x8x8
//   mid3.pool  : every 3-cycle on a mid-centre orbit (from mid.pool)
//
// Same 48-slot pool format as the other pools; the cheapest algorithm per cycle.
#include "layout.h"
#include "solvers.h"
#include <fstream>
#include <map>
#include <sstream>

static int moved(const std::array<uint8_t, 48> &s, int lo, int hi) {
  int n = 0;
  for (int j = lo; j < hi; j++) n += s[j] != j;
  return n;
}
static std::string text(const std::vector<mv> &a) {
  std::string s;
  for (auto m : a) {
    if (!s.empty()) s += ' ';
    if (m.dep > 1) s += std::to_string(m.dep);
    s += "ULFRBD"[m.face];
    if (m.twist == 2) s += '2';
    if (m.twist == 3) s += '\'';
  }
  return s;
}

struct Table {
  std::map<std::array<uint8_t, 48>, std::pair<int, std::string>> best;
  void add(const std::array<uint8_t, 48> &s, int cost, const std::string &t) {
    auto it = best.find(s);
    if (it == best.end() || cost < it->second.first) best[s] = {cost, t};
  }
  void write(const char *fn, int need) {
    FILE *f = fopen(fn, "w");
    for (auto &kv : best) {
      fprintf(f, "%d", kv.second.first);
      for (int j = 0; j < 48; j++) fprintf(f, " %d", kv.first[j]);
      fprintf(f, "  %s\n", kv.second.second.c_str());
    }
    fclose(f);
    printf("%s: %zu 3-cycles (complete = %d)%s\n", fn, best.size(), need, (int)best.size() == need ? "" : "  INCOMPLETE");
  }
};

int main() {
  // pair: 3-cycles on A only or B only, from the full small-support data
  {
    Pool p;
    p.load("../dist/gullicube/src/pair/s8.txt", 99, false);
    Table t;
    for (size_t a = 0; a < p.size(); a++) {
      int ma = moved(p.src[a], 0, 24), mb = moved(p.src[a], 24, 48);
      if ((ma == 3 && mb == 0) || (ma == 0 && mb == 3))
        t.add(p.src[a], p.cost[a], groups_str(std::vector<uint16_t>(p.gbegin(a), p.gend(a))));
    }
    t.write("pair3.pool", 2 * 4048);
  }
  // mid: straight from mid.pool
  {
    Pool p;
    p.load("mid.pool", 99, false);
    Table t;
    for (size_t a = 0; a < p.size(); a++)
      if (moved(p.src[a], 0, 24) == 3)
        t.add(p.src[a], p.cost[a], groups_str(std::vector<uint16_t>(p.gbegin(a), p.gend(a))));
    t.write("mid3.pool", 4048);
  }
  // diagonal: diag.pool 3-cycles closed under face-turn conjugation, each
  // validated on an 8x8x8 (only orbit (1,1) may change; slices -> depth 2)
  {
    Pool p;
    p.load("diag.pool", 99, false);
    Table t;
    std::vector<std::vector<mv>> frontier;
    xcube base(3, 3, 'U');
    auto check = [&](const std::vector<mv> &alg, std::array<uint8_t, 48> &perm) {
      xcube c = base;
      for (auto m : alg) domove(m, c, true);
      for (int k = 0; k < 24; k++)
        if (c.tc.corner.a[k] != base.tc.corner.a[k]) return false;
      for (auto &w : c.wingorbits)
        for (int k = 0; k < 24; k++) if (w.a[k] != k) return false;
      for (int y = 1; y <= 3; y++)
        for (int x = 1; x <= 3; x++) {
          uint8_t o[24];
          readOrbit(c, x, y, o);
          for (int k = 0; k < 24; k++)
            if (!(x == 1 && y == 1) && o[k] != k) return false;
          if (x == 1 && y == 1)
            for (int k = 0; k < 24; k++) perm[k] = o[k];
        }
      for (int k = 24; k < 48; k++) perm[k] = k;
      return moved(perm, 0, 24) == 3;
    };
    for (size_t a = 0; a < p.size(); a++) {
      if (moved(p.src[a], 0, 24) != 3) continue;
      std::vector<mv> alg;
      for (auto &m : poolMoves(p, a)) alg.push_back({m.depth > 1 ? 2 : 1, (short)m.face, (short)m.amt});
      std::array<uint8_t, 48> perm;
      if (!check(alg, perm)) { fprintf(stderr, "diag pool 3-cycle failed validation\n"); return 1; }
      t.add(perm, alg.size(), text(alg));
      frontier.push_back(alg);
    }
    for (int round = 0; round < 2 && (int)t.best.size() < 4048; round++) {
      std::vector<std::vector<mv>> next;
      for (auto &alg : frontier)
        for (short f = 0; f < 6; f++)
          for (short tw = 1; tw <= 3; tw++) {
            std::vector<mv> c = {{1, f, tw}};
            c.insert(c.end(), alg.begin(), alg.end());
            c.push_back({1, f, (short)(4 - tw)});
            std::array<uint8_t, 48> perm;
            if (!check(c, perm)) continue;
            size_t before = t.best.size();
            t.add(perm, c.size(), text(c));
            if (t.best.size() > before) next.push_back(c);
          }
      frontier = next;
      printf("diag conjugation round %d: %zu 3-cycles\n", round + 1, t.best.size());
    }
    t.write("diag3.pool", 4048);
  }
}
