#include "layout.h"
#include <fstream>
#include <sstream>
// For algorithms X c X' in the pool where X c X' != X' c X in the pair-solver
// simulator, see which one brobdicube reproduces.
static void myperm(const ps::PairGeom &G, const std::vector<mv> &alg, int loc[48]) {
  for (int p = 0; p < 48; p++) loc[p] = p;
  for (auto m : alg) for (int k = 0; k < m.twist; k++) for (int p = 0; p < 48; p++) loc[p] = G.dst[m.face][m.dep][loc[p]];
}
int main() {
  Layout::faceTw = 3; Layout::sliceTw = 1;
  Layout L; L.build();
  ps::PairGeom G;
  std::ifstream in("../dist/gullicube/src/pair/p8s6.txt");
  std::string line;
  int tested = 0, asWritten = 0, reversed = 0;
  while (std::getline(in, line) && tested < 200) {
    std::istringstream is(line);
    int len, x; is >> len; for (int k = 0; k < 48; k++) is >> x;
    std::string rest; std::getline(is, rest);
    auto alg = parse_moves(rest);
    if (alg.size() < 3 || alg[0].dep != 1 || alg[0].twist == 2) continue;
    auto flip = alg;  // invert the outer face setup turns
    flip.front().twist = 4 - flip.front().twist;
    flip.back().twist = 4 - flip.back().twist;
    int l1[48], l2[48];
    myperm(G, alg, l1); myperm(G, flip, l2);
    if (!memcmp(l1, l2, sizeof l1)) continue;
    xcube c(3, 3, 'U');
    for (auto m : alg) domove(m, c, true);
    c.normalizefacerots();
    const xorbit *X[2] = {&c.rows[1][0], &c.rows[0][1]};
    auto matches = [&](int *loc) {
      for (int p = 0; p < 48; p++) { int bq = L.phi[loc[p]]; if (X[bq / 24]->a[bq % 24] != L.phi[p] % 24) return false; }
      return true;
    };
    tested++;
    asWritten += matches(l1);
    reversed += matches(l2);
  }
  printf("%d face-setup algs where the setup direction matters: brob matches as-written %d, reversed-setup %d\n", tested, asWritten, reversed);
}
