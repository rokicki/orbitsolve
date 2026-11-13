// How cheap is it to solve a chosen subset of positions, ignoring the rest?
//
//   probe [-k K] [-n STATES] [-r SUBSETS] [-w W] [-s SEED] pool...
//
// For each random state, draws random K-subsets T of its misplaced positions
// and finds (by a move-count-bucketed beam, no cancellation) a cheap product of
// pool algorithms after which every position in T is correct; other positions
// are free.  Prints one line per probe: state, cost, algs, and features of T.
#include "pool.h"
#include "subset.h"
#include <atomic>
#include <cmath>
#include <queue>
#include <thread>
#include <unordered_set>

struct Probe {
  int state, cost, algs;
  uint64_t T;
};

int main(int argc, char **argv) {
  int K = 10, NS = 4, R = 100, W = 8, threads = 16;
  uint64_t seed = 1;
  std::vector<const char *> files;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-k") K = atoi(argv[++i]);
    else if (a == "-n") NS = atoi(argv[++i]);
    else if (a == "-r") R = atoi(argv[++i]);
    else if (a == "-w") W = atoi(argv[++i]);
    else if (a == "-s") seed = atoll(argv[++i]);
    else if (a == "-t") threads = atoi(argv[++i]);
    else files.push_back(argv[i]);
  }
  Pool pool;
  for (auto f : files) pool.load(f, 99, false);
  pool.sortByCost();
  fprintf(stderr, "pool %zu\n", pool.size());
  std::mt19937_64 rng(seed);
  std::vector<State> states;
  std::vector<Probe> probes;
  for (int s = 0; s < NS; s++) {
    states.push_back(randomState(rng));
    std::vector<int> wrong;
    for (int p = 0; p < 48; p++)
      if (states[s].col[p] != target(p)) wrong.push_back(p);
    for (int r = 0; r < R; r++) {
      std::shuffle(wrong.begin(), wrong.end(), rng);
      uint64_t T = 0;
      for (int k = 0; k < K; k++) T |= 1ULL << wrong[k];
      probes.push_back({s, 0, 0, T});
    }
  }
  std::atomic<int> next{0};
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      std::vector<std::array<uint64_t, 6>> ST(pool.size());
      for (int i; (i = next++) < (int)probes.size();) {
        auto &pr = probes[i];
        for (size_t a = 0; a < pool.size(); a++) {
          std::array<uint64_t, 6> m{};
          for (int d = 0; d < 48; d++)
            if (pr.T >> d & 1) m[target(d)] |= 1ULL << pool.src[a][d];
          ST[a] = m;
        }
        auto r = solveSubset(pool, ST, states[pr.state], pr.T, W);
        pr.cost = r.first;
        pr.algs = r.second;
      }
    });
  for (auto &t : th) t.join();
  // features: pieces in orbit A, number of distinct (orbit,face) cells touched
  for (auto &pr : probes) {
    int inA = __builtin_popcountll(pr.T & 0xFFFFFF);
    int cells = 0, faces = 0;
    for (int o = 0; o < 2; o++)
      for (int f = 0; f < 6; f++) cells += (pr.T >> (o * 24 + f * 4) & 15) != 0;
    for (int f = 0; f < 6; f++) faces += ((pr.T >> (f * 4) & 15) | (pr.T >> (24 + f * 4) & 15)) != 0;
    printf("%d %d %d %d %d %d %012llx\n", pr.state, pr.cost, pr.algs, inA, cells, faces, (unsigned long long)pr.T);
  }
}
