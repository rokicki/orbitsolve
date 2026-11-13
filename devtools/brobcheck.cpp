// Replay a scramble plus brobdicube's printed solution (-o output: move lines
// start with a space) on the independent sticker simulator (../pair/cube.h),
// and report which stickers end up wrong.
//
//   brobdicube -D algs3 -i -o -t 1 N < scramble.txt > out.txt
//   brobcheck N scramble.txt out.txt
#include "../dist/gullicube/src/pair/cube.h"
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: brobcheck N scramble.txt brob-output.txt\n"); return 2; }
  int N = atoi(argv[1]);
  Cube sim(N);
  std::vector<uint8_t> col(sim.pos.size());
  for (size_t i = 0; i < col.size(); i++) col[i] = sim.face[i];
  long nmoves = 0;
  auto play = [&](const std::string &tok) {
    Move m;
    if (!parse_move(tok, m)) return false;
    const auto &P = sim.movePerm[m.face][m.depth - 1];
    for (int k = 0; k < m.amt; k++) {
      std::vector<uint8_t> nc(col.size());
      for (size_t i = 0; i < col.size(); i++) nc[P[i]] = col[i];
      col.swap(nc);
    }
    return true;
  };
  std::ifstream sc(argv[2]);
  std::string tok;
  while (sc >> tok) play(tok);
  std::ifstream out(argv[3]);
  std::string line;
  while (std::getline(out, line)) {
    if (line.empty() || line[0] != ' ') continue;
    std::istringstream is(line);
    std::vector<std::string> toks;
    bool allMoves = true;
    while (is >> tok) {
      Move m;
      if (!parse_move(tok, m)) { allMoves = false; break; }
      toks.push_back(tok);
    }
    if (!allMoves) continue;
    for (auto &t : toks) { play(t); nmoves++; }
  }
  std::map<std::pair<int, int>, int> wrong;
  for (size_t i = 0; i < col.size(); i++)
    if (col[i] != sim.face[i]) wrong[{std::min(sim.drow(i), sim.dcol(i)), std::max(sim.drow(i), sim.dcol(i))}]++;
  printf("N=%d: replayed %ld solution moves: %s\n", N, nmoves, wrong.empty() ? "SOLVED" : "NOT SOLVED");
  for (auto &w : wrong) {
    int a = w.first.first, b = w.first.second, mid = (N + 1) / 2;
    const char *kind = a == 1 && b == 1 ? "corner" : a == 1 ? (N % 2 && b == mid ? "middle edge" : "wing")
                       : (N % 2 && a == mid && b == mid) ? "centre centre"
                       : (N % 2 && (a == mid || b == mid)) ? "mid-centre" : a == b ? "diagonal centre" : "oblique centre";
    printf("  %4d wrong stickers at depth (%d,%d)  [%s]\n", w.second, a, b, kind);
  }
  return !wrong.empty();
}
