// Check psolve -v output lines "V c0..c47 | moves" on a full 8x8x8: the moves
// must solve the pair and leave all non-diagonal, non-pair stickers in place.
#include "cube.h"
#include <cstdio>
#include <iostream>
#include <sstream>
int main() {
  Cube cube(8);
  int S = cube.pos.size();
  std::vector<int> pidx(S, -1);
  int orbA = -1, orbB = -1;
  for (int i = 0; i < S; i++)
    if (cube.face[i] == 0 && cube.row[i] == 1 && cube.col[i] == 2) orbA = cube.orbit[i];
  for (int i = 0; i < S; i++) {
    int dr = cube.drow(i), dc = cube.dcol(i);
    if (((dr == 2 && dc == 3) || (dr == 3 && dc == 2)) && cube.orbit[i] != orbA) orbB = cube.orbit[i];
  }
  for (int w = 0; w < 2; w++) {
    int k = 0;
    for (int i = 0; i < S; i++) if (cube.orbit[i] == (w ? orbB : orbA)) pidx[i] = w * 24 + k++;
  }
  std::string line;
  int n = 0, bad = 0;
  while (std::getline(std::cin, line)) {
    if (line[0] != 'V') continue;
    std::istringstream is(line.substr(1));
    std::vector<int> col(48);
    for (auto &c : col) is >> c;
    std::string rest;
    std::getline(is, rest);
    rest = rest.substr(rest.find('|') + 1);
    auto alg = parse_alg(rest);
    auto loc = cube.apply(alg);
    std::vector<int> now(48);
    bool ok = true;
    for (int i = 0; i < S; i++) {
      if (pidx[i] >= 0) now[pidx[loc[i]]] = col[pidx[i]];
      else if (loc[i] != i && cube.drow(i) != cube.dcol(i)) ok = false;
    }
    for (int p = 0; p < 48; p++) if (now[p] != (p % 24) / 4) ok = false;
    n++;
    if (!ok) bad++;
  }
  printf("verified %d, bad %d\n", n, bad);
}
