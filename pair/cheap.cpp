// Gain statistics for cheap algorithms on random states, and a greedy
// "cheap-first" pass: repeatedly apply the best gain/cost algorithm of length
// <= MAXC while gain/cost >= R.
#include "pool.h"
int main(int argc, char **argv) {
  Pool pool;
  pool.load("p8.txt", 8, false);
  pool.sortByCost();
  std::mt19937_64 rng(7);
  int N = 200;
  // best gain by cost on the initial state
  double bestg[9] = {0};
  for (int i = 0; i < N; i++) {
    State s = randomState(rng);
    int c0 = s.correct(), bg[9] = {0};
    for (size_t a = 0; a < pool.size(); a++) bg[pool.cost[a]] = std::max(bg[pool.cost[a]], correctAfter(pool.S[a], s.M) - c0);
    for (int L = 4; L <= 8; L++) bestg[L] += bg[L];
  }
  printf("initial state, mean best net gain by alg length:");
  for (int L = 4; L <= 8; L++) printf("  %d:%.2f", L, bestg[L] / N);
  printf("\n");
  for (int maxc : {4, 5, 6, 8})
    for (double R : {1.0, 0.75, 0.5}) {
      std::mt19937_64 r2(7);
      double mv = 0, left = 0;
      for (int i = 0; i < N; i++) {
        State s = randomState(r2);
        int moves = 0;
        while (true) {
          int c0 = s.correct();
          double br = 0;
          size_t ba = 0;
          for (size_t a = 0; a < pool.size() && pool.cost[a] <= maxc; a++) {
            double q = (double)(correctAfter(pool.S[a], s.M) - c0) / pool.cost[a];
            if (q > br) { br = q; ba = a; }
          }
          if (br < R || c0 == 48) break;
          s = s.apply(pool, ba);
          moves += pool.cost[ba];
        }
        mv += moves;
        left += 48 - s.correct();
      }
      printf("greedy len<=%d ratio>=%.2f: %.1f moves, %.1f misplaced left (from ~40), %.2f moves/piece\n", maxc, R,
             mv / N, left / N, mv / N / (40.0 - left / N));
    }
}
