// certcheck: verify chiral oblique pair stage certificates (written by orbitbound CERT=DIR).
//
// A certificate segment is a file .../stages/<solved>..<target>/wNN.txt: one move sequence per
// line, nothing else.  <solved> and <target> name groups ("aU", "bL", ...; "none" = no group):
// a group is one orbit's four pieces of one face colour; orbit a = the (2,3) facelets in 1-based
// rows and columns, orbit b = (3,2).  The stage keeps <solved> colour-solved and solves <target>.
// Each line's position is the one its sequence solves: the sequence run backwards from the
// solved stage.  NN is the number of target pieces off their home face.  This program uses
// nothing from the solver: cube, moves and symmetries come from certcube.h (plain 3D geometry).
// It checks:
//   1. every line: legal moves (depth at most 3), its position has NN pieces wrongly placed,
//      and no two lines give positions of one symmetry class;
//   2. coverage: every stage state with NN wrongly placed has an image under a cube symmetry
//      keeping the solved and target groups among the lines' positions (the image of that line's
//      sequence then solves it in as many moves);
//   3. side effects, on N = 6..10: every centre facelet outside the pair and the diagonals keeps
//      its colour; the report counts sequences that leave all of them in place ("pure") and those
//      that change diagonal (X) centre colours.  --keep u (stage 1u) relaxes this to: every such
//      facelet on U keeps U colour (only the U face sets are kept); --keep ud (1ud, 2ud) to U and D.
//
//   certcheck PATH... [--threads T] [--nocover] [--show] [--keep u|ud]
//     PATH: a segment file, or a stage directory (every w*.txt in it)
//     --show: print each line's position before its moves (48 letters: faces U L F R B D in turn,
//             each face's orbit-a facelets (1,2) (2,6) (6,5) (5,1) then orbit-b (2,1) (1,5) (5,6)
//             (6,2), 0-based on 8x8x8 in net orientation; the piece's home face, '-' untracked)
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <map>
#include <mutex>
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
  bool cover = true, show = false;
  int keep = 0;  // 0 every other centre keeps its colour, 1 only the U face set, 2 U and D face sets
  std::vector<std::string> paths;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--nocover") cover = false;
    else if (o == "--show") show = true;
    else if (o == "--keep" && i + 1 < argc) {
      std::string k = argv[++i];
      if (k == "u") keep = 1;
      else if (k == "ud") keep = 2;
      else { fprintf(stderr, "--keep takes u or ud\n"); return 1; }
    }
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else paths.push_back(o);
  }
  if (paths.empty()) { fprintf(stderr, "usage: certcheck PATH... [--threads T] [--nocover] [--show] [--keep u|ud]\n"); return 1; }
  const double t0 = now();
  // segment files and the stage (all must be one stage)
  std::vector<std::pair<std::string, int>> files;  // path, wrongly placed
  std::string stageDir;
  for (auto p : paths) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    std::vector<std::string> fs;
    if (DIR *d = opendir(p.c_str())) {
      while (dirent *e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > 5 && n[0] == 'w' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back(p + "/" + n);
      }
      closedir(d);
      std::sort(fs.begin(), fs.end());
    } else fs.push_back(p);
    for (auto &f : fs) {
      const size_t sl = f.rfind('/');
      const std::string dir = sl == std::string::npos ? "." : f.substr(0, sl), base = f.substr(sl == std::string::npos ? 0 : sl + 1);
      if (base.size() < 6 || base[0] != 'w' || base.compare(base.size() - 4, 4, ".txt")) { fprintf(stderr, "not a segment file (wNN.txt): %s\n", base.c_str()); return 1; }
      const int w = atoi(base.c_str() + 1);
      if (stageDir.empty()) stageDir = dir;
      else if (stageDir != dir) { fprintf(stderr, "segments from more than one stage\n"); return 1; }
      files.push_back({f, w});
    }
  }
  const std::string sname = stageDir.substr(stageDir.rfind('/') == std::string::npos ? 0 : stageDir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return 1; }
  const int S = groupMask(sname.substr(0, dd)), T = groupMask(sname.substr(dd + 2));
  if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name %s\n", sname.c_str()); return 1; }
  printf("stage %s: %d solved groups kept, %d target groups; %zu segment files\n", sname.c_str(), __builtin_popcount(S), __builtin_popcount(T), files.size());
  // the 48 pair facelets on 8^3, moves on them (dst form)
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
          if (fidx[i] < 0) { fprintf(stderr, "a move takes a pair facelet off the pair\n"); return 1; }
          mp[f][d][a][k] = fidx[i];
        }
  auto groupOf = [](int k) { return (k % 8) / 4 * 6 + k / 8; };  // orbit * 6 + face
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
  auto wrongOf = [&](const int8_t *col) { int w = 0; for (int k = 0; k < 48; k++) if (col[k] >= 0 && (T >> col[k] & 1) && col[k] != groupOf(k)) w++; return w; };
  int8_t solvedCol[48];
  for (int k = 0; k < 48; k++) solvedCol[k] = ((S | T) >> groupOf(k) & 1) ? groupOf(k) : -1;
  // 1. read every segment; each line's position = its sequence run backwards from the solved stage
  struct Line { std::vector<cc::Mv> mv; u128 key; int w; bool ok; };
  std::vector<Line> lines;
  std::vector<std::string> raw;
  std::map<int, long> segLines;
  for (auto &[f, w] : files) {
    FILE *in = fopen(f.c_str(), "r");
    if (!in) { fprintf(stderr, "cannot open a segment\n"); return 1; }
    char buf[4096];
    while (fgets(buf, sizeof buf, in)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      raw.push_back(l);
      lines.push_back({{}, 0, w, false});
      segLines[w]++;
    }
    fclose(in);
  }
  std::atomic<long> badLine{0}, wrongW{0};
  std::atomic<int> longest{0};
  std::vector<std::map<int, int>> segMaxT(threads);
  std::vector<std::array<long, 64>> lenH(threads);
  for (auto &h : lenH) h.fill(0);
  std::mutex em;
  std::vector<std::string> shown(show ? lines.size() : 0);
  {
    std::atomic<size_t> ni{0};
    auto w = [&](int tid) {
      for (size_t li; (li = ni++) < lines.size();) {
        Line &L = lines[li];
        bool ok = cc::parseMoves(raw[li], L.mv);
        for (auto &m : L.mv) ok &= m.depth >= 1 && m.depth <= 3;
        if (!ok) { if (badLine++ < 5) { std::lock_guard<std::mutex> g(em); printf("  bad line: %s\n", raw[li].c_str()); } continue; }
        int8_t a[48], b[48];
        memcpy(a, solvedCol, 48);
        for (auto it = L.mv.rbegin(); it != L.mv.rend(); ++it) {  // backwards: inverse moves in reverse order
          const int inv = 4 - it->amt;
          for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][inv][k]] = a[k];
          memcpy(a, b, 48);
        }
        const int wr = wrongOf(a);
        if (wr != L.w) { if (wrongW++ < 5) { std::lock_guard<std::mutex> g(em); printf("  wrong segment (%d wrongly placed, file says %d): %s\n", wr, L.w, raw[li].c_str()); } }
        L.key = keyOf(a);
        L.ok = true;
        const int n = L.mv.size();
        int cur = longest.load();
        while (n > cur && !longest.compare_exchange_weak(cur, n)) {}
        lenH[tid][std::min(63, n)]++;
        segMaxT[tid][L.w] = std::max(segMaxT[tid][L.w], n);
        if (show) {
          std::string t;
          for (int k = 0; k < 48; k++) t += a[k] < 0 ? '-' : cc::FACES[a[k] % 6];
          shown[li] = t + " " + raw[li];
        }
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(w, t);
    for (auto &t : th) t.join();
  }
  if (show) for (auto &s : shown) printf("%s\n", s.c_str());
  std::array<long, 64> lh{};
  for (auto &h : lenH) for (int i = 0; i < 64; i++) lh[i] += h[i];
  std::map<int, int> segMax;
  for (auto &m : segMaxT) for (auto &kv : m) segMax[kv.first] = std::max(segMax[kv.first], kv.second);
  printf("lines: %zu, %ld bad, %ld in the wrong segment; longest %d; lengths:", lines.size(), badLine.load(), wrongW.load(), longest.load());
  for (int i = 0; i < 64; i++) if (lh[i]) printf(" %d:%ld", i, lh[i]);
  printf(" (%.0fs)\n", now() - t0);
  fflush(stdout);
  // symmetries keeping the stage
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
        if (!ok) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (groupOf(k) == g) { const int h = groupOf(y.fac[k]); if (img >= 0 && img != h) ok = false; img = h; }
          y.gmap[g] = img;
        }
        if (!ok) { fprintf(stderr, "a symmetry splits a group\n"); return 1; }
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << y.gmap[g]; if (T >> g & 1) t2 |= 1 << y.gmap[g]; }
        if (s2 == S && t2 == T) syms.push_back(y);
      }
  }
  // 2. one line per symmetry class, and coverage of the segments given
  std::unordered_map<u128, int, KH> have;  // position key -> line
  have.reserve(lines.size() * 2);
  long dupClass = 0;
  {
    int8_t col[48], img[48];
    for (size_t li = 0; li < lines.size(); li++) {
      if (!lines[li].ok) continue;
      if (!have.emplace(lines[li].key, li).second) dupClass++;
    }
    for (size_t li = 0; li < lines.size(); li++) {
      if (!lines[li].ok) continue;
      u128 k = lines[li].key;
      for (int x = 0; x < 48; x++) col[x] = (S >> groupOf(x) & 1) ? groupOf(x) : -1;
      for (int i = nf - 1; i >= 0; i--) { const int dg = k % base; k /= base; col[freeK[i]] = dg ? tg[dg - 1] : -1; }
      for (auto &y : syms) {
        for (int x = 0; x < 48; x++) img[y.fac[x]] = col[x] < 0 ? -1 : y.gmap[col[x]];
        auto it = have.find(keyOf(img));
        if (it != have.end() && it->second < (int)li) { dupClass++; break; }
      }
    }
  }
  printf("classes: %zu distinct positions, %ld lines repeat a class already listed; %zu cube symmetries keep the stage\n", have.size(), dupClass, syms.size());
  std::map<int, long> stCount, stMissing;
  if (cover) {
    std::vector<std::vector<int>> orbitFree(2);
    for (int k : freeK) orbitFree[(k % 8) / 4].push_back(k);
    std::vector<std::array<int, 4>> firstChoices;
    {
      const auto &v = orbitFree[tg[0] / 6];
      const int n = v.size();
      for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) for (int c = b + 1; c < n; c++) for (int d = c + 1; d < n; d++) firstChoices.push_back({v[a], v[b], v[c], v[d]});
    }
    std::atomic<size_t> nci{0};
    std::vector<std::map<int, long>> cntT(threads), misT(threads);
    std::atomic<int> tidN{0};
    auto cw = [&] {
      const int tid = tidN++;
      int8_t col[48], img[48];
      long shownMiss = 0;
      std::function<void(size_t)> rec = [&](size_t gi) {
        if (gi == tg.size()) {
          const int w = wrongOf(col);
          if (!segLines.count(w)) return;
          cntT[tid][w]++;
          bool found = have.count(keyOf(col)) > 0;
          for (size_t s = 0; s < syms.size() && !found; s++) {
            for (int k = 0; k < 48; k++) img[syms[s].fac[k]] = col[k] < 0 ? -1 : syms[s].gmap[col[k]];
            found = have.count(keyOf(img)) > 0;
          }
          if (!found) {
            misT[tid][w]++;
            if (shownMiss++ < 2) {
              std::string t;
              for (int k = 0; k < 48; k++) t += col[k] < 0 ? '-' : cc::FACES[col[k] % 6];
              std::lock_guard<std::mutex> g(em);
              printf("  not covered: %s\n", t.c_str());
            }
          }
          return;
        }
        const auto &v = orbitFree[tg[gi] / 6];
        const int n = v.size();
        std::function<void(int, int)> pick = [&](int from, int depth) {
          if (depth == 4) { rec(gi + 1); return; }
          for (int i = from; i < n; i++) {
            if (col[v[i]] != -1) continue;
            col[v[i]] = tg[gi];
            pick(i + 1, depth + 1);
            col[v[i]] = -1;
          }
        };
        pick(0, 0);
      };
      for (size_t ci; (ci = nci++) < firstChoices.size();) {
        for (int k = 0; k < 48; k++) col[k] = (S >> groupOf(k) & 1) ? groupOf(k) : -1;
        for (int x : firstChoices[ci]) col[x] = tg[0];
        rec(1);
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(cw);
    for (auto &t : th) t.join();
    for (auto &m : cntT) for (auto &kv : m) stCount[kv.first] += kv.second;
    for (auto &m : misT) for (auto &kv : m) stMissing[kv.first] += kv.second;
  }
  long missing = 0;
  for (auto &[w, n] : segLines) {
    printf("  w%02d: %ld lines, longest %d", w, n, segMax[w]);
    if (cover) printf(", %ld stage states, %ld not covered", stCount[w], stMissing[w]);
    printf("\n");
    missing += stMissing[w];
  }
  if (cover) printf("coverage checked (%.0fs)\n", now() - t0);
  else printf("coverage: not checked (--nocover)\n");
  fflush(stdout);
  // 3. side effects on N = 6..10
  long sideBad = 0;
  for (int N = 6; N <= 10; N++) {
    cc::Cube CN(N);
    const int M = N - 1;
    std::vector<char> kind(CN.pos.size(), 0);  // 1 pair, 2 diagonal, 3 other centre
    for (int f = 0; f < 6; f++)
      for (int r = 1; r < M; r++)
        for (int c = 1; c < M; c++) {
          bool pair = false;
          for (int o = 0; o < 2 && !pair; o++) {
            int x = o ? 2 : 1, y = o ? 1 : 2;
            for (int q = 0; q < 4; q++) { if (r == x && c == y) pair = true; const int nx = y, ny = M - x; x = nx, y = ny; }
          }
          kind[CN.index(f, r, c)] = pair ? 1 : (r == c || r + c == M) ? 2 : 3;
        }
    std::vector<std::vector<std::pair<int, int>>> mvMap(6 * 4 * 4);
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 3; d++)
        for (int a = 1; a <= 3; a++)
          for (size_t i = 0; i < CN.pos.size(); i++) {
            if (!kind[i]) continue;
            int j = i;
            for (int q = 0; q < a; q++) j = CN.perms[f][d - 1][j];
            if (j != (int)i) mvMap[(f * 4 + d) * 4 + a].push_back({(int)i, j});
          }
    std::atomic<size_t> ni{0};
    std::atomic<long> colourBad{0}, impure{0}, diagMoved{0}, checked{0};
    auto w = [&] {
      std::vector<int> a(CN.pos.size()), b;
      long cb = 0, ip = 0, dm = 0, ck = 0;
      for (size_t li; (li = ni++) < lines.size();) {
        if (!lines[li].ok) continue;
        ck++;
        for (size_t i = 0; i < a.size(); i++) a[i] = i;
        for (auto &m : lines[li].mv) {
          b = a;
          for (auto &[i, j] : mvMap[(m.face * 4 + m.depth) * 4 + m.amt]) b[j] = a[i];
          a.swap(b);
        }
        bool colOk = true, pure = true, diag = false;
        for (size_t i = 0; i < a.size(); i++) {
          if (kind[i] == 3) {
            int fa = CN.faceOf[a[i]], fi = CN.faceOf[i];
            if (keep == 0) colOk &= fa == fi;
            else colOk &= (fa == 0) == (fi == 0) && (keep == 1 || (fa == 5) == (fi == 5));
            pure &= a[i] == (int)i;
          }
          else if (kind[i] == 2) diag |= CN.faceOf[a[i]] != CN.faceOf[i];
        }
        cb += !colOk, ip += colOk && !pure, dm += diag;
      }
      colourBad += cb, impure += ip, diagMoved += dm, checked += ck;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    printf("side effects on %dx%dx%d: %ld move another centre off its colour, %ld keep colours but move other centre pieces (not pure), %ld pure; %ld change diagonal colours\n",
           N, N, N, colourBad.load(), impure.load(), checked.load() - colourBad.load() - impure.load(), diagMoved.load());
    sideBad += colourBad;
  }
  const bool ok = !badLine && !wrongW && !dupClass && !missing && !sideBad;
  if (cover) printf("%s: every stage state in these segments is solved in at most %d moves (%.0fs)\n", ok ? "PASS" : "FAIL", longest.load(), now() - t0);
  else printf("%s: every line is a valid solve of its position, at most %d moves (coverage not checked) (%.0fs)\n", ok ? "PASS" : "FAIL", longest.load(), now() - t0);
  return ok ? 0 : 1;
}
