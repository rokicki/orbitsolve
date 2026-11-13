// Which algorithms ever make the top-P prefix (candidates with gain >= 1 at the
// start, sorted by moves per fixed piece = length / gain) over random pair
// states?  Ties at the cut are all counted in.  Also the analytic bound: an
// algorithm moving s pieces fixes at most s, so its best possible moves per
// piece is length / s.
//   prefixfreq POOL.cls STATES_FILE P...
#include "solvers.h"
#include "classpool.h"
#include <string>
#include <fstream>
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: prefixfreq POOL.cls STATES_FILE P...\n"); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  ClassPool cp;
  if (!cp.load(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  Pool p;
  poolFromClasses(p, cp, pairModel());
  const size_t n = p.size();
  std::vector<size_t> Ps;
  for (int i = 3; i < argc; i++) Ps.push_back(atol(argv[i]));
  std::vector<float> bestUnit(n);
  for (size_t a = 0; a < n; a++) {
    int s = 0;
    for (int d = 0; d < 48; d++) s += p.src[a][d] != d;
    bestUnit[a] = (float)p.cost[a] / s;
  }
  std::vector<std::vector<uint8_t>> ever(Ps.size(), std::vector<uint8_t>(n, 0));
  std::vector<float> maxCut(Ps.size(), 0), sumCut(Ps.size(), 0);
  std::vector<double> sumSkip(Ps.size(), 0);
  std::ifstream in(argv[2]);
  int ns = 0;
  std::vector<float> unit(n);
  std::vector<float> tmp;
  for (std::string line; std::getline(in, line);) {
    if (line.size() < 48 || line[0] == '#') continue;
    State s;
    for (int q = 0; q < 48; q++) s.col[q] = line[q] - '0';
    tmp.clear();
    for (size_t a = 0; a < n; a++) {
      int g = 0;
      for (int d = 0; d < 48; d++) {
        const int sp = p.src[a][d];
        if (sp != d) g += (s.col[sp] == target(d)) - (s.col[d] == target(d));
      }
      unit[a] = g >= 1 ? (float)p.cost[a] / g : 1e9f;
      if (g >= 1) tmp.push_back(unit[a]);
    }
    // exact top P: by moves per piece, then larger gain, then pool order
    std::vector<std::pair<float, uint32_t>> key;
    for (size_t a = 0; a < n; a++) if (unit[a] < 1e8f) key.push_back({unit[a] - 1e-4f * (p.cost[a] / unit[a]), (uint32_t)a});
    std::sort(key.begin(), key.end());
    for (size_t i = 0; i < Ps.size(); i++) {
      const size_t m = std::min(Ps[i], key.size());
      const float cut = unit[key[m - 1].second];
      maxCut[i] = std::max(maxCut[i], cut);
      sumCut[i] += cut;
      for (size_t j = 0; j < m; j++) ever[i][key[j].second] = 1;
      size_t skip = 0;  // could not reach this state's cut even fixing every piece they move
      for (size_t a = 0; a < n; a++) skip += bestUnit[a] > cut;
      sumSkip[i] += (double)skip / n;
    }
    if (ns + 1 == 10 || ns + 1 == 100 || ns + 1 == 300) {
      printf("  after %4d states: ever in the exact top P:", ns + 1);
      for (size_t i = 0; i < Ps.size(); i++) {
        size_t e = 0;
        for (size_t a = 0; a < n; a++) e += ever[i][a];
        printf(" P%zu %zu (%.1f%%)", Ps[i], e, 100.0 * e / n);
      }
      printf("\n");
      fflush(stdout);
    }
    ns++;
  }
  printf("%d states, %zu algorithms\n", ns, n);
  for (size_t i = 0; i < Ps.size(); i++) {
    size_t e = 0, possible = 0;
    for (size_t a = 0; a < n; a++) { e += ever[i][a]; possible += bestUnit[a] <= maxCut[i]; }
    // how many ever-seen are needed to cover the prefix sets as states are added: report final only
    printf("  P %6zu: cut (moves per piece) avg %.3f max %.3f; ever in the exact top P %zu (%.1f%%); "
           "could be by length/pieces-moved <= max cut %zu (%.1f%%); per state, skippable by length/pieces-moved > that state's cut: %.1f%%\n",
           Ps[i], sumCut[i] / ns, maxCut[i], e, 100.0 * e / n, possible, 100.0 * possible / n, 100.0 * sumSkip[i] / ns);
  }
}
