// enumwing D: print every canonical sequence of up to D moves over face turns
// and depth-2 slices (6x6x6 notation) that restores the corners and the depth-3
// wings -- the wings-phase algorithm candidates (centres free, depth-2 wings
// arbitrary).  This replaces the twsearch run on 6w.tws (identity on corners
// and EDGES2, with depth-2 wings and centres identical).
//
// Canonical: within a run of same-axis moves (which commute) the layers appear
// in strictly increasing order, so each merged sequence is printed once.
// Output feeds minewing.
#include "layout.h"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: enumwing D   print canonical face + depth-2 slice sequences (<= D moves)\n"
                    "                    that restore corners and depth-3 wings\n");
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  const int D = atoi(argv[1]);
  // per-move permutations of the tracked items on a 6^3: 24 corner facelets
  // (tc.corner.a) and the 24 depth-3 wings (wing orbit 1)
  using St = std::array<uint8_t, 48>;
  St mp[6][3][4];
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 2; d++)
      for (int t = 1; t <= 3; t++) {
        xcube c(2, 2, 'U');
        domove({(short)d, (short)f, (short)t}, c, true);
        for (int k = 0; k < 24; k++) mp[f][d][t][k] = c.tc.corner.a[k];
        for (int k = 0; k < 24; k++) mp[f][d][t][24 + k] = 24 + c.wingorbits[1].a[k];
      }
  St id;
  for (int k = 0; k < 48; k++) id[k] = k;
  for (int f = 0; f < 6; f++)
    for (int t = 1; t <= 3; t++)
      if (mp[f][2][t] != id) { fprintf(stderr, "depth-2 slice moves a tracked item\n"); return 1; }
  static const int AX[6] = {0, 1, 2, 1, 2, 0}, SD[6] = {0, 0, 0, 1, 1, 1};
  const char *FN = "ULFRBD";
  struct Mv { int f, d, t; };
  std::vector<Mv> seq;
  std::vector<St> stk = {id};
  long printed = 0;
  std::string line;
  // lastAxis / lastLayer: canonical order within same-axis runs
  auto rec = [&](auto &&self, int lastAxis, int lastLayer) -> void {
    if (!seq.empty() && stk.back() == id) {
      line.clear();
      for (size_t i = 0; i < seq.size(); i++) {
        if (i) line += ' ';
        if (seq[i].d > 1) line += '2';
        line += FN[seq[i].f];
        if (seq[i].t == 2) line += '2';
        if (seq[i].t == 3) line += '\'';
      }
      line += '\n';
      fputs(line.c_str(), stdout);
      printed++;
    }
    if ((int)seq.size() == D) return;
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 2; d++) {
        const int ax = AX[f], layer = SD[f] * 2 + (d - 1);
        if (ax == lastAxis && layer <= lastLayer) continue;
        for (int t = 1; t <= 3; t++) {
          St n;
          if (d == 1) {
            const St &q = mp[f][d][t], &p = stk.back();
            for (int k = 0; k < 48; k++) n[k] = p[q[k]];
          } else n = stk.back();
          seq.push_back({f, d, t});
          stk.push_back(n);
          self(self, ax, layer);
          stk.pop_back();
          seq.pop_back();
        }
      }
  };
  rec(rec, -1, -1);
  fprintf(stderr, "enumwing %d: %ld sequences\n", D, printed);
}
