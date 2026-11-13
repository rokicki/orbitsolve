// posstat: where the 8 target pieces of the 2ud stage (orbits a and b, D face) sit after each
// scramble of a twsearch .scr file (ScrambleAlg NAME / moves / End), tallied by a shape key:
// per orbit, the number of pieces on D and on each of the four side faces (sorted), and the
// number of side faces holding both an a and a b piece.  For comparing hard and easy classes.
//   posstat FILE.scr
//   posstat --lines STAGEDIR   (every class of a stage: the inverse of each certificate line in its
//                               w*.txt; prints the {on D, on D} pair counts by line length)
//   posstat --lines STAGEDIR LO HI   (also prints the certificate lines of the shortest classes
//                                     with pattern {LO,HI})
//   posstat --lines STAGEDIR LO HI MAXLEN   (instead, every such line of at most MAXLEN moves)
#include "certcube.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>
int main(int argc, char **argv) {
  const bool lines = (argc == 3 || argc == 5 || argc == 6) && std::string(argv[1]) == "--lines";
  const size_t maxLen = argc == 6 ? atoi(argv[5]) : 0;
  const int pLo = argc >= 5 ? atoi(argv[3]) : -1, pHi = argc >= 5 ? atoi(argv[4]) : -1;
  std::vector<std::string> shortest;
  size_t shortLen = 999;
  if (argc != 2 && !lines) { fprintf(stderr, "usage: posstat FILE.scr | posstat --lines STAGEDIR\n"); return 1; }
  printf("# posstat %s%s\n", argv[1], lines ? (std::string(" ") + argv[2]).c_str() : "");
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++) for (int d = 1; d <= 3; d++) for (int a = 1; a <= 3; a++)
    for (int k = 0; k < 48; k++) { int i = pf[k]; for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i]; mp[f][d][a][k] = fidx[i]; }
  if (lines) {
    // [length][min on D][max on D]
    static long t[64][5][5];
    std::vector<cc::Mv> mv;
    char buf[4096];
    for (int w = 0; w < 64; w++) {
      char fn[32];
      snprintf(fn, sizeof fn, "/w%02d.txt", w);
      FILE *in = fopen((std::string(argv[2]) + fn).c_str(), "r");
      if (!in) { snprintf(fn, sizeof fn, "/w%d.txt", w); in = fopen((std::string(argv[2]) + fn).c_str(), "r"); }
      if (!in) continue;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line\n"); return 1; }
        int on[2] = {};
        for (int k = 40; k < 48; k++) {  // pieces starting on D, played through the inverse line
          int x = k;
          for (auto it = mv.rbegin(); it != mv.rend(); ++it) x = mp[it->face][it->depth][4 - it->amt][x];
          if (x / 8 == 5) on[(k % 8) / 4]++;
        }
        t[mv.size()][std::min(on[0], on[1])][std::max(on[0], on[1])]++;
        if (std::min(on[0], on[1]) == pLo && std::max(on[0], on[1]) == pHi && (maxLen ? mv.size() <= maxLen : mv.size() <= shortLen)) {
          if (!maxLen && mv.size() < shortLen) shortest.clear(), shortLen = mv.size();
          shortest.push_back(l);
        }
      }
      fclose(in);
    }
    printf("length  {lo,hi} pieces home per orbit: classes\n");
    for (int w = 0; w < 64; w++) {
      long tot = 0;
      for (int a = 0; a < 5; a++) for (int b = 0; b < 5; b++) tot += t[w][a][b];
      if (!tot) continue;
      printf("%2d %9ld:", w, tot);
      for (int a = 0; a < 5; a++) for (int b = a; b < 5; b++) if (t[w][a][b]) printf("  {%d,%d} %ld", a, b, t[w][a][b]);
      printf("\n");
    }
    for (auto &l : shortest) printf("shortest {%d,%d}: %s\n", pLo, pHi, l.c_str());
    return 0;
  }
  FILE *in = fopen(argv[1], "r");
  if (!in) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
  std::map<std::string, long> tally;
  long n = 0, inD[2][5] = {};
  char buf[4096];
  bool want = false;
  std::vector<cc::Mv> mv;
  while (fgets(buf, sizeof buf, in)) {
    std::string l(buf);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    if (l.rfind("ScrambleAlg", 0) == 0) { want = true; continue; }
    if (!want) continue;
    want = false;
    if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line\n"); return 1; }
    int cnt[2][6] = {};
    for (int k = 0; k < 48; k++) {
      if (k / 8 != 5) continue;  // pieces starting on D
      int x = k;
      for (auto &m : mv) x = mp[m.face][m.depth][m.amt][x];
      cnt[(k % 8) / 4][x / 8]++;
    }
    std::string key;
    int both = 0;
    for (int f = 1; f <= 4; f++) both += cnt[0][f] && cnt[1][f];
    for (int o = 0; o < 2; o++) {
      std::vector<int> s(cnt[o] + 1, cnt[o] + 5);
      std::sort(s.rbegin(), s.rend());
      char t[64];
      snprintf(t, sizeof t, "%c: D%d U%d sides %d%d%d%d  ", "ab"[o], cnt[o][5], cnt[o][0], s[0], s[1], s[2], s[3]);
      key += t;
      inD[o][cnt[o][5]]++;
    }
    key += "shared sides " + std::to_string(both);
    tally[key]++, n++;
  }
  printf("%ld scrambles\n", n);
  for (int o = 0; o < 2; o++) { printf("%c pieces on D:", "ab"[o]); for (int j = 0; j <= 4; j++) printf(" %d:%ld", j, inD[o][j]); printf("\n"); }
  std::vector<std::pair<long, std::string>> v;
  for (auto &e : tally) v.push_back({e.second, e.first});
  std::sort(v.rbegin(), v.rend());
  for (size_t i = 0; i < v.size() && i < 25; i++) printf("%7ld %5.2f%%  %s\n", v[i].first, 100.0 * v[i].first / n, v[i].second.c_str());
}
