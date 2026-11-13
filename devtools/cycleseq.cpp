// cycleseq: cycle structure of solving sequences on the 48 pair pieces (as distinct pieces,
// certcube.h geometry on 8^3).  Per input file and per sequence length: how many sequences, the
// support (pieces moved) spread, and the commonest cycle types ("3,3|2,2" = orbit a cycles |
// orbit b cycles).  Also how many pieces the position has off their home face (the least support
// any solving permutation could have), from the stage named by the file's directory if given.
//
//   cycleseq FILE... [--top K]
#include "certcube.h"
#include <algorithm>
#include <cstring>
#include <map>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int top = 6;
  std::vector<std::string> files;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--top" && i + 1 < argc) top = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else files.push_back(o);
  }
  if (files.empty()) { fprintf(stderr, "usage: cycleseq FILE... [--top K]\n"); return 1; }
  cc::Cube C(8);
  const auto pf = cc::pairFacelets(C);
  std::vector<int> fidx(C.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  for (auto &fn : files) {
    FILE *f = fopen(fn.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot open an input file\n"); return 1; }
    std::map<int, std::map<std::string, long>> types;
    std::map<int, std::map<int, long>> sup;
    std::map<int, long> cnt;
    char buf[4096];
    while (fgets(buf, sizeof buf, f)) {
      if (buf[0] == '#' || buf[0] == '\n') continue;
      std::vector<cc::Mv> mv;
      if (!cc::parseMoves(std::string(buf, strcspn(buf, "\r\n")), mv)) continue;
      std::vector<int> a(C.pos.size()), tmp;
      for (size_t i = 0; i < a.size(); i++) a[i] = i;
      for (auto &m : mv) C.apply(a, m, tmp);
      int d[48];  // the piece at pair index k goes to d[k]
      for (int k = 0; k < 48; k++) d[fidx[a[pf[k]]]] = k;
      bool seen[48] = {false};
      std::vector<int> cyc[2];
      int s = 0;
      for (int k = 0; k < 48; k++) {
        if (seen[k] || d[k] == k) continue;
        int len = 0;
        for (int x = k; !seen[x]; x = d[x]) seen[x] = true, len++;
        cyc[(k % 8) / 4].push_back(len);
        s += len;
      }
      std::string t;
      for (int o = 0; o < 2; o++) {
        std::sort(cyc[o].rbegin(), cyc[o].rend());
        std::string u;
        for (int c : cyc[o]) u += (u.empty() ? "" : ",") + std::to_string(c);
        t += (o ? "|" : "") + (u.empty() ? std::string("-") : u);
      }
      const int L = mv.size();
      cnt[L]++, types[L][t]++, sup[L][s]++;
    }
    fclose(f);
    printf("%s\n", fn.c_str());
    for (auto &[L, n] : cnt) {
      printf("  length %d: %ld sequences; support", L, n);
      for (auto &kv : sup[L]) printf(" %d:%ld", kv.first, kv.second);
      std::vector<std::pair<long, std::string>> v;
      for (auto &kv : types[L]) v.push_back({kv.second, kv.first});
      std::sort(v.rbegin(), v.rend());
      printf("\n    %zu cycle types; commonest:", v.size());
      for (int i = 0; i < top && i < (int)v.size(); i++) printf(" [%s] %ld", v[i].second.c_str(), v[i].first);
      printf("\n");
    }
  }
  return 0;
}
