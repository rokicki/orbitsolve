// wingparity: for move sequences (certificate lines or any one-sequence-per-line file), the parity
// of the permutation each one makes on every wing orbit of an N^3 (wings as distinct pieces,
// certcube.h geometry).  Per input file: lines read, and per wing depth d how many lines are odd.
//
//   wingparity FILE... [--n N] [--map FROM:TO]...
//     --n N: cube size (default 8)
//     --map 3:4: play sequence depth 3 as depth 4 (e.g. the mid certificate's middle slice on 7^3)
#include "certcube.h"
#include <cstring>
#include <map>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int N = 8;
  std::map<int, int> dmap;
  std::vector<std::string> files;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--n" && i + 1 < argc) N = atoi(argv[++i]);
    else if (o == "--map" && i + 1 < argc) {
      int a, b;
      if (sscanf(argv[++i], "%d:%d", &a, &b) != 2) { fprintf(stderr, "--map wants FROM:TO\n"); return 1; }
      dmap[a] = b;
    } else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else files.push_back(o);
  }
  if (files.empty()) { fprintf(stderr, "usage: wingparity FILE... [--n N] [--map FROM:TO]...\n"); return 1; }
  cc::Cube C(N);
  const int nd = N / 2;  // wing depths 2..N/2 (odd N: the midges are not wings)
  std::vector<std::vector<int>> wf(nd + 1);
  std::vector<std::vector<int>> widx(nd + 1, std::vector<int>(C.pos.size(), -1));
  for (int d = 2; d <= nd; d++) {
    wf[d] = cc::wingFacelets(C, d);
    for (int k = 0; k < 24; k++) widx[d][wf[d][k]] = k;
  }
  for (auto &fn : files) {
    FILE *f = fopen(fn.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot open an input file\n"); return 1; }
    long lines = 0, bad = 0;
    std::map<int, long> odd;
    std::string firstOdd;
    char buf[8192];
    while (fgets(buf, sizeof buf, f)) {
      std::string s(buf, strcspn(buf, "\r\n"));
      if (s.empty() || s[0] == '#') continue;
      std::vector<cc::Mv> mv;
      if (!cc::parseMoves(s, mv)) { bad++; continue; }
      bool ok = true;
      for (auto &m : mv) {
        if (dmap.count(m.depth)) m.depth = dmap[m.depth];
        ok &= m.depth >= 1 && m.depth <= (N + 1) / 2;
      }
      if (!ok) { bad++; continue; }
      lines++;
      std::vector<int> a(C.pos.size()), tmp;
      for (size_t i = 0; i < a.size(); i++) a[i] = i;
      for (auto &m : mv) C.apply(a, m, tmp);
      bool anyOdd = false;
      for (int d = 2; d <= nd; d++) {
        int p[24];
        for (int k = 0; k < 24; k++) {
          const int src = widx[d][a[wf[d][k]]];
          if (src < 0) { fprintf(stderr, "a wing left its orbit\n"); return 1; }
          p[k] = src;
        }
        bool seen[24] = {false};
        int par = 0;
        for (int k = 0; k < 24; k++) {
          if (seen[k]) continue;
          int len = 0;
          for (int x = k; !seen[x]; x = p[x]) seen[x] = true, len++;
          par ^= (len + 1) & 1;
        }
        if (par) odd[d]++, anyOdd = true;
      }
      if (anyOdd && firstOdd.empty()) firstOdd = s;
    }
    fclose(f);
    printf("%s: %ld lines (%ld unreadable); odd on wing depth", fn.c_str(), lines, bad);
    for (int d = 2; d <= nd; d++) printf(" %d:%ld", d, odd[d]);
    printf("\n");
    if (!firstOdd.empty()) printf("  first odd: %s\n", firstOdd.c_str());
  }
  return 0;
}
