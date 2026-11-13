// Do xcube::ccs and threecube::centercenters agree after middle-layer turns?
#include "xcube.h"
#include <cstdio>
int main() {
  for (int f = 0; f < 6; f++) {
    xcube c(3, 2, 'C');  // N = 7, middle layer = depth 4
    domove({4, (short)f, 1}, c, true);
    printf("4%c: ccs =", "ULFRBD"[f]);
    for (int i = 0; i < 6; i++) printf(" %d", c.ccs[i]);
    printf("   tc.centercenters =");
    for (int i = 0; i < 6; i++) printf(" %d", c.tc.centercenters[i]);
    printf("\n");
  }
}
