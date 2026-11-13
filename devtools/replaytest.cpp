// CentreReplay must match domove exactly on the centre grid and facerots.
#include "replay.h"
#include <cstring>
int main() {
  rng.seed(9);
  int bad = 0, cases = 0;
  for (int N : {8, 9, 16, 33, 64}) {
    int My = (N - 2) / 2, Mx = N - 2 - My;
    for (int t = 0; t < 4; t++) {
      xcube a(Mx, My, 'R');
      xcube b = a;
      std::vector<mv> seq;
      for (int i = 0; i < 3000; i++) {
        mv m;
        m.face = rng() % 6;
        m.twist = 1 + rng() % 3;
        int r = rng() % 3;
        m.dep = r == 0 ? 1 : (rng() % 2 ? 2 + rng() % My : N + 1 - (2 + rng() % My));  // faces, slices from either side
        seq.push_back(m);
      }
      for (auto m : seq) domove(m, a, true);
      CentreReplay::apply(b, seq, 4);
      for (int f = 0; f < 6; f++) bad += a.facerots[f] != b.facerots[f];
      for (int y = 0; y < My; y++)
        for (int x = 0; x < Mx; x++) bad += memcmp(a.rows[y][x].a, b.rows[y][x].a, 24) != 0;
      cases++;
    }
  }
  printf("%d random sequences on N = 8, 9, 16, 33, 64: %d mismatched orbits/facerots\n", cases, bad);
}
