// nbrcount: for each class of a pair stage whose certificate line has L moves, the number of its
// one-move neighbours (legal one-move actions: outer face turns, and with --slices the inner U and
// D layers 2U 3U 2D 3D) that are also classes at L.  A class whose line drops to L-2 brings every
// such neighbour down to L-1, so the classes with the most neighbours are the ones to solve first.
// Classes are compared by their canonical colouring (the smallest image under the stage
// symmetries, as certextend).  stdout: "COUNT LINE", sorted by decreasing COUNT, then a tally.
//   nbrcount STAGEDIR L [--slices]
#include "certcube.h"
#include <algorithm>
#include <map>
#include <unordered_map>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  bool slices = false;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--slices") slices = true;
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 2) { fprintf(stderr, "usage: nbrcount STAGEDIR L [--slices]\n"); return 1; }
  std::string dir = pos[0];
  const size_t L = atoi(pos[1].c_str());
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const std::string sname = dir.substr(dir.rfind('/') == std::string::npos ? 0 : dir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  auto groupMask = [](const std::string &s) {
    int m = 0;
    if (s.size() % 2) return -1;
    for (size_t i = 0; i < s.size(); i += 2) {
      const int o = s[i] == 'a' ? 0 : s[i] == 'b' ? 1 : -1;
      const char *p = strchr(cc::FACES, s[i + 1]);
      if (o < 0 || !p || !*p) return -1;
      m |= 1 << (o * 6 + (p - cc::FACES));
    }
    return m;
  };
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return 1; }
  const int S = groupMask(sname.substr(0, dd)), TG = groupMask(sname.substr(dd + 2));
  if (S < 0 || TG <= 0 || (S & TG)) { fprintf(stderr, "bad stage name\n"); return 1; }

  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++) for (int d = 1; d <= 3; d++) for (int a = 1; a <= 3; a++)
    for (int k = 0; k < 48; k++) { int i = pf[k]; for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i]; mp[f][d][a][k] = fidx[i]; }
  auto groupOf = [](int k) { return (k % 8) / 4 * 6 + k / 8; };
  std::vector<int> tg;
  for (int g = 0; g < 12; g++) if (TG >> g & 1) tg.push_back(g);
  int8_t cidx[12];
  for (int g = 0; g < 12; g++) cidx[g] = (TG >> g & 1) ? std::find(tg.begin(), tg.end(), g) - tg.begin() + 1 : 0;
  int8_t solvedCi[48];
  for (int k = 0; k < 48; k++) solvedCi[k] = cidx[groupOf(k)];
  // the stage symmetries (as certextend)
  struct Sym { int fac[48]; int8_t cmap[13]; };
  std::vector<Sym> ss;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        int gmap[12];
        for (int k = 0; k < 48; k++) {
          std::array<int, 3> p = C8.pos[pf[k]], q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (groupOf(k) == g) gmap[g] = groupOf(y.fac[k]);
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << gmap[g]; if (TG >> g & 1) t2 |= 1 << gmap[g]; }
        if (s2 != S || t2 != TG) continue;
        y.cmap[0] = 0;
        for (size_t i = 0; i < tg.size(); i++) y.cmap[i + 1] = cidx[gmap[tg[i]]];
        ss.push_back(y);
      }
  }
  auto canon = [&](const int8_t *a) {
    std::string best;
    char b[48];
    for (auto &y : ss) {
      for (int k = 0; k < 48; k++) b[y.fac[k]] = y.cmap[a[k]];
      std::string s(b, 48);
      if (best.empty() || s < best) best = s;
    }
    return best;
  };
  auto positionOf = [&](const std::vector<cc::Mv> &mv, int8_t *a) {
    int8_t b[48];
    memcpy(a, solvedCi, 48);
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 48);
    }
  };
  std::vector<cc::Mv> acts;
  for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) acts.push_back({f, 1, a});
  if (slices) for (int f : {0, 5}) for (int d = 2; d <= 3; d++) for (int a = 1; a <= 3; a++) acts.push_back({f, d, a});

  std::vector<std::string> lines;
  std::vector<std::vector<int8_t>> P;
  std::unordered_map<std::string, int> id;
  char buf[4096];
  std::vector<cc::Mv> mv;
  for (int w = 0; w < 100; w++) {
    FILE *in = nullptr;
    for (const char *fmt : {"/w%d.txt", "/w%02d.txt"}) {
      char fn[32];
      snprintf(fn, sizeof fn, fmt, w);
      if ((in = fopen((dir + fn).c_str(), "r"))) break;
    }
    if (!in) continue;
    while (fgets(buf, sizeof buf, in)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line\n"); return 1; }
      if (mv.size() != L) continue;
      std::vector<int8_t> a(48);
      positionOf(mv, a.data());
      if (id.emplace(canon(a.data()), lines.size()).second) lines.push_back(l), P.push_back(a);
    }
    fclose(in);
  }
  fprintf(stderr, "%zu classes at %zu, %zu symmetries, %zu actions\n", lines.size(), L, ss.size(), acts.size());
  std::vector<std::pair<int, int>> cnt;
  std::map<int, long> tally;
  long edges = 0;
  for (size_t i = 0; i < lines.size(); i++) {
    std::vector<int> nb;
    int8_t b[48];
    for (auto &m : acts) {
      for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = P[i][k];
      auto it = id.find(canon(b));
      if (it != id.end() && it->second != (int)i) nb.push_back(it->second);
    }
    std::sort(nb.begin(), nb.end());
    nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
    cnt.push_back({(int)nb.size(), (int)i});
    tally[nb.size()]++;
    edges += nb.size();
  }
  std::stable_sort(cnt.begin(), cnt.end(), [](auto &x, auto &y) { return x.first > y.first; });
  for (auto &c : cnt) printf("%d %s\n", c.first, lines[c.second].c_str());
  printf("# neighbours: classes");
  for (auto &t : tally) printf("  %d:%ld", t.first, t.second);
  printf("  (%ld neighbour pairs)\n", edges / 2);
}
