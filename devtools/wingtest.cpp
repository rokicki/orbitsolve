// Test the wings-first beam on random wing states: apply the emitted moves to a
// brobdicube xcube and check wings solved, corners/middle edges untouched.
#include "wings.h"
#include "solvers.h"
#include <chrono>
int main(int argc, char **argv) {
  int N = atoi(argv[1]), W = argc > 2 ? atoi(argv[2]) : 16, trials = argc > 3 ? atoi(argv[3]) : 4;
  double lam = argc > 4 ? atof(argv[4]) : 2.0;
  int finishBelow = argc > 5 ? atoi(argv[5]) : 12, maxStack = argc > 6 ? atoi(argv[6]) : 2;
  int chunk = argc > 7 ? atoi(argv[7]) : 0, threads = argc > 8 ? atoi(argv[8]) : 1;
  bool merge = argc > 9 ? atoi(argv[9]) : 1;
  int My = (N - 2) / 2, Mx = N - 2 - My;
  WingGens G;
  G.build();
  WingFinisher F;
  {
    Pool wp;
    wp.load("wing.pool", 99, false);
    std::vector<std::vector<std::array<int, 3>>> seqs;
    for (size_t a = 0; a < wp.size(); a++) {
      std::vector<std::array<int, 3>> q;
      for (auto &m : poolMoves(wp, a)) q.push_back({m.depth > 1 ? 1 : 0, m.face, m.amt});
      seqs.push_back(q);
    }
    F.buildFromPool(wp, seqs);
  }
  rng.seed(7);
  double tot = 0, tt = 0;
  for (int tr = 0; tr < trials; tr++) {
    xcube c(Mx, My, 'R');
    xcube c0 = c;
    std::vector<Perm24> st(My);
    for (int k = 0; k < My; k++) for (int j = 0; j < 24; j++) st[k][j] = c.wingorbits[k].a[j];
    WingBeam B{&G, N, My};
    B.width = W;
    B.lambda = lam;
    B.fin = &F;
    B.finishBelow = finishBelow;
    B.maxStack = maxStack;
    auto t0 = std::chrono::steady_clock::now();
    auto mvs = chunk ? solveWingsChunked(G, F, N, My, st, W, chunk, threads, merge) : B.solve(st);
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    // merge same-axis runs for the real count
    Merger mg(N);
    mg.add(mvs, {});
    long raw, merged;
    auto seq = mg.merge(raw, merged);
    for (auto m : seq) domove(m, c, true);
    bool ok = !mvs.empty();
    for (int k = 0; k < My; k++) for (int j = 0; j < 24; j++) ok &= c.wingorbits[k].a[j] == j;
    for (int j = 0; j < 24; j++) ok &= c.tc.corner.a[j] == c0.tc.corner.a[j] && c.tc.edge.a[j] == c0.tc.edge.a[j];
    printf("N=%d trial %d: %s  %ld moves (%.1f per wing orbit)  %.2fs\n", N, tr, ok ? "ok" : "FAILED", merged,
           (double)merged / My, el);
    tot += merged;
    tt += el;
  }
  printf("chunk %d threads %d merge %d ", chunk, threads, (int)merge);
  printf("N=%d W=%d lambda=%.1f: mean %.1f moves, %.1f per wing orbit, %.2fs\n", N, W, lam, tot / trials, tot / trials / My,
         tt / trials);
}
