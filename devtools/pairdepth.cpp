// pairdepth: do pair sequences (faces, depth-2 and depth-3 slices: the (2,3)/(3,2) pair) work for
// every other oblique pair?  On an N x N x N cube, each sequence is re-mapped with its depth-2
// slices at depth p and its depth-3 slices at depth q, for every p != q in 2..floor(N/2) other
// than the middle layer.  Checks, with certcube.h geometry:
//   - the action on the (p,q)/(q,p) pair facelets equals the action of the original on the
//     (2,3)/(3,2) pair of 8x8x8 (facelets matched by face, orbit and quarter turn);
//   - every other centre facelet, except the diagonals (row == col or row + col == N-1), keeps
//     its colour.
//
//   pairdepth N FILE... [--sample K]   (K: every K-th line of each file; default every line)
#include "certcube.h"
#include <atomic>
#include <cstring>
#include <map>
#include <thread>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  long sample = 1;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--sample" && i + 1 < argc) sample = atol(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() < 2) { fprintf(stderr, "usage: pairdepth N FILE... [--sample K]\n"); return 1; }
  const int N = atoi(pos[0].c_str()), M = N - 1;
  if (N < 6) { fprintf(stderr, "N must be at least 6\n"); return 1; }
  std::vector<std::vector<cc::Mv>> seqs;
  for (size_t fi = 1; fi < pos.size(); fi++) {
    FILE *f = fopen(pos[fi].c_str(), "r");
    if (!f) { fprintf(stderr, "cannot open an input file\n"); return 1; }
    char buf[4096];
    long ln = 0;
    while (fgets(buf, sizeof buf, f)) {
      if (buf[0] == '#' || buf[0] == '\n') continue;
      if (ln++ % sample) continue;
      std::vector<cc::Mv> mv;
      if (!cc::parseMoves(std::string(buf, strcspn(buf, "\r\n")), mv)) { fprintf(stderr, "bad line\n"); return 1; }
      for (auto &m : mv) if (m.depth > 3) { fprintf(stderr, "a move deeper than 3\n"); return 1; }
      seqs.push_back(mv);
    }
    fclose(f);
  }
  cc::Cube C8(8), CN(N);
  // reference action on the 8^3 pair (dst form over the 48 pairFacelets indices)
  const auto pf8 = cc::pairFacelets(C8);
  std::vector<int> f8(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) f8[pf8[k]] = k;
  // pair facelets on N^3 for depths (p, q), in the same order (face, orbit, quarter turn)
  auto pairN = [&](int p, int q) {
    std::vector<int> v;
    for (int f = 0; f < 6; f++)
      for (int o = 0; o < 2; o++) {
        int r = o == 0 ? p - 1 : q - 1, c = o == 0 ? q - 1 : p - 1;
        for (int k = 0; k < 4; k++) { v.push_back(CN.index(f, r, c)); const int nr = c, nc = M - r; r = nr, c = nc; }
      }
    return v;
  };
  std::vector<std::pair<int, int>> pq;
  for (int p = 2; p <= N / 2; p++)
    for (int q = 2; q <= N / 2; q++) {
      if (p == q) continue;
      if (N % 2 && (p == (N + 1) / 2 || q == (N + 1) / 2)) continue;
      pq.push_back({p, q});
    }
  printf("%zu sequences, %d x %d x %d, %zu depth pairs (p, q)\n", seqs.size(), N, N, N, pq.size());
  fflush(stdout);
  std::atomic<long> pairBad{0}, colourBad{0}, checks{0};
  std::atomic<size_t> ni{0};
  auto w = [&] {
    std::vector<int> a, tmp, a8, t8;
    for (size_t si; (si = ni++) < seqs.size();) {
      const auto &mv = seqs[si];
      // reference
      a8.resize(C8.pos.size());
      for (size_t i = 0; i < a8.size(); i++) a8[i] = i;
      for (auto &m : mv) C8.apply(a8, m, t8);
      int ref[48];  // ref[k]: pair index whose sticker is now at pair index k
      for (int k = 0; k < 48; k++) ref[k] = f8[a8[pf8[k]]];
      for (auto [p, q] : pq) {
        const auto pfN = pairN(p, q);
        std::vector<int> fN(CN.pos.size(), -1);
        for (int k = 0; k < 48; k++) fN[pfN[k]] = k;
        a.resize(CN.pos.size());
        for (size_t i = 0; i < a.size(); i++) a[i] = i;
        for (auto &m : mv) CN.apply(a, cc::Mv{m.face, m.depth == 1 ? 1 : m.depth == 2 ? p : q, m.amt}, tmp);
        bool pairOk = true;
        for (int k = 0; k < 48 && pairOk; k++) pairOk = fN[a[pfN[k]]] == ref[k];
        bool colOk = true;
        for (int f = 0; f < 6 && colOk; f++)
          for (int r = 1; r < M && colOk; r++)
            for (int c = 1; c < M && colOk; c++) {
              const int j = CN.index(f, r, c);
              if (fN[j] >= 0 || r == c || r + c == M) continue;
              colOk = CN.faceOf[a[j]] == f;
            }
        checks++;
        if (!pairOk) pairBad++;
        if (!colOk) colourBad++;
      }
    }
  };
  std::vector<std::thread> th;
  const int nt = std::max(1u, std::thread::hardware_concurrency());
  for (int t = 0; t < nt; t++) th.emplace_back(w);
  for (auto &t : th) t.join();
  printf("%ld (sequence, p, q) checks: %ld act differently on their pair, %ld move another centre (not a diagonal) off its colour\n", checks.load(), pairBad.load(), colourBad.load());
  printf("%s\n", pairBad || colourBad ? "FAIL" : "PASS");
  return pairBad || colourBad ? 1 : 0;
}
