// cycletypes: for pair algorithms of small support (pieces treated as distinct), the cheapest cost
// of every permutation the pools reach, grouped by cycle type.  For each type: how many such
// permutations exist (pieces never leave their orbit), how many the pools reach, and the cost
// spread (cheapest member vs dearest cheapest-cost member).
//
//   cycletypes [--maxsupport K] [--list TYPE N] POOL.cls...   (run from dist/gullicube)
//     --list "3" 20: print N of the dearest permutations of that type (cost, moves of one source)
#include "classpool.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <map>
#include <unordered_map>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int maxSup = 6, listN = 0;
  std::string listType;
  std::vector<std::string> pools;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--maxsupport" && i + 1 < argc) maxSup = atoi(argv[++i]);
    else if (o == "--list" && i + 2 < argc) listType = argv[++i], listN = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pools.push_back(o);
  }
  if (pools.empty()) { fprintf(stderr, "usage: cycletypes [--maxsupport K] [--list TYPE N] POOL.cls...\n"); return 1; }
  OrbitModel m = pairModel();
  struct H { size_t operator()(const std::array<uint8_t, 48> &v) const { size_t h = 1469598103934665603ULL; for (auto c : v) h = (h ^ c) * 1099511628211ULL; return h; } };
  struct E { uint8_t cost; uint32_t file, ref; };
  std::unordered_map<std::array<uint8_t, 48>, E, H> best;
  std::deque<ClassPool> cps;
  for (size_t fi = 0; fi < pools.size(); fi++) {
    cps.emplace_back();
    ClassPool &cp = cps.back();
    if (!cp.load(pools[fi].c_str())) { fprintf(stderr, "cannot read a pool\n"); return 1; }
    long seen = 0;
    cp.expand(m, [&](const uint8_t *src, int c, uint32_t ref) {
      int sup = 0;
      for (int j = 0; j < 48 && sup <= maxSup; j++) sup += src[j] != j;
      if (sup == 0 || sup > maxSup) return;
      std::array<uint8_t, 48> d;
      for (int j = 0; j < 48; j++) d[src[j]] = j;  // dst form: the piece at j goes to d[j]
      seen++;
      auto it = best.find(d);
      if (it == best.end() || c < it->second.cost) best[d] = {(uint8_t)c, (uint32_t)fi, ref};
    }, false);
    printf("%s: %ld images of support <= %d; %zu permutations so far\n", pools[fi].c_str(), seen, maxSup, best.size());
    fflush(stdout);
  }
  auto typeOf = [](const std::array<uint8_t, 48> &d) {
    std::vector<int> cyc;
    bool seen[48] = {false};
    int orbitsUsed = 0;
    for (int j = 0; j < 48; j++) {
      if (seen[j] || d[j] == j) continue;
      int len = 0;
      for (int x = j; !seen[x]; x = d[x]) seen[x] = true, len++;
      cyc.push_back(len);
      orbitsUsed |= 1 << (j / 24);
    }
    std::sort(cyc.rbegin(), cyc.rend());
    std::string t;
    for (int c : cyc) t += (t.empty() ? "" : ",") + std::to_string(c);
    return t + (orbitsUsed == 3 ? " (both orbits)" : " (one orbit)");
  };
  std::map<std::string, std::map<int, long>> byType;
  std::map<std::string, std::vector<std::pair<int, std::array<uint8_t, 48>>>> members;
  for (auto &[d, e] : best) {
    const auto t = typeOf(d);
    byType[t][e.cost]++;
    if (listN && t.rfind(listType + " ", 0) == 0) members[t].push_back({e.cost, d});
  }
  // how many permutations of each type exist (pieces stay in their orbit of 24)
  auto choose = [](double n, int k) { double r = 1; for (int i = 0; i < k; i++) r = r * (n - i) / (i + 1); return r; };
  auto countType = [&](const std::string &t) {
    // cycle lengths; each cycle lies in one orbit; (one orbit): all in one of the two orbits
    std::vector<int> cyc;
    size_t i = 0;
    while (i < t.size() && t[i] != ' ') { size_t j = i; while (j < t.size() && t[j] != ',' && t[j] != ' ') j++; cyc.push_back(atoi(t.substr(i, j - i).c_str())); i = j + (t[j] == ',' ? 1 : 0); }
    // count placements: assign each cycle to an orbit, choose its pieces, (len-1)! orders; divide by symmetries of equal cycles
    double total = 0;
    const bool one = t.find("one orbit") != std::string::npos;
    const int k = cyc.size();
    for (int mask = 0; mask < (1 << k); mask++) {
      if (one && mask != 0 && mask != (1 << k) - 1) continue;
      if (!one && (mask == 0 || mask == (1 << k) - 1)) continue;
      double ways = 1;
      int used[2] = {0, 0};
      for (int c = 0; c < k; c++) {
        const int o = mask >> c & 1;
        ways *= choose(24 - used[o], cyc[c]);
        double f = 1;
        for (int x = 2; x < cyc[c]; x++) f *= x;
        ways *= f;
        used[o] += cyc[c];
      }
      total += ways;
    }
    std::map<int, int> mult;
    for (int c : cyc) mult[c]++;
    for (auto &[len, n] : mult) for (int x = 2; x <= n; x++) total /= x;
    return total;
  };
  printf("\ncycle type: permutations reached / existing; cheapest cost histogram\n");
  for (auto &[t, h] : byType) {
    long n = 0;
    for (auto &kv : h) n += kv.second;
    printf("  %-24s %8ld / %-10.0f", t.c_str(), n, countType(t));
    for (auto &kv : h) printf(" %d:%ld", kv.first, kv.second);
    printf("\n");
  }
  for (auto &[t, v] : members) {
    std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.first > b.first; });
    printf("\n%s: dearest %d of %zu\n", t.c_str(), std::min<int>(listN, v.size()), v.size());
    for (int i = 0; i < listN && i < (int)v.size(); i++) {
      const E &e = best[v[i].second];
      auto mv = m.image(cps[e.file].rep[e.ref / 96], e.ref % 96);
      std::string s;
      for (auto &x : mv) s += (s.empty() ? "" : " ") + move_str(x);
      printf("  %d: %s\n", v[i].first, s.c_str());
    }
  }
  return 0;
}
