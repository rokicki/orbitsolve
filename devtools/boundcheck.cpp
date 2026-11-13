// boundcheck: verify a boundary certificate between two consecutive chiral-pair stages.
//
// The claim: every position whose stage A state is solved first and stage B state second is
// solved in at most X moves for the two stages together, choosing A's sequence by the whole
// position.  Stage A = <S>..<T>, stage B = <S+T>..<U> (certificate directories as for certcheck,
// which must PASS on both).  The boundary file has blocks separated by empty lines; a block's
// lines are solving sequences of one A state (the state its first line solves, run backwards).
// This program uses nothing from the solvers or from boundx; it checks
//   1. both stage certificates: every state of each stage is covered (a line of some symmetric
//      image), so dA and dB (the line lengths) are defined everywhere;
//   2. every block: legal moves, each line solves the first line's A state (keeping A's solved
//      groups), and the side effects as certcheck (on N = 6..10 every centre facelet outside the
//      pair and the diagonals keeps its colour);
//      --keep u relaxes this to: every such facelet on U stays on U and every other stays off U
//      (the relaxed stage 1u); --keep ud to U and D (1ud, 2ud), as certcheck --keep;
//   3. every A state with dA > X - max dB has a block for a symmetric image of it;
//   4. per block, with L its first line: for every B state s with |L| + dB(s) > X, the colouring
//      of B's pieces that L carries onto s is solved by some line P of the block with
//      |P| + dB(carried by P) <= X (the B state found by playing P on the whole colouring).
// The stage symmetries of A and B must be the same; the claim for a symmetric image of a block's
// state follows from the block's.
//
//   boundcheck STAGEDIR_A STAGEDIR_B BOUNDARYFILE X [--threads T] [--keep u|ud]
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <mutex>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
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
static int groupOf(int k) { return (k % 8) / 4 * 6 + k / 8; }  // orbit * 6 + face
static int mp[6][4][4][48];
static uint64_t BN[49][5];  // BN[n][k] = C(n, k), k <= 4
static uint64_t binom(int n, int k) {
  if (k < 0 || n < k) return 0;
  uint64_t r = 1;
  for (int i = 1; i <= k; i++) r = r * (n - k + i) / i;
  return r;
}
struct Sym { int fac[48]; int gmap[12]; };
static std::vector<Sym> allSyms;

// one stage: colourings are group ids (-1 = not tracked) per facelet
struct Stage {
  std::string name;
  int S = 0, T = 0;
  std::vector<int> tg;            // target groups
  std::vector<int> freeO[2];      // free (not solved-group) facelets of each orbit, ascending
  std::vector<int> syms;          // indices into allSyms keeping the stage
  uint64_t total = 1;
  std::vector<uint8_t> dist;
  int W = 0;
  long lines = 0;

  // rank: per target group in order, its 4 facelets among the facelets of its orbit left by the
  // earlier groups of that orbit (colex), mixed radix
  uint64_t rank(const int8_t *col) const {
    uint64_t r = 0;
    for (int o = 0; o < 2; o++) {
      int left[24], rest[24], n = freeO[o].size();
      for (int i = 0; i < n; i++) left[i] = freeO[o][i];
      for (int g : tg) {
        if (g / 6 != o) continue;
        uint64_t sub = 0;
        int j = 0, m = 0;
        for (int i = 0; i < n; i++) {
          if (col[left[i]] == g) { j++; sub += BN[i][j]; }
          else rest[m++] = left[i];
        }
        r = r * BN[n][4] + sub;
        memcpy(left, rest, m * sizeof(int));
        n = m;
      }
    }
    return r;
  }
  bool init(const std::string &dir, int threads) {
    std::string d = dir;
    while (d.size() > 1 && d.back() == '/') d.pop_back();
    name = d.substr(d.rfind('/') == std::string::npos ? 0 : d.rfind('/') + 1);
    const size_t dd = name.find("..");
    if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return false; }
    S = groupMask(name.substr(0, dd)), T = groupMask(name.substr(dd + 2));
    if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name %s\n", name.c_str()); return false; }
    for (int g = 0; g < 12; g++) if (T >> g & 1) tg.push_back(g);
    for (int k = 0; k < 48; k++) if (!(S >> groupOf(k) & 1)) freeO[(k % 8) / 4].push_back(k);
    for (int o = 0; o < 2; o++) {
      int n = freeO[o].size();
      for (int g : tg) if (g / 6 == o) { total *= binom(n, 4); n -= 4; }
    }
    for (size_t s = 0; s < allSyms.size(); s++) {
      int s2 = 0, t2 = 0;
      for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << allSyms[s].gmap[g]; if (T >> g & 1) t2 |= 1 << allSyms[s].gmap[g]; }
      if (s2 == S && t2 == T) syms.push_back(s);
    }
    // the lines, and the table of line lengths over every symmetric image
    std::vector<std::string> fs;
    if (DIR *dp = opendir(d.c_str())) {
      while (dirent *e = readdir(dp)) {
        std::string n = e->d_name;
        if (n.size() > 5 && n[0] == 'w' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back(d + "/" + n);
      }
      closedir(dp);
    }
    if (fs.empty()) { fprintf(stderr, "no segments in %s\n", name.c_str()); return false; }
    std::sort(fs.begin(), fs.end());
    std::vector<std::string> raw;
    for (auto &f : fs) {
      FILE *in = fopen(f.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot open a segment\n"); return false; }
      char buf[4096];
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        raw.push_back(l);
      }
      fclose(in);
    }
    lines = raw.size();
    dist.assign(total, 0xFF);
    std::atomic<size_t> ni{0};
    std::atomic<long> bad{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        std::vector<cc::Mv> mv;
        int8_t a[48], b[48], img[48];
        for (size_t li; (li = ni++) < raw.size();) {
          bool ok = cc::parseMoves(raw[li], mv);
          for (auto &m : mv) ok &= m.depth >= 1 && m.depth <= 3;
          if (!ok) { bad++; continue; }
          for (int k = 0; k < 48; k++) a[k] = ((S | T) >> groupOf(k) & 1) ? groupOf(k) : -1;
          for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
            for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
            memcpy(a, b, 48);
          }
          for (int s : syms) {
            for (int k = 0; k < 48; k++) img[allSyms[s].fac[k]] = a[k] < 0 ? -1 : allSyms[s].gmap[a[k]];
            const uint64_t r = rank(img);
            uint8_t cur = std::atomic_ref<uint8_t>(dist[r]).load();
            while (mv.size() < cur && !std::atomic_ref<uint8_t>(dist[r]).compare_exchange_weak(cur, mv.size())) {}
          }
        }
      });
    for (auto &x : th) x.join();
    long un = 0;
    for (uint64_t r = 0; r < total; r++) { if (dist[r] == 0xFF) un++; else W = std::max<int>(W, dist[r]); }
    printf("stage %s: %ld lines (%ld bad), %lu states, %ld not covered, longest line %d\n", name.c_str(), lines, bad.load(), (unsigned long)total, un, W);
    fflush(stdout);
    return !bad && !un;
  }
  // every colouring of the stage's targets on its free facelets (solved groups at home)
  void forEach(const std::function<void(const int8_t *)> &fn) const {
    int8_t col[48];
    for (int k = 0; k < 48; k++) col[k] = (S >> groupOf(k) & 1) ? groupOf(k) : -1;
    std::function<void(size_t)> rec = [&](size_t gi) {
      if (gi == tg.size()) { fn(col); return; }
      const auto &v = freeO[tg[gi] / 6];
      std::function<void(size_t, int)> pick = [&](size_t from, int depth) {
        if (depth == 4) { rec(gi + 1); return; }
        for (size_t i = from; i < v.size(); i++) {
          if (col[v[i]] != -1) continue;
          col[v[i]] = tg[gi];
          pick(i + 1, depth + 1);
          col[v[i]] = -1;
        }
      };
      pick(0, 0);
    };
    rec(0);
  }
};

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = std::max(1u, std::thread::hardware_concurrency());
  int keep = 0;  // 0 every other centre keeps its colour, 1 only the U face set, 2 U and D face sets
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--keep" && i + 1 < argc) {
      const std::string k = argv[++i];
      if (k == "u") keep = 1;
      else if (k == "ud") keep = 2;
      else { fprintf(stderr, "--keep takes u or ud\n"); return 1; }
    }
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 4) { fprintf(stderr, "usage: boundcheck STAGEDIR_A STAGEDIR_B BOUNDARYFILE X [--threads T] [--keep u|ud]\n"); return 1; }
  const int X = atoi(pos[3].c_str());
  const double t0 = now();
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
  for (int n = 0; n <= 48; n++) for (int k = 0; k <= 4; k++) BN[n][k] = binom(n, k);
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        for (int k = 0; k < 48; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          int j = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) j = x;
          if (j < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
          y.fac[k] = j;
        }
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (groupOf(k) == g) { const int h = groupOf(y.fac[k]); if (img >= 0 && img != h) { fprintf(stderr, "a symmetry splits a group\n"); return 1; } img = h; }
          y.gmap[g] = img;
        }
        allSyms.push_back(y);
      }
  }
  Stage A, B;
  if (!A.init(pos[0], threads) || !B.init(pos[1], threads)) { printf("FAIL: a stage certificate does not cover its stage\n"); return 1; }
  if (B.S != (A.S | A.T)) { fprintf(stderr, "stage B must start where stage A ends\n"); return 1; }
  // a block covers its A class through A's symmetries, so each must also keep B's distances: A's
  // symmetries a subset of B's (2026-10-27: was equality; 4ha has 4, 4hb 16)
  if (!std::includes(B.syms.begin(), B.syms.end(), A.syms.begin(), A.syms.end())) { fprintf(stderr, "stage A has a symmetry stage B lacks\n"); return 1; }

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
  printf("boundary: %zu blocks, X = %d, max dB = %d\n", blocks.size(), X, B.W);
  fflush(stdout);

  // B states beyond each threshold, longest first: badList holds those with dB > X - max |L|
  int maxL = 0;
  for (auto &b : blocks) {
    std::vector<cc::Mv> mv;
    if (cc::parseMoves(b[0], mv)) maxL = std::max<int>(maxL, mv.size());
  }
  maxL = std::max(maxL, A.W);
  std::vector<std::vector<int8_t>> byD(B.W + 1);  // free-facelet group ids, concatenated per distance
  std::vector<int> Bfree;
  for (int o = 0; o < 2; o++) for (int k : B.freeO[o]) Bfree.push_back(k);
  const int nBF = Bfree.size();
  const int lowT = std::max(0, X - maxL);
  B.forEach([&](const int8_t *col) {
    const int d = B.dist[B.rank(col)];
    if (d <= lowT) return;
    for (int k : Bfree) byD[d].push_back(col[k]);
  });
  for (int d = lowT + 1; d <= B.W; d++) if (byD[d].size()) printf("  B states at %d: %zu\n", d, byD[d].size() / nBF);
  fflush(stdout);

  // side effects on N = 6..10, as certcheck
  struct NC { int N; std::vector<char> kind; std::vector<std::vector<std::pair<int, int>>> mvMap; std::vector<int> faceOf; size_t F; };
  std::vector<NC> ncs;
  for (int N = 6; N <= 10; N++) {
    cc::Cube CN(N);
    NC n;
    n.N = N;
    n.F = CN.pos.size();
    n.faceOf = CN.faceOf;
    const int M = N - 1;
    n.kind.assign(n.F, 0);
    for (int f = 0; f < 6; f++)
      for (int r = 1; r < M; r++)
        for (int c = 1; c < M; c++) {
          bool pair = false;
          for (int o = 0; o < 2 && !pair; o++) {
            int x = o ? 2 : 1, y = o ? 1 : 2;
            for (int q = 0; q < 4; q++) { if (r == x && c == y) pair = true; const int nx = y, ny = M - x; x = nx, y = ny; }
          }
          n.kind[CN.index(f, r, c)] = pair ? 1 : (r == c || r + c == M) ? 2 : 3;
        }
    n.mvMap.assign(6 * 4 * 4, {});
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 3; d++)
        for (int a = 1; a <= 3; a++)
          for (size_t i = 0; i < n.F; i++) {
            if (!n.kind[i]) continue;
            int j = i;
            for (int q = 0; q < a; q++) j = CN.perms[f][d - 1][j];
            if (j != (int)i) n.mvMap[(f * 4 + d) * 4 + a].push_back({(int)i, j});
          }
    ncs.push_back(n);
  }
  auto sideOk = [&](const std::vector<cc::Mv> &mv) {
    for (auto &n : ncs) {
      std::vector<int> a(n.F), b;
      for (size_t i = 0; i < n.F; i++) a[i] = i;
      for (auto &m : mv) {
        b = a;
        for (auto &[i, j] : n.mvMap[(m.face * 4 + m.depth) * 4 + m.amt]) b[j] = a[i];
        a.swap(b);
      }
      for (size_t i = 0; i < n.F; i++) {
        if (n.kind[i] != 3) continue;
        const int fa = n.faceOf[a[i]], fi = n.faceOf[i];
        if (keep == 0 ? fa != fi : (fa == 0) != (fi == 0) || (keep == 2 && (fa == 5) != (fi == 5))) return false;
      }
    }
    return true;
  };

  // A states with a block (every symmetric image), and the per-block check
  std::vector<uint64_t> hasBlock((A.total + 63) / 64, 0);
  std::atomic<size_t> ni{0};
  std::atomic<long> badBlock{0}, badLine{0}, sideBad{0}, notSolved{0}, uncovered{0}, tried{0}, frameBad{0};
  std::mutex em;
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      int8_t u[48], b8[48], img[48], J0[48], F[48], col[48];
      for (size_t bi; (bi = ni++) < blocks.size();) {
        const auto &blk = blocks[bi];
        std::vector<std::vector<cc::Mv>> mvs(blk.size());
        bool ok = true;
        for (size_t i = 0; i < blk.size(); i++) {
          bool lo = cc::parseMoves(blk[i], mvs[i]) && !mvs[i].empty();
          for (auto &m : mvs[i]) lo &= m.depth >= 1 && m.depth <= 3;
          if (!lo) { badLine++; ok = false; }
        }
        if (!ok) { badBlock++; continue; }
        // the A state: the first line run backwards from A solved
        for (int k = 0; k < 48; k++) u[k] = ((A.S | A.T) >> groupOf(k) & 1) ? groupOf(k) : -1;
        for (auto it = mvs[0].rbegin(); it != mvs[0].rend(); ++it) {
          for (int k = 0; k < 48; k++) b8[mp[it->face][it->depth][4 - it->amt][k]] = u[k];
          memcpy(u, b8, 48);
        }
        // every line: solves u, side effects; its facelet map (the piece at k ends at D[k])
        std::vector<std::array<int8_t, 48>> D(blk.size());
        for (size_t i = 0; i < blk.size(); i++) {
          for (int k = 0; k < 48; k++) {
            int x = k;
            for (auto &m : mvs[i]) x = mp[m.face][m.depth][m.amt][x];
            D[i][k] = x;
          }
          for (int k = 0; k < 48; k++) if (u[k] >= 0 && u[k] != groupOf(D[i][k])) { ok = false; break; }
          if (!ok) { notSolved++; break; }
          if (!sideOk(mvs[i])) { sideBad++; ok = false; break; }
        }
        if (!ok) { badBlock++; continue; }
        for (int s : A.syms) {
          for (int k = 0; k < 48; k++) img[allSyms[s].fac[k]] = u[k] < 0 ? -1 : allSyms[s].gmap[u[k]];
          const uint64_t r = A.rank(img);
          std::atomic_ref<uint64_t>(hasBlock[r >> 6]).fetch_or(1ULL << (r & 63));
        }
        // the B states L carries a colouring onto, beyond X - |L|
        const int cL = mvs[0].size();
        long left = 0, tr = 0;
        for (int d = std::max(X - cL + 1, 0); d <= B.W; d++) {
          if (d <= lowT) continue;
          const auto &v = byD[d];
          for (size_t off = 0; off < v.size(); off += nBF) {
            // the whole colouring: A's targets from u, B's pieces where L carries them onto the state
            int8_t s[48];
            for (int k = 0; k < 48; k++) s[k] = (B.S >> groupOf(k) & 1) ? groupOf(k) : -1;
            for (int i = 0; i < nBF; i++) s[Bfree[i]] = v[off + i];
            for (int k = 0; k < 48; k++) J0[k] = (u[k] >= 0 && (A.T >> u[k] & 1)) ? u[k] : s[(int)D[0][k]];
            bool good = false;
            for (size_t i = 0; i < blk.size() && !good; i++) {
              if ((int)mvs[i].size() >= X) continue;
              for (int k = 0; k < 48; k++) F[(int)D[i][k]] = J0[k];
              for (int k = 0; k < 48; k++) col[k] = (B.S >> groupOf(k) & 1) ? groupOf(k) : ((F[k] >= 0 && (B.T >> F[k] & 1)) ? F[k] : -1);
              const int dB = B.dist[B.rank(col)];
              tr++;
              if (i == 0 && dB != d) frameBad++;
              if ((int)mvs[i].size() + dB <= X) good = true;
            }
            if (!good) left++;
          }
        }
        tried += tr;
        if (left) {
          uncovered += left;
          badBlock++;
          std::lock_guard<std::mutex> g(em);
          if (badBlock <= 5) printf("  block %zu: %ld colourings not solved within %d\n", bi, left, X);
        }
      }
    });
  for (auto &x : th) x.join();
  // 3. every A state that needs a block has one
  long missing = 0, needed = 0;
  for (uint64_t r = 0; r < A.total; r++)
    if (A.dist[r] > X - B.W) {
      needed++;
      if (!(hasBlock[r >> 6] >> (r & 63) & 1)) missing++;
    }
  printf("blocks: %ld bad (%ld bad lines, %ld lines not solving the block's state, %ld with side effects); %ld colourings tried, %ld not solved; frame errors %ld\n",
         badBlock.load(), badLine.load(), notSolved.load(), sideBad.load(), tried.load(), uncovered.load(), frameBad.load());
  printf("A states with dA > %d: %ld, %ld without a block\n", X - B.W, needed, missing);
  const bool ok = !badBlock && !missing && !frameBad;
  printf("%s: every position is solved through %s and %s in at most %d moves (%.0fs)\n", ok ? "PASS" : "FAIL", A.name.c_str(), B.name.c_str(), X, now() - t0);
  return ok ? 0 : 1;
}
