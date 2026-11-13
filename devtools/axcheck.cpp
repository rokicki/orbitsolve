// axcheck: verify a certificate for stage 3ax or 4op of the oblique chiral pair (the axis split).
//
// Stages (pair = orbit a, the (2,3) facelets, and orbit b, (3,2); 48 facelets, 8 per face):
//   3ax: from "the pair's U and D pieces on U and D" to "also every F/B piece on F or B and every
//        L/R piece on L or R".  F and B pieces are one colour here, L and R pieces another.
//   4op: from the 3ax goal to all six faces solved, both orbits.
// A certificate stage directory holds lNN.txt files: one move sequence per line, nothing else, each
// at most NN moves (l00 holds the empty line of the goal).  A line's position is the one its sequence solves: the sequence run backwards
// from the stage's goal.  This program uses nothing from the solver: cube, moves and symmetries come
// from certcube.h (plain 3D geometry).  It checks:
//   1. every line: legal moves (depth at most 3), at most NN moves, and its position is a stage
//      start (3ax: the pair's U and D facelets hold their own pieces; 4op: also every side piece on
//      its own axis);
//   2. classes: no two lines give positions of one symmetry class (symmetries: the cube symmetries
//      that keep the stage, 16), and every stage start (3ax: C(16,8)^2; 4op: 70^4) has an image
//      among the lines' positions (the image of that line's sequence then solves it in as many moves);
//   3. side effects, on N = 6..10: every centre facelet outside the pair and the diagonals keeps its
//      colour (the diagonals are solved last, so they may move).
//
//   axcheck 3ax|4op STAGEDIR [--threads T] [--nocover] [--keys FILE]
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <dirent.h>
#include <map>
#include <mutex>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = std::max(1u, std::thread::hardware_concurrency());
  bool cover = true;
  std::string keysFile;  // --keys FILE: write each line's canonical key (hex; ffffffff = not a stage start)
  std::vector<std::string> args;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--nocover") cover = false;
    else if (o == "--keys" && i + 1 < argc) keysFile = argv[++i];
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else args.push_back(o);
  }
  if (args.size() != 2 || (args[0] != "3ax" && args[0] != "4op")) { fprintf(stderr, "usage: axcheck 3ax|4op STAGEDIR [--threads T] [--nocover] [--keys FILE]\n"); return 1; }
  const bool OP = args[0] == "4op";
  const double t0 = now();
  std::string dir = args[1];
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  std::vector<std::pair<std::string, int>> files;  // path, NN
  if (DIR *d = opendir(dir.c_str())) {
    while (dirent *e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() > 5 && n[0] == 'l' && n.compare(n.size() - 4, 4, ".txt") == 0) files.push_back({dir + "/" + n, atoi(n.c_str() + 1)});
    }
    closedir(d);
  } else { fprintf(stderr, "cannot open the stage directory\n"); return 1; }
  std::sort(files.begin(), files.end(), [](auto &a, auto &b) { return a.second < b.second; });
  printf("stage %s: %zu length files\n", OP ? "4op" : "3ax", files.size());

  // the 48 pair facelets on 8^3 (k = face * 8 + orbit * 4 + i), moves on them (dst form)
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
  auto faceOfK = [](int k) { return k / 8; };
  auto orbitOfK = [](int k) { return (k % 8) / 4; };
  auto groupOf = [&](int k) { return orbitOfK(k) * 6 + faceOfK(k); };  // orbit * 6 + face
  const int axisOf[6] = {0, 1, 2, 1, 2, 0};                              // U L F R B D
  auto side = [&](int f) { return axisOf[f] != 0; };
  // labels: 3ax: U, D groups keep their own label; F and B pieces of an orbit share one label (the
  // F group's), L and R pieces another (the L group's).  4op: every group its own label.
  auto labelOfGroup = [&](int g) {
    if (OP) return g;
    const int o = g / 6, f = g % 6;
    return !side(f) ? g : o * 6 + (axisOf[f] == 2 ? 2 : 1);
  };
  // key: one bit per side slot, in k order (32 slots): 3ax: the slot holds an F/B piece; 4op: the slot
  // holds the first group of its axis (F on F/B slots, L on L/R slots)
  std::vector<int> slots;
  int slotOf[48];
  for (int k = 0; k < 48; k++) { slotOf[k] = -1; if (side(faceOfK(k))) { slotOf[k] = slots.size(); slots.push_back(k); } }
  if (slots.size() != 32) { fprintf(stderr, "expected 32 side slots\n"); return 1; }
  auto bitOfGroupAt = [&](int g, int k) {  // the key bit for group g in slot k; -1 if not a stage start
    if (g / 6 != orbitOfK(k)) return -1;
    const int f = g % 6, fk = faceOfK(k);
    if (!side(f)) return -1;
    if (!OP) return axisOf[f] == 2 ? 1 : 0;
    if (axisOf[f] != axisOf[fk]) return -1;
    return (f == 2 || f == 1) ? 1 : 0;
  };
  // the stage starts: per orbit, 3ax: 8 of the 16 side slots hold F/B pieces; 4op: 4 of the 8 F/B slots
  // hold F, 4 of the 8 L/R slots hold L
  std::vector<uint32_t> cntMask;
  for (int o = 0; o < 2; o++) {
    if (!OP) { uint32_t m = 0; for (int s = 0; s < 32; s++) if (orbitOfK(slots[s]) == o) m |= 1u << s; cntMask.push_back(m); }
    else for (int ax : {2, 1}) { uint32_t m = 0; for (int s = 0; s < 32; s++) if (orbitOfK(slots[s]) == o && axisOf[faceOfK(slots[s])] == ax) m |= 1u << s; cntMask.push_back(m); }
  }
  const int want = OP ? 4 : 8;
  auto validKey = [&](uint32_t x) { for (uint32_t m : cntMask) if (__builtin_popcount(x & m) != want) return false; return true; };

  // symmetries keeping the stage (the pair's U and D groups go to U and D groups); as key maps
  struct Sym { uint32_t tab[4][256]; };
  std::vector<Sym> syms;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        int fac[48], gmap[12];
        bool ok = true;
        for (int k = 0; k < 48 && ok; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          int j = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) j = x;
          ok = j >= 0;
          fac[k] = j;
        }
        if (!ok) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (groupOf(k) == g) { const int h = groupOf(fac[k]); if (img >= 0 && img != h) ok = false; img = h; }
          gmap[g] = img;
        }
        if (!ok) { fprintf(stderr, "a symmetry splits a group\n"); return 1; }
        bool keeps = true;
        for (int g = 0; g < 12; g++) if (side(g % 6) != side(gmap[g] % 6)) keeps = false;
        if (!keeps) continue;
        // a label must map to one label (3ax: F and B pieces to one label)
        int lmap[12];
        for (int g = 0; g < 12; g++) lmap[g] = -1;
        for (int g = 0; g < 12; g++) {
          const int l = labelOfGroup(g), m = labelOfGroup(gmap[g]);
          if (lmap[l] >= 0 && lmap[l] != m) { fprintf(stderr, "a symmetry splits a label\n"); return 1; }
          lmap[l] = m;
        }
        // slot s holding bit v: its group (any group with that bit; labels make the choice irrelevant)
        Sym y;
        for (int b = 0; b < 4; b++)
          for (int v = 0; v < 256; v++) {
            uint32_t out = 0;
            for (int t = 0; t < 8; t++) {
              const int s = b * 8 + t, k = slots[s], bit = v >> t & 1;
              int g = -1;
              for (int h = 0; h < 12 && g < 0; h++) if (bitOfGroupAt(h, k) == bit) g = h;
              const int k2 = fac[k], g2 = gmap[g];
              const int b2 = bitOfGroupAt(g2, k2);
              if (b2 < 0) { fprintf(stderr, "a stage symmetry leaves the stage\n"); return 1; }
              out |= (uint32_t)b2 << slotOf[k2];
            }
            y.tab[b][v] = out;
          }
        syms.push_back(y);
      }
  }
  auto image = [&](const Sym &y, uint32_t x) { return y.tab[0][x & 255] | y.tab[1][x >> 8 & 255] | y.tab[2][x >> 16 & 255] | y.tab[3][x >> 24]; };
  auto canon = [&](uint32_t x) { uint32_t m = x; for (auto &y : syms) m = std::min(m, image(y, x)); return m; };
  printf("%zu cube symmetries keep the stage\n", syms.size());

  // side-effect tables for N = 6..10: centre facelets only (1 pair, 2 diagonal, 3 other centre)
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
          bool pair = false;
          for (int o = 0; o < 2 && !pair; o++) {
            int x = o ? 2 : 1, yy = o ? 1 : 2;
            for (int q = 0; q < 4; q++) { if (r == x && col == yy) pair = true; const int nx = yy, ny = M - x; x = nx, yy = ny; }
          }
          cid[CN.index(f, r, col)] = c.kind.size();
          c.kind.push_back(pair ? 1 : (r == col || r + col == M) ? 2 : 3);
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
            if (cid[j] < 0) { fprintf(stderr, "a move takes a centre off the centres\n"); return 1; }
            if (j != (int)i) c.mv[(f * 4 + d) * 4 + a].push_back({cid[i], cid[j]});
          }
    ncs.push_back(std::move(c));
  }

  // 1 and 3: every line, threaded, file by file
  std::vector<uint32_t> keys;  // canonical keys of the lines
  long nLines = 0, badLine = 0, tooLong = 0, notStart = 0;
  long sideBad[5] = {}, diagMoved[5] = {};
  int longest = 0;
  std::map<int, long> perFile;
  std::map<int, int> perFileMax;
  std::mutex em;
  FILE *kf = keysFile.empty() ? nullptr : fopen(keysFile.c_str(), "w");  // --keys: canonical key per line, file order
  if (!keysFile.empty() && !kf) { fprintf(stderr, "cannot write the keys file\n"); return 1; }
  for (auto &[fn, NN] : files) {
    std::vector<std::string> raw;
    {
      FILE *in = fopen(fn.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot open a length file\n"); return 1; }
      char buf[4096];
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        raw.push_back(l);
      }
      fclose(in);
    }
    const size_t base = keys.size();
    keys.resize(base + raw.size());
    std::atomic<size_t> ni{0};
    std::atomic<long> bl{0}, tl{0}, ns{0};
    std::atomic<int> lg{0};
    std::vector<std::array<long, 10>> sideT(threads);
    for (auto &a : sideT) a.fill(0);
    auto w = [&](int tid) {
      std::vector<cc::Mv> mv;
      std::vector<int> a, tmp;
      for (size_t li; (li = ni++) < raw.size();) {
        uint32_t &key = keys[base + li];
        key = 0xFFFFFFFFu;  // invalid (no valid key has all bits set)
        bool ok = cc::parseMoves(raw[li], mv);  // an empty line (l00) solves the goal itself
        for (auto &m : mv) ok &= m.depth >= 1 && m.depth <= 3 && m.amt >= 1 && m.amt <= 3;
        if (!ok) { if (bl++ < 5) { std::lock_guard<std::mutex> g(em); printf("  bad line in l%d: %s\n", NN, raw[li].c_str()); } continue; }
        const int n = mv.size();
        if (n > NN && tl++ < 5) { std::lock_guard<std::mutex> g(em); printf("  %d moves in l%d: %s\n", n, NN, raw[li].c_str()); }
        for (int c = lg.load(); n > c && !lg.compare_exchange_weak(c, n);) {}
        // position: the goal (groups) run backwards
        int8_t col[48], nb[48];
        for (int k = 0; k < 48; k++) col[k] = groupOf(k);
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
          const int inv = 4 - it->amt;
          for (int k = 0; k < 48; k++) nb[mp[it->face][it->depth][inv][k]] = col[k];
          memcpy(col, nb, 48);
        }
        bool start = true;
        uint32_t x = 0;
        for (int k = 0; k < 48 && start; k++) {
          if (!side(faceOfK(k))) { start = col[k] == groupOf(k); continue; }
          const int b = bitOfGroupAt(col[k], k);
          if (b < 0) start = false;
          else x |= (uint32_t)b << slotOf[k];
        }
        if (!start || !validKey(x)) { if (ns++ < 5) { std::lock_guard<std::mutex> g(em); printf("  not a stage start (l%d): %s\n", NN, raw[li].c_str()); } continue; }
        key = canon(x);
        // side effects
        for (int z = 0; z < 5; z++) {
          const NC &c = ncs[z];
          a.resize(c.n);
          for (int i = 0; i < c.n; i++) a[i] = i;
          for (auto &m : mv) {
            const auto &pr = c.mv[(m.face * 4 + m.depth) * 4 + m.amt];
            tmp.resize(pr.size());
            for (size_t q = 0; q < pr.size(); q++) tmp[q] = a[pr[q].first];
            for (size_t q = 0; q < pr.size(); q++) a[pr[q].second] = tmp[q];
          }
          bool colOk = true, diag = false;
          for (int i = 0; i < c.n; i++) {
            if (c.kind[i] == 3) colOk &= c.face[a[i]] == c.face[i];
            else if (c.kind[i] == 2) diag |= c.face[a[i]] != c.face[i];
          }
          sideT[tid][z] += !colOk, sideT[tid][5 + z] += diag;
        }
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(w, t);
    for (auto &t : th) t.join();
    if (kf) for (size_t li = 0; li < raw.size(); li++) fprintf(kf, "%08x\n", keys[base + li]);
    badLine += bl, tooLong += tl, notStart += ns;
    for (auto &s : sideT) for (int z = 0; z < 5; z++) sideBad[z] += s[z], diagMoved[z] += s[5 + z];
    perFile[NN] = raw.size();
    perFileMax[NN] = lg.load();
    longest = std::max(longest, lg.load());
    nLines += raw.size();
    printf("  l%02d: %zu lines, longest %d (%.0fs)\n", NN, raw.size(), lg.load(), now() - t0);
    fflush(stdout);
  }
  if (kf) fclose(kf);
  printf("lines: %ld, %ld bad, %ld longer than their file, %ld not a stage start; longest %d (%.0fs)\n", nLines, badLine, tooLong, notStart, longest, now() - t0);
  for (int z = 0; z < 5; z++) printf("side effects on %dx%dx%d: %ld move another centre off its colour; %ld change diagonal colours (allowed)\n", 6 + z, 6 + z, 6 + z, sideBad[z], diagMoved[z]);
  long sideTotal = 0;
  for (int z = 0; z < 5; z++) sideTotal += sideBad[z];

  // 2. one line per class; coverage of every stage start
  std::vector<uint32_t> sk;
  for (uint32_t k : keys) if (k != 0xFFFFFFFFu) sk.push_back(k);
  std::sort(sk.begin(), sk.end());
  long dup = 0;
  for (size_t i = 1; i < sk.size(); i++) dup += sk[i] == sk[i - 1];
  sk.erase(std::unique(sk.begin(), sk.end()), sk.end());
  printf("classes: %zu distinct, %ld lines repeat a class (%.0fs)\n", sk.size(), dup, now() - t0);
  long states = 0, missing = 0, classes = 0;
  if (cover) {
    std::vector<uint64_t> have(1ull << 26, 0);  // bitmap over 32-bit canonical keys
    for (uint32_t k : sk) have[k >> 6] |= 1ull << (k & 63);
    std::atomic<long> st{0}, mi{0}, cl{0};
    std::atomic<uint64_t> nc{0};
    auto w = [&] {
      long s = 0, m = 0, c = 0, shown = 0;
      for (uint64_t chunk; (chunk = nc++) < 4096;) {
        const uint64_t lo = chunk << 20, hi = lo + (1ull << 20);
        for (uint64_t xx = lo; xx < hi; xx++) {
          const uint32_t x = (uint32_t)xx;
          if (!validKey(x)) continue;
          s++;
          const uint32_t cx = canon(x);
          if (cx == x) c++;
          if (!(have[cx >> 6] >> (cx & 63) & 1)) { m++; if (shown++ < 2) { std::lock_guard<std::mutex> g(em); printf("  not covered: key %08x\n", x); } }
        }
      }
      st += s, mi += m, cl += c;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    states = st, missing = mi, classes = cl;
    printf("coverage: %ld stage starts in %ld classes, %ld not covered; %zu classes listed (%.0fs)\n", states, classes, missing, sk.size(), now() - t0);
  } else printf("coverage: not checked (--nocover)\n");
  for (auto &[NN, n] : perFile) printf("  l%02d: %ld lines, longest %d\n", NN, n, perFileMax[NN]);
  const bool extra = cover && (long)sk.size() != classes;
  if (extra) printf("  %zu classes listed but %ld exist\n", sk.size(), classes);
  const bool ok = !badLine && !tooLong && !notStart && !dup && !missing && !sideTotal && !extra;
  if (cover) printf("%s: every %s stage start is solved in at most %d moves (%.0fs)\n", ok ? "PASS" : "FAIL", OP ? "4op" : "3ax", longest, now() - t0);
  else printf("%s: every line is a valid %s solve of its position, at most %d moves (coverage not checked) (%.0fs)\n", ok ? "PASS" : "FAIL", OP ? "4op" : "3ax", longest, now() - t0);
  return ok ? 0 : 1;
}
