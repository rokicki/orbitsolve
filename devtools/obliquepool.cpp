// Oblique-pair class pools from a list of sequences (one per line, e.g. the
// twsearch enumeration 6centers.log: every sequence of up to 11 moves on 8^3
// that changes nothing but one oblique pair and the diagonals).  Keeps the
// cheapest sequence per symmetry class (48 symmetries x inverse) and writes
// two class files:
//   OUT.all.cls     every class
//   OUT.single.cls  classes that keep one orbit of the pair colour-solved
//                   (so they solve the other orbit alone)
// Lines carry " D1" (the diagonals may move: the source allows it).
//   xz -dc 6centers.log.xz | obliquepool OUT      (run from dist/gullicube)
#include "classpool.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_map>

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: obliquepool OUT < sequences\n"); return 1; }
  printf("# obliquepool %s\n", argv[1]);
  OrbitModel m = pairModel();
  struct H { size_t operator()(const std::array<uint8_t, 48> &v) const { size_t h = 1469598103934665603ULL; for (auto c : v) h = (h ^ c) * 1099511628211ULL; return h; } };
  struct E { int cost; std::string text; };
  std::unordered_map<std::array<uint8_t, 48>, E, H> classes;
  std::string line;
  long n = 0, bad = 0;
  while (std::getline(std::cin, line)) {
    std::vector<Move> mv;
    try { mv = parse_alg(line); } catch (...) { bad++; continue; }
    if (mv.empty()) continue;
    n++;
    auto p = m.permOf(mv);
    std::array<uint8_t, 48> key;
    m.classKey(p.data(), key.data());
    auto it = classes.find(key);
    const int c = mv.size();
    if (it == classes.end() || c < it->second.cost) {
      std::string t;
      for (auto &x : mv) t += (t.empty() ? "" : " ") + move_str(x);
      classes[key] = {c, t};
    }
    if (n % 2000000 == 0) { fprintf(stderr, "%ld sequences, %zu classes\n", n, classes.size()); }
  }
  printf("%ld sequences (%ld unparsable), %zu classes\n", n, bad, classes.size());
  std::string base = argv[1];
  FILE *fa = fopen((base + ".all.cls").c_str(), "w"), *fs = fopen((base + ".single.cls").c_str(), "w");
  fprintf(fa, "# symmetry classes (48 x inverse) of the oblique-pair sequences: %zu classes\n", classes.size());
  fprintf(fs, "# symmetry classes (48 x inverse) of single-orbit oblique sequences\n");
  long ns = 0;
  for (auto &kv : classes) {
    fprintf(fa, "%d %s D1\n", kv.second.cost, kv.second.text.c_str());
    // single: some image keeps one orbit colour-solved; the class key is one image, so test the
    // representative's own perm on both orbits (the other orbit's images are reflections)
    auto p = m.permOf(parse_alg(kv.second.text));
    bool keepB = true, keepA = true;
    for (int j = 0; j < 48; j++) {
      // src form: position j now holds the piece from p[j]; colour-solved if same group
      bool same = p[j] / 4 == j / 4;
      if (j < 24) keepA &= same; else keepB &= same;
    }
    if ((keepA || keepB) && !(keepA && keepB)) { fprintf(fs, "%d %s D1\n", kv.second.cost, kv.second.text.c_str()); ns++; }
  }
  fclose(fa);
  fclose(fs);
  printf("%ld single-orbit classes\n", ns);
}
