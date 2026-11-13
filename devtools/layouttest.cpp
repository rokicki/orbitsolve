// Validate the pair-solver <-> brobdicube layout and the depth substitution.
#include "layout.h"
#include <fstream>
#include <sstream>
#include <random>
int main() {
  Layout L;
  bool found = false;
  for (int ft : {1, 3})
    for (int st : {1, 3}) {
      Layout::faceTw = ft; Layout::sliceTw = st;
      if (L.build()) { printf("mapping found with brob face twist %d, slice twist %d\n", ft, st); found = true; goto done; }
    }
done:
  if (!found) { printf("no consistent layout mapping\n"); return 1; }
  printf("phi:");
  for (int p = 0; p < 48; p++) printf(" %d", L.phi[p]);
  printf("\n");
  // check pool algorithms: pair-solver perm (mapped through phi) must equal the
  // perm brobdicube computes on xorbits (a,b),(b,a) with 2->a+1, 3->b+1
  std::ifstream in("../dist/gullicube/src/pair/p8s6.txt");
  std::string line;
  int n = 0, bad = 0, badOther = 0;
  std::mt19937 r(1);
  while (std::getline(in, line) && n < 3000) {
    if (r() % 40) continue;
    std::istringstream is(line);
    int len, src[48];
    is >> len;
    for (auto &s : src) is >> s;
    std::string rest;
    std::getline(is, rest);
    auto alg = parse_moves(rest);
    for (int N : {8, 13, 16}) {
      int My = (N - 2) / 2, Mx = N - 2 - My;
      for (int t = 0; t < 3; t++) {
        int a = 1 + r() % My, b = 1 + r() % My;
        if (a == b) continue;
        xcube c(Mx, My, 'U');
        xcube c0 = c;
        applyMapped(c, alg, [&](int d) { return d == 2 ? a + 1 : b + 1; });
        uint8_t X0a[24], X1a[24];
        readOrbit(c, a, b, X0a);
        readOrbit(c, b, a, X1a);
        struct { uint8_t *a; } X0{X0a}, X1{X1a};
        // pair-solver: after alg, position d holds what was at src[d]
        for (int d = 0; d < 48; d++) {
          int bd = L.phi[d], bs = L.phi[src[d]];
          auto &X = bd < 24 ? X0 : X1;
          if (X.a[bd % 24] != bs % 24 || (bd < 24) != (bs < 24)) { bad++; break; }
        }
        // every other centre orbit, wing orbit, corner and middle edge unchanged
        for (int y = 0; y < My; y++)
          for (int x = 0; x < Mx; x++) {
            if ((x == a - 1 && y == b - 1) || (x == b - 1 && y == a - 1) || (x == y && (x == a - 1 || x == b - 1))) continue;
            for (int k = 0; k < 24; k++)
              if (c.rows[y][x].a[k] != c0.rows[y][x].a[k]) { badOther++; goto next; }
          }
        for (int w = 0; w < My; w++)
          for (int k = 0; k < 24; k++)
            if (c.wingorbits[w].a[k] != c0.wingorbits[w].a[k]) { badOther++; goto next; }
        for (int k = 0; k < 24; k++)
          if (c.tc.corner.a[k] != c0.tc.corner.a[k] || c.tc.edge.a[k] != c0.tc.edge.a[k]) { badOther++; goto next; }
      next:
        n++;
      }
    }
  }
  printf("checked %d (alg, N, pair) cases: %d pair-perm mismatches, %d disturbed something else\n", n, bad, badOther);
}
