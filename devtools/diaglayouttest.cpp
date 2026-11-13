// Check that a pair algorithm's diagonal effect (computed at (a,b)=(1,2) on an
// 8x8x8) is the same on (a,a)/(b,b) for any pair (a,b) and cube size.
#include "layout.h"
#include "solvers.h"
#include <random>
int main() {
  Pool p;
  p.load("../dist/gullicube/src/pair/fast6.d.txt", 99, false);
  std::mt19937 r(3);
  int n = 0, bad = 0;
  for (size_t a = 0; a < p.size(); a++) {
    if (!p.diag[a] || r() % 20) continue;
    std::vector<mv> alg;
    for (auto &m : poolMoves(p, a)) alg.push_back({m.depth, (short)m.face, (short)m.amt});
    std::array<uint8_t, 24> D2, D3;
    diagEffect(alg, D2, D3);
    for (int N : {10, 16, 17}) {
      int My = (N - 2) / 2, Mx = N - 2 - My;
      int pa = 1 + r() % My, pb = 1 + r() % My;
      if (pa == pb) continue;
      xcube c(Mx, My, 'U');
      applyMapped(c, alg, [&](int d) { return d == 2 ? pa + 1 : pb + 1; });
      uint8_t o[24];
      readOrbit(c, pa, pa, o);
      for (int k = 0; k < 24; k++) bad += o[k] != D2[k];
      readOrbit(c, pb, pb, o);
      for (int k = 0; k < 24; k++) bad += o[k] != D3[k];
      n++;
    }
  }
  printf("checked %d (alg, N, pair) cases, %d mismatched diagonal positions\n", n, bad);
}
