// Tests for the Kociemba facelet reader/writer (dist/gullicube/src/brob/facelets.*):
//   - 3x3 strings after U and after R, checked by hand;
//   - random scrambles, N = 2..12: xcube_to_facelets equals the string built
//     from the independent sticker simulator (src/pair/cube.h, Cube::kociemba);
//   - parse then render reproduces the string, and every scrambled state parses;
//   - a twisted corner, a flipped middle edge, an odd swap and a miscoloured
//     centre are rejected; a corner swap on an even cube is accepted.
//   facelettest [scrambles per size]
#include "facelets.h"
#include "../dist/gullicube/src/pair/cube.h"
#include <cstdio>
#include <random>

static const char *LET = "ULFRBD";  // colour index -> letter (xcube and simulator agree)

static std::string simFacelets(int n, const std::vector<mv> &moves) {
  Cube sim(n);
  std::vector<uint8_t> col(sim.pos.size());
  for (size_t i = 0; i < col.size(); i++) col[i] = sim.face[i];
  for (auto m : moves) {
    const auto &P = sim.movePerm[m.face][m.dep - 1];
    for (int k = 0; k < m.twist; k++) {
      std::vector<uint8_t> nc(col.size());
      for (size_t i = 0; i < col.size(); i++) nc[P[i]] = col[i];
      col.swap(nc);
    }
  }
  std::string s;
  for (int kf = 0; kf < 6; kf++)
    for (int r = 0; r < n; r++)
      for (int c = 0; c < n; c++) s += LET[col[sim.kociemba(kf, r, c)]];
  return s;
}
static std::string xcubeFacelets(int n, const std::vector<mv> &moves) {
  const int My = (n - 2) / 2, Mx = n - 2 - My;
  moveset = moves;
  xcube c(Mx, My, 'I');
  moveset.clear();
  return xcube_to_facelets(c);
}
static bool parses(int n, const std::string &s, std::string &err, std::string *back = nullptr) {
  const int My = (n - 2) / 2, Mx = n - 2 - My;
  xcube c(Mx, My, 'C');
  if (!xcube_from_facelets(c, s, err)) return false;
  if (back) *back = xcube_to_facelets(c);
  return true;
}

int main(int argc, char **argv) {
  const int reps = argc > 1 ? atoi(argv[1]) : 200;
  int bad = 0;
  // 3x3 by hand (Kociemba order U R F D L B)
  struct { const char *alg, *want; } hand[] = {
      {"U", "UUUUUUUUU" "BBBRRRRRR" "RRRFFFFFF" "DDDDDDDDD" "FFFLLLLLL" "LLLBBBBBB"},
      {"R", "UUFUUFUUF" "RRRRRRRRR" "FFDFFDFFD" "DDBDDBDDB" "LLLLLLLLL" "UBBUBBUBB"},
  };
  for (auto &h : hand) {
    auto ms = parse_moves(h.alg);
    std::string a = xcubeFacelets(3, ms), b = simFacelets(3, ms);
    printf("3x3 %s: xcube %s, simulator %s\n", h.alg, a == h.want ? "ok" : "WRONG", b == h.want ? "ok" : "WRONG");
    if (a != h.want || b != h.want) { bad++; printf("  want %s\n  xcube %s\n  sim   %s\n", h.want, a.c_str(), b.c_str()); }
  }
  std::mt19937_64 r(12345);
  for (int n = 2; n <= 12; n++) {
    int agree = 0, rt = 0, rejTwist = 0, rejFlip = 0, rejSwap = 0, accSwap = 0, rejCol = 0, tries = 0;
    std::string err, back;
    for (int k = 0; k < reps; k++) {
      std::vector<mv> ms;
      for (int i = 0; i < 30 * n; i++) ms.push_back({1 + (int)(r() % ((n + 1) / 2)), (short)(r() % 6), (short)(1 + r() % 3)});
      std::string a = xcubeFacelets(n, ms), b = simFacelets(n, ms);
      agree += a == b;
      if (a != b && bad++ < 3) printf("N=%d mismatch\n  xcube %s\n  sim   %s\n", n, a.c_str(), b.c_str());
      if (parses(n, a, err, &back) && back == a) rt++;
      else if (bad++ < 6) printf("N=%d round trip failed: %s\n", n, err.c_str());
      // corner URF (U face bottom-right, R face top-left, F face top-right) twisted
      std::string t = a;
      const int U = 0 * n * n + (n - 1) * n + (n - 1), R = 1 * n * n + 0, F = 2 * n * n + (n - 1);
      std::swap(t[U], t[R]);
      std::swap(t[R], t[F]);
      rejTwist += !parses(n, t, err);
      if (n & 1) {  // middle edge UF flipped
        t = a;
        const int h = n / 2;
        std::swap(t[0 * n * n + (n - 1) * n + h], t[2 * n * n + h]);
        rejFlip += !parses(n, t, err);
      }
      // swap corners URF and UFL (all three stickers each)
      t = a;
      const int U2 = 0 * n * n + (n - 1) * n + 0, F2 = 2 * n * n + 0, L2 = 4 * n * n + (n - 1);
      // URF stickers (U, R, F) and UFL stickers (U, F, L) in matching cyclic order
      char u = t[U], rr = t[R], f = t[F];
      t[U] = t[U2]; t[R] = t[F2]; t[F] = t[L2];
      t[U2] = u; t[F2] = rr; t[L2] = f;
      tries++;
      bool ok = parses(n, t, err);
      if (n & 1) rejSwap += !ok;
      else accSwap += ok;
      if (n >= 4) {  // one centre sticker recoloured
        t = a;
        char &c = t[2 * n * n + 1 * n + 1];
        c = c == 'U' ? 'D' : 'U';
        rejCol += !parses(n, t, err);
      }
    }
    printf("N=%2d: %d/%d match the simulator, %d/%d round-trip; rejected: twist %d/%d", n, agree, reps, rt, reps, rejTwist, reps);
    if (n & 1) printf(", flip %d/%d, corner swap %d/%d", rejFlip, reps, rejSwap, reps);
    else printf(", corner swap accepted %d/%d", accSwap, reps);
    if (n >= 4) printf(", recoloured centre %d/%d", rejCol, reps);
    printf("\n");
    if (agree != reps || rt != reps || rejTwist != reps || ((n & 1) && (rejFlip != reps || rejSwap != reps)) ||
        (!(n & 1) && accSwap != reps) || (n >= 4 && rejCol != reps))
      bad++;
  }
  printf(bad ? "FAILED\n" : "all tests passed\n");
  return bad != 0;
}
