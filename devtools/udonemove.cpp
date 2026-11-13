// udonemove: for a 1ud/2ud certificate stage directory (<solved>..<target>/wNN.txt, as certcheck), count
// per length the classes one stage move away from a class with a line one shorter (2026-11-03): a solver
// need not store their lines (one move, then the shorter class's line, in its symmetry image).
// Stage moves: those that keep the solved groups and the --keep ud side condition: every outer face turn
// and the U/D-axis slice turns (2U 3U 2D 3D, all powers).  Cube, moves, keys and symmetries as certcheck.
//
//   udonemove DIR [--threads T]
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <map>
#include <thread>
#include <unordered_map>

typedef unsigned __int128 u128;
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
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

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = std::max(1u, std::thread::hardware_concurrency());
  std::string dir;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else if (dir.empty()) dir = o;
    else { fprintf(stderr, "one stage directory only\n"); return 1; }
  }
  if (dir.empty()) { fprintf(stderr, "usage: udonemove DIR [--threads T]\n"); return 1; }
  const double t0 = now();
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  std::vector<std::string> fs;
  if (DIR *d = opendir(dir.c_str())) {
    while (dirent *e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() > 5 && n[0] == 'w' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back(dir + "/" + n);
    }
    closedir(d);
  } else { fprintf(stderr, "not a directory\n"); return 1; }
  std::sort(fs.begin(), fs.end());
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
    for (int i = 0; i < nf; i++) {
      const int g = col[freeK[i]];
      k = k * base + (g < 0 ? 0 : std::find(tg.begin(), tg.end(), g) - tg.begin() + 1);
    }
    return k;
  };
  int8_t solvedCol[48];
  for (int k = 0; k < 48; k++) solvedCol[k] = ((S | T) >> groupOf(k) & 1) ? groupOf(k) : -1;
  // symmetries keeping the stage (as certcheck)
  struct Sym { int fac[48]; int gmap[12]; };
  std::vector<Sym> syms;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        bool ok = true;
        for (int k = 0; k < 48 && ok; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          int j = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) j = x;
          ok = j >= 0;
          y.fac[k] = j;
        }
        if (!ok) continue;
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (groupOf(k) == g) { const int h = groupOf(y.fac[k]); if (img >= 0 && img != h) ok = false; img = h; }
          y.gmap[g] = img;
        }
        if (!ok) continue;
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << y.gmap[g]; if (T >> g & 1) t2 |= 1 << y.gmap[g]; }
        if (s2 == S && t2 == T) syms.push_back(y);
      }
  }
  // stage moves: every outer face turn; U/D-axis slices
  std::vector<cc::Mv> moves;
  for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) moves.push_back({f, 1, a});
  for (int f : {0, 5}) for (int d : {2, 3}) for (int a = 1; a <= 3; a++) moves.push_back({f, d, a});
  // every line's position (its sequence run backwards from the solved stage) -> its length
  std::vector<std::pair<u128, uint8_t>> pos;
  std::vector<std::vector<cc::Mv>> lineMv;
  for (auto &f : fs) {
    FILE *in = fopen(f.c_str(), "r");
    if (!in) { fprintf(stderr, "cannot open a segment\n"); return 1; }
    char buf[4096];
    std::vector<cc::Mv> mv;
    int8_t a[48], b[48];
    while (fgets(buf, sizeof buf, in)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      mv.clear();
      if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line\n"); return 1; }
      memcpy(a, solvedCol, 48);
      for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
        const int inv = 4 - it->amt;
        for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][inv][k]] = a[k];
        memcpy(a, b, 48);
      }
      pos.push_back({keyOf(a), (uint8_t)mv.size()});
      lineMv.push_back(mv);
    }
    fclose(in);
  }
  std::unordered_map<u128, uint8_t, KH> have;
  have.reserve(pos.size() * 2);
  for (auto &p : pos) { auto r = have.emplace(p.first, p.second); if (!r.second && p.second < r.first->second) r.first->second = p.second; }
  fprintf(stderr, "%zu lines, %zu classes, %zu symmetries, %zu stage moves (%.0fs)\n", pos.size(), have.size(), syms.size(), moves.size(), now() - t0);
  // per class: is some stage move's result (in some symmetry image) listed with a line at most one shorter?
  std::vector<std::map<int, std::array<long, 3>>> oc(threads);  // classes, one-move, result not listed
  std::atomic<size_t> ni{0};
  auto lenOf = [&](const int8_t *col, int8_t *img) {
    auto it = have.find(keyOf(col));
    if (it != have.end()) return (int)it->second;
    for (auto &y : syms) {
      for (int x = 0; x < 48; x++) img[y.fac[x]] = col[x] < 0 ? -1 : y.gmap[col[x]];
      it = have.find(keyOf(img));
      if (it != have.end()) return (int)it->second;
    }
    return -1;
  };
  auto work = [&](int tid) {
    int8_t col[48], nx[48], img[48];
    for (size_t i; (i = ni.fetch_add(1)) < pos.size();) {
      const u128 key = pos[i].first;
      const int d = pos[i].second;
      if (have.at(key) != d) continue;  // a duplicate line of a class; count the class once
      u128 k = key;
      for (int x = 0; x < 48; x++) col[x] = (S >> groupOf(x) & 1) ? groupOf(x) : -1;
      for (int j = nf - 1; j >= 0; j--) { const int dg = k % base; k /= base; col[freeK[j]] = dg ? tg[dg - 1] : -1; }
      bool one = false, miss = false;
      for (auto &m : moves) {
        if (d == 0) break;
        for (int x = 0; x < 48; x++) nx[mp[m.face][m.depth][m.amt][x]] = col[x];
        const int e = lenOf(nx, img);
        if (e < 0) { miss = true; continue; }
        if (e + 1 <= d) { one = true; break; }
      }
      auto &c = oc[tid][d];
      c[0]++;
      if (one) c[1]++;
      if (miss) c[2]++;
    }
  };
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++) th.emplace_back(work, t);
  for (auto &x : th) x.join();
  std::map<int, std::array<long, 3>> o;
  for (auto &m : oc) for (auto &kv : m) for (int j = 0; j < 3; j++) o[kv.first][j] += kv.second[j];
  long all = 0, ones = 0, misses = 0;
  printf("%s classes one stage move from a shorter class (length: classes/one-move):", sname.c_str());
  for (auto &kv : o) { printf(" %d:%ld/%ld", kv.first, kv.second[0], kv.second[1]); all += kv.second[0]; ones += kv.second[1]; misses += kv.second[2]; }
  printf("\n%s total: %ld classes, %ld one move from shorter (%.1f%%), %ld need a stored line; %ld classes had a move to an unlisted position (%.0fs)\n",
         sname.c_str(), all, ones, 100.0 * ones / all, all - ones, misses, now() - t0);
  // prefixes: for each class that needs a line, the shortest prefix (2+ moves) of its line that ends at a
  // stage position (the solved groups solved; on N = 6..10 every centre outside the pair and the diagonals
  // that is on U or D still on its face, as certcheck --keep ud) listed with a line at most the rest of the line
  struct NC { std::vector<char> kind; std::vector<int> face; std::vector<std::vector<std::pair<int, int>>> mv; int n; };
  std::vector<NC> ncs;
  for (int N = 6; N <= 10; N++) {
    cc::Cube CN(N);
    const int M = N - 1;
    NC c;
    std::vector<int> cid(CN.pos.size(), -1);
    for (int f = 0; f < 6; f++)
      for (int r = 1; r < M; r++)
        for (int col = 1; col < M; col++) {
          bool pr = false;
          for (int o = 0; o < 2 && !pr; o++) {
            int x = o ? 2 : 1, yy = o ? 1 : 2;
            for (int q = 0; q < 4; q++) { if (r == x && col == yy) pr = true; const int nx = yy, ny = M - x; x = nx, yy = ny; }
          }
          cid[CN.index(f, r, col)] = c.kind.size();
          c.kind.push_back(pr ? 1 : (r == col || r + col == M) ? 2 : 3);
          c.face.push_back(f);
        }
    c.n = c.kind.size();
    c.mv.resize(6 * 4 * 4);
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 3; d++)
        for (int a = 1; a <= 3; a++)
          for (size_t i = 0; i < CN.pos.size(); i++) {
            if (cid[i] < 0) continue;
            int j = i;
            for (int q = 0; q < a; q++) j = CN.perms[f][d - 1][j];
            if (j != (int)i) c.mv[(f * 4 + d) * 4 + a].push_back({cid[i], cid[j]});
          }
    ncs.push_back(std::move(c));
  }
  std::vector<std::map<int, std::array<long, 3>>> pc(threads);  // need line, prefix found, stored moves
  std::vector<std::map<int, long>> pk(threads);
  std::atomic<size_t> pi{0};
  auto pw = [&](int tid) {
    int8_t col[48], nx[48], img[48];
    std::vector<std::vector<int>> arr(ncs.size()), tmp(ncs.size());
    for (size_t i; (i = pi.fetch_add(1)) < pos.size();) {
      const u128 key = pos[i].first;
      const int d = pos[i].second;
      if (d == 0 || have.at(key) != d) continue;
      u128 k = key;
      for (int x = 0; x < 48; x++) col[x] = (S >> groupOf(x) & 1) ? groupOf(x) : -1;
      for (int j = nf - 1; j >= 0; j--) { const int dg = k % base; k /= base; col[freeK[j]] = dg ? tg[dg - 1] : -1; }
      bool one = false;
      for (auto &m : moves) {
        for (int x = 0; x < 48; x++) nx[mp[m.face][m.depth][m.amt][x]] = col[x];
        const int e = lenOf(nx, img);
        if (e >= 0 && e + 1 <= d) { one = true; break; }
      }
      if (one) continue;
      auto &c = pc[tid][d];
      c[0]++;
      const auto &mv = lineMv[i];
      for (size_t z = 0; z < ncs.size(); z++) { arr[z].resize(ncs[z].n); for (int q = 0; q < ncs[z].n; q++) arr[z][q] = q; }
      int best = 0;
      for (int j = 0; j + 1 < (int)mv.size() && !best; j++) {
        const auto &m = mv[j];
        for (int x = 0; x < 48; x++) nx[mp[m.face][m.depth][m.amt][x]] = col[x];
        memcpy(col, nx, 48);
        for (size_t z = 0; z < ncs.size(); z++) {
          tmp[z] = arr[z];
          for (auto &pr : ncs[z].mv[(m.face * 4 + m.depth) * 4 + m.amt]) arr[z][pr.second] = tmp[z][pr.first];
        }
        const int len = j + 1;
        if (len < 2) continue;
        bool ok = true;
        for (int x = 0; x < 48 && ok; x++) if ((S >> groupOf(x) & 1) && col[x] != groupOf(x)) ok = false;
        for (size_t z = 0; z < ncs.size() && ok; z++)
          for (int q = 0; q < ncs[z].n && ok; q++) {
            const int f = ncs[z].face[q];
            if (ncs[z].kind[q] == 3 && (f == 0 || f == 5) && ncs[z].face[arr[z][q]] != f) ok = false;
          }
        if (!ok) continue;
        const int e = lenOf(col, img);
        if (e >= 0 && e + len <= d) best = len;
      }
      if (best) { c[1]++; c[2] += best; pk[tid][best]++; } else c[2] += d;
    }
  };
  std::vector<std::thread> th2;
  for (int t = 0; t < threads; t++) th2.emplace_back(pw, t);
  for (auto &x : th2) x.join();
  std::map<int, std::array<long, 3>> P;
  std::map<int, long> K;
  for (auto &m : pc) for (auto &kv : m) for (int j = 0; j < 3; j++) P[kv.first][j] += kv.second[j];
  for (auto &m : pk) for (auto &kv : m) K[kv.first] += kv.second;
  long need = 0, found = 0, smoves = 0, fullMoves = 0;
  printf("%s classes that need a line, by length: classes/prefix found:", sname.c_str());
  for (auto &kv : P) { printf(" %d:%ld/%ld", kv.first, kv.second[0], kv.second[1]); need += kv.second[0]; found += kv.second[1]; smoves += kv.second[2]; fullMoves += (long)kv.first * kv.second[0]; }
  printf("\n%s prefix lengths:", sname.c_str());
  for (auto &kv : K) printf(" %d:%ld", kv.first, kv.second);
  printf("\n%s prefixes: %ld of %ld classes (%.1f%%) need only a prefix then a lookup; stored moves %ld (full lines: %ld) (%.0fs)\n", sname.c_str(), found, need, 100.0 * found / need, smoves, fullMoves, now() - t0);
  return 0;
}
