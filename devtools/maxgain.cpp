// For pair states on stdin (48 digits; "p STATE COST" lines from pairdata --path
// too), the best gain one pool algorithm achieves (correct after - correct
// before), with and without the keep-solved rule, by the state's correct count.
//   maxgain POOL.cls < states
#include "solvers.h"
#include "classpool.h"
#include <map>
#include <string>
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: maxgain POOL.cls < states\n"); return 1; }
  printf("# %s %s\n", argv[0], argv[1]);
  ClassPool cp;
  if (!cp.load(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  Pool p;
  poolFromClasses(p, cp, pairModel());
  const size_t n = p.size();
  std::vector<uint64_t> X(n);
  for (size_t a = 0; a < n; a++)
    for (int d = 0; d < 48; d++) { int sp = p.src[a][d]; if ((sp % 24) / 4 != (d % 24) / 4) X[a] |= 1ULL << sp; }
  // by correct count: states, histogram of best gain (keep-solved), count of algs with gain >= 2
  std::map<int, std::map<int, long>> hk, ha;
  std::map<int, double> ge2, ge3, ge4;
  std::map<int, long> ns;
  for (std::string line; std::getline(std::cin, line);) {
    if (line.rfind("p ", 0) == 0) line = line.substr(2);
    if (line.size() < 48 || line[0] == '#') continue;
    State s;
    for (int q = 0; q < 48; q++) s.col[q] = line[q] - '0';
    s.setMasks();
    int c = s.correct();
    if (c == 48) continue;
    uint64_t corr = 0;
    for (int q = 0; q < 48; q++) corr |= (uint64_t)(s.col[q] == target(q)) << q;
    int bk = -99, ba = -99;
    long n2 = 0, n3 = 0, n4 = 0;
    for (size_t a = 0; a < n; a++) {
      int g = 0;
      for (int d = 0; d < 48; d++) {
        int sp = p.src[a][d];
        if (sp == d) continue;
        g += (s.col[sp] == target(d)) - (s.col[d] == target(d));
      }
      ba = std::max(ba, g);
      if (X[a] & corr) continue;
      bk = std::max(bk, g);
      n2 += g >= 2; n3 += g >= 3; n4 += g >= 4;
    }
    ns[c]++; hk[c][bk]++; ha[c][ba]++;
    ge2[c] += n2; ge3[c] += n3; ge4[c] += n4;
  }
  printf("correct states  best-gain histogram (keep-solved)          algs with gain >=2 / >=3 / >=4 (avg)\n");
  for (auto &[c, k] : ns) {
    printf("%7d %6ld  ", c, k);
    std::string h;
    for (auto &[g, m] : hk[c]) h += std::to_string(g) + ":" + std::to_string(m) + " ";
    printf("%-40s %8.0f %8.0f %8.0f\n", h.c_str(), ge2[c] / k, ge3[c] / k, ge4[c] / k);
  }
}
