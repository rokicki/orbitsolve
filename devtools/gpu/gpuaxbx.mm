// gpuaxbx: axbx (devtools/axbx.cpp) with its inner loops on the GPU (Metal); writes blocks for
// gpuaxbcheck / axbcheck.  Same passes as axbx, per 3ax class with line L0 (c0 = |L0|) longer than
// X - worst(4op):
//   1. one action: every action a on the class's 3ax state u (GPU), kept when |a| + d3(a(u)) <= c0 + slack;
//      paths a + line(a(u)) plus the stage-symmetric copies of L0 that fix u (CPU: simplify as axbx,
//      the map M = D_P o D_L0^-1 on the 4 (orbit, axis) sets of 8), cheapest first, one per M;
//      every bad 4op state (|L0| + d4 > X) tries them in order on the GPU, the first that solves it
//      is used;
//   2. --deep S, for classes with states left: two actions a1 a2 then the line: a1 with
//      |a1| + d3(a1(u)) <= c0 + slack + S, a2 with |a1| + |a2| + d3 <= c0 + slack (GPU: the action
//      kernel run on the a1(u)), and the states left try those paths on the GPU.
// The block of a class: L0 and its used paths in order.  Differences from axbx: duplicates are
// dropped by M (the map on the 4op pieces) rather than by the whole facelet map, so a path that
// axbx keeps but that moves the 4op pieces like an earlier one is dropped (it could cover nothing
// new); no --check replay (gpuaxbcheck checks the blocks).
//
//   gpuaxbx STAGEDIR_3AX STAGEDIR_4OP X --out FILE [--base POOL]... [--prop C (8)] [--slack L (2)]
//           [--deep S] [--threads T (2)] [--batch K (256)] [--sample K] [--only FILE] [--left FILE]
//           [--order random|long|file (random)] [--seed N]
// --stage-acts L: the actions are every distinct effect of at most L outer turns (the 3ax stage moves; L = 6
// gives all 4,095 face rotations) instead of the single outer turns; pools only if --base is given.
// --order: the order the classes are done in (the blocks do not depend on it): random (a steady rate, so a
// predictable finish), long (longest lines first, random within a length: a final run fails early), file
// (the stage files' order: by length, shortest first).  The seed is printed; --seed N repeats an order.
#import <Metal/Metal.h>
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <functional>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
static int mp[6][4][4][48];
static inline int faceK(int k) { return k / 8; }
static inline int orbK(int k) { return (k % 8) / 4; }
static inline int grp(int k) { return orbK(k) * 6 + faceK(k); }
static const int AX[6] = {0, 1, 2, 1, 2, 0};  // U L F R B D
static uint64_t C_[17][17];
static std::vector<int> sideO[2], axO[2][3];
static int sideIdx[48], axIdx[48];
static int16_t rk16[65536];
static uint8_t r70[256];

// 3ax state as per-orbit 16-bit masks over sideO (bit = an F/B piece); rank as axbcheck's rank3
static int64_t rank3m(uint32_t m0, uint32_t m1) {
  const int a = rk16[m0], b = rk16[m1];
  if (a < 0 || b < 0) return -1;
  return (int64_t)a * 12870 + b;
}
// a colouring (groups) -> 3ax masks; false if not a 3ax start
static bool masks3(const int8_t *c, uint32_t *m) {
  for (int k = 0; k < 48; k++) if (AX[faceK(k)] == 0 && c[k] != grp(k)) return false;
  for (int o = 0; o < 2; o++) {
    m[o] = 0;
    for (int i = 0; i < 16; i++) {
      const int g = c[sideO[o][i]];
      if (g / 6 != o || AX[g % 6] == 0) return false;
      if (AX[g % 6] == 2) m[o] |= 1u << i;
    }
    if (__builtin_popcount(m[o]) != 8) return false;
  }
  return true;
}
// 4op: per (orbit, axis) group g = 0..3 (o0 F/B, o0 L/R, o1 F/B, o1 L/R) the 8-bit mask of F (resp. L) pieces
static const int GO[4] = {0, 0, 1, 1}, GA[4] = {2, 1, 2, 1};
static bool masks4(const int8_t *c, uint8_t *m) {
  for (int k = 0; k < 48; k++) {
    if (AX[faceK(k)] == 0) { if (c[k] != grp(k)) return false; continue; }
    if (c[k] / 6 != orbK(k) || AX[c[k] % 6] != AX[faceK(k)]) return false;
  }
  for (int g = 0; g < 4; g++) {
    m[g] = 0;
    for (int i = 0; i < 8; i++) if (c[axO[GO[g]][GA[g]][i]] % 6 == GA[g]) m[g] |= 1 << i;
    if (__builtin_popcount(m[g]) != 4) return false;
  }
  return true;
}
static int64_t rank4m(const uint8_t *m) { return ((int64_t)(r70[m[0]] * 70 + r70[m[1]]) * 70 + r70[m[2]]) * 70 + r70[m[3]]; }
static void unplay(const std::vector<cc::Mv> &mv, const int8_t *a, int8_t *out) {
  int8_t x[48], y[48];
  memcpy(x, a, 48);
  for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
    for (int k = 0; k < 48; k++) y[mp[it->face][it->depth][4 - it->amt][k]] = x[k];
    memcpy(x, y, 48);
  }
  memcpy(out, x, 48);
}
static void mapOf(const std::vector<cc::Mv> &mv, int8_t *D) {
  for (int k = 0; k < 48; k++) {
    int x = k;
    for (auto &m : mv) x = mp[m.face][m.depth][m.amt][x];
    D[k] = x;
  }
}
struct Sym { int fac[48], gmap[12], face[6]; bool flip; };
static std::vector<Sym> syms;  // the 16 keeping the U-D axis
static void image(const Sym &y, const int8_t *a, int8_t *b) { for (int k = 0; k < 48; k++) b[y.fac[k]] = y.gmap[a[k]]; }
static std::vector<cc::Mv> symMoves(const Sym &y, const std::vector<cc::Mv> &mv) {
  std::vector<cc::Mv> r;
  for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
  return r;
}
// as axbx: merges across turns that commute with them on the centres (same axis; or same depth)
static void simplify(std::vector<cc::Mv> &v) {
  std::vector<cc::Mv> o;
  for (auto m : v) {
    int i = (int)o.size() - 1;
    bool merged = false;
    for (; i >= 0; i--) {
      if (o[i].face == m.face && o[i].depth == m.depth) {
        const int a = (o[i].amt + m.amt) % 4;
        if (a) o[i].amt = a;
        else o.erase(o.begin() + i);
        merged = true;
        break;
      }
      const bool com = AX[o[i].face] == AX[m.face] || o[i].depth == m.depth;
      if (!com) break;
    }
    if (!merged) o.push_back(m);
  }
  v.swap(o);
}
static std::string text(const std::vector<cc::Mv> &mv) {
  std::string s;
  for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
  return s;
}
static bool poolMoves(const char *buf, std::vector<cc::Mv> &mv) {  // as axbx
  size_t n = strlen(buf);
  if (!n || buf[n - 1] != '\n' || buf[0] == '#') return false;
  std::string l(buf);
  while (!l.empty() && (l.back() == '\n' || l.back() == ' ' || l.back() == '\r')) l.pop_back();
  if (l.size() >= 3 && l[l.size() - 3] == ' ' && l[l.size() - 2] == 'D') l.resize(l.size() - 3);
  size_t a = 0;
  while (a < l.size() && l[a] == ' ') a++;
  size_t e = a;
  while (e < l.size() && l[e] >= '0' && l[e] <= '9') e++;
  if (e > a && (e == l.size() || l[e] == ' ')) a = e;
  if (l.find('\t') != std::string::npos) a = l.find('\t') + 1;
  if (!cc::parseMoves(l.substr(a), mv) || mv.empty()) return false;
  for (auto &m : mv) if (m.depth < 1 || m.depth > 3) return false;
  return true;
}
static bool forChunks(const std::string &f, size_t chunk, const std::function<void(std::vector<std::string> &)> &fn) {
  FILE *in = fopen(f.c_str(), "r");
  if (!in) return false;
  char buf[4096];
  std::vector<std::string> v;
  while (fgets(buf, sizeof buf, in)) {
    std::string l(buf);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    v.push_back(std::move(l));
    if (v.size() == chunk) { fn(v); v.clear(); }
  }
  if (!v.empty()) fn(v);
  fclose(in);
  return true;
}

// a stage table: dist over every state (3ax: rank3m, 4op: rank4m); for 3ax also a line of exactly
// that length per state (two passes) and the lines themselves
struct Stage {
  bool four;
  uint64_t total;
  std::vector<uint8_t> dist, srcSym;
  std::vector<uint32_t> srcLine, lineOff;
  std::vector<uint8_t> lineBytes;
  int W = 0;
  std::vector<cc::Mv> lineAt(size_t i) const {
    std::vector<cc::Mv> mv;
    for (uint32_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
    return mv;
  }
  std::vector<cc::Mv> lineOf(uint64_t r) const { return symMoves(syms[srcSym[r]], lineAt(srcLine[r])); }
  int64_t rankOf(const int8_t *c) const {
    if (four) { uint8_t m[4]; return masks4(c, m) ? rank4m(m) : -1; }
    uint32_t m[2];
    return masks3(c, m) ? rank3m(m[0], m[1]) : -1;
  }
  bool load(const std::string &dir, int threads, bool keep) {
    const double t0 = now();
    total = four ? 70ull * 70 * 70 * 70 : 12870ull * 12870;
    std::vector<std::pair<int, std::string>> fs;
    if (DIR *dp = opendir(dir.c_str())) {
      while (dirent *e = readdir(dp)) {
        std::string n = e->d_name;
        if (n.size() > 5 && n[0] == 'l' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back({atoi(n.c_str() + 1), dir + "/" + n});
      }
      closedir(dp);
    }
    if (fs.empty()) { fprintf(stderr, "no lNN.txt files in %s\n", dir.c_str()); return false; }
    std::sort(fs.begin(), fs.end());
    dist.assign(total, 0xFF);
    if (keep) srcSym.assign(total, 0), srcLine.assign(total, 0xFFFFFFFFu);
    std::atomic<long> bad{0};
    long nl = 0;
    for (int pass = 0; pass < (keep ? 2 : 1); pass++) {
      nl = 0;
      for (auto &[nn, f] : fs) {
        if (!forChunks(f, 1 << 19, [&](std::vector<std::string> &raw) {
              const long base = nl;
              nl += raw.size();
              if (keep && pass == 0)
                for (auto &l : raw) {
                  std::vector<cc::Mv> mv;
                  cc::parseMoves(l, mv);
                  lineOff.push_back(lineBytes.size());
                  for (auto &m : mv) lineBytes.push_back(mvByte(m));
                }
              std::atomic<size_t> ni{0};
              std::vector<std::thread> th;
              for (int t = 0; t < threads; t++)
                th.emplace_back([&] {
                  int8_t g[48], c[48], b[48];
                  for (int k = 0; k < 48; k++) g[k] = grp(k);
                  std::vector<cc::Mv> mv;
                  for (size_t i; (i = ni++) < raw.size();) {
                    bool ok = cc::parseMoves(raw[i], mv);
                    for (auto &m : mv) ok &= m.depth >= 1 && m.depth <= 3 && m.amt >= 1 && m.amt <= 3;
                    if (!ok) { if (pass == 0) bad++; continue; }
                    unplay(mv, g, c);
                    for (size_t s = 0; s < syms.size(); s++) {
                      image(syms[s], c, b);
                      const int64_t r = rankOf(b);
                      if (r < 0) { if (pass == 0) bad++; continue; }
                      if (pass == 0) {
                        uint8_t cur = __atomic_load_n(&dist[r], __ATOMIC_RELAXED);
                        while (mv.size() < cur && !__atomic_compare_exchange_n(&dist[r], &cur, (uint8_t)mv.size(), true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
                      } else if (mv.size() == dist[r]) {
                        srcLine[r] = base + i, srcSym[r] = s;  // any line of the shortest length will do
                      }
                    }
                  }
                });
              for (auto &x : th) x.join();
            })) return false;
      }
    }
    if (keep) lineOff.push_back(lineBytes.size());
    long un = 0;
    for (uint64_t r = 0; r < total; r++) { if (dist[r] == 0xFF) un++; else W = std::max<int>(W, dist[r]); }
    fprintf(stderr, "%s: %ld lines (%ld bad or not a stage start), %lu states, %ld not covered, worst %d (%.0fs)\n", four ? "4op" : "3ax", nl, bad.load(), (unsigned long)total, un, W, now() - t0);
    return !bad && !un;
  }
};

static const char *kSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct AP { uint nact, ncls, cap; };
// every (action, 3ax state): y = a(u) (per orbit: the 16-bit mask of F/B pieces moved by the action),
// kept when |a| + d3(y) <= lim
kernel void acts(device const ushort2 *cls [[buffer(0)]], device const uchar *d3 [[buffer(1)]],
                 device const uchar *act [[buffer(2)]], device const uchar *alen [[buffer(3)]],
                 device const uchar *lim [[buffer(4)]], device atomic_uint *cnt [[buffer(5)]],
                 device uint *out [[buffer(6)]], constant AP &P [[buffer(7)]], device const short *rk16 [[buffer(8)]],
                 uint2 gid [[thread_position_in_grid]]) {
  const uint a = gid.x, k = gid.y;
  if (a >= P.nact) return;
  const uint al = alen[a], L = lim[k];
  if (al > L) return;
  device const uchar *m = act + a * 32;
  const ushort2 c = cls[k];
  uint x = c.x, y0 = 0;
  while (x) { y0 |= 1u << m[ctz(x)]; x &= x - 1; }
  x = c.y;
  uint y1 = 0;
  while (x) { y1 |= 1u << m[16 + ctz(x)]; x &= x - 1; }
  const uint ry = (uint)rk16[y0] * 12870u + (uint)rk16[y1];
  if (al + d3[ry] > L) return;
  const uint i = atomic_fetch_add_explicit(cnt, 1u, memory_order_relaxed);
  if (i < P.cap) { out[3 * i] = k; out[3 * i + 1] = a; out[3 * i + 2] = ry; }
}
struct CP { uint X, leftCap; };
struct Cls { uint path0, npath, nstates, used0, list0, pad0, pad1, pad2; };
// every (bad 4op state, class): paths in order until |P| + d4(M(s)) <= X; states either the prefix of
// the global list (list0 == ~0) or listed (indices from list0 in idx)
kernel void check(device const uchar4 *st [[buffer(0)]], device const uchar *d4 [[buffer(1)]],
                  device const uchar *paths [[buffer(2)]], device const uchar *plen [[buffer(3)]],
                  device const Cls *cl [[buffer(4)]], device atomic_uint *used [[buffer(5)]],
                  device atomic_uint *left [[buffer(6)]], device uint *leftOut [[buffer(7)]],
                  constant CP &P [[buffer(8)]], constant uchar *r70 [[buffer(9)]], device const uint *idx [[buffer(10)]],
                  uint2 gid [[thread_position_in_grid]]) {
  const uint k = gid.y;
  const Cls C = cl[k];
  if (gid.x >= C.nstates) return;
  const uint si = C.list0 == 0xFFFFFFFFu ? gid.x : idx[C.list0 + gid.x];
  const uchar4 s = st[si];
  for (uint i = 0; i < C.npath; i++) {
    const uint len = plen[C.path0 + i];
    device const uchar *m = paths + (C.path0 + i) * 32;
    uint r = 0;
    for (uint g = 0; g < 4; g++) {
      uint x = s[g], y = 0;
      while (x) { y |= 1u << m[g * 8 + ctz(x)]; x &= x - 1; }
      r = r * 70 + r70[y];
    }
    if (len + d4[r] <= P.X) {
      atomic_fetch_or_explicit(&used[C.used0 + i / 32], 1u << (i % 32), memory_order_relaxed);
      return;
    }
  }
  const uint j = atomic_fetch_add_explicit(&left[0], 1u, memory_order_relaxed);
  if (j < P.leftCap) { leftOut[2 * j] = k; leftOut[2 * j + 1] = si; }
}
)";

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int C = 8, SL = 2, DEEP = -1, C2 = -1, threads = 2, batch = 256, samp = 1, stageActs = 0;
  std::string order = "random";
  uint64_t seed = std::random_device{}() * 0x100000000ull + std::random_device{}();
  std::vector<std::string> pos, base;
  std::string outFile, onlyFile, leftFile;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--prop" && i + 1 < argc) C = atoi(argv[++i]);
    else if (o == "--slack" && i + 1 < argc) SL = atoi(argv[++i]);
    else if (o == "--deep" && i + 1 < argc) DEEP = atoi(argv[++i]);
    else if (o == "--deepprop" && i + 1 < argc) C2 = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--batch" && i + 1 < argc) batch = atoi(argv[++i]);
    else if (o == "--sample" && i + 1 < argc) samp = std::max(1, atoi(argv[++i]));
    else if (o == "--out" && i + 1 < argc) outFile = argv[++i];
    else if (o == "--only" && i + 1 < argc) onlyFile = argv[++i];
    else if (o == "--left" && i + 1 < argc) leftFile = argv[++i];
    else if (o == "--order" && i + 1 < argc) order = argv[++i];
    else if (o == "--stage-acts" && i + 1 < argc) stageActs = atoi(argv[++i]);
    else if (o == "--seed" && i + 1 < argc) seed = strtoull(argv[++i], nullptr, 10);
    else if (o == "--base" && i + 1 < argc) base.push_back(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (order != "random" && order != "long" && order != "file") { fprintf(stderr, "--order: random, long or file\n"); return 1; }
  if (pos.size() != 3 || outFile.empty()) {
    fprintf(stderr, "usage: gpuaxbx STAGEDIR_3AX STAGEDIR_4OP X --out FILE [--base POOL]... [--prop C] [--slack L] [--deep S] [--deepprop C2] [--threads T] [--batch K] [--sample K] [--only FILE] [--left FILE] [--order random|long|file] [--seed N] [--stage-acts L]\n");
    return 1;
  }
  const int X = atoi(pos[2].c_str());
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
  for (int k = 0; k < 48; k++) sideIdx[k] = axIdx[k] = -1;
  for (int k = 0; k < 48; k++)
    if (AX[faceK(k)]) {
      sideIdx[k] = sideO[orbK(k)].size(), sideO[orbK(k)].push_back(k);
      axIdx[k] = axO[orbK(k)][AX[faceK(k)]].size(), axO[orbK(k)][AX[faceK(k)]].push_back(k);
    }
  // colex ranks (as axbcheck: per orbit the 16 side facelets in k order; per (orbit, axis) the 8)
  for (int v = 0; v < 65536; v++) {
    rk16[v] = -1;
    if (__builtin_popcount(v) != 8) continue;
    int j = 0, ro = 0;
    for (int i = 0; i < 16; i++) if (v >> i & 1) ro += C_[i][++j];
    rk16[v] = ro;
  }
  for (int v = 0; v < 256; v++) {
    r70[v] = 0;
    if (__builtin_popcount(v) != 4) continue;
    int j = 0, ro = 0;
    for (int i = 0; i < 8; i++) if (v >> i & 1) ro += C_[i][++j];
    r70[v] = ro;
  }
  std::vector<std::pair<std::array<int, 6>, bool>> all48;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        auto map = [&](const std::array<int, 3> &p) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          return q;
        };
        for (int k = 0; k < 48; k++) {
          const auto q = map(C8.pos[pf[k]]);
          y.fac[k] = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
          if (y.fac[k] < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (grp(k) == g) y.gmap[g] = grp(y.fac[k]);
        for (int f = 0; f < 6; f++) {
          const auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        y.flip = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1) < 0;
        all48.push_back({{y.face[0], y.face[1], y.face[2], y.face[3], y.face[4], y.face[5]}, y.flip});
        bool keeps = true;
        for (int g = 0; g < 12; g++) if ((AX[g % 6] == 0) != (AX[y.gmap[g] % 6] == 0)) keeps = false;
        if (keeps) syms.push_back(y);
      }
    if (syms.size() != 16) { fprintf(stderr, "expected 16 stage symmetries\n"); return 1; }
  }
  // self-test: a symmetric copy of a move sequence has the conjugated facelet map
  {
    std::vector<cc::Mv> mv;
    uint32_t x = 12345;
    for (int i = 0; i < 20; i++) { x = x * 1103515245 + 12345; mv.push_back({(int)(x >> 8) % 6, 1 + (int)(x >> 12) % 3, 1 + (int)(x >> 16) % 3}); }
    int8_t D[48], E[48];
    mapOf(mv, D);
    for (auto &y : syms) {
      mapOf(symMoves(y, mv), E);
      for (int k = 0; k < 48; k++) if (E[y.fac[k]] != y.fac[(int)D[k]]) { fprintf(stderr, "symmetry self-test failed\n"); return 1; }
    }
  }

  Stage A, B;
  A.four = false, B.four = true;
  if (!B.load(pos[1], threads, false) || !A.load(pos[0], threads, true)) return 1;
  {  // self-check: stored source lines solve their states
    long bad = 0;
    int8_t g[48], c[48];
    for (int k = 0; k < 48; k++) g[k] = grp(k);
    for (uint64_t r = 0; r < A.total; r += 99991) {
      const auto l = A.lineOf(r);
      unplay(l, g, c);
      if (A.rankOf(c) != (int64_t)r || l.size() != A.dist[r]) bad++;
    }
    if (bad) { fprintf(stderr, "3ax source lines do not solve their states (%ld)\n", bad); return 1; }
  }

  // actions (as axbx): outer turns and the 48 cube images of pool algorithms (and inverses) of <= C
  // moves, simplified, that keep the pair's U and D pieces home; by facelet map, cheapest kept
  struct Act { int8_t D[48]; std::vector<cc::Mv> mv; };
  std::vector<Act> acts;
  {
    std::unordered_map<std::string, size_t> idx;
    auto add = [&](std::vector<cc::Mv> mv) {
      simplify(mv);
      if (mv.empty() || (int)mv.size() > C) return;
      Act a;
      a.mv = mv;
      mapOf(mv, a.D);
      bool id = true;
      for (int k = 0; k < 48; k++) if (a.D[k] != k) id = false;
      if (id) return;
      for (int k = 0; k < 48; k++) if (AX[faceK(k)] == 0 && grp(a.D[k]) != grp(k)) return;
      std::string key((const char *)a.D, 48);
      auto it = idx.find(key);
      if (it != idx.end()) { if (acts[it->second].mv.size() > mv.size()) acts[it->second].mv = mv; return; }
      idx[key] = acts.size();
      acts.push_back(a);
    };
    if (stageActs <= 0) for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
    else {
      // --stage-acts L (2026-11-09): every distinct effect of at most L stage moves (the 18 outer turns: the
      // only moves that keep the other obliques' faces; on centres they commute, so L = 6 is all 4,095
      // face rotations), breadth first, so each effect keeps its shortest sequence
      std::vector<cc::Mv> sm;
      for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) sm.push_back({f, 1, a});
      size_t f0 = acts.size();
      for (auto &m : sm) add({m});
      for (int len = 2; len <= stageActs; len++) {
        const size_t f1 = acts.size();
        for (size_t i = f0; i < f1; i++)
          for (auto &m : sm) {
            if (acts[i].mv.back().face == m.face) continue;
            auto mv = acts[i].mv;
            mv.push_back(m);
            add(mv);
          }
        fprintf(stderr, "stage actions of %d moves: %zu new\n", len, acts.size() - f1);
        f0 = f1;
      }
    }
    for (auto &bf : base) {
      FILE *in = fopen(bf.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot read %s\n", bf.c_str()); return 1; }
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        if (!poolMoves(buf, mv) || (int)mv.size() > C) continue;
        std::vector<cc::Mv> inv;
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
        for (auto &[fm, fl] : all48) {
          std::vector<cc::Mv> a, b;
          for (auto &m : mv) a.push_back({fm[m.face], m.depth, fl ? 4 - m.amt : m.amt});
          for (auto &m : inv) b.push_back({fm[m.face], m.depth, fl ? 4 - m.amt : m.amt});
          add(a), add(b);
        }
      }
      fclose(in);
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.mv.size() < b.mv.size(); });
    fprintf(stderr, "%zu actions of cost <= %d keep U and D\n", acts.size(), C);
  }
  // per action: where it sends each side facelet (side index, orbit 0 then 1)
  std::vector<uint8_t> actTab(acts.size() * 32), actLen(acts.size());
  for (size_t a = 0; a < acts.size(); a++) {
    actLen[a] = acts[a].mv.size();
    for (int o = 0; o < 2; o++)
      for (int i = 0; i < 16; i++) {
        const int to = acts[a].D[sideO[o][i]];
        if (sideIdx[to] < 0 || orbK(to) != o) { fprintf(stderr, "an action moves a side piece off the sides\n"); return 1; }
        actTab[a * 32 + o * 16 + i] = sideIdx[to];
      }
  }
  auto actOn = [&](size_t a, const uint16_t *u, uint16_t *y) {
    for (int o = 0; o < 2; o++) {
      uint32_t x = u[o], r = 0;
      while (x) { r |= 1u << actTab[a * 32 + o * 16 + __builtin_ctz(x)]; x &= x - 1; }
      y[o] = r;
    }
  };

  // bad 4op states, longest first; cntAt[d] = how many have d4 >= d
  const int lowT = std::max(0, X - A.W);
  std::vector<uint8_t> states;
  std::vector<size_t> cntAt(B.W + 2, 0);
  {
    std::vector<std::vector<uint32_t>> byD(B.W + 1);
    for (uint64_t r = 0; r < B.total; r++) if (B.dist[r] > lowT) byD[B.dist[r]].push_back(r);
    uint8_t ur70[70];
    for (int v = 0; v < 256; v++) if (__builtin_popcount(v) == 4) ur70[r70[v]] = v;
    for (int d = B.W; d >= 0; d--) {
      cntAt[d] = cntAt[d + 1] + byD[d].size();
      for (uint32_t r : byD[d]) {
        uint8_t m[4];
        uint32_t q = r;
        for (int g = 3; g >= 0; g--) m[g] = ur70[q % 70], q /= 70;
        if (rank4m(m) != r) { fprintf(stderr, "4op unrank self-test failed\n"); return 1; }
        states.insert(states.end(), m, m + 4);
      }
      if (d > lowT) fprintf(stderr, "4op states at %d: %zu\n", d, byD[d].size());
    }
  }

  // the classes
  std::vector<uint32_t> todo;
  for (size_t i = 0; i + 1 < A.lineOff.size(); i++) if ((int)(A.lineOff[i + 1] - A.lineOff[i]) > X - B.W) todo.push_back(i);
  if (samp > 1) {
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i += samp) todo[k++] = todo[i];
    todo.resize(k);
  }
  if (!onlyFile.empty()) {
    std::unordered_set<std::string> want;
    if (!forChunks(onlyFile, 1 << 16, [&](std::vector<std::string> &v) { for (auto &l : v) { while (!l.empty() && l.back() == ' ') l.pop_back(); if (!l.empty()) want.insert(l); } })) {
      fprintf(stderr, "cannot read the --only file\n");
      return 1;
    }
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i++) if (want.count(text(A.lineAt(todo[i])))) todo[k++] = todo[i];
    todo.resize(k);
    fprintf(stderr, "--only: %zu of %zu lines found\n", k, want.size());
  }
  // 2026-11-07 (user): random order for a predictable finish; longest first for a final run
  if (order != "file") {
    std::mt19937_64 rng(seed);
    std::shuffle(todo.begin(), todo.end(), rng);
    if (order == "long")
      std::stable_sort(todo.begin(), todo.end(), [&](uint32_t a, uint32_t b) { return A.lineOff[a + 1] - A.lineOff[a] > A.lineOff[b + 1] - A.lineOff[b]; });
  }
  fprintf(stderr, "3ax lines longer than %d: %zu to do (order %s, seed %llu)\n", X - B.W, todo.size(), order.c_str(), (unsigned long long)seed);
  // --out FILE.zst: written through zstd (2026-11-05: AX 38's broad-pass blocks were 24 GB plain)
  const bool outZst = outFile.size() > 4 && outFile.compare(outFile.size() - 4, 4, ".zst") == 0;
  FILE *out = outZst ? popen(("zstd -q -T2 -10 -f -o '" + outFile + "'").c_str(), "w") : fopen(outFile.c_str(), "w"), *leftOut = nullptr;
  if (!out) { fprintf(stderr, "cannot write %s\n", outFile.c_str()); return 1; }
  if (!leftFile.empty() && !(leftOut = fopen(leftFile.c_str(), "w"))) { fprintf(stderr, "cannot write %s\n", leftFile.c_str()); return 1; }

  // GPU
  id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
  if (!dev) { fprintf(stderr, "no Metal device\n"); return 1; }
  NSError *err = nil;
  id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:kSrc] options:nil error:&err];
  if (!lib) { fprintf(stderr, "Metal compile: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLComputePipelineState> psActs = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"acts"] error:&err];
  id<MTLComputePipelineState> psCheck = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"check"] error:&err];
  if (!psActs || !psCheck) { fprintf(stderr, "pipeline: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLCommandQueue> q = [dev newCommandQueue];
  // at least 16 bytes; an empty input gets a zeroed buffer (2026-11-09: gpubound read 16 bytes from a null
  // pointer for an empty vector)
  auto buf = [&](const void *p, size_t n) {
    return n ? [dev newBufferWithBytes:p length:std::max<size_t>(n, 16) options:MTLResourceStorageModeShared]
             : [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
  };
  auto zbuf = [&](size_t n) { return [dev newBufferWithLength:std::max<size_t>(n, 16) options:MTLResourceStorageModeShared]; };
  id<MTLBuffer> bD3 = buf(A.dist.data(), A.dist.size()), bD4 = buf(B.dist.data(), B.dist.size());
  id<MTLBuffer> bAct = buf(actTab.data(), actTab.size()), bALen = buf(actLen.data(), actLen.size());
  id<MTLBuffer> bRk16 = buf(rk16, sizeof rk16), bR70 = buf(r70, 256), bSt = buf(states.data(), states.size());
  const uint32_t recCap = 1u << 24;
  uint32_t leftCap = 1u << 24;  // grows for one class with more left states (at most the 4op states)
  const size_t maxCls = std::max<size_t>(1, recCap / acts.size());  // every action of every class fits
  // --deepprop C2: the two-action pass uses only the actions of at most C2 moves (a prefix of acts)
  if (C2 < 0) C2 = C;
  size_t nDeepAct = 0;
  while (nDeepAct < acts.size() && (int)acts[nDeepAct].mv.size() <= C2) nDeepAct++;
  if (DEEP >= 0) fprintf(stderr, "deep pass: %zu actions of cost <= %d\n", nDeepAct, C2);
  id<MTLBuffer> bCls = zbuf(maxCls * 4), bLim = zbuf(maxCls), bCnt = zbuf(16), bRec = zbuf((size_t)recCap * 12);
  id<MTLBuffer> bLeft = zbuf(16), bLeftOut = zbuf((size_t)leftCap * 8);
  id<MTLBuffer> bPaths = nil, bPLen = nil, bUsed = nil, bIdx = nil, bClsInfo = nil;
  size_t pathsCap = 0, usedCap = 0, idxCap = 0, ciCap = 0;
  struct AP { uint32_t nact, ncls, cap; };
  struct CP { uint32_t X, leftCap; };
  struct ClsInfo { uint32_t path0, npath, nstates, used0, list0, pad0, pad1, pad2; };
  double tAct = 0, tCpu = 0, tChk = 0;

  // the action kernel on a list of 3ax states (masks) with limits: records (state index, action, y),
  // in groups that cannot overflow the record buffer
  // nact: use the first nact actions only (acts are sorted by cost; 0 = all)
  auto runActs = [&](const std::vector<std::array<uint16_t, 2>> &cs, const std::vector<uint8_t> &lim, std::vector<std::array<uint32_t, 3>> &recs, size_t nact = 0) {
    if (!nact) nact = acts.size();
    const double ta = now();
    recs.clear();
    for (size_t g0 = 0; g0 < cs.size(); g0 += maxCls) {
      const size_t n = std::min(maxCls, cs.size() - g0);
      memcpy([bCls contents], cs.data() + g0, n * 4);
      memcpy([bLim contents], lim.data() + g0, n);
      *(uint32_t *)[bCnt contents] = 0;
      AP P = {(uint32_t)nact, (uint32_t)n, recCap};
      id<MTLCommandBuffer> cb = [q commandBuffer];
      id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
      [ce setComputePipelineState:psActs];
      [ce setBuffer:bCls offset:0 atIndex:0];
      [ce setBuffer:bD3 offset:0 atIndex:1];
      [ce setBuffer:bAct offset:0 atIndex:2];
      [ce setBuffer:bALen offset:0 atIndex:3];
      [ce setBuffer:bLim offset:0 atIndex:4];
      [ce setBuffer:bCnt offset:0 atIndex:5];
      [ce setBuffer:bRec offset:0 atIndex:6];
      [ce setBytes:&P length:sizeof P atIndex:7];
      [ce setBuffer:bRk16 offset:0 atIndex:8];
      [ce dispatchThreads:MTLSizeMake(nact, n, 1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(psActs.maxTotalThreadsPerThreadgroup, 256), 1, 1)];
      [ce endEncoding];
      [cb commit];
      [cb waitUntilCompleted];
      if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU acts failed: %s\n", [[cb.error description] UTF8String]); exit(1); }
      const uint32_t c = *(uint32_t *)[bCnt contents];
      if (c > recCap) { fprintf(stderr, "action records overflow\n"); exit(1); }
      const uint32_t *r = (const uint32_t *)[bRec contents];
      for (uint32_t i = 0; i < c; i++) recs.push_back({(uint32_t)(r[3 * i] + g0), r[3 * i + 1], r[3 * i + 2]});
    }
    tAct += now() - ta;
  };

  // a path: kind 0 = symmetry y of L0; 1 = action a then line of 3ax state y; 2 = actions a, a2 then line of y
  struct Path { uint8_t c, kind; uint32_t a, a2, y; std::array<uint8_t, 32> M; };
  auto hashM = [](const std::array<uint8_t, 32> &M) {
    uint64_t h = 0x9E3779B97F4A7C15ULL, w[4];
    memcpy(w, M.data(), 32);
    for (int i = 0; i < 4; i++) { h ^= w[i] + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2); h *= 0xBF58476D1CE4E5B9ULL; h ^= h >> 31; }
    return h;
  };
  long nOk = 0, nFail = 0, nDup = 0, nDeep = 0;
  uint64_t nBad = 0, nLeft = 0, nPaths = 0;
  std::vector<long> usedHist(64, 0), leftHist(5, 0);
  double lastRep = now();
  for (size_t b0 = 0; b0 < todo.size(); b0 += batch) {
    @autoreleasepool {  // drain the Metal command buffers and encoders every batch (2026-11-04: they piled up)
    const size_t K = std::min<size_t>(batch, todo.size() - b0);
    std::vector<std::vector<cc::Mv>> L0(K);
    std::vector<std::array<int8_t, 48>> U(K), D0(K);
    std::vector<std::array<uint16_t, 2>> cs(K);
    std::vector<uint8_t> lim(K), skip(K, 0);
    int8_t g[48];
    for (int k = 0; k < 48; k++) g[k] = grp(k);
    for (size_t k = 0; k < K; k++) {
      L0[k] = A.lineAt(todo[b0 + k]);
      unplay(L0[k], g, U[k].data());
      uint32_t m[2];
      if (!masks3(U[k].data(), m)) { fprintf(stderr, "a 3ax line is not a stage start\n"); return 1; }
      cs[k] = {(uint16_t)m[0], (uint16_t)m[1]};
      mapOf(L0[k], D0[k].data());
      if (A.dist[rank3m(m[0], m[1])] != L0[k].size()) skip[k] = 1;  // a duplicate class line
      if (X - (int)L0[k].size() >= B.W) skip[k] = 2;                 // nothing bad
      lim[k] = skip[k] ? 0 : L0[k].size() + SL;
    }
    std::vector<std::array<uint32_t, 3>> recs;
    runActs(cs, lim, recs);
    std::vector<std::vector<Path>> paths(K);
    std::vector<std::unordered_set<uint64_t>> seen(K);
    auto pathMoves = [&](size_t k, const Path &p) {
      std::vector<cc::Mv> mv;
      if (p.kind == 0) mv = symMoves(syms[p.y], L0[k]);
      else {
        mv = acts[p.a].mv;
        if (p.kind == 2) mv.insert(mv.end(), acts[p.a2].mv.begin(), acts[p.a2].mv.end());
        const auto l = A.lineOf(p.y);
        mv.insert(mv.end(), l.begin(), l.end());
      }
      simplify(mv);
      return mv;
    };
    // paths of class k from candidates, cheapest first, new maps only
    auto buildPaths = [&](size_t k, std::vector<Path> &cand) {
      std::vector<Path> &P = paths[k];
      int inv0[48];
      for (int x = 0; x < 48; x++) inv0[(int)D0[k][x]] = x;
      std::vector<Path> ok;
      for (auto &p : cand) {
        const auto mv = pathMoves(k, p);
        if ((int)mv.size() >= X) continue;
        p.c = mv.size();
        int8_t D[48];
        mapOf(mv, D);
        for (int x = 0; x < 48; x++) {
          const int8_t c = U[k][x];
          const int to = D[x];
          const bool home = AX[faceK(to)] == 0 ? c == grp(to) : (c / 6 == orbK(to) && AX[c % 6] == AX[faceK(to)]);
          if (!home) { fprintf(stderr, "a path does not solve its class's state\n"); exit(1); }
        }
        for (int gi = 0; gi < 4; gi++)
          for (int j = 0; j < 8; j++) {
            const int to = D[inv0[axO[GO[gi]][GA[gi]][j]]];
            if (orbK(to) != GO[gi] || AX[faceK(to)] != GA[gi]) { fprintf(stderr, "a path moves a 4op piece off its axis\n"); exit(1); }
            p.M[gi * 8 + j] = axIdx[to];
          }
        ok.push_back(p);
      }
      std::stable_sort(ok.begin(), ok.end(), [](const Path &a, const Path &b) { return a.c < b.c; });
      for (auto &p : ok) if (seen[k].insert(hashM(p.M)).second) P.push_back(p);
    };
    const double tc = now();
    {
      std::vector<std::vector<Path>> cand(K);
      for (auto &r : recs) { Path p{}; p.kind = 1, p.a = r[1], p.y = r[2]; cand[r[0]].push_back(p); }
      std::vector<std::array<uint32_t, 3>>().swap(recs);
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          for (size_t k; (k = ni++) < K;) {
            if (skip[k]) continue;
            std::array<uint8_t, 32> M0;
            for (int gi = 0; gi < 4; gi++) for (int j = 0; j < 8; j++) M0[gi * 8 + j] = j;
            seen[k].insert(hashM(M0));  // L0 itself
            std::vector<Path> c;
            for (size_t s = 0; s < syms.size(); s++) {
              int8_t img[48];
              image(syms[s], U[k].data(), img);
              uint32_t m[2];
              if (masks3(img, m) && m[0] == cs[k][0] && m[1] == cs[k][1]) { Path p{}; p.kind = 0, p.y = s; c.push_back(p); }
            }
            std::sort(cand[k].begin(), cand[k].end(), [](const Path &a, const Path &b) { return a.a != b.a ? a.a < b.a : a.y < b.y; });
            c.insert(c.end(), cand[k].begin(), cand[k].end());
            std::vector<Path>().swap(cand[k]);
            buildPaths(k, c);
          }
        });
      for (auto &x : th) x.join();
    }
    tCpu += now() - tc;
    // check: classes ks on their states (all bad states, or the listed ones)
    std::vector<std::vector<uint32_t>> leftIdx(K);
    std::vector<std::vector<char>> used(K);
    std::vector<size_t> from(K, 0);  // first path of the current pass
    // one GPU run; false (nothing changed) when the left states overflow the buffer
    auto runCheckOnce = [&](const std::vector<size_t> &ks, bool listed) {
      const double tk = now();
      std::vector<ClsInfo> ci(K, {0, 0, 0, 0, 0xFFFFFFFFu, 0, 0, 0});
      size_t np = 0, nu = 0, ni = 0;
      for (size_t k : ks) {
        const size_t n = paths[k].size() - from[k];
        ci[k].path0 = np, ci[k].npath = n, np += n;
        ci[k].used0 = nu, nu += (n + 31) / 32;
        if (listed) ci[k].list0 = ni, ci[k].nstates = leftIdx[k].size(), ni += leftIdx[k].size();
        else ci[k].nstates = cntAt[std::min(std::max(X - (int)L0[k].size() + 1, lowT + 1), B.W + 1)];
      }
      if (np > pathsCap) { pathsCap = np * 2 + 16; bPaths = zbuf(pathsCap * 32); bPLen = zbuf(pathsCap); }
      if (nu > usedCap) { usedCap = nu * 2 + 16; bUsed = zbuf(usedCap * 4); }
      if (ni > idxCap || !bIdx) { idxCap = ni * 2 + 16; bIdx = zbuf(idxCap * 4); }
      if (K > ciCap || !bClsInfo) { ciCap = K; bClsInfo = zbuf(K * sizeof(ClsInfo)); }
      uint8_t *pp = (uint8_t *)[bPaths contents], *pl = (uint8_t *)[bPLen contents];
      uint32_t *ix = (uint32_t *)[bIdx contents];
      uint32_t mx = 0;
      for (size_t k : ks) {
        for (size_t i = 0; i < ci[k].npath; i++) { memcpy(pp + (ci[k].path0 + i) * 32, paths[k][from[k] + i].M.data(), 32); pl[ci[k].path0 + i] = paths[k][from[k] + i].c; }
        if (listed) memcpy(ix + ci[k].list0, leftIdx[k].data(), leftIdx[k].size() * 4);
        mx = std::max(mx, ci[k].nstates);
      }
      memset([bUsed contents], 0, nu * 4);
      memcpy([bClsInfo contents], ci.data(), K * sizeof(ClsInfo));
      *(uint32_t *)[bLeft contents] = 0;
      CP P = {(uint32_t)X, leftCap};
      if (mx) {
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
        [ce setComputePipelineState:psCheck];
        [ce setBuffer:bSt offset:0 atIndex:0];
        [ce setBuffer:bD4 offset:0 atIndex:1];
        [ce setBuffer:bPaths offset:0 atIndex:2];
        [ce setBuffer:bPLen offset:0 atIndex:3];
        [ce setBuffer:bClsInfo offset:0 atIndex:4];
        [ce setBuffer:bUsed offset:0 atIndex:5];
        [ce setBuffer:bLeft offset:0 atIndex:6];
        [ce setBuffer:bLeftOut offset:0 atIndex:7];
        [ce setBytes:&P length:sizeof P atIndex:8];
        [ce setBuffer:bR70 offset:0 atIndex:9];
        [ce setBuffer:bIdx offset:0 atIndex:10];
        [ce dispatchThreads:MTLSizeMake(mx, K, 1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(psCheck.maxTotalThreadsPerThreadgroup, 256), 1, 1)];
        [ce endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU check failed: %s\n", [[cb.error description] UTF8String]); exit(1); }
      }
      const uint32_t nl = *(uint32_t *)[bLeft contents];
      tChk += now() - tk;
      if (nl > leftCap) return false;
      for (size_t k : ks) { if (listed) leftIdx[k].clear(); else nBad += ci[k].nstates; }
      const uint32_t *lo = (const uint32_t *)[bLeftOut contents];
      for (uint32_t j = 0; j < nl; j++) leftIdx[lo[2 * j]].push_back(lo[2 * j + 1]);
      const uint32_t *uw = (const uint32_t *)[bUsed contents];
      for (size_t k : ks) {
        used[k].resize(paths[k].size(), 0);
        for (size_t i = 0; i < ci[k].npath; i++) used[k][from[k] + i] = uw[ci[k].used0 + i / 32] >> (i % 32) & 1;
        std::sort(leftIdx[k].begin(), leftIdx[k].end());
      }
      return true;
    };
    // split the classes in halves until the left states fit (2026-11-04: the AX 40 broad pass's last
    // batches, lines of 22-23 with up to 13.5M bad states each, overflowed 8M)
    std::function<void(const std::vector<size_t> &, bool)> runCheck = [&](const std::vector<size_t> &ks, bool listed) {
      if (ks.empty() || runCheckOnce(ks, listed)) return;
      if (ks.size() == 1) {
        // 2026-11-05: the AX 38 broad pass's last batches had a class with more than 2^24 states left
        const uint32_t nl = *(uint32_t *)[bLeft contents];
        leftCap = nl + nl / 4;
        bLeftOut = zbuf((size_t)leftCap * 8);
        fprintf(stderr, "  left states overflow for one class (%u): leftCap -> %u\n", nl, leftCap);
        if (!runCheckOnce(ks, listed)) { fprintf(stderr, "left states overflow after growing leftCap\n"); exit(1); }
        return;
      }
      fprintf(stderr, "  left states overflow: splitting %zu classes\n", ks.size());
      const std::vector<size_t> a(ks.begin(), ks.begin() + ks.size() / 2), b(ks.begin() + ks.size() / 2, ks.end());
      runCheck(a, listed), runCheck(b, listed);
    };
    std::vector<size_t> ks;
    for (size_t k = 0; k < K; k++) if (!skip[k]) ks.push_back(k);
    runCheck(ks, false);
    // --deep: two actions then the line, for classes with states left (one class at a time)
    if (DEEP >= 0) {
      for (size_t k : ks) {
        if (leftIdx[k].empty()) continue;
        nDeep++;
        const double td0 = now();
        const size_t leftBefore = leftIdx[k].size();
        const int c0 = L0[k].size();
        // a1: |a1| + d3(a1(u)) <= c0 + SL + DEEP
        std::vector<std::array<uint16_t, 2>> one = {cs[k]};
        std::vector<uint8_t> lim1 = {(uint8_t)(c0 + SL + DEEP)};
        std::vector<std::array<uint32_t, 3>> r1;
        runActs(one, lim1, r1, nDeepAct);
        std::sort(r1.begin(), r1.end(), [](const auto &a, const auto &b) { return a[1] < b[1]; });
        // a2 on each a1(u): |a1| + |a2| + d3 <= c0 + SL
        std::vector<std::array<uint16_t, 2>> ys;
        std::vector<uint8_t> lim2;
        std::vector<uint32_t> a1of;
        for (auto &r : r1) {
          const int l = c0 + SL - (int)actLen[r[1]];
          if (l < 1) continue;
          std::array<uint16_t, 2> y;
          actOn(r[1], cs[k].data(), y.data());
          ys.push_back(y), lim2.push_back(l), a1of.push_back(r[1]);
        }
        std::vector<std::array<uint32_t, 3>> r2;
        runActs(ys, lim2, r2, nDeepAct);
        const double tc2 = now();
        std::vector<Path> cand;
        for (auto &r : r2) { Path p{}; p.kind = 2, p.a = a1of[r[0]], p.a2 = r[1], p.y = r[2]; cand.push_back(p); }
        std::vector<std::array<uint32_t, 3>>().swap(r2);
        std::sort(cand.begin(), cand.end(), [](const Path &a, const Path &b) { return a.a != b.a ? a.a < b.a : a.a2 != b.a2 ? a.a2 < b.a2 : a.y < b.y; });
        from[k] = paths[k].size();
        buildPaths(k, cand);
        tCpu += now() - tc2;
        runCheck({k}, true);
        // a line per class of the two-action pass that took 5 s or more, else at most one a minute (progress
        // without flooding the log: a broad pass sends ~10^5 classes here)
        static double lastDeepRep = 0;
        if (now() - td0 >= 5 || now() - lastDeepRep >= 60) lastDeepRep = now(), fprintf(stderr, "  deep %ld: class %zu of %zu (%d moves): %zu -> %zu states left, %zu paths, %.1fs: %s\n", nDeep, b0 + k + 1, todo.size(), c0,
                leftBefore, leftIdx[k].size(), paths[k].size() - from[k], now() - td0, leftIdx[k].empty() ? "covered" : "NOT covered");
      }
    }
    // blocks
    for (size_t k = 0; k < K; k++) {
      if (skip[k] == 1) { nDup++; continue; }
      if (skip[k] == 2) { nOk++; fprintf(out, "%s\n\n", text(L0[k]).c_str()); continue; }
      nPaths += paths[k].size();
      int nu = 0;
      std::string blk = text(L0[k]) + "\n";
      for (size_t i = 0; i < paths[k].size(); i++) if (used[k][i]) blk += text(pathMoves(k, paths[k][i])) + "\n", nu++;
      blk += "\n";
      fputs(blk.c_str(), out);
      usedHist[std::min(nu, 63)]++;
      const size_t lf = leftIdx[k].size();
      nLeft += lf;
      leftHist[lf == 0 ? 0 : lf < 10 ? 1 : lf < 100 ? 2 : lf < 1000 ? 3 : 4]++;
      if (lf) { nFail++; if (leftOut) fprintf(leftOut, "%s\n", text(L0[k]).c_str()); }
      else nOk++;
    }
    if (now() - lastRep > 60 || b0 + K == todo.size()) {
      fprintf(stderr, "  %zu/%zu classes, %ld not covered, %ld to the deep pass; GPU acts %.0fs, CPU paths %.0fs, GPU check %.0fs; %.0fs\n", b0 + K, todo.size(), nFail, nDeep, tAct, tCpu, tChk, now() - t0);
      lastRep = now();
    }
    }
  }
  if (outZst) { if (pclose(out) != 0) { fprintf(stderr, "zstd failed writing %s\n", outFile.c_str()); return 1; } }
  else fclose(out);
  if (leftOut) fclose(leftOut);
  fprintf(stderr, "classes: %ld covered, %ld not (%ld duplicate lines skipped); bad positions %lu, left %lu; %.1f alternatives per class (%.0fs)\n",
          nOk, nFail, nDup, (unsigned long)nBad, (unsigned long)nLeft, todo.size() ? (double)nPaths / todo.size() : 0.0, now() - t0);
  fprintf(stderr, "classes by positions left: 0: %ld, 1-9: %ld, 10-99: %ld, 100-999: %ld, 1000+: %ld\n", leftHist[0], leftHist[1], leftHist[2], leftHist[3], leftHist[4]);
  fprintf(stderr, "alternatives used per class:");
  for (int i = 0; i < 64; i++) if (usedHist[i]) fprintf(stderr, " %d:%ld", i, usedHist[i]);
  fprintf(stderr, "\n");
  printf("3ax 4op X=%d: %ld of %zu classes not covered\n", X, nFail, todo.size() - nDup);
  return 0;
}
