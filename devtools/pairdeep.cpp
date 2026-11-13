// How close is the pair beam to its own limit?  Solve K random pair states
// (two 24-piece oblique orbits) with the TableBeam at widths 1, g, g^2, ...
// up to a maximum, and print the cost per state at each width, the average,
// and the running best per state.  States are solved in parallel, one beam
// per thread.
//   pairdeep [-k K] [--seed s] [-b maxwidth] [--grow g] [-t threads] [--pool file] [--reps r]
#include "solvers.h"
#include "poolcache.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

static double now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fputs(
        "usage: pairdeep [options]   solve K random pair states at widths 1, g, g^2, ... up to -b\n"
        "  -k K            states (default 8)\n"
        "  --seed s        state seed (default 1)\n"
        "  -b w            maximum width (default 65536)\n"
        "  --grow g        width growth factor (default 4)\n"
        "  --reps r        runs per width with different table seeds (default 1)\n"
        "  -t T            threads (default: all cores)\n"
        "  --pool file     pair pool (default ../pair/p8s10.d.txt)\n",
        stderr);
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int K = 8, maxW = 65536, threads = std::thread::hardware_concurrency(), reps = 1;
  double grow = 4;
  uint64_t seed = 1;
  const char *poolFile = "../dist/gullicube/src/pair/p8s10.d.txt";
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-k") K = atoi(argv[++i]);
    else if (a == "--seed") seed = strtoull(argv[++i], 0, 10);
    else if (a == "-b") maxW = atoi(argv[++i]);
    else if (a == "--grow") grow = atof(argv[++i]);
    else if (a == "-t") threads = atoi(argv[++i]);
    else if (a == "--pool") poolFile = argv[++i];
    else if (a == "--reps") reps = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  Pool pool;
  loadPoolCached(pool, poolFile);
  pool.sortByCost();
  ColourSolver cs;
  cs.build(pool, nullptr, Weights());
  TableBeam beam;
  beam.build(cs);
  std::mt19937_64 rng(seed);
  std::vector<State> st;
  for (int k = 0; k < K; k++) st.push_back(randomState(rng));
  std::vector<int> best(K, 1 << 30);
  printf("pool %s, %d states, seed %llu\n", poolFile, K, (unsigned long long)seed);
  uint64_t run = seed * 1000003;
  for (long w = 1; w <= maxW; w = std::max(w + 1, (long)std::lround(w * grow))) {
    for (int r = 0; r < reps; r++) {
      std::vector<int> cost(K, -1);
      std::atomic<int> nx{0};
      double t0 = now();
      beam.slotSeed = run++;
      const TableBeam &b = beam;
      std::vector<std::thread> th;
      for (int t = 0; t < std::min(threads, K); t++)
        th.emplace_back([&] {
          for (int k; (k = nx++) < K;) {
            bool ok;
            auto sol = b.solve(st[k], ok, (int)w);
            if (!ok) continue;
            State s = st[k];
            int c = 0;
            for (auto &x : sol) { s = s.apply(pool, x.idx); c += pool.cost[x.idx]; }
            if (s.correct() == 48) cost[k] = c;
          }
        });
      for (auto &t : th) t.join();
      long sum = 0, bsum = 0;
      printf("w %7ld:", w);
      for (int k = 0; k < K; k++) {
        printf(" %3d", cost[k]);
        if (cost[k] >= 0) best[k] = std::min(best[k], cost[k]);
        sum += cost[k];
        bsum += best[k];
      }
      printf("  avg %.2f  best-avg %.2f  %.1fs\n", (double)sum / K, (double)bsum / K, now() - t0);
      fflush(stdout);
    }
  }
}
