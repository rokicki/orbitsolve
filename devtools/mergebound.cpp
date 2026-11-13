// Merge headroom.  Reads a merge input written by gullicube with MERGEDUMP=prefix
// and reports, for the same instances:
//   - raw moves; the current greedy merge and merge beams (widths 16/64/256)
//     with their saved moves;
//   - S[i][j] = moves saved when instance j directly follows instance i, exact
//     (full cascade: whole groups cancelling expose the groups before them),
//     written as an n x n uint8 matrix for tools/assignbound.py (the
//     assignment bound: each instance one successor and one predecessor,
//     ignoring order constraints and cycles);
//   - along the best order found: savings per seam, how many of the previous
//     groups each seam consumed (depth), instances that vanish completely
//     (their moves cancel into what precedes them, so three or more instances
//     interact and pairwise savings no longer add up), and the order's actual
//     saving against the sum of its pairwise savings.
//   mergebound DUMP.txt [S.bin]
#include "merge.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <chrono>

struct Grp { int8_t axis; std::vector<std::pair<int16_t, int8_t>> layers; };
// as Merger::push; also counts groups removed from the stack
static void push(std::vector<Grp> &t, long &c, const PLayer &p, int &popped) {
  if (!t.empty() && t.back().axis == p.axis) {
    auto &L = t.back().layers;
    for (size_t k = 0; k < L.size(); k++)
      if (L[k].first == p.pos) {
        int na = (L[k].second + p.amt) & 3;
        if (na) L[k].second = na;
        else {
          L.erase(L.begin() + k);
          c--;
          if (L.empty()) { t.pop_back(); popped++; }
        }
        return;
      }
    L.push_back({p.pos, p.amt});
    c++;
  } else {
    t.push_back({p.axis, {{p.pos, p.amt}}});
    c++;
  }
}
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) { fprintf(stderr, "usage: mergebound DUMP.txt [S.bin]\n"); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::ifstream in(argv[1]);
  if (!in) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  std::string line, what;
  int N = 0;
  std::getline(in, line);
  {
    std::istringstream h(line);
    std::string a, b, c;
    h >> a >> b >> what >> c >> N;
  }
  Merger mg(N);
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ls(line);
    MergeInst I;
    size_t nd, nm;
    ls >> nd;
    for (size_t k = 0; k < nd; k++) {
      int comp;
      std::string hex;
      ls >> comp >> hex;
      I.deps.push_back({comp, std::stoull(hex, nullptr, 16)});
    }
    ls >> nm;
    for (size_t k = 0; k < nm; k++) {
      int a, p, t;
      ls >> a >> p >> t;
      I.moves.push_back({(int8_t)a, (int16_t)p, (int8_t)t});
    }
    mg.insts.push_back(std::move(I));
  }
  const size_t n = mg.insts.size();
  long raw = 0;
  for (auto &I : mg.insts) raw += I.moves.size();
  printf("%s merge, N %d: %zu instances, %ld raw moves\n", what.c_str(), N, n, raw);

  // current mergers
  struct Run { std::string name; long merged; std::vector<int> order; double sec; };
  std::vector<Run> runs;
  {
    Merger g = mg;
    long r, m;
    std::vector<int> ord;
    double t0 = now();
    g.merge(r, m, &ord);
    runs.push_back({"greedy", m, ord, now() - t0});
  }
  {
    Merger g = mg;
    long r, m;
    std::vector<int> ord;
    double t0 = now();
    g.mergePairwise(r, m, &ord);
    runs.push_back({"pairwise", m, ord, now() - t0});
    printf("  (pairwise: %ld joins, %ld seams refused by the constraints (%ld at the search cap), %ld piece visits)\n",
           g.pairStatJoins, g.pairStatRejected, g.pairStatCapped, g.pairStatVisits);
  }
  for (int w : {16, 64, 256}) {
    if (9e-8 * (double)n * n * w > 300) continue;
    Merger b = mg;
    long r, m;
    std::vector<int> ord;
    double t0 = now();
    b.mergeBeam(w, r, m, &ord);
    runs.push_back({"beam " + std::to_string(w), m, ord, now() - t0});
  }
  // every order must respect the order constraints (as merge.h builds them)
  {
    std::vector<std::vector<std::pair<int, uint64_t>>> comp;
    std::vector<std::pair<int, int>> cons;
    for (size_t i = 0; i < n; i++)
      for (auto &d : mg.insts[i].deps) {
        if ((int)comp.size() <= d.first) comp.resize(d.first + 1);
        for (auto &prev : comp[d.first])
          if (prev.second & d.second) cons.push_back({prev.first, (int)i});
        comp[d.first].push_back({(int)i, d.second});
      }
    for (auto &R : runs) {
      std::vector<int> at(n, -1);
      for (size_t k = 0; k < R.order.size(); k++) at[R.order[k]] = k;
      long bad = R.order.size() == n ? 0 : 1;
      for (auto &c : cons) bad += at[c.first] < 0 || at[c.second] < 0 || at[c.first] > at[c.second];
      if (bad) printf("  %s: ORDER VIOLATES %ld constraints\n", R.name.c_str(), bad);
    }
    printf("  %zu order constraints checked\n", cons.size());
  }
  for (auto &R : runs)
    printf("  %-9s %6ld moves, saves %5ld (%.1f%% of raw)  %.2fs\n", R.name.c_str(), R.merged, raw - R.merged,
           100.0 * (raw - R.merged) / raw, R.sec);

  // exact pairwise savings and the depth they reach
  std::vector<uint8_t> S(n * n, 0);
  std::map<int, long> depthHist;  // groups of i consumed by j, over pairs with a saving
  std::map<int, long> saveHist;
  long positive = 0;
  for (size_t i = 0; i < n; i++) {
    std::vector<Grp> base;
    long c0 = 0;
    int pp = 0;
    for (auto &p : mg.insts[i].moves) push(base, c0, p, pp);
    for (size_t j = 0; j < n; j++) {
      if (j == i) continue;
      std::vector<Grp> t = base;
      long c = c0;
      int popped = 0;
      for (auto &p : mg.insts[j].moves) push(t, c, p, popped);
      long save = c0 + (long)mg.insts[j].moves.size() - c;
      S[i * n + j] = (uint8_t)std::min<long>(save, 255);
      if (save > 0) { positive++; saveHist[save]++; depthHist[popped]++; }
    }
  }
  printf("  pairs with a saving: %ld of %zu (%.2f%%); saving histogram:", positive, n * (n - 1), 100.0 * positive / (n * (n - 1.0)));
  for (auto &kv : saveHist) printf(" %d:%ld", kv.first, kv.second);
  printf("\n  ... groups of the first instance consumed:");
  for (auto &kv : depthHist) printf(" %d:%ld", kv.first, kv.second);
  printf("\n");
  if (argc == 3) {
    FILE *f = fopen(argv[2], "wb");
    fwrite(S.data(), 1, S.size(), f);
    fclose(f);
  }

  // along the best order
  const Run *best = &runs[0];
  for (auto &R : runs) if (R.merged < best->merged) best = &R;
  {
    std::vector<Grp> t;
    long c = 0, pairSum = 0, vanished = 0;
    std::map<int, long> seamSave, seamDepth;
    int prev = -1;
    for (int i : best->order) {
      long before = c;
      int popped = 0;
      size_t tb = t.size();
      for (auto &p : mg.insts[i].moves) push(t, c, p, popped);
      long save = (long)mg.insts[i].moves.size() - (c - before);
      if (prev >= 0) pairSum += S[(size_t)prev * n + i];
      if (save > 0) { seamSave[save]++; seamDepth[popped]++; }
      if (t.size() <= tb && popped > 0 && c - before <= 0) vanished++;
      prev = i;
    }
    printf("  best order (%s): saves %ld; sum of its pairwise savings %ld; seams saving:", best->name.c_str(), raw - c, pairSum);
    for (auto &kv : seamSave) printf(" %d:%ld", kv.first, kv.second);
    printf("\n  ... groups consumed per saving seam:");
    for (auto &kv : seamDepth) printf(" %d:%ld", kv.first, kv.second);
    printf("; instances vanishing completely: %ld\n", vanished);
  }
}
