// which conjugation matches ClassPool images: perm(image(A, 2s)) == sigma o perm(A) o sigma^-1 ?
#include "classpool.h"
#include <cstdio>
int main() {
  OrbitModel m = diagModel();
  ClassPool cp;
  cp.load("../dist/gullicube/data/diag3.cls");
  int c1ok = 0, c2ok = 0, n = 0;
  for (size_t r = 0; r < 5; r++)
    for (size_t s = 0; s < m.syms.size(); s++) {
      auto A = cp.rep[r];
      auto pa = m.permOf(A), pb = m.permOf(m.image(A, 2 * s));
      bool c1 = true, c2 = true;
      for (int k = 0; k < m.P; k++) {
        c1 &= pb[k] == m.sigma[s][pa[m.sigmaInv[s][k]]];
        c2 &= pb[k] == m.sigmaInv[s][pa[m.sigma[s][k]]];
      }
      c1ok += c1, c2ok += c2, n++;
    }
  printf("%d cases: sigma pa sigma^-1 %d, sigma^-1 pa sigma %d\n", n, c1ok, c2ok);
}
