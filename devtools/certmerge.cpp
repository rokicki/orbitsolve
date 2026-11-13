// certmerge: merge improved solving sequences into a stage certificate (see certcheck.cpp for
// the format).  Each improvement is a move sequence; its position is the one it solves (run
// backwards from the solved stage), placed in the segment of its wrongly placed count.  It
// replaces the line of the same symmetry class when it is shorter and keeps every centre outside
// the pair and the diagonals colour-solved on N = 6..10.  Only the segments that change are
// rewritten (via a temporary file), and their MANIFEST lines (DIR/MANIFEST two levels above the
// stage directory) are updated.  --keep u (stage 1u) relaxes the side condition to: every such
// centre on U stays U-coloured (only the U face sets kept); --keep ud (1ud, 2ud): U and D.
//
//   certmerge STAGEDIR IMPROVEMENTS.txt... [--dry] [--keep u|ud]
#include "certcube.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <climits>
#include <cstdlib>
#include <unordered_map>

typedef unsigned __int128 u128;
struct KH { size_t operator()(u128 k) const { uint64_t a = (uint64_t)k, b = (uint64_t)(k >> 64); return (a ^ (b * 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL; } };

static int groupMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  if (s.size() % 2) return -1;
  for (size_t i = 0; i < s.size(); i += 2) {
    const int o = s[i] == 'a' ? 0 : s[i] == 'b' ? 1 : -1;
    const char *p = strchr(cc::FACES, s[i + 1]);
    if (o < 0 || !p || !*p) return -1;
    m |= 1 << (o * 6 + (p - cc::FACES));
  }
  return m;
}
static std::vector<std::string> readLines(const std::string &f) {
  std::vector<std::string> v;
  FILE *in = fopen(f.c_str(), "r");
  if (!in) return v;
  char buf[4096];
  while (fgets(buf, sizeof buf, in)) {
    std::string l(buf);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    if (!l.empty()) v.push_back(l);
  }
  fclose(in);
  return v;
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  bool dry = false;
  int keep = 0;  // 0 every other centre keeps its colour, 1 only the U face set, 2 U and D face sets
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--dry") dry = true;
    else if (o == "--keep" && i + 1 < argc) {
      std::string k = argv[++i];
      if (k == "u") keep = 1;
      else if (k == "ud") keep = 2;
      else { fprintf(stderr, "--keep takes u or ud\n"); return 1; }
    }
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() < 2) { fprintf(stderr, "usage: certmerge STAGEDIR IMPROVEMENTS.txt... [--dry] [--keep u|ud]\n"); return 1; }
  char rp[PATH_MAX];
  if (!realpath(pos[0].c_str(), rp)) { fprintf(stderr, "no such stage directory\n"); return 1; }
  std::string dir = rp;  // absolute, so the MANIFEST two levels up is found from any working directory
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const std::string sname = dir.substr(dir.rfind('/') == std::string::npos ? 0 : dir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return 1; }
  const int S = groupMask(sname.substr(0, dd)), T = groupMask(sname.substr(dd + 2));
  if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name\n"); return 1; }
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          mp[f][d][a][k] = fidx[i];
        }
  auto groupOf = [](int k) { return (k % 8) / 4 * 6 + k / 8; };
  std::vector<int> tg;
  for (int g = 0; g < 12; g++) if (T >> g & 1) tg.push_back(g);
  std::vector<int> freeK;
  for (int k = 0; k < 48; k++) if (!(S >> groupOf(k) & 1)) freeK.push_back(k);
  const int nf = freeK.size(), base = tg.size() + 1;
  auto keyOf = [&](const int8_t *col) {
    u128 k = 0;
    for (int i = 0; i < nf; i++) { const int g = col[freeK[i]]; k = k * base + (g < 0 ? 0 : std::find(tg.begin(), tg.end(), g) - tg.begin() + 1); }
    return k;
  };
  // symmetries keeping the stage (identity included)
  struct Sym { int fac[48]; int gmap[12]; };
  std::vector<Sym> syms;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        for (int k = 0; k < 48; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (groupOf(k) == g) y.gmap[g] = groupOf(y.fac[k]);
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << y.gmap[g]; if (T >> g & 1) t2 |= 1 << y.gmap[g]; }
        if (s2 == S && t2 == T) syms.push_back(y);
      }
  }
  int8_t solvedCol[48];
  for (int k = 0; k < 48; k++) solvedCol[k] = ((S | T) >> groupOf(k) & 1) ? groupOf(k) : -1;
  // a sequence -> (class key, wrongly placed); false if it has a bad move
  auto classOf = [&](const std::string &l, std::vector<cc::Mv> &mv, u128 &cls, int &w) {
    if (!cc::parseMoves(l, mv)) return false;
    for (auto &m : mv) if (m.depth < 1 || m.depth > 3) return false;
    int8_t a[48], b[48];
    memcpy(a, solvedCol, 48);
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 48);
    }
    w = 0;
    for (int k = 0; k < 48; k++) if (a[k] >= 0 && (T >> a[k] & 1) && a[k] != groupOf(k)) w++;
    cls = ~(u128)0;
    for (auto &y : syms) {
      for (int k = 0; k < 48; k++) b[y.fac[k]] = a[k] < 0 ? -1 : y.gmap[a[k]];
      cls = std::min(cls, keyOf(b));
    }
    return true;
  };
  // side effects: other centres keep their colours on N = 6..10
  std::vector<cc::Cube> cubes;
  for (int N = 6; N <= 10; N++) cubes.emplace_back(N);
  auto sideOk = [&](const std::vector<cc::Mv> &mv) {
    for (auto &CN : cubes) {
      const int N = CN.N, M = N - 1;
      std::vector<int> a(CN.pos.size()), tmp;
      for (size_t i = 0; i < a.size(); i++) a[i] = i;
      for (auto &m : mv) CN.apply(a, m, tmp);
      for (int f = 0; f < 6; f++)
        for (int r = 1; r < M; r++)
          for (int c = 1; c < M; c++) {
            bool pair = false;
            for (int o = 0; o < 2 && !pair; o++) {
              int x = o ? 2 : 1, y = o ? 1 : 2;
              for (int q = 0; q < 4; q++) { if (r == x && c == y) pair = true; const int nx = y, ny = M - x; x = nx, y = ny; }
            }
            if (pair || r == c || r + c == M) continue;
            const int j = CN.index(f, r, c);
            const int fa = CN.faceOf[a[j]];
            if (keep == 0) { if (fa != f) return false; }
            else if ((fa == 0) != (f == 0) || (keep == 2 && (fa == 5) != (f == 5))) return false;
          }
    }
    return true;
  };
  // improvements, best per class
  struct Imp { std::string line; int len, w; };
  std::unordered_map<u128, Imp, KH> imps;
  long bad = 0, side = 0, total = 0;
  for (size_t i = 1; i < pos.size(); i++)
    for (auto &l : readLines(pos[i])) {
      total++;
      std::vector<cc::Mv> mv;
      u128 cls;
      int w;
      if (!classOf(l, mv, cls, w)) { bad++; continue; }
      if (!sideOk(mv)) { side++; continue; }
      std::string norm;
      for (auto &m : mv) { if (!norm.empty()) norm += ' '; norm += cc::mvText(m); }
      auto it = imps.find(cls);
      if (it == imps.end() || (int)mv.size() < it->second.len) imps[cls] = {norm, (int)mv.size(), w};
    }
  printf("improvements: %ld lines, %ld bad, %ld disturb another centre, %zu classes\n", total, bad, side, imps.size());
  std::map<int, std::vector<std::pair<u128, const Imp *>>> byW;
  for (auto &kv : imps) byW[kv.second.w].push_back({kv.first, &kv.second});
  long replaced = 0, notShorter = 0, absent = 0;
  std::map<std::string, std::pair<long, int>> manifestUpd;
  for (auto &[w, list] : byW) {
    char fn[32];
    snprintf(fn, sizeof fn, "w%0*d.txt", tg.size() * 4 >= 10 ? 2 : 1, w);
    const std::string path = dir + "/" + fn;
    auto lines = readLines(path);
    std::unordered_map<u128, size_t, KH> at;
    for (size_t li = 0; li < lines.size(); li++) {
      std::vector<cc::Mv> mv;
      u128 cls;
      int w2;
      if (classOf(lines[li], mv, cls, w2)) at[cls] = li;
    }
    long here = 0;
    for (auto &[cls, im] : list) {
      auto it = at.find(cls);
      if (it == at.end()) { absent++; continue; }
      std::vector<cc::Mv> mv;
      cc::parseMoves(lines[it->second], mv);
      if (im->len >= (int)mv.size()) { notShorter++; continue; }
      lines[it->second] = im->line;
      here++;
    }
    replaced += here;
    int longest = 0;
    for (auto &l : lines) { std::vector<cc::Mv> mv; cc::parseMoves(l, mv); longest = std::max<int>(longest, mv.size()); }
    printf("  %s: %ld replaced; %zu lines, longest now %d\n", fn, here, lines.size(), longest);
    if (here && !dry) {
      const std::string tmp = path + ".tmp";
      FILE *f = fopen(tmp.c_str(), "w");
      if (!f) { fprintf(stderr, "cannot write a segment\n"); return 1; }
      for (auto &l : lines) fprintf(f, "%s\n", l.c_str());
      fclose(f);
      if (rename(tmp.c_str(), path.c_str())) { fprintf(stderr, "cannot replace a segment\n"); return 1; }
      manifestUpd["stages/" + sname + "/" + fn] = {(long)lines.size(), longest};
    }
  }
  printf("replaced %ld lines; %ld not shorter, %ld classes not in the certificate%s\n", replaced, notShorter, absent, dry ? " (dry run: nothing written)" : "");
  // MANIFEST
  if (!manifestUpd.empty()) {
    const std::string stagesDir = dir.substr(0, dir.rfind('/'));
    const std::string man = stagesDir.substr(0, stagesDir.rfind('/')) + "/MANIFEST";
    auto ml = readLines(man);
    for (auto &l : ml) {
      const std::string key = l.substr(0, l.find(' '));
      auto it = manifestUpd.find(key);
      if (it != manifestUpd.end()) l = key + " " + std::to_string(it->second.first) + " lines, longest " + std::to_string(it->second.second);
    }
    FILE *f = fopen((man + ".tmp").c_str(), "w");
    if (f) {
      for (auto &l : ml) fprintf(f, "%s\n", l.c_str());
      fclose(f);
      rename((man + ".tmp").c_str(), man.c_str());
    }
  }
  return 0;
}
