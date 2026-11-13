// Benchmark the wing table beam on random single wing orbits, against the
// current wing solver (WingBeam), and check every solution on a real cube.
//
//   wingbench POOL [-k states] [-b width] [--seed s] [--mul q] [--nofinish] [--old] [-t threads]
#include "wingbeam.h"
#include "wings.h"
#include "solvers.h"
#include "../dist/gullicube/src/pair/seq.h"
#include <chrono>
#include <thread>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: wingbench POOL [options]   solve random wing orbits (4x4x4 layout)\n"
                    "  -k n        states (default 50)\n"
                    "  -b w        beam width (default 64)\n"
                    "  --seed s    state seed (default: entropy)\n"
                    "  --mul q     quarter-moves per wing in place (default 8)\n"
                    "  --maxcost c pool algorithms up to c moves (default all)\n"
                    "  --nofinish  no finish table\n"
                    "  --finpool f finish table from pool f (default: POOL itself)\n"
                    "  --keep k    reject algorithms moving >= k wings already in place (0: off)\n"
                    "  --old       also run the current WingBeam (width 4x... as given)\n"
                    "  -t n        threads (default all)\n");
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int K = 50, W = 64, mul = 8, maxCost = 99, threads = std::thread::hardware_concurrency();
  uint64_t seed = 0;
  bool finish = true, old = false;
  const char *finFile = nullptr;
  int keep = 2;
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-k") K = atoi(argv[++i]);
    else if (a == "-b") W = atoi(argv[++i]);
    else if (a == "--seed") seed = strtoull(argv[++i], 0, 10);
    else if (a == "--mul") mul = atoi(argv[++i]);
    else if (a == "--maxcost") maxCost = atoi(argv[++i]);
    else if (a == "--nofinish") finish = false;
    else if (a == "--finpool") finFile = argv[++i];
    else if (a == "--keep") keep = atoi(argv[++i]);
    else if (a == "--old") old = true;
    else if (a == "-t") threads = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  if (!seed) seed = std::random_device()() | 1;
  printf("# seed %llu\n", (unsigned long long)seed);
  WingPool pool;
  if (!pool.load(argv[1], maxCost)) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  WingPool finPool;
  if (finFile && !finPool.load(finFile)) { fprintf(stderr, "cannot read %s\n", finFile); return 1; }
  WingTableBeam B;
  B.build(pool, finFile ? &finPool : nullptr);
  B.mul = mul;
  B.useFinish = finish;
  B.keepSolved = keep;
  B.seed = seed;
  // random states: random permutations of the 24 wings (any parity: slices are in the pool)
  std::mt19937_64 rng(seed);
  std::vector<std::array<uint8_t, 24>> st(K);
  for (auto &s : st) {
    for (int j = 0; j < 24; j++) s[j] = j;
    std::shuffle(s.begin(), s.end(), rng);
  }
  std::vector<int> cost(K, -1), merged(K, -1), oldCost(K, -1);
  std::vector<uint8_t> verified(K, 0);
  auto t0 = std::chrono::steady_clock::now();
  std::atomic<int> nx{0};
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (int k; (k = nx++) < K;) {
        bool ok;
        auto sol = B.solve(st[k], W, ok);
        if (!ok) continue;
        int c = 0;
        std::vector<mv> moves;
        for (auto r : sol) {
          const WingPool &P = B.poolOf(r);
          uint32_t a = r & ~WingTableBeam::FIN;
          c += P.cost[a];
          for (auto &x : P.movesOf(a)) moves.push_back({(short)x[0], (short)x[1], (short)x[2]});
        }
        cost[k] = c;
        // real cube (4x4x4, wing orbit 0 at depth 2): set the orbit, replay, check
        xcube cube(1, 1, 'U');
        for (int j = 0; j < 24; j++) cube.wingorbits[0].a[j] = st[k][j];
        xcube c0 = cube;
        Merger mg(4);
        mg.add(moves, {});
        long raw, mcount;
        auto seq = mg.merge(raw, mcount);
        merged[k] = mcount;
        for (auto m : seq) domove(m, cube, true);
        bool good = true;
        for (int j = 0; j < 24; j++) good &= cube.wingorbits[0].a[j] == j;
        for (int j = 0; j < 24; j++) good &= cube.tc.corner.a[j] == c0.tc.corner.a[j];
        verified[k] = good;
      }
    });
  for (auto &t : th) t.join();
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  long sum = 0, msum = 0, nok = 0, nver = 0;
  for (int k = 0; k < K; k++)
    if (cost[k] >= 0) { sum += cost[k]; msum += merged[k]; nok++; nver += verified[k]; }
  printf("pool %zu algs, W %d, mul %d, finish %d: %ld/%d solved, %ld verified on a 4^3 cube; avg %.2f moves (%.2f merged); %.2fs\n",
         pool.size(), W, mul, (int)finish, nok, K, nver, (double)sum / std::max(1L, nok), (double)msum / std::max(1L, nok), el);
  if (old) {
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
    auto t1 = std::chrono::steady_clock::now();
    nx = 0;
    th.clear();
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        for (int k; (k = nx++) < K;) {
          std::vector<Perm24> s1(1);
          for (int j = 0; j < 24; j++) s1[0][j] = st[k][j];
          WingBeam OB{&G, 4, 1};
          OB.width = W;
          OB.fin = &F;
          OB.finishBelow = 1 << 20;
          auto mvs = OB.solve(s1);
          Merger mg(4);
          mg.add(mvs, {});
          long raw, mc;
          mg.merge(raw, mc);
          oldCost[k] = mvs.empty() ? -1 : mc;
        }
      });
    for (auto &t : th) t.join();
    double el2 = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
    long s2 = 0, n2 = 0;
    for (int k = 0; k < K; k++) if (oldCost[k] >= 0) { s2 += oldCost[k]; n2++; }
    printf("old WingBeam W %d: %ld/%d solved, avg %.2f moves merged; %.2fs\n", W, n2, K, (double)s2 / std::max(1L, n2), el2);
  }
}
