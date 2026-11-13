// Which pair algorithms disturb the diagonal centre orbits?
//
// The pool generator requires everything except the (2,3)/(3,2) pair and the
// diagonal orbits to be restored; the diagonals (2,2) and (3,3) are ignored.
// For each pool line "LEN s0..s47 moves" this computes the algorithm's
// permutation on the 48 diagonal positions (orbit (2,2) then (3,3)) and reports
// how many algorithms are not the identity there.
//
//   diagflag [-w OUT] pool     -w: write the pool back with a trailing flag
//                                  " D0" (diagonals untouched) or " D1"
#include "cube.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  const char *out = nullptr, *in = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-w")) out = argv[++i];
    else in = argv[i];
  }
  Cube cube(8);
  int S = cube.pos.size();
  // diagonal positions: drow == dcol == 2 (orbit (2,2)) and == 3 (orbit (3,3))
  std::vector<int> didx(S, -1), dst;
  for (int dep = 2; dep <= 3; dep++)
    for (int i = 0; i < S; i++)
      if (cube.drow(i) == dep && cube.dcol(i) == dep) { didx[i] = dst.size(); dst.push_back(i); }
  if (dst.size() != 48) { fprintf(stderr, "expected 48 diagonal stickers, got %zu\n", dst.size()); return 1; }
  // restricted quarter-turn maps for depths 1..3
  std::vector<std::array<uint8_t, 48>> pm(6 * 4);
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int p = 0; p < 48; p++) pm[f * 4 + d][p] = didx[cube.movePerm[f][d - 1][dst[p]]];
  std::ifstream fin(in);
  FILE *fo = out ? fopen(out, "w") : nullptr;
  std::string line;
  std::map<int, std::array<long, 4>> byLen;  // len -> {total, touches (2,2), touches (3,3), touches either}
  long total = 0, touched = 0;
  while (std::getline(fin, line)) {
    // skip "LEN" and the 48 pair-permutation numbers
    const char *p = line.c_str();
    char *e;
    int len = strtol(p, &e, 10);
    p = e;
    for (int k = 0; k < 48; k++) { strtol(p, &e, 10); p = e; }
    auto alg = parse_alg(p);
    std::array<uint8_t, 48> l;
    for (int q = 0; q < 48; q++) l[q] = q;
    for (auto &m : alg)
      for (int k = 0; k < m.amt; k++)
        for (int q = 0; q < 48; q++) l[q] = pm[m.face * 4 + m.depth][l[q]];
    bool t2 = false, t3 = false;
    for (int q = 0; q < 48; q++)
      if (l[q] != q) (q < 24 ? t2 : t3) = true;
    auto &b = byLen[len];
    b[0]++;
    b[1] += t2;
    b[2] += t3;
    b[3] += t2 || t3;
    total++;
    touched += t2 || t3;
    if (fo) fprintf(fo, "%s D%d\n", line.c_str(), (t2 || t3) ? 1 : 0);
  }
  if (fo) fclose(fo);
  printf("%s: %ld algorithms, %ld (%.1f%%) move diagonal pieces\n", in, total, touched, 100.0 * touched / total);
  for (auto &kv : byLen)
    printf("   len %2d: %9ld algs, disturb (2,2) %5.1f%%, (3,3) %5.1f%%, either %5.1f%%\n", kv.first, kv.second[0],
           100.0 * kv.second[1] / kv.second[0], 100.0 * kv.second[2] / kv.second[0],
           100.0 * kv.second[3] / kv.second[0]);
}
