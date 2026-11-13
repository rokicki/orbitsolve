// Replay scramble + brobdicube's printed solution using brobdicube's OWN code
// (a fresh solved xcube, domove for every move) and check every sticker via
// xcube::get(), plus wings and corners.  No normalizefacerots, no threecube
// centre-centre tracker: this is what an end-to-end self-check would see.
//
//   brobreplay N scramble.txt brob-output.txt
#include "xcube.h"
#include <cstdio>
#include <fstream>
#include <sstream>
int main(int argc, char **argv) {
  int N = atoi(argv[1]);
  int My = (N - 2) / 2, Mx = N - 2 - My;
  xcube c(Mx, My, 'C');
  for (auto &w : c.wingorbits) uclear(w);  // solved wings: slot i holds piece i
  long n = 0;
  auto apply = [&](const std::string &line) {
    const char *s = line.c_str();
    while (*s) {
      while (*s && *s <= ' ') s++;
      if (!*s) break;
      mv m = parse_move(s);
      domove(m, c, true);
      n++;
    }
  };
  std::ifstream sc(argv[2]);
  std::string line, all;
  while (std::getline(sc, line)) apply(line);
  std::ifstream out(argv[3]);
  while (std::getline(out, line)) {
    if (line.empty() || line[0] != ' ') continue;
    bool ok = true;  // only lines made entirely of moves
    std::istringstream is(line);
    std::string t;
    while (is >> t) ok &= (t.find_first_not_of("0123456789ULFRBD'2") == std::string::npos);
    if (ok) apply(line);
  }
  int badCentres = 0, badWings = 0, badCorners = 0;
  for (int f = 0; f < 6; f++)
    for (int x = 1; x <= Mx + My; x++)
      for (int y = 1; y <= Mx + My; y++) badCentres += c.get(f, x, y) != '0' + f;
  for (auto &w : c.wingorbits)
    for (int k = 0; k < 24; k++) badWings += w.a[k] != k;
  for (int k = 0; k < 24; k++) badCorners += c.tc.corner.a[k] != k / 4;
  bool ok = !badCentres && !badWings && !badCorners;
  printf("N=%d: replayed %ld moves with brobdicube's own domove: %s (wrong centre stickers %d, wings %d, corner stickers %d)\n",
         N, n, ok ? "SOLVED" : "NOT SOLVED", badCentres, badWings, badCorners);
  return !ok;
}
