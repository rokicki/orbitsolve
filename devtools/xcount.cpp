// Distinct keep-solved rejection masks per pool: the face-leave masks X[a]
// (TableBeam) and the moved-position masks (strictKeep; for wing pools the
// wing beam's suppMask).  How much would a (mask, algorithm-list) grouping
// shrink the per-node rejection work?
//
//   xcount pair|diag|mid|wing FILE...     (.cls, or an expanded pool file)
#include "solvers.h"
#include "classpool.h"
#include <map>
#include <string>
#include <array>

template <class M> static void report(const char *what, size_t n, const M &xs, int nw) {
  std::map<int, int> bysize;
  for (auto &kv : xs) bysize[__builtin_popcountll(kv.first)]++;
  size_t big = 0;  // algorithms in masks shared by >= 64 algorithms
  for (auto &kv : xs) if (kv.second >= 64) big += kv.second;
  printf("  %-12s %9zu distinct (%.1f algs/mask; %.1f%% of algs in masks with >= 64 algs; bitset words %zu)\n",
         what, xs.size(), (double)n / xs.size(), 100.0 * big / n, (n + 63) / 64);
  printf("    by pieces in mask:");
  for (auto &kv : bysize) printf(" %d:%d", kv.first, kv.second);
  printf("\n");
  (void)nw;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: xcount pair|diag|mid|wing FILE...\n");
    return 1;
  }
  std::string kind = argv[1];
  if (kind != "pair" && kind != "diag" && kind != "mid" && kind != "wing") { fprintf(stderr, "unknown kind %s\n", argv[1]); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  for (int f = 2; f < argc; f++) {
    const char *fn = argv[f];
    const bool cls = std::string(fn).size() > 4 && std::string(fn).substr(std::string(fn).size() - 4) == ".cls";
    std::map<uint64_t, int> face, supp;
    size_t n = 0;
    if (kind == "wing") {
      WingPool wp;
      if (!cls) { fprintf(stderr, "wing pools: .cls only\n"); return 1; }
      ClassPool cp;
      if (!cp.load(fn)) { fprintf(stderr, "cannot read %s\n", fn); return 1; }
      OrbitModel m = wingModel();
      loadWingPoolFromClasses(wp, cp, m, getenv("MAXCOST") ? atoi(getenv("MAXCOST")) : 99);
      n = wp.size();
      for (size_t a = 0; a < n; a++) supp[wp.suppMask[a]]++;
      printf("%s: %zu algorithms\n", fn, n);
      report("moved-slot", n, supp, 0);
      continue;
    }
    Pool p;
    if (cls) {
      ClassPool cp;
      if (!cp.load(fn)) { fprintf(stderr, "cannot read %s\n", fn); return 1; }
      OrbitModel m = kind == "pair" ? pairModel() : kind == "diag" ? diagModel() : midModel();
      poolFromClasses(p, cp, m);
    } else p.load(fn, 99, false);
    n = p.size();
    for (size_t a = 0; a < n; a++) {
      uint64_t X = 0, S = 0;
      for (int d = 0; d < 48; d++) {
        int sp = p.src[a][d];
        if ((sp % 24) / 4 != (d % 24) / 4) X |= 1ULL << sp;
        if (sp != d) S |= 1ULL << sp;
      }
      face[X]++;
      supp[S]++;
    }
    printf("%s: %zu algorithms\n", fn, n);
    {  // inverse pairing: does inv(a) share a's face-leave / moved-position mask?
      std::map<std::array<uint8_t, 48>, size_t> idx;
      for (size_t a = 0; a < n; a++) idx.emplace(p.src[a], a);
      auto maskX = [&](size_t a, bool face) {
        uint64_t X = 0;
        for (int d = 0; d < 48; d++) {
          int sp = p.src[a][d];
          if (face ? (sp % 24) / 4 != (d % 24) / 4 : sp != d) X |= 1ULL << sp;
        }
        return X;
      };
      size_t noInv = 0, invol = 0, sameF = 0, sameS = 0, extraBits = 0;
      for (size_t a = 0; a < n; a++) {
        std::array<uint8_t, 48> inv;
        for (int d = 0; d < 48; d++) inv[p.src[a][d]] = d;
        auto it = idx.find(inv);
        if (it == idx.end()) { noInv++; continue; }
        if (inv == p.src[a]) invol++;
        uint64_t xa = maskX(a, true), xb = maskX(it->second, true);
        sameF += xa == xb;
        sameS += maskX(a, false) == maskX(it->second, false);
        extraBits += __builtin_popcountll((xa | xb) & ~xa);
      }
      printf("  inverses: %zu missing, %zu involutions (same text perm), face-leave mask same as inverse's: %.1f%%, moved-pos same: %.1f%%, "
             "union adds %.2f bits/alg to the face-leave mask\n",
             noInv, invol, 100.0 * sameF / n, 100.0 * sameS / n, (double)extraBits / n);
    }
    report("face-leave", n, face, 0);
    report("moved-pos", n, supp, 0);
  }
}
