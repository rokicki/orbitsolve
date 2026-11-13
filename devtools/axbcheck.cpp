// axbcheck: verify a boundary certificate between stages 3ax and 4op of the oblique chiral pair
// (2026-11-01; the 3ax/4op counterpart of boundcheck).
//
// The claim: every position at the start of 3ax (the pair's U and D pieces home) is taken through
// 3ax and then 4op in at most X moves, choosing the 3ax sequence by the whole position.  Stages as
// axcheck: 3ax ends with every side piece on its own axis, 4op ends with all six faces solved.
// The stage certificates are directories of lNN.txt lines (axcheck must PASS on both: it checks
// classes and side effects of the stage lines).  The boundary file has blocks separated by empty
// lines; a block's lines are 3ax solving sequences of one 3ax state, the one its first line solves.
// This program uses nothing from axbx or the solvers: cube, moves and symmetries come from
// certcube.h (plain 3D geometry), and every state is computed from whole colourings of the 48 pair
// facelets (groups = orbit and home face).  It checks
//   1. both stage certificates: every line legal and its position a stage start; every state of
//      each stage is solved by a line of some stage-symmetric image, so d3 and d4 (line lengths)
//      are defined everywhere;
//   2. every block: legal moves, each line solves the first line's 3ax state (every side piece on
//      its own axis afterwards, U and D pieces home), and the side effects as axcheck (on N = 6..10
//      every centre facelet outside the pair and the diagonals keeps its colour);
//   3. every 3ax state with d3 > X - max d4 has a block for a stage-symmetric image of it;
//   4. per block, with L its first line: for every 4op state s with |L| + d4(s) > X, the whole
//      position that L takes to s (its side pieces' groups: those that L brings to s's) is solved by
//      some line P of the block followed by 4op with |P| + d4(the state P reaches) <= X; and for
//      P = L the state reached is s itself (frame check).
// The 16 stage symmetries (those keeping the U-D axis) are the same for both stages, so the claim
// for a symmetric image of a block's state follows from the block's.
//
//   axbcheck STAGEDIR_3AX STAGEDIR_4OP BOUNDARYFILE X [--threads T]
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <mutex>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int mp[6][4][4][48];  // dst form: the piece at k goes to mp[f][d][a][k]
static inline int faceK(int k) { return k / 8; }
static inline int orbK(int k) { return (k % 8) / 4; }
static inline int grp(int k) { return orbK(k) * 6 + faceK(k); }
static const int AX[6] = {0, 1, 2, 1, 2, 0};  // U L F R B D
static uint64_t C_[17][17];
// side facelets per orbit, in k order (16 each); per orbit and axis (8 each)
static std::vector<int> sideO[2], axO[2][3];

// 3ax state of a colouring (groups per facelet): U/D facelets must hold their own groups; per orbit,
// which of its 16 side facelets hold F/B pieces (8 of them): colex rank; -1 if not a 3ax start
static int64_t rank3(const int8_t *c) {
  int64_t r = 0;
  for (int k = 0; k < 48; k++) if (AX[faceK(k)] == 0 && c[k] != grp(k)) return -1;
  for (int o = 0; o < 2; o++) {
    int64_t ro = 0;
    int j = 0;
    for (int i = 0; i < 16; i++) {
      const int g = c[sideO[o][i]];
      if (g / 6 != o || AX[g % 6] == 0) return -1;
      if (AX[g % 6] == 2) ro += C_[i][++j];
    }
    if (j != 8) return -1;
    r = r * 12870 + ro;
  }
  return r;
}
// 4op state: also every side piece on its own axis; per orbit and axis, which 4 of the 8 hold F (resp. L)
static int64_t rank4(const int8_t *c) {
  int64_t r = 0;
  for (int k = 0; k < 48; k++) {
    if (AX[faceK(k)] == 0) { if (c[k] != grp(k)) return -1; continue; }
    if (c[k] / 6 != orbK(k) || AX[c[k] % 6] != AX[faceK(k)]) return -1;
  }
  for (int o = 0; o < 2; o++)
    for (int ax : {2, 1}) {
      int64_t ro = 0;
      int j = 0;
      for (int i = 0; i < 8; i++) if (c[axO[o][ax][i]] % 6 == ax) ro += C_[i][++j];  // F is face 2, L face 1
      if (j != 4) return -1;
      r = r * 70 + ro;
    }
  return r;
}
// a 4op colouring of rank r
static void unrank4(int64_t r, int8_t *c) {
  int v[4];
  for (int i = 3; i >= 0; i--) v[i] = r % 70, r /= 70;
  for (int k = 0; k < 48; k++) c[k] = grp(k);
  int q = 0;
  for (int o = 0; o < 2; o++)
    for (int ax : {2, 1}) {
      int64_t rest = v[q++];
      bool take[8] = {};
      int p = 8;
      for (int t = 4; t >= 1; t--) {
        p--;
        while (C_[p][t] > (uint64_t)rest) p--;
        rest -= C_[p][t];
        take[p] = true;
      }
      for (int i = 0; i < 8; i++) c[axO[o][ax][i]] = o * 6 + (take[i] ? ax : ax + 2);  // F 2 / B 4, L 1 / R 3
    }
}
static void play(const std::vector<cc::Mv> &mv, const int8_t *a, int8_t *out) {
  int8_t x[48], y[48];
  memcpy(x, a, 48);
  for (auto &m : mv) {
    for (int k = 0; k < 48; k++) y[mp[m.face][m.depth][m.amt][k]] = x[k];
    memcpy(x, y, 48);
  }
  memcpy(out, x, 48);
}
static void unplay(const std::vector<cc::Mv> &mv, const int8_t *a, int8_t *out) {
  int8_t x[48], y[48];
  memcpy(x, a, 48);
  for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
    for (int k = 0; k < 48; k++) y[mp[it->face][it->depth][4 - it->amt][k]] = x[k];
    memcpy(x, y, 48);
  }
  memcpy(out, x, 48);
}
struct Sym { int fac[48], gmap[12]; };
static std::vector<Sym> syms;  // the 16 keeping the U-D axis
static void image(const Sym &y, const int8_t *a, int8_t *b) { for (int k = 0; k < 48; k++) b[y.fac[k]] = y.gmap[a[k]]; }

static bool readLines(const std::string &dir, std::vector<std::vector<cc::Mv>> &out) {
  std::vector<std::pair<int, std::string>> fs;
  if (DIR *d = opendir(dir.c_str())) {
    while (dirent *e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() > 5 && n[0] == 'l' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back({atoi(n.c_str() + 1), dir + "/" + n});
    }
    closedir(d);
  }
  if (fs.empty()) return false;
  std::sort(fs.begin(), fs.end());
  for (auto &[nn, f] : fs) {
    FILE *in = fopen(f.c_str(), "r");
    if (!in) return false;
    char buf[4096];
    while (fgets(buf, sizeof buf, in)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      std::vector<cc::Mv> mv;
      if (!cc::parseMoves(l, mv)) return false;
      for (auto &m : mv) if (m.depth < 1 || m.depth > 3 || m.amt < 1 || m.amt > 3) return false;
      if ((int)mv.size() > nn) return false;
      out.push_back(mv);
    }
    fclose(in);
  }
  return true;
}
// d over every state: the shortest line of any symmetric image; returns the number of lines that are not a stage start
static long fill(const std::vector<std::vector<cc::Mv>> &L, bool four, std::vector<uint8_t> &d, int threads) {
  std::atomic<size_t> ni{0};
  std::atomic<long> bad{0};
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      int8_t g[48], c[48], b[48];
      for (int k = 0; k < 48; k++) g[k] = grp(k);
      for (size_t i; (i = ni++) < L.size();) {
        unplay(L[i], g, c);
        if ((four ? rank4(c) : rank3(c)) < 0) { bad++; continue; }
        for (auto &y : syms) {
          image(y, c, b);
          const int64_t r = four ? rank4(b) : rank3(b);
          if (r < 0) { bad++; continue; }
          uint8_t cur = __atomic_load_n(&d[r], __ATOMIC_RELAXED);
          while (L[i].size() < cur && !__atomic_compare_exchange_n(&d[r], &cur, (uint8_t)L[i].size(), true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
        }
      }
    });
  for (auto &x : th) x.join();
  return bad;
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 4) { fprintf(stderr, "usage: axbcheck STAGEDIR_3AX STAGEDIR_4OP BOUNDARYFILE X [--threads T]\n"); return 1; }
  const int X = atoi(pos[3].c_str());
  const double t0 = now();
  for (int n = 0; n <= 16; n++) { C_[n][0] = 1; for (int k = 1; k <= 16; k++) C_[n][k] = n == 0 ? 0 : C_[n - 1][k - 1] + C_[n - 1][k]; }
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          if (fidx[i] < 0) { fprintf(stderr, "a move takes a pair facelet off the pair\n"); return 1; }
          mp[f][d][a][k] = fidx[i];
        }
  for (int k = 0; k < 48; k++) if (AX[faceK(k)]) sideO[orbK(k)].push_back(k), axO[orbK(k)][AX[faceK(k)]].push_back(k);
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        for (int k = 0; k < 48; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          y.fac[k] = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
          if (y.fac[k] < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
        }
        bool keeps = true;
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (grp(k) == g) { const int h = grp(y.fac[k]); if (img >= 0 && img != h) { fprintf(stderr, "a symmetry splits a group\n"); return 1; } img = h; }
          y.gmap[g] = img;
          if ((AX[g % 6] == 0) != (AX[img % 6] == 0)) keeps = false;
        }
        if (keeps) syms.push_back(y);
      }
    if (syms.size() != 16) { fprintf(stderr, "expected 16 stage symmetries\n"); return 1; }
  }

  // 1. the stages
  std::vector<std::vector<cc::Mv>> L3, L4;
  if (!readLines(pos[0], L3) || !readLines(pos[1], L4)) { printf("FAIL: a stage directory has a bad line or none\n"); return 1; }
  std::vector<uint8_t> d3(12870ull * 12870, 0xFF), d4(70ull * 70 * 70 * 70, 0xFF);
  const long b3 = fill(L3, false, d3, threads), b4 = fill(L4, true, d4, threads);
  int W3 = 0, W4 = 0;
  long u3 = 0, u4 = 0;
  for (auto v : d3) { if (v == 0xFF) u3++; else W3 = std::max<int>(W3, v); }
  for (auto v : d4) { if (v == 0xFF) u4++; else W4 = std::max<int>(W4, v); }
  printf("3ax: %zu lines (%ld not a stage start), %zu states, %ld not covered, longest %d\n", L3.size(), b3, d3.size(), u3, W3);
  printf("4op: %zu lines (%ld not a stage start), %zu states, %ld not covered, longest %d (%.0fs)\n", L4.size(), b4, d4.size(), u4, W4, now() - t0);
  fflush(stdout);
  if (b3 || b4 || u3 || u4) { printf("FAIL: a stage certificate does not cover its stage\n"); return 1; }

  // the blocks
  std::vector<std::vector<std::string>> blocks;
  {
    FILE *in = fopen(pos[2].c_str(), "r");
    if (!in) { fprintf(stderr, "cannot read the boundary file\n"); return 1; }
    char buf[4096];
    std::vector<std::string> cur;
    while (fgets(buf, sizeof buf, in)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      if (l.empty()) { if (!cur.empty()) blocks.push_back(cur), cur.clear(); }
      else cur.push_back(l);
    }
    if (!cur.empty()) blocks.push_back(cur);
    fclose(in);
  }
  // 4op states by length, beyond the lowest threshold any block can need
  int maxL = W3;
  for (auto &b : blocks) { std::vector<cc::Mv> mv; if (cc::parseMoves(b[0], mv)) maxL = std::max<int>(maxL, mv.size()); }
  const int lowT = std::max(0, X - maxL);
  std::vector<std::vector<int64_t>> byD(W4 + 1);
  for (int64_t r = 0; r < (int64_t)d4.size(); r++) if (d4[r] > lowT) byD[d4[r]].push_back(r);
  printf("boundary: %zu blocks, X = %d, longest 4op line %d\n", blocks.size(), X, W4);
  for (int d = lowT + 1; d <= W4; d++) if (byD[d].size()) printf("  4op states at %d: %zu\n", d, byD[d].size());
  fflush(stdout);

  // side effects on N = 6..10, as axcheck
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
  auto sideOk = [&](const std::vector<cc::Mv> &mv) {
    std::vector<int> a, tmp;
    for (auto &c : ncs) {
      a.resize(c.n);
      for (int i = 0; i < c.n; i++) a[i] = i;
      for (auto &m : mv) {
        const auto &pr = c.mv[(m.face * 4 + m.depth) * 4 + m.amt];
        tmp.resize(pr.size());
        for (size_t q = 0; q < pr.size(); q++) tmp[q] = a[pr[q].first];
        for (size_t q = 0; q < pr.size(); q++) a[pr[q].second] = tmp[q];
      }
      for (int i = 0; i < c.n; i++) if (c.kind[i] == 3 && c.face[a[i]] != c.face[i]) return false;
    }
    return true;
  };

  std::vector<uint64_t> has((d3.size() + 63) / 64, 0);
  std::atomic<size_t> ni{0};
  std::atomic<long> badBlock{0}, badLine{0}, notSolved{0}, sideBad{0}, tried{0}, unsolved{0}, frameBad{0};
  std::mutex em;
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      int8_t g[48], u[48], img[48], s[48], J0[48], J[48];
      for (int k = 0; k < 48; k++) g[k] = grp(k);
      for (size_t bi; (bi = ni++) < blocks.size();) {
        const auto &blk = blocks[bi];
        std::vector<std::vector<cc::Mv>> mvs(blk.size());
        bool ok = true;
        for (size_t i = 0; i < blk.size() && ok; i++) {
          ok = cc::parseMoves(blk[i], mvs[i]) && !mvs[i].empty();
          for (auto &m : mvs[i]) ok &= m.depth >= 1 && m.depth <= 3 && m.amt >= 1 && m.amt <= 3;
          if (!ok) badLine++;
        }
        if (!ok) { badBlock++; continue; }
        // the block's 3ax state: the first line run backwards from the 3ax goal (labels: F/B pieces vs L/R pieces)
        unplay(mvs[0], g, u);
        if (rank3(u) < 0) { notSolved++; badBlock++; continue; }
        // every line solves u: played on u, every side piece lands on its axis and U, D stay home
        for (size_t i = 0; i < blk.size() && ok; i++) {
          play(mvs[i], u, J);
          for (int k = 0; k < 48 && ok; k++) {
            if (AX[faceK(k)] == 0) ok = J[k] == grp(k);
            else ok = J[k] / 6 == orbK(k) && AX[J[k] % 6] == AX[faceK(k)];
          }
          if (!ok) { notSolved++; break; }
          if (!sideOk(mvs[i])) { sideBad++; ok = false; }
        }
        if (!ok) { badBlock++; continue; }
        for (auto &y : syms) {
          image(y, u, img);
          const int64_t r = rank3(img);
          __atomic_fetch_or(&has[r >> 6], 1ull << (r & 63), __ATOMIC_RELAXED);
        }
        // 4. the positions L carries to 4op states beyond X - |L|
        const int cL = mvs[0].size();
        // each line's facelet map (dst form: the piece at k goes to Dm[i][k]), from its moves
        std::vector<std::array<int8_t, 48>> Dm(blk.size());
        for (size_t i = 0; i < blk.size(); i++) {
          int8_t idx[48], m[48];
          for (int k = 0; k < 48; k++) idx[k] = k;
          play(mvs[i], idx, m);  // m[x] = the facelet whose piece the line brings to x
          for (int x = 0; x < 48; x++) Dm[i][(int)m[x]] = x;
        }
        long left = 0, tr = 0;
        for (int d = std::max(X - cL + 1, lowT + 1); d <= W4; d++)
          for (int64_t r : byD[d]) {
            unrank4(r, s);
            // the whole position: the piece at k is the one L brings to where s has it
            for (int k = 0; k < 48; k++) J0[k] = s[(int)Dm[0][k]];
            bool good = false;
            for (size_t i = 0; i < blk.size() && !good; i++) {
              if ((int)mvs[i].size() >= X) continue;
              for (int k = 0; k < 48; k++) J[(int)Dm[i][k]] = J0[k];
              const int64_t r4 = rank4(J);
              tr++;
              if (r4 < 0) { std::lock_guard<std::mutex> lg(em); printf("  block %zu: a line leaves a side piece off its axis\n", bi); continue; }
              if (i == 0 && r4 != r) frameBad++;
              if ((int)mvs[i].size() + d4[r4] <= X) good = true;
            }
            if (!good) left++;
          }
        tried += tr;
        if (left) {
          unsolved += left;
          badBlock++;
          std::lock_guard<std::mutex> lg(em);
          if (badBlock <= 5) printf("  block %zu: %ld positions not solved within %d\n", bi, left, X);
        }
      }
    });
  for (auto &x : th) x.join();
  long needed = 0, missing = 0;
  for (size_t r = 0; r < d3.size(); r++)
    if (d3[r] > X - W4) {
      needed++;
      if (!(has[r >> 6] >> (r & 63) & 1)) missing++;
    }
  printf("blocks: %ld bad (%ld bad lines, %ld lines not solving the block's state, %ld with side effects); %ld positions tried, %ld not solved; frame errors %ld\n",
         badBlock.load(), badLine.load(), notSolved.load(), sideBad.load(), tried.load(), unsolved.load(), frameBad.load());
  printf("3ax states with d3 > %d: %ld, %ld without a block\n", X - W4, needed, missing);
  const bool ok = !badBlock && !missing && !frameBad;
  printf("%s: every 3ax start is solved through 3ax and 4op in at most %d moves (%.0fs)\n", ok ? "PASS" : "FAIL", X, now() - t0);
  return ok ? 0 : 1;
}
